#include "Combat/HealthComponent.h"

UHealthComponent::UHealthComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UHealthComponent::BeginPlay()
{
	Super::BeginPlay();
	MaxHealth = FMath::Max(1.0f, MaxHealth);
	CurrentHealth = MaxHealth;
	bDepleted = false;
	bInitialized = true;
}

void UHealthComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	OnHealthChanged.Clear();
	OnHealthDepleted.Clear();
	bInitialized = false;
	Super::EndPlay(EndPlayReason);
}

bool UHealthComponent::ApplyDamage(const FCombatDamageContext& DamageContext)
{
	if (!IsHealthActive() || !FMath::IsFinite(DamageContext.Damage) || DamageContext.Damage <= 0.0f)
	{
		return false;
	}
	const float PreviousHealth = CurrentHealth;
	CurrentHealth = FMath::Clamp(CurrentHealth - DamageContext.Damage, 0.0f, MaxHealth);
	if (FMath::IsNearlyEqual(PreviousHealth, CurrentHealth))
	{
		return false;
	}
	OnHealthChanged.Broadcast(PreviousHealth, CurrentHealth, DamageContext);
	if (CurrentHealth <= 0.0f && !bDepleted)
	{
		bDepleted = true;
		OnHealthDepleted.Broadcast(DamageContext);
	}
	return true;
}

bool UHealthComponent::RestoreHealthToRatio(const float HealthRatio)
{
	if (!bInitialized || !FMath::IsFinite(HealthRatio))
	{
		return false;
	}
	const float PreviousHealth = CurrentHealth;
	CurrentHealth = FMath::Clamp(MaxHealth * FMath::Clamp(HealthRatio, 0.0f, 1.0f), 0.0f, MaxHealth);
	bDepleted = CurrentHealth <= 0.0f;
	if (!FMath::IsNearlyEqual(PreviousHealth, CurrentHealth))
	{
		OnHealthChanged.Broadcast(PreviousHealth, CurrentHealth, FCombatDamageContext());
	}
	return CurrentHealth > 0.0f;
}
