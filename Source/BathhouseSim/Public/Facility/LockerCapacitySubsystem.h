#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "LockerCapacitySubsystem.generated.h"

class ULockerActionSlotComponent;

USTRUCT(BlueprintType)
struct BATHHOUSESIM_API FLockerCapacityLeaseHandle
{
	GENERATED_BODY()

	bool IsValid() const { return LeaseId.IsValid(); }
	void Reset() { LeaseId.Invalidate(); }

private:
	friend class FBathhouseFacilityPlacementMathTest;
	friend class ULockerCapacitySubsystem;
	UPROPERTY()
	FGuid LeaseId;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOnLockerCapacityChanged,
	int32,
	InstalledCapacity,
	int32,
	ActiveLeaseCount);

UCLASS()
class BATHHOUSESIM_API ULockerCapacitySubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	bool ValidateLockerBankRegistration(const AActor* Bank, const TArray<ULockerActionSlotComponent*>& Slots, int32 DefinitionSlotCount, FText& OutFailureReason);
	bool RegisterLockerBank(AActor* Bank, const TArray<ULockerActionSlotComponent*>& Slots, int32 DefinitionSlotCount, FText& OutFailureReason, bool bPublish = true);
	void UnregisterLockerBank(AActor* Bank, bool bUnexpectedEndPlay, bool bPublish = true);
	bool IsLockerBankRegistered(const AActor* Bank) const;
	bool CanInstallLockerSlots(int32 AdditionalSlots, FText& OutFailureReason) const;
	bool CanRemoveLockerBank(const AActor* Bank, FText& OutFailureReason) const;

	bool CanAcquireLease(FText* OutFailureReason = nullptr) const;
	bool TryAcquireProvisionalLease(AActor* Customer, FLockerCapacityLeaseHandle& OutHandle, FText& OutFailureReason);
	bool CommitLease(const FLockerCapacityLeaseHandle& Handle);
	bool RollbackLease(FLockerCapacityLeaseHandle& Handle);
	bool ReleaseLease(FLockerCapacityLeaseHandle& Handle);

	bool TryReserveRandomActionSlot(
		AActor* Requestor,
		AActor*& OutBank,
		ULockerActionSlotComponent*& OutSlot);
	void CompactInvalidEntries();
	void PublishCapacityMutation();

	UFUNCTION(BlueprintPure, Category = "Locker")
	int32 GetInstalledLockerCapacity() const;

	UFUNCTION(BlueprintPure, Category = "Locker")
	int32 GetActiveLeaseCount() const;

	UFUNCTION(BlueprintPure, Category = "Locker")
	int64 GetRevision() const { return Revision; }

	UFUNCTION(BlueprintPure, Category = "Locker")
	bool HasInvariantFault() const { return bInvariantFault || GetInstalledLockerCapacity() < GetReservedLeaseCount(); }

	UPROPERTY(BlueprintAssignable, Category = "Locker")
	FOnLockerCapacityChanged OnLockerCapacityChanged;

private:
	friend class FBathhouseFacilityPlacementMathTest;
	struct FBankRecord
	{
		TWeakObjectPtr<AActor> Bank;
		TArray<TWeakObjectPtr<ULockerActionSlotComponent>> Slots;
	};
	struct FLeaseRecord
	{
		TWeakObjectPtr<AActor> Customer;
		bool bCommitted = false;
	};

	void BroadcastMutation();
	void RecomputeInvariant();
	int32 GetReservedLeaseCount() const;

	TArray<FBankRecord> Banks;
	TMap<FGuid, FLeaseRecord> Leases;
	int32 InstalledLockerCapacity = 0;
	int64 Revision = 1;
	bool bInvariantFault = false;
};
