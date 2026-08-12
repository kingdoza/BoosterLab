#include "Cleaning/WaterStainActor.h"

#include "Cleaning/CleaningWorldSubsystem.h"
#include "Cleaning/StainSpawnZoneActor.h"
#include "Cleaning/WetMopActor.h"
#include "Components/SphereComponent.h"
#include "Interaction/PlayerCarryComponent.h"

#define LOCTEXT_NAMESPACE "WaterStainActor"

AWaterStainActor::AWaterStainActor()
{
	PrimaryActorTick.bCanEverTick = false;
	InteractionCollision = CreateDefaultSubobject<USphereComponent>(TEXT("InteractionCollision"));
	SetRootComponent(InteractionCollision);
	InteractionCollision->InitSphereRadius(45.0f);
	InteractionCollision->SetCollisionProfileName(TEXT("BlockAllDynamic"));
}

void AWaterStainActor::SetSpawnZone(AStainSpawnZoneActor* InSpawnZone)
{
	SpawnZone = InSpawnZone;
}

void AWaterStainActor::BeginPlay()
{
	Super::BeginPlay();
	if (UCleaningWorldSubsystem* Subsystem = GetWorld()->GetSubsystem<UCleaningWorldSubsystem>())
	{
		Subsystem->RegisterStain(this);
	}
}

void AWaterStainActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		if (UCleaningWorldSubsystem* Subsystem = World->GetSubsystem<UCleaningWorldSubsystem>())
		{
			Subsystem->UnregisterStain(this);
		}
	}
	ActiveCleaner = nullptr;
	OnCleaningStarted.Clear();
	OnCleaningProgressChanged.Clear();
	OnCleaningCancelled.Clear();
	OnCleaningCompleted.Clear();
	Super::EndPlay(EndPlayReason);
}

FPlayerInteractionQuery AWaterStainActor::QueryInteraction(const FPlayerInteractionContext& Context) const
{
	FPlayerInteractionQuery Query;
	if (CleaningState == EStainCleaningState::Removed)
	{
		return Query;
	}
	Query.bVisible = true;
	Query.TargetName = LOCTEXT("WaterStain", "물 얼룩");
	Query.ActionName = LOCTEXT("CleanWaterStain", "물걸레질");
	Query.PrimaryActivationMode = EPlayerInteractionActivationMode::Hold;
	Query.HoldProgress = GetCleaningProgress();
	const bool bCleanerAvailable = !ActiveCleaner || ActiveCleaner == Context.Interactor;
	Query.bCanInteract = HasRequiredMop(Context) && bCleanerAvailable;
	if (!HasRequiredMop(Context))
	{
		Query.FailureReason = LOCTEXT("WetMopRequired", "물걸레가 필요합니다.");
	}
	else if (!bCleanerAvailable)
	{
		Query.FailureReason = LOCTEXT("AlreadyCleaning", "다른 사람이 청소 중입니다.");
	}
	return Query;
}

FPlayerInteractionResult AWaterStainActor::ExecuteInteraction(const FPlayerInteractionContext& Context)
{
	return FPlayerInteractionResult::Failed(LOCTEXT("HoldRequired", "길게 눌러 청소해야 합니다."));
}

bool AWaterStainActor::BeginHoldInteraction(
	const FPlayerInteractionContext& Context,
	FText& OutFailureReason)
{
	const FPlayerInteractionQuery Query = QueryInteraction(Context);
	if (!Query.bCanInteract || !IsValid(Context.Interactor))
	{
		OutFailureReason = Query.FailureReason;
		return false;
	}
	if (CleaningState == EStainCleaningState::Cleaning && ActiveCleaner == Context.Interactor)
	{
		return true;
	}
	ActiveCleaner = Context.Interactor;
	CleaningElapsedSeconds = 0.0f;
	CleaningState = EStainCleaningState::Cleaning;
	OnCleaningStarted.Broadcast(ActiveCleaner);
	OnCleaningProgressChanged.Broadcast(0.0f);
	return true;
}

FPlayerHoldInteractionUpdate AWaterStainActor::UpdateHoldInteraction(
	const FPlayerInteractionContext& Context,
	const float DeltaTime)
{
	FPlayerHoldInteractionUpdate Update;
	if (CleaningState != EStainCleaningState::Cleaning
		|| ActiveCleaner != Context.Interactor
		|| !HasRequiredMop(Context))
	{
		ResetCleaning(true);
		Update.FailureReason = LOCTEXT("CleaningInvalidated", "청소 조건이 유지되지 않았습니다.");
		return Update;
	}
	CleaningElapsedSeconds += FMath::Max(0.0f, DeltaTime);
	Update.Progress = GetCleaningProgress();
	OnCleaningProgressChanged.Broadcast(Update.Progress);
	if (Update.Progress >= 1.0f)
	{
		CompleteCleaning();
		Update.State = EPlayerHoldInteractionState::Succeeded;
		Update.Progress = 1.0f;
		return Update;
	}
	Update.State = EPlayerHoldInteractionState::Running;
	return Update;
}

void AWaterStainActor::CancelHoldInteraction(const FPlayerInteractionContext& Context)
{
	if (CleaningState == EStainCleaningState::Cleaning && ActiveCleaner == Context.Interactor)
	{
		ResetCleaning(true);
	}
}

float AWaterStainActor::GetCleaningProgress() const
{
	return CleaningState == EStainCleaningState::Removed
		? 1.0f
		: FMath::Clamp(CleaningElapsedSeconds / FMath::Max(0.1f, RemovalDurationSeconds), 0.0f, 1.0f);
}

bool AWaterStainActor::HasRequiredMop(const FPlayerInteractionContext& Context) const
{
	return Context.CarryComponent && Cast<AWetMopActor>(Context.CarryComponent->GetHeldObject()) != nullptr;
}

void AWaterStainActor::ResetCleaning(const bool bNotify)
{
	if (CleaningState != EStainCleaningState::Cleaning)
	{
		return;
	}
	CleaningState = EStainCleaningState::Idle;
	CleaningElapsedSeconds = 0.0f;
	ActiveCleaner = nullptr;
	OnCleaningProgressChanged.Broadcast(0.0f);
	if (bNotify)
	{
		OnCleaningCancelled.Broadcast();
	}
}

void AWaterStainActor::CompleteCleaning()
{
	if (bTerminalCommitted)
	{
		return;
	}
	bTerminalCommitted = true;
	CleaningState = EStainCleaningState::Removed;
	CleaningElapsedSeconds = FMath::Max(0.1f, RemovalDurationSeconds);
	ActiveCleaner = nullptr;
	InteractionCollision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (UCleaningWorldSubsystem* Subsystem = GetWorld()->GetSubsystem<UCleaningWorldSubsystem>())
	{
		Subsystem->UnregisterStain(this);
	}
	OnCleaningCompleted.Broadcast();
	SetLifeSpan(0.05f);
}

#undef LOCTEXT_NAMESPACE
