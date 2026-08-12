#include "Towel/UsedTowelBinActor.h"

#include "Engine/World.h"
#include "Interaction/PlayerCarryComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Towel/TowelBasketActor.h"
#include "Towel/TowelCirculationSubsystem.h"
#include "Towel/TowelInventoryComponent.h"
#include "Towel/Presentation/TowelStackVisualComponent.h"
#include "Towel/TowelTransferSubsystem.h"
#include "Towel/WorldUsedTowelActor.h"

#define LOCTEXT_NAMESPACE "UsedTowelBinActor"

AUsedTowelBinActor::AUsedTowelBinActor()
{
	FacilityType = EBathhouseFacilityType::TowelBasket;
	Inventory = CreateDefaultSubobject<UTowelInventoryComponent>(TEXT("TowelInventory"));
	Inventory->ConfigureDefaults(ETowelState::None, 0, 20);
	TowelPresentationVisual = CreateDefaultSubobject<UTowelStackVisualComponent>(TEXT("TowelPresentationVisual"));
	TowelPresentationVisual->SetupAttachment(GetRootComponent());
}

void AUsedTowelBinActor::BeginPlay()
{
	Super::BeginPlay();
	TowelPresentationVisual->BindInventorySource(Inventory);
}

void AUsedTowelBinActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	TowelPresentationVisual->UnbindInventorySource();
	Super::EndPlay(EndPlayReason);
}

FPlayerInteractionQuery AUsedTowelBinActor::QueryInteraction(const FPlayerInteractionContext& Context) const
{
	FPlayerInteractionQuery Query;
	Query.bVisible = true;
	Query.TargetName = LOCTEXT("UsedBin", "사용 수건통");
	Query.ActionName = LOCTEXT("TakeOne", "수건 한 장 담기");
	Query.bSecondaryVisible = true;
	Query.SecondaryActionName = LOCTEXT("TakeMax", "가능한 만큼 담기");
	const ATowelBasketActor* Basket = Context.CarryComponent
		? Cast<ATowelBasketActor>(Context.CarryComponent->GetHeldObject())
		: nullptr;
	const bool bCanTransfer = Basket && Inventory->GetSnapshot().Count > 0
		&& Basket->GetInventory()->CanAccept(ETowelState::Used);
	Query.bCanInteract = bCanTransfer;
	Query.bCanSecondaryInteract = bCanTransfer;
	if (!bCanTransfer)
	{
		Query.FailureReason = Inventory->GetSnapshot().Count <= 0
			? LOCTEXT("BinEmpty", "수건통이 비어 있습니다.")
			: LOCTEXT("UsedBasketRequired", "비어 있거나 사용한 수건용 바구니가 필요합니다.");
		Query.SecondaryFailureReason = Query.FailureReason;
	}
	return Query;
}

FPlayerInteractionResult AUsedTowelBinActor::ExecuteInteraction(const FPlayerInteractionContext& Context)
{
	return TransferToHeldBasket(Context, 1, EPlayerInteractionIntent::Primary);
}

FPlayerInteractionResult AUsedTowelBinActor::ExecuteSecondaryInteraction(const FPlayerInteractionContext& Context)
{
	return TransferToHeldBasket(Context, MAX_int32, EPlayerInteractionIntent::Secondary);
}

