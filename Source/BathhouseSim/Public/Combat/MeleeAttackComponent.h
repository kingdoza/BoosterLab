#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/EngineTypes.h"
#include "MeleeAttackComponent.generated.h"

class UCameraComponent;
class UCurveVector;
class UHeldEquipmentMotionComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnMeleeAttackEvent);

UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class BATHHOUSESIM_API UMeleeAttackComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMeleeAttackComponent();
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	bool StartAttack(
		AActor* User,
		AActor* Weapon,
		UCameraComponent* Camera,
		UHeldEquipmentMotionComponent* MotionComponent);
	void CancelAttack();
	bool IsAttacking() const { return bAttacking; }

	UPROPERTY(BlueprintAssignable, Category = "Combat|Presentation")
	FOnMeleeAttackEvent OnAttackStarted;

	UPROPERTY(BlueprintAssignable, Category = "Combat|Presentation")
	FOnMeleeAttackEvent OnAttackHit;

	UPROPERTY(BlueprintAssignable, Category = "Combat|Presentation")
	FOnMeleeAttackEvent OnAttackEnded;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat|Attack", meta = (ClampMin = "0.05"))
	float AttackDurationSeconds = 0.55f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat|Attack", meta = (ClampMin = "0.0"))
	float HitTimeSeconds = 0.22f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat|Attack", meta = (ClampMin = "0.0"))
	float AttackDistance = 145.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat|Attack", meta = (ClampMin = "1.0"))
	float AttackRadius = 65.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat|Attack", meta = (ClampMin = "0.0"))
	float Damage = 40.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat|Attack", meta = (ClampMin = "0.0"))
	float ImpulseStrength = 850.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat|Attack")
	float VerticalImpulse = 220.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat|Attack")
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Pawn;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat|Motion")
	TObjectPtr<UCurveVector> SwingPositionCurve = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat|Motion")
	TObjectPtr<UCurveVector> SwingRotationCurve = nullptr;

private:
	friend class FBathhouseMeleeAttackTest;

	void PerformHit();
	void FinishAttack();

	UPROPERTY(Transient)
	TObjectPtr<AActor> AttackUser = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<AActor> AttackWeapon = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UCameraComponent> AttackCamera = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UHeldEquipmentMotionComponent> ActiveMotion = nullptr;

	float ElapsedSeconds = 0.0f;
	bool bAttacking = false;
	bool bHitCommitted = false;
};
