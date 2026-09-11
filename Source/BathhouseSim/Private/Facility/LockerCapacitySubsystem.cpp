#include "Facility/LockerCapacitySubsystem.h"

#include "Facility/BathhouseFacilitySubsystem.h"
#include "Facility/LockerActionSlotComponent.h"

#define LOCTEXT_NAMESPACE "LockerCapacitySubsystem"

ELockerBankRegistrationResult ULockerCapacitySubsystem::ValidateLockerBankRegistrationTyped(
	const AActor* Bank,
	const TArray<ULockerActionSlotComponent*>& Slots,
	const int32 DefinitionSlotCount,
	FText& OutFailureReason)
{
	CompactInvalidEntries(false);
	if (IsLockerBankRegistered(Bank))
	{
		return ELockerBankRegistrationResult::AlreadyRegistered;
	}
	if (!IsValid(Bank) || DefinitionSlotCount <= 0 || Slots.Num() != DefinitionSlotCount
		|| Slots.Contains(nullptr))
	{
		OutFailureReason = LOCTEXT("InvalidBank", "락커 Definition과 실제 행동 슬롯 구성이 일치하지 않습니다.");
		return ELockerBankRegistrationResult::InvalidTopology;
	}
	TSet<FName> Ids;
	for (const ULockerActionSlotComponent* Slot : Slots)
	{
		if (!Slot || !Slot->HasStableLockerSlotId() || Ids.Contains(Slot->GetLockerSlotId()))
		{
			OutFailureReason = LOCTEXT("DuplicateSlotId", "락커 행동 슬롯 식별자가 누락되었거나 중복되었습니다.");
			return ELockerBankRegistrationResult::InvalidTopology;
		}
		Ids.Add(Slot->GetLockerSlotId());
	}
	const UBathhouseFacilitySubsystem* Facilities = GetWorld()
		? GetWorld()->GetSubsystem<UBathhouseFacilitySubsystem>() : nullptr;
	if (!Facilities || !Facilities->GetExpansionAuthority())
	{
		OutFailureReason = LOCTEXT("AuthorityNotReady", "확장 단계 관리자가 아직 준비되지 않았습니다.");
		return ELockerBankRegistrationResult::AuthorityNotReady;
	}
	if (InstalledLockerCapacity + DefinitionSlotCount > Facilities->GetMaxInstalledLockerSlots())
	{
		OutFailureReason = LOCTEXT("ExpansionLimit", "현재 확장 단계의 설치 가능한 락커 칸 수를 초과합니다.");
		return ELockerBankRegistrationResult::ExpansionLimitExceeded;
	}
	return ELockerBankRegistrationResult::Success;
}

bool ULockerCapacitySubsystem::ValidateLockerBankRegistration(
	const AActor* Bank,
	const TArray<ULockerActionSlotComponent*>& Slots,
	const int32 DefinitionSlotCount,
	FText& OutFailureReason)
{
	const ELockerBankRegistrationResult Result = ValidateLockerBankRegistrationTyped(
		Bank, Slots, DefinitionSlotCount, OutFailureReason);
	return Result == ELockerBankRegistrationResult::Success
		|| Result == ELockerBankRegistrationResult::AlreadyRegistered;
}

ELockerBankRegistrationResult ULockerCapacitySubsystem::RegisterLockerBankTyped(
	AActor* Bank,
	const TArray<ULockerActionSlotComponent*>& Slots,
	const int32 DefinitionSlotCount,
	FText& OutFailureReason,
	const bool bPublish)
{
	const ELockerBankRegistrationResult Validation = ValidateLockerBankRegistrationTyped(
		Bank, Slots, DefinitionSlotCount, OutFailureReason);
	if (Validation != ELockerBankRegistrationResult::Success)
	{
		return Validation;
	}
	FBankRecord Record;
	Record.Bank = Bank;
	for (ULockerActionSlotComponent* Slot : Slots)
	{
		Record.Slots.Add(Slot);
	}
	Banks.Add(MoveTemp(Record));
	InstalledLockerCapacity += DefinitionSlotCount;
	RecomputeInvariant();
	if (bPublish)
	{
		BroadcastMutation();
	}
	return ELockerBankRegistrationResult::Success;
}

bool ULockerCapacitySubsystem::RegisterLockerBank(
	AActor* Bank,
	const TArray<ULockerActionSlotComponent*>& Slots,
	const int32 DefinitionSlotCount,
	FText& OutFailureReason,
	const bool bPublish)
{
	const ELockerBankRegistrationResult Result = RegisterLockerBankTyped(
		Bank, Slots, DefinitionSlotCount, OutFailureReason, bPublish);
	return Result == ELockerBankRegistrationResult::Success
		|| Result == ELockerBankRegistrationResult::AlreadyRegistered;
}

