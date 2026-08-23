#include "Cleaning/WaterStainActor.h"

#include "Cleaning/CleaningWorldSubsystem.h"
#include "Cleaning/StainSpawnZoneActor.h"
#include "Components/SceneComponent.h"
#include "Components/SphereComponent.h"
#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#define LOCTEXT_NAMESPACE "WaterStainActor"

AWaterStainActor::AWaterStainActor()
{
	PrimaryActorTick.bCanEverTick = false;
	InteractionCollision = CreateDefaultSubobject<USphereComponent>(TEXT("InteractionCollision"));
	SetRootComponent(InteractionCollision);
	InteractionCollision->InitSphereRadius(45.0f);
	InteractionCollision->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	StainVisualRoot = CreateDefaultSubobject<USceneComponent>(TEXT("StainVisualRoot"));
	StainVisualRoot->SetupAttachment(InteractionCollision);
}

void AWaterStainActor::SetSpawnZone(AStainSpawnZoneActor* InSpawnZone)
{
	SpawnZone = InSpawnZone;
}

void AWaterStainActor::ConfigureVisualVariationSeed(const int32 InSeed)
{
	if (bVisualVariationInitialized)
	{
		return;
	}
	VisualVariationSeed = InSeed;
	bHasConfiguredVisualVariationSeed = true;
}

void AWaterStainActor::BeginPlay()
{
	Super::BeginPlay();
	ResolveAndApplyVisualVariation();
	if (UCleaningWorldSubsystem* Subsystem = GetWorld()->GetSubsystem<UCleaningWorldSubsystem>())
	{
		Subsystem->RegisterStain(this);
	}
}

void AWaterStainActor::ResolveAndApplyVisualVariation()
{
	if (bVisualVariationInitialized)
	{
		return;
	}
	bVisualVariationInitialized = true;
	if (!bHasConfiguredVisualVariationSeed)
	{
		VisualVariationSeed = FMath::Rand();
	}

	FRandomStream RandomStream(VisualVariationSeed);
	TArray<UMaterialInterface*> ValidMaterials;
	ValidMaterials.Reserve(MaterialVariants.Num());
	for (UMaterialInterface* Material : MaterialVariants)
	{
		if (IsValid(Material))
		{
			ValidMaterials.Add(Material);
		}
	}
	if (ValidMaterials.Num() == 1)
	{
		SelectedMaterialVariant = ValidMaterials[0];
	}
	else if (ValidMaterials.Num() > 1)
	{
		SelectedMaterialVariant = ValidMaterials[RandomStream.RandRange(0, ValidMaterials.Num() - 1)];
	}

	constexpr float MinimumSafeVisualScale = 0.001f;
	const float MinimumX = FMath::Max(
		MinimumSafeVisualScale,
		FMath::Min(MinXYScale.X, MaxXYScale.X));
	const float MaximumX = FMath::Max(
		MinimumX,
		FMath::Max(MinXYScale.X, MaxXYScale.X));
	const float MinimumY = FMath::Max(
		MinimumSafeVisualScale,
		FMath::Min(MinXYScale.Y, MaxXYScale.Y));
	const float MaximumY = FMath::Max(
		MinimumY,
		FMath::Max(MinXYScale.Y, MaxXYScale.Y));
	SelectedXYScale.X = RandomStream.FRandRange(MinimumX, MaximumX);
	SelectedXYScale.Y = RandomStream.FRandRange(MinimumY, MaximumY);
	SelectedYawDegrees = RandomStream.FRandRange(
		FMath::Min(MinYawDegrees, MaxYawDegrees),
		FMath::Max(MinYawDegrees, MaxYawDegrees));

	if (StainVisualRoot)
	{
		StainVisualRoot->SetRelativeScale3D(FVector(SelectedXYScale.X, SelectedXYScale.Y, 1.0f));
		StainVisualRoot->SetRelativeRotation(FRotator(0.0f, SelectedYawDegrees, 0.0f));
	}
	if (SelectedMaterialVariant)
	{
		ApplyStainMaterialVariant(SelectedMaterialVariant);
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
	Query.bVisible = false;
	Query.TargetName = LOCTEXT("WaterStain", "물 얼룩");
	Query.bEquipmentUseVisible = true;
	Query.bCanEquipmentUse = false;
	Query.EquipmentActionName = LOCTEXT("CleanWaterStain", "물걸레질");
	Query.EquipmentFailureReason = LOCTEXT("WetMopRequired", "물걸레가 필요합니다.");
	Query.EquipmentActivationMode = EPlayerInteractionActivationMode::Hold;
	Query.EquipmentUseProgress = GetCleaningProgress();
	return Query;
}

FPlayerInteractionResult AWaterStainActor::ExecuteInteraction(const FPlayerInteractionContext& Context)
{
	return FPlayerInteractionResult::Failed(LOCTEXT("UseMopInput", "물걸레를 들고 마우스 왼쪽 버튼을 길게 누르세요."));
}

bool AWaterStainActor::QueryMopCleaning(
	AActor* Cleaner,
	FText& OutFailureReason,
	float& OutProgress) const
{
	OutFailureReason = FText::GetEmpty();
	OutProgress = GetCleaningProgress();
	if (CleaningState == EStainCleaningState::Removed || bTerminalCommitted)
	{
		OutFailureReason = LOCTEXT("StainAlreadyRemoved", "이미 제거된 물 얼룩입니다.");
		return false;
	}
	if (!IsValid(Cleaner))
	{
		OutFailureReason = LOCTEXT("InvalidCleaner", "청소 주체를 확인할 수 없습니다.");
		return false;
	}
	if (ActiveCleaner && ActiveCleaner != Cleaner)
	{
		OutFailureReason = LOCTEXT("AlreadyCleaning", "다른 사람이 청소 중입니다.");
		return false;
	}
	return true;
}

bool AWaterStainActor::BeginMopCleaning(AActor* Cleaner, FText& OutFailureReason)
{
	float Progress = 0.0f;
	if (!QueryMopCleaning(Cleaner, OutFailureReason, Progress))
	{
		return false;
	}
	if (CleaningState == EStainCleaningState::Cleaning && ActiveCleaner == Cleaner)
	{
		return true;
	}
	ActiveCleaner = Cleaner;
	CleaningElapsedSeconds = 0.0f;
	CleaningState = EStainCleaningState::Cleaning;
	OnCleaningStarted.Broadcast(ActiveCleaner);
	OnCleaningProgressChanged.Broadcast(0.0f);
	return true;
}

FHeldEquipmentUseUpdate AWaterStainActor::UpdateMopCleaning(AActor* Cleaner, const float DeltaTime)
{
	FHeldEquipmentUseUpdate Update;
	if (CleaningState != EStainCleaningState::Cleaning
		|| ActiveCleaner != Cleaner)
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

void AWaterStainActor::CancelMopCleaning(AActor* Cleaner)
{
	if (CleaningState == EStainCleaningState::Cleaning && ActiveCleaner == Cleaner)
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

#if WITH_EDITOR
EDataValidationResult AWaterStainActor::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	if (MinXYScale.X <= 0.0f || MinXYScale.Y <= 0.0f
		|| MaxXYScale.X <= 0.0f || MaxXYScale.Y <= 0.0f)
	{
		Context.AddWarning(LOCTEXT(
			"NonPositiveVisualScale",
			"Water stain XY scale ranges should be positive. Non-positive values are clamped at runtime."));
	}
	return Result == EDataValidationResult::NotValidated ? EDataValidationResult::Valid : Result;
}
#endif

#undef LOCTEXT_NAMESPACE
