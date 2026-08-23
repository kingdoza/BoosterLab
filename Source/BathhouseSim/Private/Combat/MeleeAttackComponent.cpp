#include "Combat/MeleeAttackComponent.h"

#include "Camera/CameraComponent.h"
#include "Combat/CombatTypes.h"
#include "Combat/HealthComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "Interaction/HeldEquipmentMotionComponent.h"

UMeleeAttackComponent::UMeleeAttackComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
}

void UMeleeAttackComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CancelAttack();
	OnAttackStarted.Clear();
	OnAttackHit.Clear();
	OnAttackEnded.Clear();
	Super::EndPlay(EndPlayReason);
}

bool UMeleeAttackComponent::StartAttack(
	AActor* User,
	AActor* Weapon,
	UCameraComponent* Camera,
	UHeldEquipmentMotionComponent* MotionComponent)
{
	if (bAttacking || !IsValid(User) || !IsValid(Weapon) || !IsValid(Camera))
	{
		return false;
	}
	bAttacking = true;
	bHitCommitted = false;
	ElapsedSeconds = 0.0f;
	AttackUser = User;
	AttackWeapon = Weapon;
	AttackCamera = Camera;
	ActiveMotion = MotionComponent;
	SetComponentTickEnabled(true);
	if (MotionComponent && Weapon->GetRootComponent())
	{
		MotionComponent->StartOneShot(
			Weapon->GetRootComponent(),
			SwingPositionCurve,
			SwingRotationCurve,
			FMath::Max(0.05f, AttackDurationSeconds));
	}
	OnAttackStarted.Broadcast();
	return true;
}

void UMeleeAttackComponent::TickComponent(
	const float DeltaTime,
	const ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!bAttacking)
	{
		return;
	}
	ElapsedSeconds += FMath::Max(0.0f, DeltaTime);
	if (!bHitCommitted && ElapsedSeconds >= FMath::Clamp(HitTimeSeconds, 0.0f, AttackDurationSeconds))
	{
		bHitCommitted = true;
		PerformHit();
	}
	if (ElapsedSeconds >= FMath::Max(0.05f, AttackDurationSeconds))
	{
		FinishAttack();
	}
}

void UMeleeAttackComponent::CancelAttack()
{
	if (!bAttacking)
	{
		return;
	}
	if (ActiveMotion)
	{
		ActiveMotion->StopMotion();
	}
	FinishAttack();
}

void UMeleeAttackComponent::PerformHit()
{
	UWorld* World = GetWorld();
	if (!World || !AttackCamera || !AttackUser || !AttackWeapon)
	{
		return;
	}
	const FVector CameraOrigin = AttackCamera->GetComponentLocation();
	const FVector CameraDirection = AttackCamera->GetForwardVector().GetSafeNormal();
	if (CameraDirection.IsNearlyZero())
	{
		return;
	}
	const FVector Center = CameraOrigin + CameraDirection * FMath::Max(0.0f, AttackDistance);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(BathhouseMeleeAttack), false);
	Params.AddIgnoredActor(AttackUser);
	Params.AddIgnoredActor(AttackWeapon);
	TArray<FHitResult> Hits;
	World->SweepMultiByChannel(
		Hits,
		Center,
		Center + CameraDirection * UE_KINDA_SMALL_NUMBER,
		FQuat::Identity,
		TraceChannel,
		FCollisionShape::MakeSphere(FMath::Max(1.0f, AttackRadius)),
		Params);

	TSet<TObjectPtr<AActor>> DamagedActors;
	for (const FHitResult& Hit : Hits)
	{
		AActor* Target = Hit.GetActor();
		if (!IsValid(Target) || Target == AttackUser || Target == AttackWeapon || DamagedActors.Contains(Target))
		{
			continue;
		}
		UHealthComponent* Health = Target->FindComponentByClass<UHealthComponent>();
		if (!Health || !Health->IsHealthActive())
		{
			continue;
		}
		DamagedActors.Add(Target);
		FCombatDamageContext DamageContext;
		DamageContext.InstigatorActor = AttackUser;
		DamageContext.CauserActor = AttackWeapon;
		DamageContext.Damage = FMath::Max(0.0f, Damage);
		DamageContext.CameraOrigin = CameraOrigin;
		DamageContext.CameraDirection = CameraDirection;
		DamageContext.ImpulseStrength = FMath::Max(0.0f, ImpulseStrength);
		DamageContext.VerticalImpulse = VerticalImpulse;
		Health->ApplyDamage(DamageContext);
	}
	OnAttackHit.Broadcast();
}

void UMeleeAttackComponent::FinishAttack()
{
	if (!bAttacking)
	{
		return;
	}
	bAttacking = false;
	bHitCommitted = false;
	ElapsedSeconds = 0.0f;
	AttackUser = nullptr;
	AttackWeapon = nullptr;
	AttackCamera = nullptr;
	ActiveMotion = nullptr;
	SetComponentTickEnabled(false);
	OnAttackEnded.Broadcast();
}
