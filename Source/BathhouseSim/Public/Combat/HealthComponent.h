#pragma once

#include "CoreMinimal.h"
#include "Combat/CombatTypes.h"
#include "Components/ActorComponent.h"
#include "HealthComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FOnHealthChanged,
	float,
	PreviousHealth,
	float,
	CurrentHealth,
	const FCombatDamageContext&,
	DamageContext);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnHealthDepleted, const FCombatDamageContext&, DamageContext);

UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class BATHHOUSESIM_API UHealthComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHealthComponent();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION(BlueprintCallable, Category = "Combat|Health")
	bool ApplyDamage(const FCombatDamageContext& DamageContext);

	UFUNCTION(BlueprintCallable, Category = "Combat|Health")
	bool RestoreHealthToRatio(float HealthRatio);

	UFUNCTION(BlueprintPure, Category = "Combat|Health")
	float GetCurrentHealth() const { return CurrentHealth; }

	UFUNCTION(BlueprintPure, Category = "Combat|Health")
	float GetMaxHealth() const { return MaxHealth; }

	UFUNCTION(BlueprintPure, Category = "Combat|Health")
	bool IsHealthActive() const { return bInitialized && !bDepleted && CurrentHealth > 0.0f; }

	UFUNCTION(BlueprintPure, Category = "Combat|Health")
	bool IsDepleted() const { return bDepleted; }

	UPROPERTY(BlueprintAssignable, Category = "Combat|Health")
	FOnHealthChanged OnHealthChanged;

	UPROPERTY(BlueprintAssignable, Category = "Combat|Health")
	FOnHealthDepleted OnHealthDepleted;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat|Health", meta = (ClampMin = "1.0"))
	float MaxHealth = 100.0f;

private:
	friend class FBathhouseHealthComponentTest;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Combat|Health", meta = (AllowPrivateAccess = "true"))
	float CurrentHealth = 0.0f;

	bool bInitialized = false;
	bool bDepleted = false;
};
