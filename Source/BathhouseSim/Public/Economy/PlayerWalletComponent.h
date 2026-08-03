#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PlayerWalletComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnMoneyChanged, int32, PreviousMoney, int32, CurrentMoney);

UCLASS(ClassGroup = (Economy), meta = (BlueprintSpawnableComponent))
class BATHHOUSESIM_API UPlayerWalletComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPlayerWalletComponent();

	UFUNCTION(BlueprintPure, Category = "Economy")
	int32 GetCurrentMoney() const { return CurrentMoney; }

	bool CanAddMoney(int32 Amount) const;
	bool TryAddMoney(int32 Amount);

	UPROPERTY(BlueprintAssignable, Category = "Economy")
	FOnMoneyChanged OnMoneyChanged;

private:
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Economy", meta = (AllowPrivateAccess = "true"))
	int32 CurrentMoney = 0;
};
