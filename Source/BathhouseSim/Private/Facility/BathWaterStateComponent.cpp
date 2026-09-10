#include "Facility/BathWaterStateComponent.h"

UBathWaterStateComponent::UBathWaterStateComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

bool UBathWaterStateComponent::SetWaterState(const EBathWaterState NewState)
{
	if (WaterState == NewState)
	{
		return false;
	}
	const EBathWaterState Previous = WaterState;
	WaterState = NewState;
	OnWaterStateChanged.Broadcast(Previous, WaterState);
	return true;
}

void UBathWaterStateComponent::SetNormalizedAmount(const float NewAmount)
{
	NormalizedAmount = FMath::Clamp(NewAmount, 0.0f, 1.0f);
}

void UBathWaterStateComponent::ResetEmptyForPlacement()
{
	WaterState = EBathWaterState::Empty;
	NormalizedAmount = 0.0f;
}
