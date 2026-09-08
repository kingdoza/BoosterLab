#include "Towel/TowelProcessingMachineActor.h"

#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Interaction/PhysicalCarryFixedSlot.h"
#include "Interaction/PlayerCarryComponent.h"
#include "NavModifierComponent.h"
#include "Placement/FacilityPlacementComponent.h"
#include "Placement/FacilityPlacementZoneActor.h"
#include "Towel/TowelBasketActor.h"
#include "Towel/TowelInventoryComponent.h"
#include "Towel/TowelMachineControlComponent.h"
#include "Towel/Presentation/TowelPileVisualComponent.h"
#include "Towel/TowelTransferPortComponent.h"

#define LOCTEXT_NAMESPACE "TowelProcessingMachineActor"

ATowelProcessingMachineActor::ATowelProcessingMachineActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
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
	Inventory = CreateDefaultSubobject<UTowelInventoryComponent>(TEXT("TowelInventory"));
	Inventory->ConfigureDefaults(ETowelState::None, 0, 10);
	TransferPort = CreateDefaultSubobject<UTowelTransferPortComponent>(TEXT("TransferPort"));
	TransferPort->SetupAttachment(SceneRoot);
	MachineControl = CreateDefaultSubobject<UTowelMachineControlComponent>(TEXT("MachineControl"));
	MachineControl->SetupAttachment(SceneRoot);
	TowelPresentationVisual = CreateDefaultSubobject<UTowelPileVisualComponent>(TEXT("TowelPresentationVisual"));
	TowelPresentationVisual->SetupAttachment(SceneRoot);
}

void ATowelProcessingMachineActor::FellOutOfWorld(const UDamageType& DamageType)
{
	if (FacilityPlacement && FacilityPlacement->GetMode() == EPlaceableFacilityMode::Packaged)
	{
		FacilityPlacement->RestoreLastSafePackagedWorld();
		return;
	}
	Super::FellOutOfWorld(DamageType);
}

FPlayerInteractionQuery ATowelProcessingMachineActor::QueryInteraction(const FPlayerInteractionContext& Context) const
{
	FPlayerInteractionQuery Query;
	if (!FacilityPlacement || FacilityPlacement->GetMode() != EPlaceableFacilityMode::Packaged) return Query;
	Query.bVisible = true;
	Query.TargetName = GetPhysicalCarryDisplayName();
	Query.ActionName = LOCTEXT("TakeMachine", "포장 기계 들기");
	FText FailureReason;
	Query.bCanInteract = Context.CarryComponent && CanBeTakenBy(*Context.CarryComponent, FailureReason);
	Query.FailureReason = FailureReason;
	return Query;
}

FPlayerInteractionResult ATowelProcessingMachineActor::ExecuteInteraction(const FPlayerInteractionContext& Context)
{
	FText FailureReason;
	return Context.CarryComponent && Context.CarryComponent->TryTakePhysicalObject(this, FailureReason)
		? FPlayerInteractionResult::Succeeded()
		: FPlayerInteractionResult::Failed(FailureReason.IsEmpty() ? LOCTEXT("TakeMachineFailed", "포장 기계를 들 수 없습니다.") : FailureReason);
}

