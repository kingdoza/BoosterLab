#include "Tests/BathhouseCleaningTowelTestProbe.h"

#include "Cleaning/WaterStainActor.h"
#include "Cleaning/WetMopActor.h"
#include "Interaction/PhysicalCarryFixedSlotActor.h"
#include "Interaction/PlayerCarryComponent.h"
#include "Interaction/PlayerInteractionComponent.h"
#include "Towel/TowelInventoryComponent.h"

bool ABathhousePhysicalCarryFailureProbeActor::NotifyTakenFromFixedSlotCommitted(
	UPlayerCarryComponent& Carry,
	AActor& SlotActor)
{
	return FailurePoint != EFailurePoint::FixedTake
		&& Super::NotifyTakenFromFixedSlotCommitted(Carry, SlotActor);
}

bool ABathhousePhysicalCarryFailureProbeActor::NotifyStoredInFixedSlotCommitted(
	UPlayerCarryComponent& Carry,
	AActor& SlotActor)
{
	return FailurePoint != EFailurePoint::FixedStore
		&& Super::NotifyStoredInFixedSlotCommitted(Carry, SlotActor);
}

bool ABathhousePhysicalCarryFailureProbeActor::NotifyPhysicalDropCommitted(
	UPlayerCarryComponent& Carry)
{
	return FailurePoint != EFailurePoint::FreeDrop
		&& Super::NotifyPhysicalDropCommitted(Carry);
}

void UBathhousePhysicalCarryCommitProbe::Bind(
	AWetMopActor* InMop,
	UPlayerCarryComponent* InCarry,
	APhysicalCarryFixedSlotActor* InSlot)
{
	Unbind();
	Mop = InMop;
	Carry = InCarry;
	Slot = InSlot;
	if (Mop)
	{
		Mop->OnHeldPresentationChanged.AddDynamic(
			this,
			&UBathhousePhysicalCarryCommitProbe::HandleItemPresentationChanged);
	}
	if (Carry)
	{
		Carry->OnHeldObjectChanged.AddDynamic(
			this,
			&UBathhousePhysicalCarryCommitProbe::HandleCarryPresentationChanged);
	}
	if (Slot)
	{
		Slot->OnSlotOccupancyChanged.AddDynamic(
			this,
			&UBathhousePhysicalCarryCommitProbe::HandleSlotPresentationChanged);
	}
}

void UBathhousePhysicalCarryCommitProbe::Unbind()
{
	if (IsValid(Mop))
	{
		Mop->OnHeldPresentationChanged.RemoveDynamic(
			this,
			&UBathhousePhysicalCarryCommitProbe::HandleItemPresentationChanged);
	}
	if (IsValid(Carry))
	{
		Carry->OnHeldObjectChanged.RemoveDynamic(
			this,
			&UBathhousePhysicalCarryCommitProbe::HandleCarryPresentationChanged);
	}
	if (IsValid(Slot))
	{
		Slot->OnSlotOccupancyChanged.RemoveDynamic(
			this,
			&UBathhousePhysicalCarryCommitProbe::HandleSlotPresentationChanged);
	}
	Mop = nullptr;
	Carry = nullptr;
	Slot = nullptr;
}

void UBathhousePhysicalCarryCommitProbe::ResetCounts()
{
	ItemPresentationCount = 0;
	CarryPresentationCount = 0;
	SlotPresentationCount = 0;
	LowLevelReleaseAttemptCount = 0;
	bLowLevelReleaseSucceeded = false;
}

void UBathhousePhysicalCarryCommitProbe::HandleItemPresentationChanged(const bool bIsHeld)
{
	++ItemPresentationCount;
	if (bIsHeld && bAttemptLowLevelReleaseWhenHeld && Carry && Mop)
	{
		++LowLevelReleaseAttemptCount;
		bLowLevelReleaseSucceeded = Carry->CommitReleasePhysicalObject(Mop);
	}
	if (bIsHeld && bDestroyItemOnHeldPresentation && IsValid(Mop))
	{
		Mop->Destroy();
	}
}

void UBathhousePhysicalCarryCommitProbe::HandleCarryPresentationChanged(AActor* HeldObject)
{
	(void)HeldObject;
	++CarryPresentationCount;
}

void UBathhousePhysicalCarryCommitProbe::HandleSlotPresentationChanged(const bool bIsOccupied)
{
	(void)bIsOccupied;
	++SlotPresentationCount;
}

