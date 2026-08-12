#include "Tests/BathhouseCleaningTowelTestProbe.h"

#include "Cleaning/WaterStainActor.h"
#include "Cleaning/WetMopActor.h"
#include "Interaction/PlayerCarryComponent.h"
#include "Towel/TowelInventoryComponent.h"

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
