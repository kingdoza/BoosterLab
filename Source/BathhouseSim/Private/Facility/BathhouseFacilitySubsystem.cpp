#include "Facility/BathhouseFacilitySubsystem.h"

#include "Facility/BathhouseFacilityActor.h"
#include "Facility/BathhouseFacilitySlotComponent.h"

#define LOCTEXT_NAMESPACE "BathhouseFacilitySubsystem"

void UBathhouseFacilitySubsystem::RegisterFacility(ABathhouseFacilityActor* Facility)
{
	if (IsValid(Facility) && !RegisteredFacilities.Contains(Facility))
	{
		RegisteredFacilities.Add(Facility);
		NotifyFacilityAvailabilityChanged(Facility->GetFacilityType());
	}
}

void UBathhouseFacilitySubsystem::UnregisterFacility(ABathhouseFacilityActor* Facility)
{
	RegisteredFacilities.Remove(Facility);
	if (Facility)
	{
		NotifyFacilityAvailabilityChanged(Facility->GetFacilityType());
	}
}

void UBathhouseFacilitySubsystem::NotifyFacilityAvailabilityChanged(const EBathhouseFacilityType FacilityType)
{
	OnFacilityAvailabilityChanged.Broadcast(FacilityType);
}

void UBathhouseFacilitySubsystem::RegisterKeyHook(AActor* KeyHook, const int32 KeyNumber)
{
	if (!IsValid(KeyHook))
	{
		return;
	}

	TArray<TWeakObjectPtr<AActor>> Existing;
	RegisteredKeyHooks.MultiFind(KeyNumber, Existing);
	if (!Existing.Contains(KeyHook))
	{
		RegisteredKeyHooks.Add(KeyNumber, KeyHook);
		OnKeyTopologyChanged.Broadcast();
	}
}

void UBathhouseFacilitySubsystem::UnregisterKeyHook(AActor* KeyHook, const int32 KeyNumber)
{
	RegisteredKeyHooks.RemoveSingle(KeyNumber, KeyHook);
	OnKeyTopologyChanged.Broadcast();
}

bool UBathhouseFacilitySubsystem::ValidateKeyNumber(
	const int32 KeyNumber,
	const AActor* ExpectedKeyHook,
	FText* OutFailureReason) const
{
	if (KeyNumber < 0)
	{
		if (OutFailureReason)
		{
			*OutFailureReason = LOCTEXT("InvalidNumber", "키 번호가 올바르지 않습니다.");
		}
		return false;
	}

	TArray<TWeakObjectPtr<AActor>> Hooks;
	RegisteredKeyHooks.MultiFind(KeyNumber, Hooks);
	Hooks.RemoveAll([](const TWeakObjectPtr<AActor>& Hook) { return !Hook.IsValid(); });
	if (Hooks.Num() != 1 || (ExpectedKeyHook && Hooks[0].Get() != ExpectedKeyHook))
	{
		if (OutFailureReason)
		{
			*OutFailureReason = LOCTEXT("InvalidHookCount", "이 번호의 키 걸이가 누락되었거나 중복되었습니다.");
		}
		return false;
	}

	for (const EBathhouseFacilityType Type : { EBathhouseFacilityType::ShoeLocker, EBathhouseFacilityType::ClothesLocker })
	{
		int32 Count = 0;
		for (const TWeakObjectPtr<ABathhouseFacilityActor>& Facility : RegisteredFacilities)
		{
			if (Facility.IsValid()
				&& Facility->IsFacilityEnabled()
				&& Facility->GetFacilityType() == Type
				&& Facility->GetFacilityNumber() == KeyNumber)
			{
				++Count;
			}
		}

		if (Count != 1)
		{
			if (OutFailureReason)
			{
				*OutFailureReason = Type == EBathhouseFacilityType::ShoeLocker
					? LOCTEXT("InvalidShoeLockerCount", "이 번호의 신발장이 누락되었거나 중복되었습니다.")
					: LOCTEXT("InvalidClothesLockerCount", "이 번호의 옷장이 누락되었거나 중복되었습니다.");
			}
			return false;
		}
	}

	return true;
}

