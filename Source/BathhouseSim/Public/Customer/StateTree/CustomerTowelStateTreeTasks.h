#pragma once

#include "CoreMinimal.h"
#include "StateTreeTaskBase.h"
#include "CustomerTowelStateTreeTasks.generated.h"

class UCustomerSessionComponent;

USTRUCT()
struct FCustomerAcquireTowelTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Context)
	TObjectPtr<UCustomerSessionComponent> Session = nullptr;

	UPROPERTY(EditAnywhere, Category = Output)
	bool bProceedingWithoutTowel = false;
};

USTRUCT(meta = (DisplayName = "Acquire Or Wait For Clean Towel", Category = "Bathhouse|Customer"))
struct BATHHOUSESIM_API FCustomerAcquireTowelTask : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	FCustomerAcquireTowelTask();

	using FInstanceDataType = FCustomerAcquireTowelTaskInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(
		FStateTreeExecutionContext& Context,
		const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, float DeltaTime) const override;
	virtual void ExitState(
		FStateTreeExecutionContext& Context,
		const FStateTreeTransitionResult& Transition) const override;
};

USTRUCT()
struct FCustomerTowelHandleTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Context)
	TObjectPtr<UCustomerSessionComponent> Session = nullptr;

	UPROPERTY(EditAnywhere, Category = Output)
	bool bHadTowel = false;
};

USTRUCT(meta = (DisplayName = "Mark Customer Towel Used", Category = "Bathhouse|Customer"))
struct BATHHOUSESIM_API FCustomerMarkTowelUsedTask : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FCustomerTowelHandleTaskInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(
		FStateTreeExecutionContext& Context,
		const FStateTreeTransitionResult& Transition) const override;
};

USTRUCT(meta = (DisplayName = "Return Customer Towel", Category = "Bathhouse|Customer"))
struct BATHHOUSESIM_API FCustomerReturnTowelTask : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FCustomerTowelHandleTaskInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(
		FStateTreeExecutionContext& Context,
		const FStateTreeTransitionResult& Transition) const override;
};
