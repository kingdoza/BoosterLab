#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Customer/BathhouseCustomerTypes.h"
#include "CustomerRoutineDefinition.generated.h"

UCLASS(BlueprintType)
class BATHHOUSESIM_API UCustomerRoutineDefinition : public UDataAsset
{
	GENERATED_BODY()

public:
	float GetActivityDuration(EBathhouseCustomerActivity Activity) const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Check In", meta = (ClampMin = "0.1"))
	float CheckInTimeoutSeconds = 60.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bath", meta = (ClampMin = "0.1"))
	float BathStayDurationSeconds = 60.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bath", meta = (ClampMin = "0.1"))
	float BathDwellMinSeconds = 10.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bath", meta = (ClampMin = "0.1"))
	float BathDwellMaxSeconds = 20.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Activities", meta = (ClampMin = "0.0"))
	float StoreShoesSeconds = 2.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Activities", meta = (ClampMin = "0.0"))
	float UndressSeconds = 3.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Activities", meta = (ClampMin = "0.0"))
	float PreShowerSeconds = 5.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Activities", meta = (ClampMin = "0.0"))
	float MainShowerSeconds = 5.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Activities", meta = (ClampMin = "0.0"))
	float DryingSeconds = 3.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Activities", meta = (ClampMin = "0.0"))
	float ReturnTowelSeconds = 2.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Activities", meta = (ClampMin = "0.0"))
	float DressSeconds = 3.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Activities", meta = (ClampMin = "0.0"))
	float WearShoesSeconds = 2.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Retry", meta = (ClampMin = "0.05"))
	float FacilityRetryIntervalSeconds = 0.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Retry", meta = (ClampMin = "0"))
	int32 MaxNavigationRetries = 3;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Towel", meta = (ClampMin = "0.0"))
	float TowelAvailabilityWaitSeconds = 10.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Towel", meta = (ClampMin = "0.0"))
	float TowelUnavailableSatisfactionPenalty = 10.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Economy", meta = (ClampMin = "1"))
	int32 UsageFee = 10000;
};