ABathhouseFacilityActor* UBathhouseFacilitySubsystem::FindNumberedFacility(
	const EBathhouseFacilityType FacilityType,
	const int32 FacilityNumber) const
{
	ABathhouseFacilityActor* Result = nullptr;
	for (const TWeakObjectPtr<ABathhouseFacilityActor>& Facility : RegisteredFacilities)
	{
		if (Facility.IsValid()
			&& Facility->IsFacilityEnabled()
			&& Facility->GetFacilityType() == FacilityType
			&& Facility->GetFacilityNumber() == FacilityNumber)
		{
			if (Result)
			{
				return nullptr;
			}
			Result = Facility.Get();
		}
	}
	return Result;
}

void UBathhouseFacilitySubsystem::GetFacilitiesOfType(
	const EBathhouseFacilityType FacilityType,
	TArray<ABathhouseFacilityActor*>& OutFacilities) const
{
	OutFacilities.Reset();
	for (const TWeakObjectPtr<ABathhouseFacilityActor>& Facility : RegisteredFacilities)
	{
		if (Facility.IsValid() && Facility->IsFacilityEnabled() && Facility->GetFacilityType() == FacilityType)
		{
			OutFacilities.Add(Facility.Get());
		}
	}
}

bool UBathhouseFacilitySubsystem::TryReserveRandomSlot(
	const EBathhouseFacilityType FacilityType,
	AActor* Requestor,
	ABathhouseFacilityActor*& OutFacility,
	UBathhouseFacilitySlotComponent*& OutSlot,
	const int32 FacilityNumber,
	const ABathhouseFacilityActor* ExcludedFacility) const
{
	OutFacility = nullptr;
	OutSlot = nullptr;
	if (!IsValid(Requestor))
	{
		return false;
	}

	struct FCandidate
	{
		ABathhouseFacilityActor* Facility = nullptr;
		UBathhouseFacilitySlotComponent* Slot = nullptr;
		float Weight = 1.0f;
	};

	TArray<FCandidate> Candidates;
	auto Gather = [&](const bool bApplyExclusion)
	{
		Candidates.Reset();
		for (const TWeakObjectPtr<ABathhouseFacilityActor>& WeakFacility : RegisteredFacilities)
		{
			ABathhouseFacilityActor* Facility = WeakFacility.Get();
			if (!IsValid(Facility)
				|| !Facility->IsFacilityEnabled()
				|| Facility->GetFacilityType() != FacilityType
				|| (FacilityNumber != INDEX_NONE && Facility->GetFacilityNumber() != FacilityNumber)
				|| (bApplyExclusion && Facility == ExcludedFacility))
			{
				continue;
			}

			for (UBathhouseFacilitySlotComponent* Slot : Facility->GetFacilitySlots())
			{
				if (Slot && Slot->IsAvailable())
				{
					Candidates.Add({ Facility, Slot, FMath::Max(Facility->GetSelectionWeight(), KINDA_SMALL_NUMBER) });
				}
			}
		}
	};

	Gather(FacilityType == EBathhouseFacilityType::Bath && ExcludedFacility != nullptr);
	if (Candidates.IsEmpty() && FacilityType == EBathhouseFacilityType::Bath && ExcludedFacility)
	{
		Gather(false);
	}
	if (Candidates.IsEmpty())
	{
		return false;
	}

	float TotalWeight = 0.0f;
	for (const FCandidate& Candidate : Candidates)
	{
		TotalWeight += Candidate.Weight;
	}
	float Choice = FMath::FRandRange(0.0f, TotalWeight);
	for (const FCandidate& Candidate : Candidates)
	{
		Choice -= Candidate.Weight;
		if (Choice <= 0.0f && Candidate.Slot->TryReserve(Requestor))
		{
			OutFacility = Candidate.Facility;
			OutSlot = Candidate.Slot;
			return true;
		}
	}

	for (const FCandidate& Candidate : Candidates)
	{
		if (Candidate.Slot->TryReserve(Requestor))
		{
			OutFacility = Candidate.Facility;
			OutSlot = Candidate.Slot;
			return true;
		}
	}
	return false;
}

void UBathhouseFacilitySubsystem::CompactRegistrations()
{
	RegisteredFacilities.RemoveAll([](const TWeakObjectPtr<ABathhouseFacilityActor>& Facility) { return !Facility.IsValid(); });
	for (auto It = RegisteredKeyHooks.CreateIterator(); It; ++It)
	{
		if (!It.Value().IsValid())
		{
			It.RemoveCurrent();
		}
	}
}

#undef LOCTEXT_NAMESPACE
