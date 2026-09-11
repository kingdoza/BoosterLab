#include "Towel/TowelProcessingMachineActor.h"

#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Interaction/PhysicalCarryFixedSlot.h"
#include "Interaction/PlayerCarryComponent.h"
#include "Placement/FacilityPlacementComponent.h"
#include "Placement/FacilityPlacementSettings.h"
#include "Placement/FacilityPlacementDefinition.h"
#include "Placement/FacilityPlacementZoneActor.h"
#include "Placement/FacilityActorConversionTransaction.h"
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
	PackagePhysicalRoot->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PackagePhysicalRoot->SetCanEverAffectNavigation(false);
	PackagePhysicalRoot->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	PackagePhysicalRoot->BodyInstance.bUseCCD = true;
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SceneRoot->SetupAttachment(PackagePhysicalRoot);
	PlacementFootprint = CreateDefaultSubobject<UBoxComponent>(TEXT("PlacementFootprint"));
	PlacementFootprint->SetupAttachment(SceneRoot);
	PlacementFootprint->SetBoxExtent(FVector(5.0f, 5.0f, 50.0f));
	PlacementFootprint->SetRelativeLocation(FVector(0.0f, 0.0f, 50.0f));
	PlacementFootprint->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PlacementFootprint->SetCanEverAffectNavigation(false);
	FacilityPlacement = CreateDefaultSubobject<UFacilityPlacementComponent>(TEXT("FacilityPlacement"));
	FacilityPlacement->Configure(PlacementFootprint, PackagePhysicalRoot);
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
	Super::FellOutOfWorld(DamageType);
}

FPlayerInteractionQuery ATowelProcessingMachineActor::QueryInteraction(const FPlayerInteractionContext& Context) const
{
	(void)Context;
	return FPlayerInteractionQuery();
}

FPlayerInteractionResult ATowelProcessingMachineActor::ExecuteInteraction(const FPlayerInteractionContext& Context)
{
	(void)Context;
	return FPlayerInteractionResult::Failed(LOCTEXT("LegacyMachineCarryDisabled", "배치된 수건 처리기는 직접 들 수 없습니다."));
}

