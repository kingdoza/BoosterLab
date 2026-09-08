#include "Facility/BathhouseFacilityActor.h"

#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Facility/BathWaterStateComponent.h"
#include "Facility/BathhouseFacilitySlotComponent.h"
#include "Facility/BathhouseFacilitySubsystem.h"
#include "Facility/LockerCapacitySubsystem.h"
#include "Interaction/PhysicalCarryFixedSlot.h"
#include "Interaction/PlayerCarryComponent.h"
#include "NavModifierComponent.h"
#include "Placement/FacilityPlacementComponent.h"
#include "Placement/FacilityPlacementDefinition.h"
#include "Placement/FacilityPlacementZoneActor.h"

#define LOCTEXT_NAMESPACE "BathhouseFacilityActor"

ABathhouseFacilityActor::ABathhouseFacilityActor()
{
	PrimaryActorTick.bCanEverTick = false;
	PackagePhysicalRoot = CreateDefaultSubobject<UBoxComponent>(TEXT("PackagePhysicalRoot"));
	SetRootComponent(PackagePhysicalRoot);
	PackagePhysicalRoot->SetBoxExtent(FVector(5.0f));
	PackagePhysicalRoot->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	PackagePhysicalRoot->BodyInstance.bUseCCD = true;
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SceneRoot->SetupAttachment(PackagePhysicalRoot);
	PlacementFootprint = CreateDefaultSubobject<UBoxComponent>(TEXT("PlacementFootprint"));
	PlacementFootprint->SetupAttachment(SceneRoot);
	PlacementFootprint->SetBoxExtent(FVector(5.0f, 5.0f, 50.0f));
	PlacementFootprint->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PlacementFootprint->SetCanEverAffectNavigation(false);
	PlacementNavModifier = CreateDefaultSubobject<UNavModifierComponent>(TEXT("PlacementNavModifier"));
	FacilityPlacement = CreateDefaultSubobject<UFacilityPlacementComponent>(TEXT("FacilityPlacement"));
	FacilityPlacement->Configure(PlacementFootprint, PackagePhysicalRoot, PlacementNavModifier);
	BathWaterState = CreateDefaultSubobject<UBathWaterStateComponent>(TEXT("BathWaterState"));
}

void ABathhouseFacilityActor::BeginPlay()
{
	Super::BeginPlay();

	GetComponents<UBathhouseFacilitySlotComponent>(FacilitySlots);
	for (UBathhouseFacilitySlotComponent* Slot : FacilitySlots)
	{
		if (Slot)
		{
			Slot->OnSlotStateChanged.AddDynamic(this, &ABathhouseFacilityActor::HandleSlotStateChanged);
		}
	}
	if (UBathhouseFacilitySubsystem* Subsystem = GetWorld()->GetSubsystem<UBathhouseFacilitySubsystem>())
	{
		ExpansionAuthorityChangedHandle = Subsystem->OnExpansionAuthorityChanged.AddUObject(
			this,
			&ABathhouseFacilityActor::HandleExpansionAuthorityChanged);
	}

	if (FacilityPlacement && FacilityPlacement->GetMode() == EPlaceableFacilityMode::Placed)
	{
		FText FailureReason;
		if (!RegisterPlacedDomain(FailureReason))
		{
			UE_LOG(LogTemp, Error, TEXT("Facility %s could not register its placed state: %s"), *GetName(), *FailureReason.ToString());
		}
	}
}

void ABathhouseFacilityActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	bEndingPlay = true;
	if (UBathhouseFacilitySubsystem* Subsystem = GetWorld() ? GetWorld()->GetSubsystem<UBathhouseFacilitySubsystem>() : nullptr)
	{
		if (ExpansionAuthorityChangedHandle.IsValid())
		{
			Subsystem->OnExpansionAuthorityChanged.Remove(ExpansionAuthorityChangedHandle);
		}
	}
	ExpansionAuthorityChangedHandle.Reset();
	UnregisterPlacedDomain(EndPlayReason == EEndPlayReason::Destroyed);

	for (UBathhouseFacilitySlotComponent* Slot : FacilitySlots)
	{
		if (Slot)
		{
			Slot->OnSlotStateChanged.RemoveDynamic(this, &ABathhouseFacilityActor::HandleSlotStateChanged);
			Slot->ForceRelease();
		}
	}

	Super::EndPlay(EndPlayReason);
}