FPlayerInteractionQuery ATowelProcessingMachineActor::MergeSupplementalInteractionQuery(
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

FFacilityPlacementTransactionResult ATowelProcessingMachineActor::QueryFacilityPlacement(
	const FTransform& CandidateTransform,
	const AFacilityPlacementZoneActor& Zone) const
{
	(void)CandidateTransform;
	FText FailureReason;
	if (!FacilityPlacement || !FacilityPlacement->IsOperational(FailureReason))
		return FFacilityPlacementTransactionResult::Failed(EFacilityPlacementFailureCode::InvalidComponents, FailureReason);
	if (FacilityPlacement->GetMode() != EPlaceableFacilityMode::Packaged)
		return FFacilityPlacementTransactionResult::Failed(EFacilityPlacementFailureCode::WrongMode, LOCTEXT("MachineNotPackaged", "포장 상태의 기계만 설치할 수 있습니다."));
	if (!Zone.IsDefinitionAllowed(*FacilityPlacement->GetDefinition()))
		return FFacilityPlacementTransactionResult::Failed(EFacilityPlacementFailureCode::NoCompatibleZone, LOCTEXT("MachineZoneMismatch", "이 구역에는 해당 기계를 설치할 수 없습니다."));
	return FFacilityPlacementTransactionResult::Succeeded();
}

FFacilityPlacementTransactionResult ATowelProcessingMachineActor::QueryFacilityRecovery() const
{
	FText FailureReason;
	if (!FacilityPlacement || !FacilityPlacement->IsOperational(FailureReason)
		|| FacilityPlacement->GetMode() != EPlaceableFacilityMode::Placed)
		return FFacilityPlacementTransactionResult::Failed(EFacilityPlacementFailureCode::WrongMode,
			FailureReason.IsEmpty() ? LOCTEXT("MachineNotPlaced", "설치된 기계만 회수할 수 있습니다.") : FailureReason);
	if (!Inventory || Inventory->GetSnapshot().Count != 0 || MachineState != ETowelMachineState::Waiting)
		return FFacilityPlacementTransactionResult::Failed(EFacilityPlacementFailureCode::DomainCondition, LOCTEXT("MachineNotEmpty", "기계가 비어 있고 대기 상태여야 회수할 수 있습니다."));
	if (!FacilityPlacement->CanEnablePackagedCollision(FailureReason))
		return FFacilityPlacementTransactionResult::Failed(EFacilityPlacementFailureCode::Blocked, FailureReason);
	return FFacilityPlacementTransactionResult::Succeeded();
}

bool ATowelProcessingMachineActor::CommitPlaceableFacilityMode(const EPlaceableFacilityMode NewMode, FText& OutFailureReason)
{
	if (IsActorBeingDestroyed() || !FacilityPlacement) return false;
	if (FacilityPlacement->GetMode() == NewMode) return true;
	if (!FacilityPlacement->BeginTransition(OutFailureReason)) return false;
	FTransform RecoveryDropTransform;
	if (NewMode == EPlaceableFacilityMode::Packaged
		&& (!FacilityPlacement->GetRecoveryDropTransform(RecoveryDropTransform, OutFailureReason)
			|| !FacilityPlacement->CanEnablePackagedCollision(OutFailureReason)))
	{
		FacilityPlacement->EndTransition();
		return false;
	}
	const EPlaceableFacilityMode PreviousMode = FacilityPlacement->GetMode();
	const FTransform PreviousTransform = GetActorTransform();
	bool bResult = true;
	if (NewMode == EPlaceableFacilityMode::Packaged)
	{
		bResult = SetActorTransform(
			RecoveryDropTransform,
			false,
			nullptr,
			ETeleportType::TeleportPhysics);
		if (!bResult)
		{
			OutFailureReason = LOCTEXT("RecoveryDropMoveFailed", "기계를 회수 위치로 옮길 수 없습니다.");
		}
	}
	if (bResult)
	{
		bResult = FacilityPlacement->ApplyMode(
			NewMode,
			NewMode == EPlaceableFacilityMode::Packaged,
			OutFailureReason,
			false);
	}
	if (bResult && NewMode == EPlaceableFacilityMode::Packaged && PackagePhysicalRoot)
	{
		PackagePhysicalRoot->SetPhysicsLinearVelocity(FVector::ZeroVector);
		PackagePhysicalRoot->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
	}
	if (bResult)
	{
		TWeakObjectPtr<ATowelProcessingMachineActor> Self(this);
		FacilityPlacement->PublishModeChanged(PreviousMode, NewMode);
		if (!Self.IsValid())
		{
			return true;
		}
	}
	else if (NewMode == EPlaceableFacilityMode::Packaged)
	{
		FText Ignored;
		SetActorTransform(PreviousTransform, false, nullptr, ETeleportType::TeleportPhysics);
		if (FacilityPlacement->GetMode() != PreviousMode)
		{
			FacilityPlacement->ApplyMode(PreviousMode, false, Ignored, false);
		}
	}
	FacilityPlacement->EndTransition();
	return bResult;
}

FText ATowelProcessingMachineActor::GetPhysicalCarryDisplayName() const { return LOCTEXT("TowelMachinePackage", "포장 수건 처리기"); }
FTransform ATowelProcessingMachineActor::GetHeldTransform() const { return FacilityPlacement ? FacilityPlacement->GetHeldTransform() : FTransform::Identity; }
bool ATowelProcessingMachineActor::CanBeTakenBy(const UPlayerCarryComponent& Carry, FText& OutFailureReason) const
{
	if (!FacilityPlacement || FacilityPlacement->GetMode() != EPlaceableFacilityMode::Packaged)
	{
		OutFailureReason = LOCTEXT("MachineNotPackage", "이 기계는 포장 상태가 아닙니다.");
		return false;
	}
	if (!Carry.IsHandEmpty())
	{
		OutFailureReason = LOCTEXT("MachineHandOccupied", "이미 다른 물건을 들고 있습니다.");
		return false;
	}
	return FacilityPlacement->IsOperational(OutFailureReason);
}
bool ATowelProcessingMachineActor::HandleTakenBy(UPlayerCarryComponent& Carry, USceneComponent* HeldAnchor)
{
	if (!HeldAnchor || !FacilityPlacement) return false;
	FacilityPlacement->ApplyHeldPresentation(*HeldAnchor, GetHeldTransform());
	return true;
}
bool ATowelProcessingMachineActor::CanFreeDrop(FText& OutFailureReason) const { return FacilityPlacement && FacilityPlacement->GetMode() == EPlaceableFacilityMode::Packaged; }
UPrimitiveComponent* ATowelProcessingMachineActor::GetPhysicalCarryPrimitive() const { return PackagePhysicalRoot; }
float ATowelProcessingMachineActor::GetThrowImpulseStrength() const { return FacilityPlacement ? FacilityPlacement->GetThrowImpulseStrength() : 120.0f; }
float ATowelProcessingMachineActor::GetUpwardThrowImpulseStrength() const { return FacilityPlacement ? FacilityPlacement->GetUpwardThrowImpulseStrength() : 15.0f; }
AActor* ATowelProcessingMachineActor::GetAssignedPhysicalCarryFixedSlot() const { return FacilityPlacement ? FacilityPlacement->GetAssignedFixedSlot() : nullptr; }
bool ATowelProcessingMachineActor::TryBindPhysicalCarryFixedSlot(AActor& SlotActor, FText& OutFailureReason) { return FacilityPlacement && FacilityPlacement->TryBindFixedSlot(SlotActor, OutFailureReason); }
void ATowelProcessingMachineActor::ClearPhysicalCarryFixedSlotBinding(AActor& ExpectedSlot) { if (FacilityPlacement) FacilityPlacement->ClearFixedSlot(ExpectedSlot); }
void ATowelProcessingMachineActor::NotifyPhysicalCarryFixedSlotBindingConflict() { if (FacilityPlacement) FacilityPlacement->MarkFixedSlotBindingConflict(); }
bool ATowelProcessingMachineActor::IsStoredInAssignedPhysicalCarryFixedSlot() const { return FacilityPlacement && FacilityPlacement->IsStoredInFixedSlot(); }
bool ATowelProcessingMachineActor::NotifyTakenFromFixedSlotCommitted(UPlayerCarryComponent& Carry, AActor& SlotActor) { return FacilityPlacement != nullptr; }
bool ATowelProcessingMachineActor::NotifyStoredInFixedSlotCommitted(UPlayerCarryComponent& Carry, AActor& SlotActor) { return FacilityPlacement != nullptr; }
bool ATowelProcessingMachineActor::NotifyRecoveredToFixedSlotCommitted(AActor& SlotActor) { return FacilityPlacement != nullptr; }
void ATowelProcessingMachineActor::NotifyFixedSlotDestroyed(AActor& SlotActor) { ClearPhysicalCarryFixedSlotBinding(SlotActor); }
bool ATowelProcessingMachineActor::NotifyPhysicalDropCommitted(UPlayerCarryComponent& Carry)
{
	FText FailureReason;
	return FacilityPlacement && FacilityPlacement->ApplyMode(EPlaceableFacilityMode::Packaged, true, FailureReason);
}
void ATowelProcessingMachineActor::PublishPhysicalCarryCommit(EPhysicalCarryCommitTransition Transition) {}
void ATowelProcessingMachineActor::RecoverPhysicalCarryable(UPlayerCarryComponent* PreviousCarry)
{
	if (IPhysicalCarryFixedSlot* Slot = Cast<IPhysicalCarryFixedSlot>(GetAssignedPhysicalCarryFixedSlot()); Slot && Slot->TryRecoverAssignedPhysicalCarryItem(*this)) return;
	if (FacilityPlacement) FacilityPlacement->RestoreLastSafePackagedWorld();
}

void ATowelProcessingMachineActor::BeginPlay()
{
	Super::BeginPlay();
	Inventory->OnInventoryChanged.AddDynamic(this, &ATowelProcessingMachineActor::HandleInventoryChanged);
	TowelPresentationVisual->BindInventorySource(Inventory);
}

void ATowelProcessingMachineActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(ProcessingTimerHandle);
	TowelPresentationVisual->UnbindInventorySource();
	Inventory->OnInventoryChanged.RemoveDynamic(this, &ATowelProcessingMachineActor::HandleInventoryChanged);
	Inventory->SetExternalMutationBlocked(false);
	OnMachineStateChanged.Clear();
	OnMachineProgressChanged.Clear();
	OnMachineContentsChanged.Clear();
	Super::EndPlay(EndPlayReason);
}

