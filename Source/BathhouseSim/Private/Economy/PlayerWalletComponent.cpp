#include "Economy/PlayerWalletComponent.h"

UPlayerWalletComponent::UPlayerWalletComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

bool UPlayerWalletComponent::CanAddMoney(const int32 Amount) const
{
	return Amount > 0 && CurrentMoney <= MAX_int32 - Amount;
}

bool UPlayerWalletComponent::TryAddMoney(const int32 Amount)
{
	if (!CanAddMoney(Amount))
	{
		return false;
	}
	const int32 PreviousMoney = CurrentMoney;
	CurrentMoney += Amount;
	OnMoneyChanged.Broadcast(PreviousMoney, CurrentMoney);
	return true;
}