void ABathhouseFacilityActor::FellOutOfWorld(const UDamageType& DamageType)
{
	if (FacilityPlacement && FacilityPlacement->GetMode() == EPlaceableFacilityMode::Packaged)
	{
		FacilityPlacement->RestoreLastSafePackagedWorld();
		return;
	}
	Super::FellOutOfWorld(DamageType);
}

FPlayerInteractionQuery ABathhouseFacilityActor::QueryInteraction(const FPlayerInteractionContext& Context) const
{
	FPlayerInteractionQuery Query;
	if (!FacilityPlacement || FacilityPlacement->GetMode() != EPlaceableFacilityMode::Packaged)
	{
		return Query;
	}
	Query.bVisible = true;
	Query.TargetName = GetPhysicalCarryDisplayName();
	Query.ActionName = LOCTEXT("TakePackage", "포장 설비 들기");
	FText FailureReason;
	Query.bCanInteract = Context.CarryComponent && CanBeTakenBy(*Context.CarryComponent, FailureReason);
	Query.FailureReason = FailureReason;
	return Query;
}

FPlayerInteractionResult ABathhouseFacilityActor::ExecuteInteraction(const FPlayerInteractionContext& Context)
{
	FText FailureReason;
	return Context.CarryComponent && Context.CarryComponent->TryTakePhysicalObject(this, FailureReason)
		? FPlayerInteractionResult::Succeeded()
		: FPlayerInteractionResult::Failed(FailureReason.IsEmpty() ? LOCTEXT("TakeFailed", "포장 설비를 들 수 없습니다.") : FailureReason);
}

FPlayerInteractionQuery ABathhouseFacilityActor::MergeSupplementalInteractionQuery(
	const FPlayerInteractionQuery& BaseQuery) const
{
	FPlayerInteractionQuery Query = BaseQuery;
	if (!FacilityPlacement || FacilityPlacement->GetMode() != EPlaceableFacilityMode::Placed)
	{
		return Query;
	}

	const FFacilityPlacementTransactionResult Recovery = QueryFacilityRecovery();
	Query.bRecoveryVisible = true;
	Query.bCanRecover = Recovery.bSucceeded;
	Query.RecoveryActionName = LOCTEXT("RecoverFacility", "설비 회수");
	Query.RecoveryFailureReason = Recovery.FailureReason;
	Query.RecoveryProgress = 0.0f;
	return Query;
}

FFacilityPlacementTransactionResult ABathhouseFacilityActor::QueryFacilityPlacement(
	const FTransform& CandidateTransform,
	const AFacilityPlacementZoneActor& Zone) const
{
	(void)CandidateTransform;
	FText FailureReason;
	if (!FacilityPlacement || !FacilityPlacement->IsOperational(FailureReason))
	{
		return FFacilityPlacementTransactionResult::Failed(EFacilityPlacementFailureCode::InvalidComponents, FailureReason);
	}
	if (FacilityPlacement->GetMode() != EPlaceableFacilityMode::Packaged)
	{
		return FFacilityPlacementTransactionResult::Failed(EFacilityPlacementFailureCode::WrongMode, LOCTEXT("NotPackaged", "포장 상태의 설비만 설치할 수 있습니다."));
	}
	if (!Zone.IsDefinitionAllowed(*FacilityPlacement->GetDefinition()))
	{
		return FFacilityPlacementTransactionResult::Failed(EFacilityPlacementFailureCode::NoCompatibleZone, LOCTEXT("ZoneTagMismatch", "이 구역에는 해당 설비를 설치할 수 없습니다."));
	}
	if (FacilityType == EBathhouseFacilityType::ClothesLocker)
	{
		if (!ValidatePlacedDomain(FailureReason))
		{
			return FFacilityPlacementTransactionResult::Failed(EFacilityPlacementFailureCode::ExpansionLimit, FailureReason);
		}
	}
	return FFacilityPlacementTransactionResult::Succeeded();
}