void ATowelProcessingMachineActor::Tick(const float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (MachineState == ETowelMachineState::Processing)
	{
		OnMachineProgressChanged.Broadcast(GetProcessingProgress());
	}
}

float ATowelProcessingMachineActor::GetProcessingProgress() const
{
	if (MachineState == ETowelMachineState::Complete)
	{
		return 1.0f;
	}
	if (MachineState != ETowelMachineState::Processing || !GetWorld())
	{
		return 0.0f;
	}
	const double Remaining = FMath::Max(0.0, ProcessingEndTime - GetWorld()->GetTimeSeconds());
	return FMath::Clamp(
		1.0f - static_cast<float>(Remaining / FMath::Max(0.1f, ProcessingDurationSeconds)),
		0.0f,
		1.0f);
}

ETowelState ATowelProcessingMachineActor::GetInputState() const
{
	return MachineKind == ETowelMachineKind::Washer ? ETowelState::Used : ETowelState::Wet;
}

ETowelState ATowelProcessingMachineActor::GetOutputState() const
{
	return MachineKind == ETowelMachineKind::Washer ? ETowelState::Wet : ETowelState::Clean;
}

bool ATowelProcessingMachineActor::CanStartProcessing(FText& OutFailureReason) const
{
	if (FacilityPlacement && FacilityPlacement->GetMode() == EPlaceableFacilityMode::Packaged)
	{
		OutFailureReason = LOCTEXT("MachinePackaged", "포장 상태의 기계는 사용할 수 없습니다.");
		return false;
	}
	const FTowelInventorySnapshot Snapshot = Inventory->GetSnapshot();
	if (MachineState != ETowelMachineState::Waiting)
	{
		OutFailureReason = LOCTEXT("MachineNotWaiting", "기계가 대기 상태가 아닙니다.");
		return false;
	}
	if (Snapshot.Count <= 0 || Snapshot.State != GetInputState())
	{
		OutFailureReason = LOCTEXT("WrongContents", "처리할 올바른 수건이 없습니다.");
		return false;
	}
	return true;
}