bool ULockerCapacitySubsystem::UnregisterLockerBank(
	AActor* Bank,
	const bool bUnexpectedEndPlay,
	const bool bPublish)
{
	CompactInvalidEntries();
	bool bRemoved = false;
	for (int32 Index = Banks.Num() - 1; Index >= 0; --Index)
	{
		if (Banks[Index].Bank.Get() == Bank)
		{
			InstalledLockerCapacity = FMath::Max(0, InstalledLockerCapacity - Banks[Index].Slots.Num());
			Banks.RemoveAtSwap(Index);
			bRemoved = true;
		}
	}
	RecomputeInvariant();
	if (bUnexpectedEndPlay && bInvariantFault)
	{
		bInvariantFault = true;
		UE_LOG(LogTemp, Error, TEXT("Unexpected locker-bank EndPlay reduced installed capacity below active/provisional leases."));
	}
	if (bRemoved && bPublish)
	{
		BroadcastMutation();
	}
	return bRemoved;
}

bool ULockerCapacitySubsystem::IsLockerBankRegistered(const AActor* Bank) const
{
	return Banks.ContainsByPredicate([Bank](const FBankRecord& Record) { return Record.Bank.Get() == Bank; });
}

bool ULockerCapacitySubsystem::CanInstallLockerSlots(const int32 AdditionalSlots, FText& OutFailureReason) const
{
	const_cast<ULockerCapacitySubsystem*>(this)->CompactInvalidEntries();
	const UBathhouseFacilitySubsystem* Facilities = GetWorld() ? GetWorld()->GetSubsystem<UBathhouseFacilitySubsystem>() : nullptr;
	if (!Facilities || !Facilities->GetExpansionAuthority())
	{
		OutFailureReason = LOCTEXT("AuthorityNotReady", "확장 단계 관리자가 아직 준비되지 않았습니다.");
		return false;
	}
	const int32 Limit = Facilities->GetMaxInstalledLockerSlots();
	if (AdditionalSlots <= 0 || InstalledLockerCapacity + AdditionalSlots > Limit)
	{
		OutFailureReason = LOCTEXT("ExpansionLimit", "현재 확장 단계의 설치 가능한 락커 칸 수를 초과합니다.");
		return false;
	}
	return true;
}

bool ULockerCapacitySubsystem::CanRemoveLockerBank(const AActor* Bank, FText& OutFailureReason) const
{
	const_cast<ULockerCapacitySubsystem*>(this)->CompactInvalidEntries();
	const FBankRecord* Record = Banks.FindByPredicate([Bank](const FBankRecord& Entry) { return Entry.Bank.Get() == Bank; });
	if (!Record)
	{
		OutFailureReason = LOCTEXT("BankNotRegistered", "설치된 락커 은행을 확인할 수 없습니다.");
		return false;
	}
	for (const TWeakObjectPtr<ULockerActionSlotComponent>& WeakSlot : Record->Slots)
	{
		const ULockerActionSlotComponent* Slot = WeakSlot.Get();
		if (!Slot || Slot->GetSlotState() != EBathhouseFacilitySlotState::Available)
		{
			OutFailureReason = LOCTEXT("BankInUse", "사용 또는 예약 중인 락커가 있어 회수할 수 없습니다.");
			return false;
		}
	}
	if (InstalledLockerCapacity - Record->Slots.Num() < GetReservedLeaseCount())
	{
		OutFailureReason = LOCTEXT("LeaseCapacity", "활성 고객 수용량을 유지할 수 없어 락커를 회수할 수 없습니다.");
		return false;
	}
	return true;
}

bool ULockerCapacitySubsystem::CanAcquireLease(FText* OutFailureReason) const
{
	const_cast<ULockerCapacitySubsystem*>(this)->CompactInvalidEntries();
	const bool bCanAcquire = !bInvariantFault && GetReservedLeaseCount() < InstalledLockerCapacity;
	if (!bCanAcquire && OutFailureReason)
	{
		*OutFailureReason = LOCTEXT("NoCapacity", "사용 가능한 락커 수용량이 없습니다.");
	}
	return bCanAcquire;
}

bool ULockerCapacitySubsystem::TryAcquireProvisionalLease(
	AActor* Customer,
	FLockerCapacityLeaseHandle& OutHandle,
	FText& OutFailureReason)
{
	CompactInvalidEntries();
	if (!IsValid(Customer) || OutHandle.IsValid() || !CanAcquireLease(&OutFailureReason))
	{
		return false;
	}
	OutHandle.LeaseId = FGuid::NewGuid();
	Leases.Add(OutHandle.LeaseId, { Customer, false });
	BroadcastMutation();
	return true;
}