FFacilityPlacementTransactionResult ABathhouseFacilityActor::QueryFacilityRecovery() const
{
	FText FailureReason;
	if (!FacilityPlacement || !FacilityPlacement->IsOperational(FailureReason)
		|| FacilityPlacement->GetMode() != EPlaceableFacilityMode::Placed)
	{
		return FFacilityPlacementTransactionResult::Failed(EFacilityPlacementFailureCode::WrongMode,
			FailureReason.IsEmpty() ? LOCTEXT("NotPlaced", "설치된 설비만 회수할 수 있습니다.") : FailureReason);
	}
	for (const UBathhouseFacilitySlotComponent* Slot : FacilitySlots)
	{
		if (!Slot || Slot->GetSlotState() != EBathhouseFacilitySlotState::Available)
		{
			return FFacilityPlacementTransactionResult::Failed(EFacilityPlacementFailureCode::DomainCondition, LOCTEXT("FacilityInUse", "사용 또는 예약 중인 설비는 회수할 수 없습니다."));
		}
	}
	if (FacilityType == EBathhouseFacilityType::Bath && (!BathWaterState || !BathWaterState->IsEmpty()))
	{
		return FFacilityPlacementTransactionResult::Failed(EFacilityPlacementFailureCode::DomainCondition, LOCTEXT("BathNotEmpty", "욕조의 물이 비어 있어야 회수할 수 있습니다."));
	}
	if (FacilityType == EBathhouseFacilityType::ClothesLocker)
	{
		const ULockerCapacitySubsystem* Lockers = GetWorld() ? GetWorld()->GetSubsystem<ULockerCapacitySubsystem>() : nullptr;
		if (!Lockers || !Lockers->CanRemoveLockerBank(this, FailureReason))
		{
			return FFacilityPlacementTransactionResult::Failed(EFacilityPlacementFailureCode::DomainCondition, FailureReason);
		}
	}
	if (!FacilityPlacement->CanEnablePackagedCollision(FailureReason))
	{
		return FFacilityPlacementTransactionResult::Failed(EFacilityPlacementFailureCode::Blocked, FailureReason);
	}
	return FFacilityPlacementTransactionResult::Succeeded();
}

