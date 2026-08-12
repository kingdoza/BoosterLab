#include "Towel/TowelProcessingMachineActor.h"

#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Towel/TowelBasketActor.h"
#include "Towel/TowelInventoryComponent.h"
#include "Towel/TowelMachineControlComponent.h"
#include "Towel/TowelTransferPortComponent.h"

#define LOCTEXT_NAMESPACE "TowelProcessingMachineActor"

ATowelProcessingMachineActor::ATowelProcessingMachineActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
	Inventory = CreateDefaultSubobject<UTowelInventoryComponent>(TEXT("TowelInventory"));
	Inventory->ConfigureDefaults(ETowelState::None, 0, 10);
	TransferPort = CreateDefaultSubobject<UTowelTransferPortComponent>(TEXT("TransferPort"));
	TransferPort->SetupAttachment(SceneRoot);
	MachineControl = CreateDefaultSubobject<UTowelMachineControlComponent>(TEXT("MachineControl"));
	MachineControl->SetupAttachment(SceneRoot);
}

void ATowelProcessingMachineActor::BeginPlay()
{
	Super::BeginPlay();
	Inventory->OnInventoryChanged.AddDynamic(this, &ATowelProcessingMachineActor::HandleInventoryChanged);
}

void ATowelProcessingMachineActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(ProcessingTimerHandle);
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
