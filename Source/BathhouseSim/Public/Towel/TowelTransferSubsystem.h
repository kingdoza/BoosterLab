#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Towel/TowelTypes.h"
#include "TowelTransferSubsystem.generated.h"

UCLASS()
class BATHHOUSESIM_API UTowelTransferSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	FTowelTransferResult TryTransfer(const FTowelTransferRequest& Request);

private:
	int64 NextTransactionId = 1;
};