bool ULockerCapacitySubsystem::CommitLease(const FLockerCapacityLeaseHandle& Handle)
{
	CompactInvalidEntries();
	FLeaseRecord* Record = Handle.IsValid() ? Leases.Find(Handle.LeaseId) : nullptr;
	if (!Record)
	{
		return false;
	}
	Record->bCommitted = true;
	BroadcastMutation();
	return true;
}

bool ULockerCapacitySubsystem::RollbackLease(FLockerCapacityLeaseHandle& Handle)
{
	CompactInvalidEntries();
	if (!Handle.IsValid())
	{
		return true;
	}
	const int32 Removed = Leases.Remove(Handle.LeaseId);
	Handle.Reset();
	if (Removed > 0)
	{
		BroadcastMutation();
	}
	return Removed > 0;
}

bool ULockerCapacitySubsystem::ReleaseLease(FLockerCapacityLeaseHandle& Handle)
{
	return RollbackLease(Handle);
}

int32 ULockerCapacitySubsystem::GetInstalledLockerCapacity() const
{
	int32 Count = 0;
	for (const FBankRecord& Bank : Banks)
	{
		const bool bValidRecord = Bank.Bank.IsValid()
			&& !Bank.Slots.ContainsByPredicate([](const TWeakObjectPtr<ULockerActionSlotComponent>& Slot)
			{
				return !Slot.IsValid();
			});
		Count += bValidRecord ? Bank.Slots.Num() : 0;
	}
	return Count;
}

int32 ULockerCapacitySubsystem::GetActiveLeaseCount() const
{
	int32 Count = 0;
	for (const TPair<FGuid, FLeaseRecord>& Pair : Leases)
	{
		Count += Pair.Value.bCommitted && Pair.Value.Customer.IsValid() ? 1 : 0;
	}
	return Count;
}

bool ULockerCapacitySubsystem::TryReserveRandomActionSlot(
	AActor* Requestor,
	AActor*& OutBank,
	ULockerActionSlotComponent*& OutSlot)
{
	CompactInvalidEntries();
	OutBank = nullptr;
	OutSlot = nullptr;
	if (!IsValid(Requestor))
	{
		return false;
	}
	TArray<TPair<AActor*, ULockerActionSlotComponent*>> Candidates;
	for (const FBankRecord& Bank : Banks)
	{
		for (const TWeakObjectPtr<ULockerActionSlotComponent>& WeakSlot : Bank.Slots)
		{
			ULockerActionSlotComponent* Slot = WeakSlot.Get();
			if (Bank.Bank.IsValid() && Slot && Slot->IsAvailable())
			{
				Candidates.Emplace(Bank.Bank.Get(), Slot);
			}
		}
	}
	while (!Candidates.IsEmpty())
	{
		const int32 Index = FMath::RandHelper(Candidates.Num());
		const TPair<AActor*, ULockerActionSlotComponent*> Candidate = Candidates[Index];
		if (Candidate.Value->TryReserve(Requestor))
		{
			OutBank = Candidate.Key;
			OutSlot = Candidate.Value;
			return true;
		}
		Candidates.RemoveAtSwap(Index);
	}
	return false;
}

void ULockerCapacitySubsystem::BroadcastMutation()
{
	RecomputeInvariant();
	++Revision;
	OnLockerCapacityChanged.Broadcast(GetInstalledLockerCapacity(), GetActiveLeaseCount());
}

void ULockerCapacitySubsystem::PublishCapacityMutation()
{
	BroadcastMutation();
}

int32 ULockerCapacitySubsystem::GetReservedLeaseCount() const
{
	int32 Count = 0;
	for (const TPair<FGuid, FLeaseRecord>& Pair : Leases)
	{
		Count += Pair.Value.Customer.IsValid() ? 1 : 0;
	}
	return Count;
}

void ULockerCapacitySubsystem::RecomputeInvariant()
{
	InstalledLockerCapacity = GetInstalledLockerCapacity();
	bInvariantFault = InstalledLockerCapacity < GetReservedLeaseCount();
}

void ULockerCapacitySubsystem::CompactInvalidEntries(const bool bPublish)
{
	const int32 PreviousBanks = Banks.Num();
	const int32 PreviousLeases = Leases.Num();
	Banks.RemoveAll([](const FBankRecord& Bank)
	{
		return !Bank.Bank.IsValid()
			|| Bank.Slots.ContainsByPredicate([](const TWeakObjectPtr<ULockerActionSlotComponent>& Slot)
			{
				return !Slot.IsValid();
			});
	});
	for (auto It = Leases.CreateIterator(); It; ++It)
	{
		if (!It.Value().Customer.IsValid())
		{
			It.RemoveCurrent();
		}
	}
	RecomputeInvariant();
	if (bPublish && (Banks.Num() != PreviousBanks || Leases.Num() != PreviousLeases))
	{
		BroadcastMutation();
	}
}

#undef LOCTEXT_NAMESPACE
