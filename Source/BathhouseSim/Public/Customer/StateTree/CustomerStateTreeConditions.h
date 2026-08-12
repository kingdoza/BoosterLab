#pragma once

#include "CoreMinimal.h"
#include "StateTreeConditionBase.h"
#include "CustomerStateTreeConditions.generated.h"

class UCustomerSessionComponent;

UENUM()
enum class ECustomerSessionCondition : uint8
{
	HasAssignedKey,
	CheckInTimedOut,
	QueueFront,
	HasFacility,
	BathStayExpired,
	CashClaimed,
	TechnicalAbort,
	Finished,
	HasTowel,
	TowelWaitExpired
};

USTRUCT()
struct FCustomerSessionConditionInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Context)
	TObjectPtr<UCustomerSessionComponent> Session = nullptr;

	UPROPERTY(EditAnywhere, Category = Parameter)
	ECustomerSessionCondition Condition = ECustomerSessionCondition::HasAssignedKey;

	UPROPERTY(EditAnywhere, Category = Parameter)
	bool bInvert = false;
};

USTRUCT(meta = (DisplayName = "Customer Session Condition", Category = "Bathhouse|Customer"))
struct BATHHOUSESIM_API FCustomerSessionStateCondition : public FStateTreeConditionCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FCustomerSessionConditionInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;
};

USTRUCT()
struct FCustomerBathTimeConditionInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Context)
	TObjectPtr<UCustomerSessionComponent> Session = nullptr;

	UPROPERTY(EditAnywhere, Category = Parameter)
	bool bInvert = false;
};

USTRUCT(meta = (DisplayName = "Customer Has Bath Time Remaining", Category = "Bathhouse|Customer"))
struct BATHHOUSESIM_API FCustomerBathTimeCondition : public FStateTreeConditionCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FCustomerBathTimeConditionInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;
};
