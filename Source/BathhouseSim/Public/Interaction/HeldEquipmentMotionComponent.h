#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HeldEquipmentMotionComponent.generated.h"

class UCurveVector;
class USceneComponent;

UENUM()
enum class EHeldEquipmentMotionMode : uint8
{
	None,
	OneShot,
	LoopWhileInputHeld
};

UCLASS(ClassGroup = (Interaction), meta = (BlueprintSpawnableComponent))
class BATHHOUSESIM_API UHeldEquipmentMotionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHeldEquipmentMotionComponent();

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	bool StartOneShot(
		USceneComponent* Target,
		UCurveVector* PositionCurve,
		UCurveVector* RotationCurve,
		float DurationSeconds);
	bool StartLoop(
		USceneComponent* Target,
		UCurveVector* PositionCurve,
		UCurveVector* RotationCurve,
		float PeriodSeconds);
	void StopMotion();

	bool IsMotionActive() const { return MotionMode != EHeldEquipmentMotionMode::None; }
	EHeldEquipmentMotionMode GetMotionMode() const { return MotionMode; }

private:
	friend class FBathhouseHeldEquipmentMotionTest;

	bool StartMotion(
		USceneComponent* Target,
		UCurveVector* PositionCurve,
		UCurveVector* RotationCurve,
		float DurationSeconds,
		EHeldEquipmentMotionMode Mode);
	void ApplyMotion(float EvaluationTime);
	void RestoreBaseline();

	TWeakObjectPtr<USceneComponent> MotionTarget;
	TWeakObjectPtr<UCurveVector> ActivePositionCurve;
	TWeakObjectPtr<UCurveVector> ActiveRotationCurve;
	FTransform BaselineRelativeTransform;
	float ElapsedSeconds = 0.0f;
	float MotionDurationSeconds = 0.0f;
	EHeldEquipmentMotionMode MotionMode = EHeldEquipmentMotionMode::None;
};
