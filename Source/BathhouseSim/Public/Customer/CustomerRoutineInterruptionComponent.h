#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CustomerRoutineInterruptionComponent.generated.h"

UCLASS(ClassGroup = (Customer), meta = (BlueprintSpawnableComponent))
class BATHHOUSESIM_API UCustomerRoutineInterruptionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCustomerRoutineInterruptionComponent();
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	bool BeginSoftInterruption();
	bool EndSoftInterruption();

	uint64 RegisterRestartableOperation();
	void ClearRestartableOperation(uint64 OperationToken);
	bool IsRestartableOperationCurrent(uint64 OperationToken) const;

	UFUNCTION(BlueprintPure, Category = "Customer|Recovery")
	bool IsSoftInterrupted() const { return bSoftInterrupted; }

	uint64 GetInterruptionSerial() const { return InterruptionSerial; }

private:
	friend class FBathhouseCustomerInterruptionTest;

	uint64 AllocateNonZeroToken(uint64& Counter);
	void CompleteSoftInterruptionResume();
	void HandleQueueRecoveryFinished(bool bSucceeded);

	uint64 InterruptionSerial = 0;
	uint64 NextOperationToken = 1;
	uint64 ActiveOperationToken = 0;
	bool bSoftInterrupted = false;
	bool bQueueRecoveryPending = false;
};
