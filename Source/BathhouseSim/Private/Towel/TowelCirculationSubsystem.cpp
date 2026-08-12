#include "Towel/TowelCirculationSubsystem.h"

#include "Towel/CleanTowelStackActor.h"
#include "Towel/TowelInventoryComponent.h"
#include "Towel/UsedTowelBinActor.h"
#include "Towel/WorldUsedTowelActor.h"

void UTowelCirculationSubsystem::Tick(const float DeltaTime)
{
	RetryAccumulator += DeltaTime;
	if (RetryAccumulator < 0.5f)
	{
		return;
	}
	RetryAccumulator = 0.0f;
	for (int32 Index = PendingSpills.Num() - 1; Index >= 0; --Index)
	{
		FPendingTowelSpill& Pending = PendingSpills[Index];
		if (!IsValid(Pending.PreferredBin))
		{
			CommitRecovery(ETowelState::Used, Pending.Count);
			PendingSpills.RemoveAtSwap(Index);
			continue;
		}
		while (Pending.Count > 0)
		{
			AWorldUsedTowelActor* Towel = nullptr;
			if (!Pending.PreferredBin->TryStageOverflowTowel(Towel))
			{
				break;
			}
			--Pending.Count;
			Towel->CommitStagedToken();
		}
		if (Pending.Count <= 0)
		{
			PendingSpills.RemoveAtSwap(Index);
		}
	}
}

TStatId UTowelCirculationSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UTowelCirculationSubsystem, STATGROUP_Tickables);
}

bool UTowelCirculationSubsystem::TryAcquireCleanTowel(
	ACleanTowelStackActor* Stack,
	FTowelUseHandle& InOutHandle)
{
	if (!IsValid(Stack) || InOutHandle.HasToken())
	{
		return false;
	}
	UTowelInventoryComponent* Inventory = Stack->GetInventory();
	const FTowelInventorySnapshot Before = Inventory ? Inventory->GetSnapshot() : FTowelInventorySnapshot();
	if (!Inventory || Before.State != ETowelState::Clean || Before.Count <= 0
		|| Inventory->IsExternalMutationBlocked() || !Inventory->TryBeginTransaction())
	{
		return false;
	}
	const int64 TransactionId = NextTransactionId++;
	Inventory->CommitInternal(ETowelState::Clean, Before.Count - 1);
	InOutHandle.Token = FGuid::NewGuid();
	InOutHandle.OriginalStack = Stack;
	InOutHandle.bUsed = false;
	InOutHandle.bTerminal = false;
	Inventory->EndTransaction();
	Inventory->BroadcastCommit(Before, TransactionId);
	return true;
}

bool UTowelCirculationSubsystem::MarkHandleUsed(FTowelUseHandle& InOutHandle)
{
	if (!InOutHandle.HasToken() || InOutHandle.bUsed)
	{
		return false;
	}
	InOutHandle.bUsed = true;
	return true;
}

bool UTowelCirculationSubsystem::TryReturnUsedTowel(
	AUsedTowelBinActor* Bin,
	FTowelUseHandle& InOutHandle)
{
	if (!InOutHandle.HasToken() || !InOutHandle.bUsed)
	{
		return false;
	}
	int64 TransactionId = 0;
	FTowelInventorySnapshot Previous;
	if (IsValid(Bin) && TryCommitOneToInventory(
		Bin->GetInventory(),
		ETowelState::Used,
		Previous,
		TransactionId))
	{
		InOutHandle.bTerminal = true;
		Bin->GetInventory()->BroadcastCommit(Previous, TransactionId);
		return true;
	}
	AWorldUsedTowelActor* WorldTowel = nullptr;
	if (IsValid(Bin) && Bin->TryStageOverflowTowel(WorldTowel))
	{
		InOutHandle.bTerminal = true;
		WorldTowel->CommitStagedToken();
		return true;
	}
	if (IsValid(Bin))
	{
		FPendingTowelSpill& Pending = PendingSpills.AddDefaulted_GetRef();
		Pending.PreferredBin = Bin;
		Pending.Count = 1;
	}
	else
	{
		CommitRecovery(ETowelState::Used, 1);
	}
	InOutHandle.bTerminal = true;
	return true;
}