bool ATowelProcessingMachineActor::StartProcessing(FText& OutFailureReason)
{
	if (!CanStartProcessing(OutFailureReason))
	{
		return false;
	}
	Inventory->SetExternalMutationBlocked(true);
	ProcessingEndTime = GetWorld()->GetTimeSeconds() + FMath::Max(0.1f, ProcessingDurationSeconds);
	CommitMachineState(ETowelMachineState::Processing);
	SetActorTickEnabled(true);
	GetWorldTimerManager().SetTimer(
		ProcessingTimerHandle,
		this,
		&ATowelProcessingMachineActor::CompleteProcessing,
		FMath::Max(0.1f, ProcessingDurationSeconds),
		false);
	return true;
}

bool ATowelProcessingMachineActor::AllowsInventoryTransfer(
	const UTowelInventoryComponent* Source,
	const UTowelInventoryComponent* Destination,
	const FTowelInventorySnapshot& SourceSnapshot,
	const FTowelInventorySnapshot& DestinationSnapshot) const
{
	const bool bMachineIsSource = Source == Inventory;
	const bool bMachineIsDestination = Destination == Inventory;
	if (bMachineIsSource == bMachineIsDestination || MachineState == ETowelMachineState::Processing)
	{
		return false;
	}

	const UTowelInventoryComponent* BasketInventory = bMachineIsSource ? Destination : Source;
	const ATowelBasketActor* Basket = BasketInventory
		? Cast<ATowelBasketActor>(BasketInventory->GetOwner())
		: nullptr;
	if (!Basket || Basket->GetInventory() != BasketInventory)
	{
		return false;
	}

	if (bMachineIsDestination)
	{
		return MachineState == ETowelMachineState::Waiting
			&& SourceSnapshot.State == GetInputState()
			&& (DestinationSnapshot.Count == 0 || DestinationSnapshot.State == GetInputState());
	}

	return MachineState == ETowelMachineState::Complete
		&& SourceSnapshot.State == GetOutputState()
		&& (DestinationSnapshot.Count == 0 || DestinationSnapshot.State == GetOutputState());
}

