#pragma once

#include "CoreMinimal.h"

class UPrimitiveComponent;
class UWorld;

namespace FacilityPlacementCollision
{
	bool HasBlockingOverlap(
		UWorld& World,
		const FVector& Location,
		const FQuat& Rotation,
		const FCollisionShape& Shape,
		const UPrimitiveComponent& CollisionSource,
		const FCollisionQueryParams& QueryParams);
}