void UTowelCirculationSubsystem::CleanupHandle(
	FTowelUseHandle& InOutHandle,
	AUsedTowelBinActor* PreferredBin)
{
	if (!InOutHandle.HasToken())
	{
		return;
	}
	if (InOutHandle.bUsed)
	{
		TryReturnUsedTowel(PreferredBin, InOutHandle);
	}
	else if (!TryReturnUnusedToOriginal(InOutHandle))
	{
		CommitRecovery(ETowelState::Clean, 1);
		InOutHandle.bTerminal = true;
	}
}

void UTowelCirculationSubsystem::RecoverInventory(UTowelInventoryComponent* Inventory)
{
	if (!IsValid(Inventory))
	{
		return;
	}
	const FTowelInventorySnapshot Before = Inventory->GetSnapshot();
	if (Before.Count <= 0 || !Inventory->TryBeginTransaction())
	{
		return;
	}
	const int64 TransactionId = NextTransactionId++;
	Inventory->CommitInternal(ETowelState::None, 0);
	Inventory->EndTransaction();
	CommitRecovery(Before.State, Before.Count);
	Inventory->BroadcastCommit(Before, TransactionId);
}

int32 UTowelCirculationSubsystem::GetRecoveryCount(const ETowelState State) const
{
	return RecoveryCounts.FindRef(State);
}

int32 UTowelCirculationSubsystem::GetPendingSpillCount() const
{
	int32 Total = 0;
	for (const FPendingTowelSpill& Pending : PendingSpills)
	{
		Total += Pending.Count;
	}
	return Total;
}

void UTowelCirculationSubsystem::RegisterWorldTowel(AWorldUsedTowelActor* Towel)
{
	CompactWorldTowels();
	if (IsValid(Towel) && !WorldTowels.Contains(Towel))
	{
		WorldTowels.Add(Towel);
	}
}

void UTowelCirculationSubsystem::UnregisterWorldTowel(AWorldUsedTowelActor* Towel)
{
	WorldTowels.RemoveAll([Towel](const TWeakObjectPtr<AWorldUsedTowelActor>& Entry)
	{
		return !Entry.IsValid() || Entry.Get() == Towel;
	});
}

bool UTowelCirculationSubsystem::IsWorldTowelLocationClear(
	const FVector& Location,
	const float MinimumSpacing)
{
	CompactWorldTowels();
	const float MinimumSpacingSquared = FMath::Square(FMath::Max(0.0f, MinimumSpacing));
	for (const TWeakObjectPtr<AWorldUsedTowelActor>& Towel : WorldTowels)
	{
		if (Towel.IsValid() && !Towel->IsConsumed()
			&& FVector::DistSquared(Towel->GetActorLocation(), Location) < MinimumSpacingSquared)
		{
			return false;
		}
	}
	return true;
}

bool UTowelCirculationSubsystem::TryCommitOneToInventory(
	UTowelInventoryComponent* Inventory,
	const ETowelState State,
	FTowelInventorySnapshot& OutPrevious,
	int64& OutTransactionId)
{
	if (!IsValid(Inventory) || !Inventory->CanAccept(State) || !Inventory->TryBeginTransaction())
	{
		return false;
	}
	OutPrevious = Inventory->GetSnapshot();
	OutTransactionId = NextTransactionId++;
	Inventory->CommitInternal(State, OutPrevious.Count + 1);
	Inventory->EndTransaction();
	return true;
}

bool UTowelCirculationSubsystem::TryReturnUnusedToOriginal(FTowelUseHandle& InOutHandle)
{
	if (!IsValid(InOutHandle.OriginalStack))
	{
		return false;
	}
	int64 TransactionId = 0;
	FTowelInventorySnapshot Previous;
	UTowelInventoryComponent* Inventory = InOutHandle.OriginalStack->GetInventory();
	if (!TryCommitOneToInventory(Inventory, ETowelState::Clean, Previous, TransactionId))
	{
		return false;
	}
	InOutHandle.bTerminal = true;
	Inventory->BroadcastCommit(Previous, TransactionId);
	return true;
}

void UTowelCirculationSubsystem::CommitRecovery(const ETowelState State, const int32 Count)
{
	if (State != ETowelState::None && Count > 0)
	{
		RecoveryCounts.FindOrAdd(State) += Count;
	}
}

void UTowelCirculationSubsystem::CompactWorldTowels()
{
	WorldTowels.RemoveAll([](const TWeakObjectPtr<AWorldUsedTowelActor>& Entry)
	{
		return !Entry.IsValid();
	});
}