void UBathhouseTowelAtomicCommitProbe::Bind(
	UTowelInventoryComponent* InObserved,
	UTowelInventoryComponent* InPeer)
{
	Unbind();
	Observed = InObserved;
	Peer = InPeer;
	if (Observed)
	{
		Observed->OnInventoryChanged.AddDynamic(
			this,
			&UBathhouseTowelAtomicCommitProbe::HandleInventoryChanged);
	}
}

void UBathhouseTowelAtomicCommitProbe::Unbind()
{
	if (Observed)
	{
		Observed->OnInventoryChanged.RemoveDynamic(
			this,
			&UBathhouseTowelAtomicCommitProbe::HandleInventoryChanged);
	}
	Observed = nullptr;
	Peer = nullptr;
}

void UBathhouseTowelAtomicCommitProbe::HandleInventoryChanged(
	const FTowelInventorySnapshot& Previous,
	const FTowelInventorySnapshot& Current,
	const int64 TransactionId)
{
	(void)Previous;
	++BroadcastCount;
	ObservedAtBroadcast = Current;
	PeerAtBroadcast = Peer ? Peer->GetSnapshot() : FTowelInventorySnapshot();
	ObservedTransactionId = TransactionId;
}

void UBathhouseCleaningCancelProbe::Bind(AWaterStainActor* InStain)
{
	Unbind();
	Stain = InStain;
	if (Stain)
	{
		Stain->OnCleaningCancelled.AddDynamic(
			this,
			&UBathhouseCleaningCancelProbe::HandleCleaningCancelled);
	}
}

void UBathhouseCleaningCancelProbe::Unbind()
{
	if (Stain)
	{
		Stain->OnCleaningCancelled.RemoveDynamic(
			this,
			&UBathhouseCleaningCancelProbe::HandleCleaningCancelled);
	}
	Stain = nullptr;
}

void UBathhouseCleaningCancelProbe::HandleCleaningCancelled()
{
	++CancelCount;
}

void UBathhouseInteractionQueryProbe::Bind(UPlayerInteractionComponent* InInteraction)
{
	Unbind();
	Interaction = InInteraction;
	if (Interaction)
	{
		Interaction->OnInteractionQueryChanged.AddDynamic(
			this,
			&UBathhouseInteractionQueryProbe::HandleInteractionQueryChanged);
	}
}

void UBathhouseInteractionQueryProbe::Unbind()
{
	if (Interaction)
	{
		Interaction->OnInteractionQueryChanged.RemoveDynamic(
			this,
			&UBathhouseInteractionQueryProbe::HandleInteractionQueryChanged);
	}
	Interaction = nullptr;
}

void UBathhouseInteractionQueryProbe::HandleInteractionQueryChanged(const FPlayerInteractionQuery& Query)
{
	++BroadcastCount;
	LastQuery = Query;
}

void UBathhousePhysicalDropReentryProbe::Bind(
	AWetMopActor* InMop,
	UPlayerCarryComponent* InCarry,
	const FVector& InViewOrigin,
	const FVector& InThrowDirection)
{
	Unbind();
	Mop = InMop;
	Carry = InCarry;
	ViewOrigin = InViewOrigin;
	ThrowDirection = InThrowDirection;
	if (Mop)
	{
		Mop->OnHeldPresentationChanged.AddDynamic(
			this,
			&UBathhousePhysicalDropReentryProbe::HandleHeldPresentationChanged);
	}
}

void UBathhousePhysicalDropReentryProbe::Unbind()
{
	if (Mop)
	{
		Mop->OnHeldPresentationChanged.RemoveDynamic(
			this,
			&UBathhousePhysicalDropReentryProbe::HandleHeldPresentationChanged);
	}
	Mop = nullptr;
	Carry = nullptr;
}

void UBathhousePhysicalDropReentryProbe::HandleHeldPresentationChanged(const bool bIsHeld)
{
	if (bIsHeld)
	{
		return;
	}

	++ReleasePresentationCount;
	if (!Carry)
	{
		return;
	}

	++NestedDropAttemptCount;
	const FPlayerInteractionResult NestedResult = Carry->TryReleaseHeldEquipment(
		ViewOrigin,
		ThrowDirection);
	bNestedDropSucceeded = NestedResult.bSucceeded;
	NestedDropFailureReason = NestedResult.FailureReason;
}
