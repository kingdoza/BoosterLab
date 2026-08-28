#pragma once

#include "CoreMinimal.h"
#include "Facility/BathhouseFacilityTypes.h"
#include "StateTreeTaskBase.h"
#include "CustomerQueueStateTreeTasks.generated.h"

class ABathhouseCustomerCharacter;
class UCustomerSessionComponent;

USTRUCT()
struct FCustomerMoveToCurrentQueueAssignmentTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Context)
	TObjectPtr<ABathhouseCustomerCharacter> Customer = nullptr;

	UPROPERTY(EditAnywhere, Category = Context)
	TObjectPtr<UCustomerSessionComponent> Session = nullptr;

	UPROPERTY(EditAnywhere, Category = Parameter)
	EBathhouseCounterLane ExpectedLane = EBathhouseCounterLane::CheckIn;

	uint64 ExecutionToken = 0;
};

USTRUCT(meta = (DisplayName = "Move To Current Queue Assignment", Category = "Bathhouse|Customer"))
struct BATHHOUSESIM_API FCustomerMoveToCurrentQueueAssignmentTask : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FCustomerMoveToCurrentQueueAssignmentTaskInstanceData;
	FCustomerMoveToCurrentQueueAssignmentTask();
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, float DeltaTime) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};
