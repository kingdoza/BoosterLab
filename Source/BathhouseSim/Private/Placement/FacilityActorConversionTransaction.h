#pragma once

#include "CoreMinimal.h"

class AActor;
class AFacilityPlacementZoneActor;
class APlaceableFacilityItemActor;
class UPlayerCarryComponent;

/** Actor replacement mechanics for facility recovery and placement. Domain rules stay behind IPlaceableFacility. */
class FFacilityActorConversionTransaction
{
public:
#if WITH_DEV_AUTOMATION_TESTS
	enum class ETestFault : uint8
	{
		None,
		RecoverySpawn,
		RecoveryPayload,
		RecoveryCommitCollision,
		RecoveryDomainUnregistration,
		RecoveryActivation,
		RecoverySourceDestroy,
		PlacementSpawn,
		PlacementImport,
		PlacementDomainRegistration,
		PlacementCarryCommit
	};

	static void SetTestFault(ETestFault Fault);
	static void ClearTestFault();
#endif

	static bool ValidateRecoveryCandidate(
		AActor& FacilityActor,
		FTransform& OutItemTransform,
		FText& OutFailureReason);

	static APlaceableFacilityItemActor* RecoverFacilityToItem(
		AActor& FacilityActor,
		FText& OutFailureReason);

	static AActor* PlaceItemAsFacility(
		APlaceableFacilityItemActor& ItemActor,
		const FTransform& CandidateTransform,
		const AFacilityPlacementZoneActor& Zone,
		UPlayerCarryComponent& Carry,
		FText& OutFailureReason);

private:
#if WITH_DEV_AUTOMATION_TESTS
	static bool ConsumeTestFault(ETestFault Fault);
#endif
};
