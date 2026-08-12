#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Towel/TowelTypes.h"
#include "TowelCirculationSubsystem.generated.h"

class ACleanTowelStackActor;
class AUsedTowelBinActor;
class AWorldUsedTowelActor;
class UTowelInventoryComponent;

USTRUCT()
struct BATHHOUSESIM_API FPendingTowelSpill
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	TObjectPtr<AUsedTowelBinActor> PreferredBin = nullptr;

	int32 Count = 0;
};

UCLASS()
class BATHHOUSESIM_API UTowelCirculationSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override { return !IsTemplate() && PendingSpills.Num() > 0; }

	bool TryAcquireCleanTowel(ACleanTowelStackActor* Stack, FTowelUseHandle& InOutHandle);
	bool MarkHandleUsed(FTowelUseHandle& InOutHandle);
	bool TryReturnUsedTowel(AUsedTowelBinActor* Bin, FTowelUseHandle& InOutHandle);
	void CleanupHandle(FTowelUseHandle& InOutHandle, AUsedTowelBinActor* PreferredBin = nullptr);

	void RecoverInventory(UTowelInventoryComponent* Inventory);
	int32 GetRecoveryCount(ETowelState State) const;
	int32 GetPendingSpillCount() const;

	void RegisterWorldTowel(AWorldUsedTowelActor* Towel);
	void UnregisterWorldTowel(AWorldUsedTowelActor* Towel);
	bool IsWorldTowelLocationClear(const FVector& Location, float MinimumSpacing);

private:
	bool TryCommitOneToInventory(
		UTowelInventoryComponent* Inventory,
		ETowelState State,
		FTowelInventorySnapshot& OutPrevious,
		int64& OutTransactionId);
	bool TryReturnUnusedToOriginal(FTowelUseHandle& InOutHandle);
	void CommitRecovery(ETowelState State, int32 Count);
	void CompactWorldTowels();

	UPROPERTY(Transient)
	TArray<FPendingTowelSpill> PendingSpills;

	TArray<TWeakObjectPtr<AWorldUsedTowelActor>> WorldTowels;
	TMap<ETowelState, int32> RecoveryCounts;
	int64 NextTransactionId = 1;
	float RetryAccumulator = 0.0f;
};