bool AUsedTowelBinActor::TryStageOverflowTowel(AWorldUsedTowelActor*& OutTowel)
{
	OutTowel = nullptr;
	UWorld* World = GetWorld();
	UTowelCirculationSubsystem* Subsystem = World ? World->GetSubsystem<UTowelCirculationSubsystem>() : nullptr;
	if (!World || !Subsystem || !WorldUsedTowelClass)
	{
		return false;
	}
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(TowelOverflowPlacement), true, this);
	FRandomStream RandomStream(FMath::Rand());
	for (int32 Attempt = 0; Attempt < PlacementAttempts; ++Attempt)
	{
		const float Angle = RandomStream.FRandRange(0.0f, 2.0f * PI);
		const float Radius = RandomStream.FRandRange(
			FMath::Min(OverflowMinRadius, OverflowMaxRadius),
			FMath::Max(OverflowMinRadius, OverflowMaxRadius));
		const FVector Offset(FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Radius, 0.0f);
		const FVector Start = GetActorLocation() + Offset + FVector::UpVector * FloorTraceDistance * 0.5f;
		const FVector End = Start - FVector::UpVector * FloorTraceDistance;
		FHitResult FloorHit;
		if (!World->LineTraceSingleByChannel(FloorHit, Start, End, FloorTraceChannel, QueryParams)
			|| !Subsystem->IsWorldTowelLocationClear(FloorHit.ImpactPoint, TowelSpacing))
		{
			continue;
		}
		FCollisionObjectQueryParams ClearanceObjects;
		ClearanceObjects.AddObjectTypesToQuery(ECC_Pawn);
		ClearanceObjects.AddObjectTypesToQuery(ECC_WorldDynamic);
		if (World->OverlapAnyTestByObjectType(
			FloorHit.ImpactPoint + FVector::UpVector * 5.0f,
			FQuat::Identity,
			ClearanceObjects,
			FCollisionShape::MakeSphere(PawnClearance),
			QueryParams))
		{
			continue;
		}
		if (PawnClearance > 0.0f && World->OverlapBlockingTestByChannel(
			FloorHit.ImpactPoint + FVector::UpVector * (PawnClearance + 2.0f),
			FQuat::Identity,
			FloorTraceChannel,
			FCollisionShape::MakeSphere(PawnClearance),
			QueryParams))
		{
			continue;
		}
		const FTransform SpawnTransform(
			FRotationMatrix::MakeFromZ(FloorHit.ImpactNormal).ToQuat(),
			FloorHit.ImpactPoint);
		AWorldUsedTowelActor* Staged = World->SpawnActorDeferred<AWorldUsedTowelActor>(
			WorldUsedTowelClass,
			SpawnTransform,
			this,
			nullptr,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!Staged)
		{
			continue;
		}
		Staged->SetPreferredBin(this);
		OutTowel = Cast<AWorldUsedTowelActor>(UGameplayStatics::FinishSpawningActor(Staged, SpawnTransform));
		return IsValid(OutTowel);
	}
	return false;
}

FPlayerInteractionResult AUsedTowelBinActor::TransferToHeldBasket(
	const FPlayerInteractionContext& Context,
	const int32 RequestedCount,
	const EPlayerInteractionIntent Intent)
{
	ATowelBasketActor* Basket = Context.CarryComponent
		? Cast<ATowelBasketActor>(Context.CarryComponent->GetHeldObject())
		: nullptr;
	UTowelTransferSubsystem* Transfer = GetWorld()->GetSubsystem<UTowelTransferSubsystem>();
	if (!Basket || !Transfer)
	{
		return FPlayerInteractionResult::Failed(LOCTEXT("UsedBasketRequired", "사용한 수건용 바구니가 필요합니다."), Intent);
	}
	FTowelTransferRequest Request;
	Request.Source = Inventory;
	Request.Destination = Basket->GetInventory();
	Request.RequestedCount = RequestedCount;
	Request.ExpectedSourceRevision = Inventory->GetSnapshot().Revision;
	Request.ExpectedDestinationRevision = Basket->GetInventory()->GetSnapshot().Revision;
	const FTowelTransferResult Result = Transfer->TryTransfer(Request);
	return Result.bSucceeded
		? FPlayerInteractionResult::Succeeded(Intent)
		: FPlayerInteractionResult::Failed(LOCTEXT("TransferFailed", "수건을 바구니로 옮길 수 없습니다."), Intent);
}

#undef LOCTEXT_NAMESPACE