bool ABathhouseFacilityActor::CommitPlaceableFacilityMode(
	const EPlaceableFacilityMode NewMode,
	FText& OutFailureReason)
{
	if (bEndingPlay || !FacilityPlacement)
	{
		return false;
	}
	if (FacilityPlacement->GetMode() == NewMode)
	{
		return true;
	}
	if (!FacilityPlacement->BeginTransition(OutFailureReason))
	{
		return false;
	}
	const EPlaceableFacilityMode Previous = FacilityPlacement->GetMode();
	const FTransform PreviousTransform = GetActorTransform();
	bool bSucceeded = true;
	if (NewMode == EPlaceableFacilityMode::Packaged)
	{
		FTransform RecoveryDropTransform;
		if (!FacilityPlacement->GetRecoveryDropTransform(RecoveryDropTransform, OutFailureReason)
			|| !FacilityPlacement->CanEnablePackagedCollision(OutFailureReason))
		{
			FacilityPlacement->EndTransition();
			return false;
		}
		UnregisterPlacedDomain(false, false);
		bSucceeded = SetActorTransform(
			RecoveryDropTransform,
			false,
			nullptr,
			ETeleportType::TeleportPhysics);
		if (!bSucceeded)
		{
			OutFailureReason = LOCTEXT("RecoveryDropMoveFailed", "설비를 회수 위치로 옮길 수 없습니다.");
		}
		else
		{
			bSucceeded = FacilityPlacement->ApplyMode(NewMode, true, OutFailureReason, false);
		}
		if (bSucceeded && PackagePhysicalRoot)
		{
			PackagePhysicalRoot->SetPhysicsLinearVelocity(FVector::ZeroVector);
			PackagePhysicalRoot->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
		}
		if (!bSucceeded)
		{
			FText Ignored;
			SetActorTransform(PreviousTransform, false, nullptr, ETeleportType::TeleportPhysics);
			FacilityPlacement->ApplyMode(Previous, false, Ignored, false);
			RegisterPlacedDomain(Ignored, false);
		}
	}
	else
	{
		bSucceeded = ValidatePlacedDomain(OutFailureReason)
			&& RegisterPlacedDomain(OutFailureReason, false)
			&& FacilityPlacement->ApplyMode(NewMode, false, OutFailureReason, false);
		if (!bSucceeded)
		{
			UnregisterPlacedDomain(false, false);
			if (FacilityPlacement->GetMode() != Previous)
			{
				FText Ignored;
				FacilityPlacement->ApplyMode(Previous, true, Ignored, false);
			}
		}
	}
	if (!bSucceeded)
	{
		FacilityPlacement->EndTransition();
		return false;
	}

	TWeakObjectPtr<ABathhouseFacilityActor> Self(this);
	FacilityPlacement->PublishModeChanged(Previous, NewMode);
	if (!Self.IsValid() || bEndingPlay)
	{
		return true;
	}
	if (UBathhouseFacilitySubsystem* Facilities = GetWorld()->GetSubsystem<UBathhouseFacilitySubsystem>())
	{
		Facilities->NotifyFacilityAvailabilityChanged(FacilityType);
	}
	if (!Self.IsValid() || bEndingPlay)
	{
		return true;
	}
	if (FacilityType == EBathhouseFacilityType::ClothesLocker)
	{
		if (ULockerCapacitySubsystem* Lockers = GetWorld()->GetSubsystem<ULockerCapacitySubsystem>())
		{
			Lockers->PublishCapacityMutation();
		}
	}
	if (!Self.IsValid() || bEndingPlay)
	{
		return true;
	}
	FacilityPlacement->EndTransition();
	return true;
}

FText ABathhouseFacilityActor::GetPhysicalCarryDisplayName() const
{
	return LOCTEXT("FacilityPackage", "포장 설비");
}