FPlayerInteractionQuery ATowelProcessingMachineActor::MergeSupplementalInteractionQuery(
	const FPlayerInteractionQuery& BaseQuery) const
{
	FPlayerInteractionQuery Query = BaseQuery;
	if (!FacilityPlacement || FacilityPlacement->GetMode() != EPlaceableFacilityMode::Placed
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

FFacilityPlacementTransactionResult ATowelProcessingMachineActor::QueryFacilityPlacement(
	const FTransform& CandidateTransform,
	const AFacilityPlacementZoneActor& Zone) const
{
	(void)CandidateTransform;
	FText FailureReason;
	if (!FacilityPlacement || !FacilityPlacement->IsOperational(FailureReason)
		|| !FacilityPlacement->GetDefinition()->ValidateRuntime(FailureReason))
		return FFacilityPlacementTransactionResult::Failed(EFacilityPlacementFailureCode::InvalidComponents, FailureReason);
	if (FacilityPlacement->GetMode() != EPlaceableFacilityMode::Placed
		|| !FacilityPlacement->IsStagedPlacement())
		return FFacilityPlacementTransactionResult::Failed(EFacilityPlacementFailureCode::WrongMode, LOCTEXT("MachineNotStaged", "새로 생성된 staged 처리기만 설치할 수 있습니다."));
	if (!Zone.IsDefinitionAllowed(*FacilityPlacement->GetDefinition()))
		return FFacilityPlacementTransactionResult::Failed(EFacilityPlacementFailureCode::NoCompatibleZone, LOCTEXT("MachineZoneMismatch", "이 구역에는 해당 기계를 설치할 수 없습니다."));
	return FFacilityPlacementTransactionResult::Succeeded();
}

FFacilityPlacementTransactionResult ATowelProcessingMachineActor::QueryFacilityRecovery() const
{
	FText FailureReason;
	if (!FacilityPlacement || !FacilityPlacement->IsOperational(FailureReason)
		|| FacilityPlacement->GetMode() != EPlaceableFacilityMode::Placed
		|| FacilityPlacement->IsStagedPlacement()
		|| !FacilityPlacement->IsPlacedDomainActive()
		|| !FacilityPlacement->GetDefinition()->ValidateRuntime(FailureReason))
		return FFacilityPlacementTransactionResult::Failed(EFacilityPlacementFailureCode::WrongMode,
			FailureReason.IsEmpty() ? LOCTEXT("MachineNotPlaced", "설치된 기계만 회수할 수 있습니다.") : FailureReason);
	if (!Inventory || Inventory->GetSnapshot().Count != 0 || MachineState != ETowelMachineState::Waiting)
		return FFacilityPlacementTransactionResult::Failed(EFacilityPlacementFailureCode::DomainCondition, LOCTEXT("MachineNotEmpty", "기계가 비어 있고 대기 상태여야 회수할 수 있습니다."));
	FTransform ItemTransform;
	if (!FFacilityActorConversionTransaction::ValidateRecoveryCandidate(
		*const_cast<ATowelProcessingMachineActor*>(this), ItemTransform, FailureReason))
		return FFacilityPlacementTransactionResult::Failed(EFacilityPlacementFailureCode::Blocked, FailureReason);
	return FFacilityPlacementTransactionResult::Succeeded();
}

bool ATowelProcessingMachineActor::CommitPlaceableFacilityMode(const EPlaceableFacilityMode NewMode, FText& OutFailureReason)
{
	if (NewMode == EPlaceableFacilityMode::Placed && FacilityPlacement
		&& FacilityPlacement->GetMode() == EPlaceableFacilityMode::Placed)
	{
		return true;
	}
	OutFailureReason = LOCTEXT("LegacyMachineModeDisabled", "수건 처리기의 legacy Placed/Packaged 전환은 비활성화되었습니다.");
	return false;
}

FText ATowelProcessingMachineActor::GetPhysicalCarryDisplayName() const { return LOCTEXT("TowelMachinePackage", "포장 수건 처리기"); }
FTransform ATowelProcessingMachineActor::GetHeldTransform() const
{
	return GetDefault<UFacilityPlacementSettings>()->GetFacilityItemHeldTransform();
}
bool ATowelProcessingMachineActor::CanBeTakenBy(const UPlayerCarryComponent& Carry, FText& OutFailureReason) const
{
	(void)Carry;
	OutFailureReason = LOCTEXT("PlacedMachineNotCarryable", "배치된 수건 처리기는 직접 들 수 없습니다.");
	return false;
}
bool ATowelProcessingMachineActor::HandleTakenBy(UPlayerCarryComponent& Carry, USceneComponent* HeldAnchor)
{
	(void)Carry;
	(void)HeldAnchor;
	return false;
}
bool ATowelProcessingMachineActor::CanFreeDrop(FText& OutFailureReason) const { OutFailureReason = LOCTEXT("PlacedMachineNoDrop", "배치된 수건 처리기는 내려놓을 수 없습니다."); return false; }
UPrimitiveComponent* ATowelProcessingMachineActor::GetPhysicalCarryPrimitive() const { return nullptr; }
float ATowelProcessingMachineActor::GetThrowImpulseStrength() const { return FacilityPlacement ? FacilityPlacement->GetThrowImpulseStrength() : 120.0f; }
float ATowelProcessingMachineActor::GetUpwardThrowImpulseStrength() const { return FacilityPlacement ? FacilityPlacement->GetUpwardThrowImpulseStrength() : 15.0f; }
AActor* ATowelProcessingMachineActor::GetAssignedPhysicalCarryFixedSlot() const { return nullptr; }
bool ATowelProcessingMachineActor::TryBindPhysicalCarryFixedSlot(AActor& SlotActor, FText& OutFailureReason) { OutFailureReason = LOCTEXT("PlacedMachineNoFixedSlot", "배치된 수건 처리기는 고정 슬롯을 지원하지 않습니다."); return false; }
void ATowelProcessingMachineActor::ClearPhysicalCarryFixedSlotBinding(AActor& ExpectedSlot) {}
void ATowelProcessingMachineActor::NotifyPhysicalCarryFixedSlotBindingConflict() {}
bool ATowelProcessingMachineActor::IsStoredInAssignedPhysicalCarryFixedSlot() const { return false; }
bool ATowelProcessingMachineActor::NotifyTakenFromFixedSlotCommitted(UPlayerCarryComponent& Carry, AActor& SlotActor) { return false; }
bool ATowelProcessingMachineActor::NotifyStoredInFixedSlotCommitted(UPlayerCarryComponent& Carry, AActor& SlotActor) { return false; }
bool ATowelProcessingMachineActor::NotifyRecoveredToFixedSlotCommitted(AActor& SlotActor) { return false; }
void ATowelProcessingMachineActor::NotifyFixedSlotDestroyed(AActor& SlotActor) {}
bool ATowelProcessingMachineActor::NotifyPhysicalDropCommitted(UPlayerCarryComponent& Carry) { return false; }
void ATowelProcessingMachineActor::PublishPhysicalCarryCommit(EPhysicalCarryCommitTransition Transition) {}
void ATowelProcessingMachineActor::RecoverPhysicalCarryable(UPlayerCarryComponent* PreviousCarry) {}

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
	if (FacilityPlacement && !FacilityPlacement->IsPlacedDomainActive())
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
	if (FacilityPlacement && !FacilityPlacement->IsPlacedDomainActive())
	{
		return false;
	}
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
