#pragma once

#include "CoreMinimal.h"
#include "FacilityPlacementTypes.generated.h"

UENUM(BlueprintType)
enum class EPlaceableFacilityMode : uint8
{
	Placed,
	Packaged
};

UENUM(BlueprintType)
enum class EFacilityPlacementFailureCode : uint8
{
	None,
	InvalidActor,
	InvalidDefinition,
	InvalidComponents,
	WrongMode,
	Busy,
	NoCompatibleZone,
	OutsideZone,
	InvalidFootprint,
	NoFloorSupport,
	Blocked,
	ExpansionLimit,
	DomainCondition,
	RegistrationFailed,
	CarryCommitFailed,
	StateChanged
};

USTRUCT(BlueprintType)
struct BATHHOUSESIM_API FFacilityPlacementTransactionResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Facility Placement")
	bool bSucceeded = false;

	UPROPERTY(BlueprintReadOnly, Category = "Facility Placement")
	EFacilityPlacementFailureCode FailureCode = EFacilityPlacementFailureCode::None;

	UPROPERTY(BlueprintReadOnly, Category = "Facility Placement")
	FText FailureReason;

	static FFacilityPlacementTransactionResult Succeeded()
	{
		FFacilityPlacementTransactionResult Result;
		Result.bSucceeded = true;
		return Result;
	}

	static FFacilityPlacementTransactionResult Failed(
		EFacilityPlacementFailureCode Code,
		const FText& Reason)
	{
		FFacilityPlacementTransactionResult Result;
		Result.FailureCode = Code;
		Result.FailureReason = Reason;
		return Result;
	}
};