FTransform ABathhouseFacilityActor::GetHeldTransform() const { return FacilityPlacement ? FacilityPlacement->GetHeldTransform() : FTransform::Identity; }
bool ABathhouseFacilityActor::CanBeTakenBy(const UPlayerCarryComponent& Carry, FText& OutFailureReason) const
{
	if (!FacilityPlacement || FacilityPlacement->GetMode() != EPlaceableFacilityMode::Packaged)
	{
		OutFailureReason = LOCTEXT("NotPackage", "이 설비는 포장 상태가 아닙니다.");
		return false;
	}
	if (!Carry.IsHandEmpty())
	{
		OutFailureReason = LOCTEXT("HandOccupied", "이미 다른 물건을 들고 있습니다.");
		return false;
	}
	return FacilityPlacement->IsOperational(OutFailureReason);
}
bool ABathhouseFacilityActor::HandleTakenBy(UPlayerCarryComponent& Carry, USceneComponent* HeldAnchor)
{
	if (!HeldAnchor || !FacilityPlacement || FacilityPlacement->GetMode() != EPlaceableFacilityMode::Packaged) return false;
	FacilityPlacement->ApplyHeldPresentation(*HeldAnchor, GetHeldTransform());
	return true;
}
bool ABathhouseFacilityActor::CanFreeDrop(FText& OutFailureReason) const { return FacilityPlacement && FacilityPlacement->GetMode() == EPlaceableFacilityMode::Packaged; }
UPrimitiveComponent* ABathhouseFacilityActor::GetPhysicalCarryPrimitive() const { return PackagePhysicalRoot; }
float ABathhouseFacilityActor::GetThrowImpulseStrength() const { return FacilityPlacement ? FacilityPlacement->GetThrowImpulseStrength() : 120.0f; }
float ABathhouseFacilityActor::GetUpwardThrowImpulseStrength() const { return FacilityPlacement ? FacilityPlacement->GetUpwardThrowImpulseStrength() : 15.0f; }
AActor* ABathhouseFacilityActor::GetAssignedPhysicalCarryFixedSlot() const { return FacilityPlacement ? FacilityPlacement->GetAssignedFixedSlot() : nullptr; }
bool ABathhouseFacilityActor::TryBindPhysicalCarryFixedSlot(AActor& SlotActor, FText& OutFailureReason) { return FacilityPlacement && FacilityPlacement->TryBindFixedSlot(SlotActor, OutFailureReason); }
void ABathhouseFacilityActor::ClearPhysicalCarryFixedSlotBinding(AActor& ExpectedSlot) { if (FacilityPlacement) FacilityPlacement->ClearFixedSlot(ExpectedSlot); }
void ABathhouseFacilityActor::NotifyPhysicalCarryFixedSlotBindingConflict() { if (FacilityPlacement) FacilityPlacement->MarkFixedSlotBindingConflict(); }
bool ABathhouseFacilityActor::IsStoredInAssignedPhysicalCarryFixedSlot() const { return FacilityPlacement && FacilityPlacement->IsStoredInFixedSlot(); }
bool ABathhouseFacilityActor::NotifyTakenFromFixedSlotCommitted(UPlayerCarryComponent& Carry, AActor& SlotActor) { return FacilityPlacement && FacilityPlacement->GetMode() == EPlaceableFacilityMode::Packaged; }
bool ABathhouseFacilityActor::NotifyStoredInFixedSlotCommitted(UPlayerCarryComponent& Carry, AActor& SlotActor) { return FacilityPlacement && FacilityPlacement->GetMode() == EPlaceableFacilityMode::Packaged; }
bool ABathhouseFacilityActor::NotifyRecoveredToFixedSlotCommitted(AActor& SlotActor) { return FacilityPlacement && FacilityPlacement->GetMode() == EPlaceableFacilityMode::Packaged; }
void ABathhouseFacilityActor::NotifyFixedSlotDestroyed(AActor& SlotActor) { ClearPhysicalCarryFixedSlotBinding(SlotActor); }
bool ABathhouseFacilityActor::NotifyPhysicalDropCommitted(UPlayerCarryComponent& Carry)
{
	FText FailureReason;
	return FacilityPlacement && FacilityPlacement->ApplyMode(EPlaceableFacilityMode::Packaged, true, FailureReason);
}
void ABathhouseFacilityActor::PublishPhysicalCarryCommit(EPhysicalCarryCommitTransition Transition) {}
void ABathhouseFacilityActor::RecoverPhysicalCarryable(UPlayerCarryComponent* PreviousCarry)
{
	if (IPhysicalCarryFixedSlot* Slot = Cast<IPhysicalCarryFixedSlot>(GetAssignedPhysicalCarryFixedSlot()); Slot && Slot->TryRecoverAssignedPhysicalCarryItem(*this))
	{
		return;
	}
	if (FacilityPlacement) FacilityPlacement->RestoreLastSafePackagedWorld();
}

void ABathhouseFacilityActor::HandleSlotStateChanged(
	UBathhouseFacilitySlotComponent* Slot,
	const EBathhouseFacilitySlotState PreviousState,
	const EBathhouseFacilitySlotState NewState)
{
	OnSlotReservationChanged(Slot, NewState);

	if (NewState == EBathhouseFacilitySlotState::Occupied)
	{
		OnSlotUseStarted(Slot, Slot ? Slot->GetCurrentUser() : nullptr);
	}
	else if (PreviousState == EBathhouseFacilitySlotState::Occupied)
	{
		OnSlotUseEnded(Slot, Slot ? Slot->GetCurrentUser() : nullptr);
	}

	if (UBathhouseFacilitySubsystem* Subsystem = GetWorld()->GetSubsystem<UBathhouseFacilitySubsystem>())
	{
		Subsystem->NotifyFacilityAvailabilityChanged(FacilityType);
	}
}

#undef LOCTEXT_NAMESPACE
