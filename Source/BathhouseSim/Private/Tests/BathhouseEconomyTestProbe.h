#pragma once

#include "CoreMinimal.h"
#include "Interaction/InteractionTypes.h"
#include "UObject/Object.h"
#include "BathhouseEconomyTestProbe.generated.h"

class ABathhouseCashPaymentActor;
class UPlayerWalletComponent;

UCLASS(Transient, NotBlueprintable)
class UBathhouseCashReentryTestProbe : public UObject
{
	GENERATED_BODY()

public:
	void Initialize(ABathhouseCashPaymentActor* InCashActor, const FPlayerInteractionContext& InContext);
	void BindToWallet(UPlayerWalletComponent* InWallet);
	void UnbindFromWallet();
	int32 GetReentryAttemptCount() const { return ReentryAttemptCount; }
	const FPlayerInteractionResult& GetReentryResult() const { return ReentryResult; }

private:
	UFUNCTION()
	void HandleMoneyChanged(int32 PreviousMoney, int32 CurrentMoney);

	UPROPERTY(Transient)
	TObjectPtr<ABathhouseCashPaymentActor> CashActor = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UPlayerWalletComponent> Wallet = nullptr;

	UPROPERTY(Transient)
	FPlayerInteractionContext InteractionContext;

	FPlayerInteractionResult ReentryResult;
	int32 ReentryAttemptCount = 0;
	bool bDidAttemptReentry = false;
};
