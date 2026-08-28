#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Customer/BathhouseCustomerTypes.h"
#include "Interaction/PlayerInteractable.h"
#include "BathhouseCustomerCharacter.generated.h"

class ABathhouseCounterActor;
class ABathhouseCustomerCharacter;
class UCustomerMontagePlaybackComponent;
class UCustomerRoutineDefinition;
class UCustomerSessionComponent;
class UCustomerKnockdownComponent;
class UCustomerRoutineInterruptionComponent;
class UCustomerQueueNavigationComponent;
class UHealthComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnBathhouseCustomerFinished, ABathhouseCustomerCharacter*, Customer, EBathhouseCustomerDepartureReason, Reason);

UCLASS(Blueprintable)
class BATHHOUSESIM_API ABathhouseCustomerCharacter : public ACharacter, public IPlayerInteractable
{
	GENERATED_BODY()

public:
	ABathhouseCustomerCharacter();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual FPlayerInteractionQuery QueryInteraction(const FPlayerInteractionContext& Context) const override;
	virtual FPlayerInteractionResult ExecuteInteraction(const FPlayerInteractionContext& Context) override;

	void InitializeCustomer(UCustomerRoutineDefinition* InRoutineDefinition, ABathhouseCounterActor* InCounter);
	void NotifyActivityStarted(EBathhouseCustomerActivity Activity);
	void NotifyActivityFinished(EBathhouseCustomerActivity Activity);
	void NotifyPresentationState(EBathhouseCustomerPresentationState PresentationState);
	void NotifySatisfactionChanged(float PreviousSatisfaction, float NewSatisfaction);
	void NotifyCustomerFinished(EBathhouseCustomerDepartureReason Reason);

	UFUNCTION(BlueprintPure, Category = "Customer")
	UCustomerSessionComponent* GetCustomerSession() const { return CustomerSession; }

	UFUNCTION(BlueprintPure, Category = "Customer")
	UCustomerMontagePlaybackComponent* GetCustomerMontagePlayback() const { return CustomerMontagePlayback; }

	UFUNCTION(BlueprintPure, Category = "Customer|Combat")
	UHealthComponent* GetHealth() const { return Health; }

	UFUNCTION(BlueprintPure, Category = "Customer|Recovery")
	UCustomerKnockdownComponent* GetCustomerKnockdown() const { return CustomerKnockdown; }

	UFUNCTION(BlueprintPure, Category = "Customer|Recovery")
	UCustomerRoutineInterruptionComponent* GetCustomerRoutineInterruption() const { return CustomerRoutineInterruption; }

	UFUNCTION(BlueprintPure, Category = "Customer|Queue")
	UCustomerQueueNavigationComponent* GetCustomerQueueNavigation() const { return CustomerQueueNavigation; }

	UFUNCTION(BlueprintImplementableEvent, Category = "Customer|Presentation")
	void OnActivityStarted(EBathhouseCustomerActivity ActivityType);

	UFUNCTION(BlueprintImplementableEvent, Category = "Customer|Presentation")
	void OnActivityFinished(EBathhouseCustomerActivity ActivityType);

	UFUNCTION(BlueprintImplementableEvent, Category = "Customer|Presentation")
	void OnCustomerPresentationStateChanged(EBathhouseCustomerPresentationState PresentationState);

	UFUNCTION(BlueprintImplementableEvent, Category = "Customer|Presentation")
	void OnCustomerSatisfactionChanged(float PreviousSatisfaction, float NewSatisfaction);

	UPROPERTY(BlueprintAssignable, Category = "Customer")
	FOnBathhouseCustomerFinished OnCustomerFinished;

protected:
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Customer")
	TObjectPtr<UCustomerRoutineDefinition> RoutineDefinition = nullptr;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Customer")
	TObjectPtr<ABathhouseCounterActor> Counter = nullptr;

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Customer", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCustomerSessionComponent> CustomerSession;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Customer", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCustomerMontagePlaybackComponent> CustomerMontagePlayback;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Customer|Combat", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UHealthComponent> Health;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Customer|Recovery", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCustomerKnockdownComponent> CustomerKnockdown;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Customer|Recovery", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCustomerRoutineInterruptionComponent> CustomerRoutineInterruption;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Customer|Queue", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCustomerQueueNavigationComponent> CustomerQueueNavigation;

	bool bFinishBroadcast = false;
};
