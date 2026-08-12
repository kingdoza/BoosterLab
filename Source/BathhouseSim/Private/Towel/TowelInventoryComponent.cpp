#include "Towel/TowelInventoryComponent.h"

#include "Engine/World.h"
#include "Towel/TowelCirculationSubsystem.h"

UTowelInventoryComponent::UTowelInventoryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UTowelInventoryComponent::BeginPlay()
{
	Super::BeginPlay();
	Capacity = FMath::Max(0, Capacity);
	Count = FMath::Clamp(InitialCount, 0, Capacity);
	State = Count == 0 ? ETowelState::None : InitialState;
	if (State == ETowelState::None && Count > 0)
	{
		Count = 0;
	}
}

void UTowelInventoryComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (bRecoverContentsOnEndPlay && Count > 0)
	{
		if (UTowelCirculationSubsystem* Circulation = GetWorld()
			? GetWorld()->GetSubsystem<UTowelCirculationSubsystem>()
			: nullptr)
		{
			Circulation->RecoverInventory(this);
		}
	}
	Super::EndPlay(EndPlayReason);
}

FTowelInventorySnapshot UTowelInventoryComponent::GetSnapshot() const
{
	FTowelInventorySnapshot Snapshot;
	Snapshot.State = State;
	Snapshot.Count = Count;
	Snapshot.Capacity = Capacity;
	Snapshot.Revision = Revision;
	return Snapshot;
}

bool UTowelInventoryComponent::CanAccept(const ETowelState InState) const
{
	return !bExternalMutationBlocked && InState != ETowelState::None && Count < Capacity
		&& (Count == 0 || State == InState);
}

void UTowelInventoryComponent::ConfigureDefaults(
	const ETowelState InState,
	const int32 InCount,
	const int32 InCapacity)
{
	InitialState = InState;
	InitialCount = FMath::Max(0, InCount);
	Capacity = FMath::Max(0, InCapacity);
}

bool UTowelInventoryComponent::TryBeginTransaction()
{
	if (bTransactionActive)
	{
		return false;
	}
	bTransactionActive = true;
	return true;
}

void UTowelInventoryComponent::EndTransaction()
{
	bTransactionActive = false;
}

void UTowelInventoryComponent::CommitInternal(const ETowelState NewState, const int32 NewCount)
{
	Count = FMath::Clamp(NewCount, 0, Capacity);
	State = Count == 0 ? ETowelState::None : NewState;
	++Revision;
}

void UTowelInventoryComponent::BroadcastCommit(
	const FTowelInventorySnapshot& Previous,
	const int64 TransactionId)
{
	OnInventoryChanged.Broadcast(Previous, GetSnapshot(), TransactionId);
}