void ATowelProcessingMachineActor::HandleCommittedInventoryTransfer(
	const UTowelInventoryComponent* Source)
{
	if (Source == Inventory
		&& MachineState == ETowelMachineState::Complete
		&& Inventory->GetSnapshot().Count == 0)
	{
		CommitMachineState(ETowelMachineState::Waiting);
		ProcessingEndTime = 0.0;
	}
}

void ATowelProcessingMachineActor::HandleInventoryChanged(
	const FTowelInventorySnapshot& Previous,
	const FTowelInventorySnapshot& Current,
	const int64 TransactionId)
{
	(void)Previous;
	(void)TransactionId;
	OnMachineContentsChanged.Broadcast(Current);
}

void ATowelProcessingMachineActor::CompleteProcessing()
{
	if (MachineState != ETowelMachineState::Processing)
	{
		return;
	}
	const FTowelInventorySnapshot Before = Inventory->GetSnapshot();
	if (Before.Count <= 0 || !Inventory->TryBeginTransaction())
	{
		Inventory->SetExternalMutationBlocked(false);
		CommitMachineState(ETowelMachineState::Waiting);
		SetActorTickEnabled(false);
		return;
	}
	Inventory->CommitInternal(GetOutputState(), Before.Count);
	Inventory->EndTransaction();
	Inventory->SetExternalMutationBlocked(false);
	CommitMachineState(ETowelMachineState::Complete);
	SetActorTickEnabled(false);
	Inventory->BroadcastCommit(Before, static_cast<int64>(FPlatformTime::Cycles64()));
	OnMachineProgressChanged.Broadcast(1.0f);
}

void ATowelProcessingMachineActor::CommitMachineState(const ETowelMachineState NewState)
{
	const ETowelMachineState Previous = MachineState;
	MachineState = NewState;
	if (Previous != NewState)
	{
		OnMachineStateChanged.Broadcast(Previous, NewState);
	}
}

#undef LOCTEXT_NAMESPACE
