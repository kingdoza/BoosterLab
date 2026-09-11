#include "Facility/BathhouseFacilitySubsystem.h"

#include "Engine/World.h"
#include "Facility/BathhouseFacilityActor.h"
#include "Facility/LockerCapacitySubsystem.h"

#define LOCTEXT_NAMESPACE "BathhouseFacilityStartup"

void UBathhouseFacilitySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	if (UWorld* World = GetWorld())
	{
		WorldBeginPlayHandle = World->OnWorldBeginPlay.AddUObject(
			this, &UBathhouseFacilitySubsystem::HandleWorldBeginPlay);
	}
}

void UBathhouseFacilitySubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld(); World && WorldBeginPlayHandle.IsValid())
	{
		World->OnWorldBeginPlay.Remove(WorldBeginPlayHandle);
	}
	WorldBeginPlayHandle.Reset();
	PendingStartupLockers.Reset();
	PermanentStartupFailures.Reset();
	LoggedStartupFailures.Reset();
	AcceptedStartupRegistrationOwners.Reset();
	Super::Deinitialize();
}

void UBathhouseFacilitySubsystem::SubmitStartupLocker(ABathhouseFacilityActor* Locker)
{
	if (!IsValid(Locker) || PermanentStartupFailures.Contains(Locker))
	{
		return;
	}
	if (IsFacilityRegistered(Locker))
	{
		return;
	}
	if (const TWeakObjectPtr<ABathhouseFacilityActor>* AcceptedOwner =
		AcceptedStartupRegistrationOwners.Find(Locker->GetRegistrationId());
		AcceptedOwner && AcceptedOwner->Get() == Locker)
	{
		return;
	}
	const int32 PreviousCount = PendingStartupLockers.Num();
	PendingStartupLockers.Add(Locker);
	if (PendingStartupLockers.Num() != PreviousCount)
	{
		++PendingStartupRevision;
	}
	if (bStartupSubmissionClosed)
	{
		ReconcileStartupLockers();
	}
}

void UBathhouseFacilitySubsystem::HandleWorldBeginPlay()
{
	bStartupSubmissionClosed = true;
	ReconcileStartupLockers();
}

void UBathhouseFacilitySubsystem::MarkStartupLockerPermanentFailure(
	ABathhouseFacilityActor& Locker,
	const FText& FailureReason)
{
	PendingStartupLockers.Remove(&Locker);
	PermanentStartupFailures.Add(&Locker);
	Locker.FailStartupLockerDomain();
	if (!LoggedStartupFailures.Contains(&Locker))
	{
		LoggedStartupFailures.Add(&Locker);
		UE_LOG(LogTemp, Error, TEXT("Startup locker %s was rejected permanently: %s"),
			*Locker.GetName(), *FailureReason.ToString());
	}
}

void UBathhouseFacilitySubsystem::ReconcileStartupLockers()
{
	if (!bStartupSubmissionClosed || bReconcilingStartupLockers
		|| (LastReconciledPendingRevision == PendingStartupRevision
			&& LastReconciledAuthorityRevision == AuthorityReadinessRevision))
	{
		return;
	}
	TGuardValue<bool> ReentryGuard(bReconcilingStartupLockers, true);
	uint64 ReconciledPendingRevision = 0;
	uint64 ReconciledAuthorityRevision = 0;
	do
	{
		ReconciledPendingRevision = PendingStartupRevision;
		ReconciledAuthorityRevision = AuthorityReadinessRevision;
		for (auto It = PendingStartupLockers.CreateIterator(); It; ++It)
		{
			if (!It->IsValid())
			{
				It.RemoveCurrent();
			}
		}
		if (!ExpansionAuthority.IsValid())
		{
			if (!PendingStartupLockers.IsEmpty() && !bLoggedAuthorityNotReady)
			{
				bLoggedAuthorityNotReady = true;
				UE_LOG(LogTemp, Warning, TEXT("Startup lockers are pending until the expansion authority is ready."));
			}
		}
		else
		{
			bLoggedAuthorityNotReady = false;
			TArray<ABathhouseFacilityActor*> Ordered;
			for (const TWeakObjectPtr<ABathhouseFacilityActor>& Locker : PendingStartupLockers)
			{
				if (Locker.IsValid())
				{
					Ordered.Add(Locker.Get());
				}
			}
			Ordered.Sort([](const ABathhouseFacilityActor& Left, const ABathhouseFacilityActor& Right)
			{
				return Left.GetRegistrationId().ToString(EGuidFormats::Digits)
					< Right.GetRegistrationId().ToString(EGuidFormats::Digits);
			});
			TMap<FGuid, int32> IdCounts;
			for (const ABathhouseFacilityActor* Locker : Ordered)
			{
				if (Locker)
				{
					IdCounts.FindOrAdd(Locker->GetRegistrationId())++;
				}
			}

			int32 AcceptedCount = 0;
			for (ABathhouseFacilityActor* Locker : Ordered)
			{
				if (!IsValid(Locker) || !PendingStartupLockers.Contains(Locker))
				{
					continue;
				}
				const FGuid Id = Locker->GetRegistrationId();
				if (const TWeakObjectPtr<ABathhouseFacilityActor>* AcceptedOwner =
					AcceptedStartupRegistrationOwners.Find(Id))
				{
					if (AcceptedOwner->Get() == Locker)
					{
						PendingStartupLockers.Remove(Locker);
						continue;
					}
					MarkStartupLockerPermanentFailure(
						*Locker,
						LOCTEXT("DuplicateAcceptedStartupLockerId", "startup 락커 RegistrationId가 이미 다른 락커에 사용되었습니다."));
					continue;
				}
				if (!Id.IsValid() || IdCounts.FindRef(Id) != 1)
				{
					MarkStartupLockerPermanentFailure(
						*Locker,
						LOCTEXT("DuplicateStartupLockerId", "startup 락커 RegistrationId가 누락되었거나 중복되었습니다."));
					continue;
				}
				FText FailureReason;
				const ELockerBankRegistrationResult Result = Locker->RegisterStartupLockerDomain(FailureReason);
				if (Result == ELockerBankRegistrationResult::AuthorityNotReady)
				{
					continue;
				}
				if (Result != ELockerBankRegistrationResult::Success
					&& Result != ELockerBankRegistrationResult::AlreadyRegistered)
				{
					MarkStartupLockerPermanentFailure(*Locker, FailureReason);
					continue;
				}
				if (!Locker->CommitStartupLockerDomain(FailureReason))
				{
					MarkStartupLockerPermanentFailure(*Locker, FailureReason);
					continue;
				}
				AcceptedStartupRegistrationOwners.Add(Id, Locker);
				PendingStartupLockers.Remove(Locker);
				++AcceptedCount;
			}
			if (AcceptedCount > 0)
			{
				NotifyFacilityAvailabilityChanged(EBathhouseFacilityType::ClothesLocker);
				if (ULockerCapacitySubsystem* Lockers = GetWorld()->GetSubsystem<ULockerCapacitySubsystem>())
				{
					Lockers->PublishCapacityMutation();
				}
			}
		}
		LastReconciledPendingRevision = ReconciledPendingRevision;
		LastReconciledAuthorityRevision = ReconciledAuthorityRevision;
	}
	while (ReconciledPendingRevision != PendingStartupRevision
		|| ReconciledAuthorityRevision != AuthorityReadinessRevision);
}

#undef LOCTEXT_NAMESPACE
