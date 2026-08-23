#include "Interaction/HeldEquipmentMotionComponent.h"

#include "Components/SceneComponent.h"
#include "Curves/CurveVector.h"

UHeldEquipmentMotionComponent::UHeldEquipmentMotionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
}

void UHeldEquipmentMotionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopMotion();
	Super::EndPlay(EndPlayReason);
}

void UHeldEquipmentMotionComponent::TickComponent(
	const float DeltaTime,
	const ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (MotionMode == EHeldEquipmentMotionMode::None || !MotionTarget.IsValid())
	{
		StopMotion();
		return;
	}

	ElapsedSeconds += FMath::Max(0.0f, DeltaTime);
	if (MotionMode == EHeldEquipmentMotionMode::OneShot)
	{
		ApplyMotion(FMath::Min(ElapsedSeconds, MotionDurationSeconds));
		if (ElapsedSeconds >= MotionDurationSeconds)
		{
			StopMotion();
		}
		return;
	}

	const float LoopTime = FMath::Fmod(ElapsedSeconds, MotionDurationSeconds);
	ApplyMotion(LoopTime);
}

bool UHeldEquipmentMotionComponent::StartOneShot(
	USceneComponent* Target,
	UCurveVector* PositionCurve,
	UCurveVector* RotationCurve,
	const float DurationSeconds)
{
	return StartMotion(Target, PositionCurve, RotationCurve, DurationSeconds, EHeldEquipmentMotionMode::OneShot);
}

bool UHeldEquipmentMotionComponent::StartLoop(
	USceneComponent* Target,
	UCurveVector* PositionCurve,
	UCurveVector* RotationCurve,
	const float PeriodSeconds)
{
	return StartMotion(Target, PositionCurve, RotationCurve, PeriodSeconds, EHeldEquipmentMotionMode::LoopWhileInputHeld);
}

bool UHeldEquipmentMotionComponent::StartMotion(
	USceneComponent* Target,
	UCurveVector* PositionCurve,
	UCurveVector* RotationCurve,
	const float DurationSeconds,
	const EHeldEquipmentMotionMode Mode)
{
	if (!IsValid(Target) || DurationSeconds <= 0.0f || (!PositionCurve && !RotationCurve))
	{
		return false;
	}
	StopMotion();
	MotionTarget = Target;
	ActivePositionCurve = PositionCurve;
	ActiveRotationCurve = RotationCurve;
	BaselineRelativeTransform = Target->GetRelativeTransform();
	ElapsedSeconds = 0.0f;
	MotionDurationSeconds = DurationSeconds;
	MotionMode = Mode;
	SetComponentTickEnabled(true);
	ApplyMotion(0.0f);
	return true;
}

void UHeldEquipmentMotionComponent::StopMotion()
{
	RestoreBaseline();
	MotionTarget.Reset();
	ActivePositionCurve.Reset();
	ActiveRotationCurve.Reset();
	ElapsedSeconds = 0.0f;
	MotionDurationSeconds = 0.0f;
	MotionMode = EHeldEquipmentMotionMode::None;
	SetComponentTickEnabled(false);
}

void UHeldEquipmentMotionComponent::ApplyMotion(const float EvaluationTime)
{
	USceneComponent* Target = MotionTarget.Get();
	if (!Target)
	{
		return;
	}
	const FVector PositionOffset = ActivePositionCurve.IsValid()
		? ActivePositionCurve->GetVectorValue(EvaluationTime)
		: FVector::ZeroVector;
	const FVector RotationOffset = ActiveRotationCurve.IsValid()
		? ActiveRotationCurve->GetVectorValue(EvaluationTime)
		: FVector::ZeroVector;
	const FQuat OffsetRotation = FRotator(RotationOffset.Y, RotationOffset.Z, RotationOffset.X).Quaternion();
	Target->SetRelativeLocationAndRotation(
		BaselineRelativeTransform.GetLocation() + PositionOffset,
		BaselineRelativeTransform.GetRotation() * OffsetRotation);
}

void UHeldEquipmentMotionComponent::RestoreBaseline()
{
	if (USceneComponent* Target = MotionTarget.Get())
	{
		Target->SetRelativeTransform(BaselineRelativeTransform);
	}
}
