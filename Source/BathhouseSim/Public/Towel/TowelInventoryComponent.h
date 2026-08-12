#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Towel/TowelTypes.h"
#include "TowelInventoryComponent.generated.h"

UCLASS(ClassGroup = (Towel), meta = (BlueprintSpawnableComponent))
class BATHHOUSESIM_API UTowelInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTowelInventoryComponent();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION(BlueprintPure, Category = "Towel")
	FTowelInventorySnapshot GetSnapshot() const;

	UFUNCTION(BlueprintPure, Category = "Towel")
	int32 GetRemainingCapacity() const { return FMath::Max(0, Capacity - Count); }

	bool CanAccept(ETowelState InState) const;
	bool IsExternalMutationBlocked() const { return bExternalMutationBlocked; }

	UPROPERTY(BlueprintAssignable, Category = "Towel|Presentation")
	FOnTowelInventoryChanged OnInventoryChanged;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Towel", meta = (ClampMin = "0"))
	int32 Capacity = 10;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Towel")
	ETowelState InitialState = ETowelState::None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Towel", meta = (ClampMin = "0"))
	int32 InitialCount = 0;

private:
	friend class UTowelTransferSubsystem;
	friend class UTowelCirculationSubsystem;
	friend class ATowelProcessingMachineActor;
	friend class ATowelBasketActor;
	friend class ACleanTowelStackActor;
	friend class AUsedTowelBinActor;
	friend class AWorldUsedTowelActor;
	friend class FBathhouseTowelTransferTest;
	friend class FBathhouseCustomerTowelTest;

	void ConfigureDefaults(ETowelState InState, int32 InCount, int32 InCapacity);
	bool TryBeginTransaction();
	void EndTransaction();
	void CommitInternal(ETowelState NewState, int32 NewCount);
	void BroadcastCommit(const FTowelInventorySnapshot& Previous, int64 TransactionId);
	void SetExternalMutationBlocked(bool bBlocked) { bExternalMutationBlocked = bBlocked; }
	void SetRecoverContentsOnEndPlay(bool bRecover) { bRecoverContentsOnEndPlay = bRecover; }

	ETowelState State = ETowelState::None;
	int32 Count = 0;
	int64 Revision = 0;
	bool bTransactionActive = false;
	bool bExternalMutationBlocked = false;
	bool bRecoverContentsOnEndPlay = true;
};
