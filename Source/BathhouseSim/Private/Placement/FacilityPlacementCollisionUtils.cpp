#include "Placement/FacilityPlacementCollisionUtils.h"

#include "Components/PrimitiveComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"

bool FacilityPlacementCollision::HasBlockingOverlap(
	UWorld& World,
	const FVector& Location,
	const FQuat& Rotation,
	const FCollisionShape& Shape,
	const UPrimitiveComponent& CollisionSource,
	const FCollisionQueryParams& QueryParams)
{
	TArray<FOverlapResult> Overlaps;
	World.OverlapMultiByChannel(
		Overlaps,
		Location,
		Rotation,
		CollisionSource.GetCollisionObjectType(),
		Shape,
		QueryParams,
		FCollisionResponseParams(CollisionSource.GetCollisionResponseToChannels()));
	return Overlaps.ContainsByPredicate([](const FOverlapResult& Overlap)
	{
		return Overlap.bBlockingHit && IsValid(Overlap.GetComponent());
	});
}
