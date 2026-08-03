#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/PlayerInteractable.h"
#include "BathhouseCashPaymentActor.generated.h"

class ABathhouseCashPaymentActor;
class UPlayerWalletComponent;
class USceneComponent;
class UStaticMeshComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCashClaimed, ABathhouseCashPaymentActor*, CashActor);

UCLASS(Blueprintable)
class BATHHOUSESIM_API ABathhouseCashPaymentActor : public AActor, public IPlayerInteractable
{
	GENERATED_BODY()

public:
	ABathhouseCashPaymentActor();

	virtual void BeginPlay() override;
	virtual FPlayerInteractionQuery QueryInteraction(const FPlayerInteractionContext& Context) const override;
	virtual FPlayerInteractionResult ExecuteInteraction(const FPlayerInteractionContext& Context) override;

	UFUNCTION(BlueprintPure, Category = "Economy")
	int32 GetPaymentAmount() const { return PaymentAmount; }

	UFUNCTION(BlueprintPure, Category = "Economy")
	bool IsClaimed() const { return bClaimed; }

	void ConfigurePaymentAmount(int32 InPaymentAmount);

	UPROPERTY(BlueprintAssignable, Category = "Economy")
	FOnCashClaimed OnCashClaimed;

	UFUNCTION(BlueprintImplementableEvent, Category = "Economy")
	void OnCashAvailable();

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Economy")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Economy")
	TObjectPtr<UStaticMeshComponent> WorldMesh;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Economy", meta = (ClampMin = "1"))
	int32 PaymentAmount = 10000;

private:
	UPlayerWalletComponent* ResolveWallet(const FPlayerInteractionContext& Context) const;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Economy", meta = (AllowPrivateAccess = "true"))
	bool bClaimed = false;
};
