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
#include "Placement/FacilityActorConversionTransaction.h"

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

	if (FacilityPlacement && FacilityPlacement->GetMode() == EPlaceableFacilityMode::Placed
		&& !FacilityPlacement->IsStagedPlacement())
	{
		FText FailureReason;
		if (!RegisterPlacedDomain(FailureReason))
		{
			FacilityPlacement->SetPlacedDomainActive(false);
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
	Super::FellOutOfWorld(DamageType);
}

FPlayerInteractionQuery ABathhouseFacilityActor::QueryInteraction(const FPlayerInteractionContext& Context) const
{
	(void)Context;
	return FPlayerInteractionQuery();
}

FPlayerInteractionResult ABathhouseFacilityActor::ExecuteInteraction(const FPlayerInteractionContext& Context)
{
	(void)Context;
	return FPlayerInteractionResult::Failed(LOCTEXT("LegacyPlacedCarryDisabled", "배치 설비 Actor는 직접 들 수 없습니다."));
}

FPlayerInteractionQuery ABathhouseFacilityActor::MergeSupplementalInteractionQuery(
	const FPlayerInteractionQuery& BaseQuery) const
{
	FPlayerInteractionQuery Query = BaseQuery;
	if (!SupportsFacilityActorConversion()
		|| !FacilityPlacement || FacilityPlacement->GetMode() != EPlaceableFacilityMode::Placed
		|| FacilityPlacement->IsStagedPlacement())
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
	if (!SupportsFacilityActorConversion())
	{
		return FFacilityPlacementTransactionResult::Failed(
			EFacilityPlacementFailureCode::WrongMode,
			LOCTEXT("FacilityConversionUnsupported", "이 설비는 배치 및 회수 대상이 아닙니다."));
	}
	FText FailureReason;
	if (!FacilityPlacement || !FacilityPlacement->IsOperational(FailureReason)
		|| !FacilityPlacement->GetDefinition()->ValidateRuntime(FailureReason))
	{
		return FFacilityPlacementTransactionResult::Failed(EFacilityPlacementFailureCode::InvalidComponents, FailureReason);
	}
	if (FacilityPlacement->GetMode() != EPlaceableFacilityMode::Placed
		|| !FacilityPlacement->IsStagedPlacement())
	{
		return FFacilityPlacementTransactionResult::Failed(EFacilityPlacementFailureCode::WrongMode, LOCTEXT("NotStaged", "새로 생성된 staged 설비만 설치할 수 있습니다."));
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
	if (!SupportsFacilityActorConversion())
	{
		return FFacilityPlacementTransactionResult::Failed(
			EFacilityPlacementFailureCode::WrongMode,
			LOCTEXT("FacilityRecoveryUnsupported", "이 설비는 회수 대상이 아닙니다."));
	}
	FText FailureReason;
	if (!FacilityPlacement || !FacilityPlacement->IsOperational(FailureReason)
		|| FacilityPlacement->GetMode() != EPlaceableFacilityMode::Placed
		|| FacilityPlacement->IsStagedPlacement()
		|| !FacilityPlacement->IsPlacedDomainActive()
		|| !FacilityPlacement->GetDefinition()->ValidateRuntime(FailureReason))
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
	FTransform ItemTransform;
	if (!FFacilityActorConversionTransaction::ValidateRecoveryCandidate(
		*const_cast<ABathhouseFacilityActor*>(this), ItemTransform, FailureReason))
	{
		return FFacilityPlacementTransactionResult::Failed(EFacilityPlacementFailureCode::Blocked, FailureReason);
	}
	return FFacilityPlacementTransactionResult::Succeeded();
}

bool ABathhouseFacilityActor::CommitPlaceableFacilityMode(
	const EPlaceableFacilityMode NewMode,
	FText& OutFailureReason)
{
	if (NewMode == EPlaceableFacilityMode::Placed && FacilityPlacement
		&& FacilityPlacement->GetMode() == EPlaceableFacilityMode::Placed)
	{
		return true;
	}
	OutFailureReason = LOCTEXT("LegacyModeTransitionDisabled", "배치 설비 Actor의 legacy Placed/Packaged 전환은 비활성화되었습니다.");
	return false;
}

FText ABathhouseFacilityActor::GetPhysicalCarryDisplayName() const
{
	return LOCTEXT("FacilityPackage", "포장 설비");
}

FTransform ABathhouseFacilityActor::GetHeldTransform() const { return FTransform::Identity; }
bool ABathhouseFacilityActor::CanBeTakenBy(const UPlayerCarryComponent& Carry, FText& OutFailureReason) const
{
	(void)Carry;
	OutFailureReason = LOCTEXT("PlacedFacilityNotCarryable", "배치 설비 Actor는 직접 들 수 없습니다.");
	return false;
}
bool ABathhouseFacilityActor::HandleTakenBy(UPlayerCarryComponent& Carry, USceneComponent* HeldAnchor)
{
	(void)Carry;
	(void)HeldAnchor;
	return false;
}
bool ABathhouseFacilityActor::CanFreeDrop(FText& OutFailureReason) const { OutFailureReason = LOCTEXT("PlacedFacilityNoDrop", "배치 설비 Actor는 내려놓을 수 없습니다."); return false; }
UPrimitiveComponent* ABathhouseFacilityActor::GetPhysicalCarryPrimitive() const { return nullptr; }
float ABathhouseFacilityActor::GetThrowImpulseStrength() const { return FacilityPlacement ? FacilityPlacement->GetThrowImpulseStrength() : 120.0f; }
float ABathhouseFacilityActor::GetUpwardThrowImpulseStrength() const { return FacilityPlacement ? FacilityPlacement->GetUpwardThrowImpulseStrength() : 15.0f; }
AActor* ABathhouseFacilityActor::GetAssignedPhysicalCarryFixedSlot() const { return nullptr; }
bool ABathhouseFacilityActor::TryBindPhysicalCarryFixedSlot(AActor& SlotActor, FText& OutFailureReason) { OutFailureReason = LOCTEXT("PlacedFacilityNoFixedSlot", "배치 설비 Actor는 고정 슬롯을 지원하지 않습니다."); return false; }
void ABathhouseFacilityActor::ClearPhysicalCarryFixedSlotBinding(AActor& ExpectedSlot) {}
void ABathhouseFacilityActor::NotifyPhysicalCarryFixedSlotBindingConflict() {}
bool ABathhouseFacilityActor::IsStoredInAssignedPhysicalCarryFixedSlot() const { return false; }
bool ABathhouseFacilityActor::NotifyTakenFromFixedSlotCommitted(UPlayerCarryComponent& Carry, AActor& SlotActor) { return false; }
bool ABathhouseFacilityActor::NotifyStoredInFixedSlotCommitted(UPlayerCarryComponent& Carry, AActor& SlotActor) { return false; }
bool ABathhouseFacilityActor::NotifyRecoveredToFixedSlotCommitted(AActor& SlotActor) { return false; }
void ABathhouseFacilityActor::NotifyFixedSlotDestroyed(AActor& SlotActor) {}
bool ABathhouseFacilityActor::NotifyPhysicalDropCommitted(UPlayerCarryComponent& Carry) { return false; }
void ABathhouseFacilityActor::PublishPhysicalCarryCommit(EPhysicalCarryCommitTransition Transition) {}
void ABathhouseFacilityActor::RecoverPhysicalCarryable(UPlayerCarryComponent* PreviousCarry) {}

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
