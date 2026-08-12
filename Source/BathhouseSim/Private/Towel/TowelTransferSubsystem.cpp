#include "Towel/TowelTransferSubsystem.h"

#include "Towel/TowelInventoryComponent.h"
#include "Towel/TowelProcessingMachineActor.h"

FTowelTransferResult UTowelTransferSubsystem::TryTransfer(const FTowelTransferRequest& Request)
{
	FTowelTransferResult Result;
	UTowelInventoryComponent* Source = Request.Source;
	UTowelInventoryComponent* Destination = Request.Destination;
	if (!IsValid(Source) || !IsValid(Destination) || Source == Destination
		|| Source->GetWorld() != GetWorld() || Destination->GetWorld() != GetWorld())
	{
		Result.Failure = ETowelTransferFailure::InvalidEndpoint;
		return Result;
	}
	if (Request.RequestedCount <= 0)
	{
		Result.Failure = ETowelTransferFailure::InvalidCount;
		return Result;
	}
	const FTowelInventorySnapshot SourceBefore = Source->GetSnapshot();
	const FTowelInventorySnapshot DestinationBefore = Destination->GetSnapshot();
	ATowelProcessingMachineActor* SourceMachine = Cast<ATowelProcessingMachineActor>(Source->GetOwner());
	if (SourceMachine && SourceMachine->GetInventory() != Source)
	{
		SourceMachine = nullptr;
	}
	ATowelProcessingMachineActor* DestinationMachine = Cast<ATowelProcessingMachineActor>(Destination->GetOwner());
	if (DestinationMachine && DestinationMachine->GetInventory() != Destination)
	{
		DestinationMachine = nullptr;
	}
	if ((Request.ExpectedSourceRevision != MAX_int64
			&& Request.ExpectedSourceRevision != SourceBefore.Revision)
		|| (Request.ExpectedDestinationRevision != MAX_int64
			&& Request.ExpectedDestinationRevision != DestinationBefore.Revision))
	{
		Result.Failure = ETowelTransferFailure::RevisionMismatch;
		return Result;
	}
	if (Source->IsExternalMutationBlocked() || Destination->IsExternalMutationBlocked())
	{
		Result.Failure = ETowelTransferFailure::EndpointBlocked;
		return Result;
	}
	if (SourceBefore.Count <= 0 || SourceBefore.State == ETowelState::None)
	{
		Result.Failure = ETowelTransferFailure::SourceEmpty;
		return Result;
	}
	if (DestinationBefore.Count >= DestinationBefore.Capacity)
	{
		Result.Failure = ETowelTransferFailure::DestinationFull;
		return Result;
	}
	if (DestinationBefore.Count > 0 && DestinationBefore.State != SourceBefore.State)
	{
		Result.Failure = ETowelTransferFailure::StateMismatch;
		return Result;
	}
	if ((SourceMachine && !SourceMachine->AllowsInventoryTransfer(
			Source,
			Destination,
			SourceBefore,
			DestinationBefore))
		|| (DestinationMachine && !DestinationMachine->AllowsInventoryTransfer(
			Source,
			Destination,
			SourceBefore,
			DestinationBefore)))
	{
		Result.Failure = ETowelTransferFailure::EndpointBlocked;
		return Result;
	}
	if (!Source->TryBeginTransaction())
	{
		Result.Failure = ETowelTransferFailure::Reentry;
		return Result;
	}
	if (!Destination->TryBeginTransaction())
	{
		Source->EndTransaction();
		Result.Failure = ETowelTransferFailure::Reentry;
		return Result;
	}

	const FTowelInventorySnapshot SourceAtCommit = Source->GetSnapshot();
	const FTowelInventorySnapshot DestinationAtCommit = Destination->GetSnapshot();
	auto AbortCommit = [&Result, Source, Destination](const ETowelTransferFailure Failure)
	{
		Destination->EndTransaction();
		Source->EndTransaction();
		Result.Failure = Failure;
	};
	if (SourceAtCommit.Revision != SourceBefore.Revision
		|| DestinationAtCommit.Revision != DestinationBefore.Revision)
	{
		AbortCommit(ETowelTransferFailure::RevisionMismatch);
		return Result;
	}
	if (Source->IsExternalMutationBlocked() || Destination->IsExternalMutationBlocked()
		|| (SourceMachine && !SourceMachine->AllowsInventoryTransfer(
			Source,
			Destination,
			SourceAtCommit,
			DestinationAtCommit))
		|| (DestinationMachine && !DestinationMachine->AllowsInventoryTransfer(
			Source,
			Destination,
			SourceAtCommit,
			DestinationAtCommit)))
	{
		AbortCommit(ETowelTransferFailure::EndpointBlocked);
		return Result;
	}
	if (SourceAtCommit.Count <= 0 || SourceAtCommit.State == ETowelState::None)
	{
		AbortCommit(ETowelTransferFailure::SourceEmpty);
		return Result;
	}
	if (DestinationAtCommit.Count >= DestinationAtCommit.Capacity)
	{
		AbortCommit(ETowelTransferFailure::DestinationFull);
		return Result;
	}
	if (DestinationAtCommit.Count > 0 && DestinationAtCommit.State != SourceAtCommit.State)
	{
		AbortCommit(ETowelTransferFailure::StateMismatch);
		return Result;
	}
	const int32 MovedCount = FMath::Min3(
		Request.RequestedCount,
		SourceAtCommit.Count,
		DestinationAtCommit.Capacity - DestinationAtCommit.Count);
	if (MovedCount <= 0)
	{
		AbortCommit(ETowelTransferFailure::DestinationFull);
		return Result;
	}

	const int64 TransactionId = NextTransactionId++;
	Source->CommitInternal(SourceAtCommit.State, SourceAtCommit.Count - MovedCount);
	Destination->CommitInternal(SourceAtCommit.State, DestinationAtCommit.Count + MovedCount);
	Source->EndTransaction();
	Destination->EndTransaction();

	Result.bSucceeded = true;
	Result.MovedCount = MovedCount;
	Result.CommittedSourceRevision = Source->GetSnapshot().Revision;
	Result.CommittedDestinationRevision = Destination->GetSnapshot().Revision;
	Result.TransactionId = TransactionId;
	Source->BroadcastCommit(SourceBefore, TransactionId);
	Destination->BroadcastCommit(DestinationBefore, TransactionId);
	if (IsValid(SourceMachine))
	{
		SourceMachine->HandleCommittedInventoryTransfer(Source);
	}
	if (IsValid(DestinationMachine))
	{
		DestinationMachine->HandleCommittedInventoryTransfer(Source);
	}
	return Result;
}
