#include "Economy/BathhousePlayerState.h"

#include "Economy/PlayerWalletComponent.h"

ABathhousePlayerState::ABathhousePlayerState()
{
	Wallet = CreateDefaultSubobject<UPlayerWalletComponent>(TEXT("Wallet"));
}
