#include "Interaction/CheckoutKeyPlacementUtils.h"

#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "Facility/BathhouseCounterActor.h"
#include "Interaction/BathhouseKeyActor.h"
#include "Interaction/PhysicalCarryPlacementTransaction.h"

namespace
{
bool HasBlockingWorldOverlap(
	const ABathhouseKeyActor& Key,
	const UPrimitiveComponent& Primitive,
	const FTransform& Candidate)
{
	const UWorld* World = Key.GetWorld();
	if (!World)
	{
		return true;
	}
	FCollisionObjectQueryParams ObjectQuery;
	ObjectQuery.AddObjectTypesToQuery(ECC_WorldStatic);
	ObjectQuery.AddObjectTypesToQuery(ECC_WorldDynamic);
	// PhysicsActor-profile carryables use ECC_PhysicsBody. Include them so an
	// already returned physical key blocks the exact candidate as well.
	ObjectQuery.AddObjectTypesToQuery(ECC_PhysicsBody);
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(BathhouseReturnedKeyPlacement), false, &Key);
	TArray<FOverlapResult> Overlaps;
	const bool bAnyOverlap = World->OverlapMultiByObjectType(
		Overlaps,
		Candidate.GetLocation(),
		Candidate.GetRotation(),
		ObjectQuery,
		Primitive.GetCollisionShape(),
		QueryParams);
	if (!bAnyOverlap)
	{
		return false;
	}
	for (const FOverlapResult& Overlap : Overlaps)
	{
		const UPrimitiveComponent* Other = Overlap.Component.Get();
		if (Other
			&& Primitive.GetCollisionResponseToChannel(Other->GetCollisionObjectType()) == ECR_Block
			&& Other->GetCollisionResponseToChannel(Primitive.GetCollisionObjectType()) == ECR_Block)
		{
			return true;
		}
	}
	return false;
}
}

bool BathhouseCheckoutKeyPlacement::TryPlaceKeyInFreeWorld(
	ABathhouseKeyActor& Key,
	const ABathhouseCounterActor& Counter)
{
	UPrimitiveComponent* Primitive = Key.GetPhysicalCarryPrimitive();
	const USceneComponent* DropPoint = Counter.GetReturnedKeyDropPoint();
	if (!Primitive || !DropPoint || !Key.GetWorld())
	{
		return false;
	}

	const FTransform DropTransform = DropPoint->GetComponentTransform();
	const FVector2D SearchExtent = Counter.GetReturnedKeyDropLocalXYExtent().GetAbs();
	const int32 AttemptCount = Counter.GetReturnedKeyDropAttemptCount();
	for (int32 Attempt = 0; Attempt < AttemptCount; ++Attempt)
	{
		const FVector LocalOffset = Attempt == 0
			? FVector::ZeroVector
			: FVector(
				FMath::FRandRange(-SearchExtent.X, SearchExtent.X),
				FMath::FRandRange(-SearchExtent.Y, SearchExtent.Y),
				0.0f);
		FTransform Candidate(
			DropTransform.GetRotation(),
			DropTransform.TransformPosition(LocalOffset),
			Key.GetActorScale3D());
		if (HasBlockingWorldOverlap(Key, *Primitive, Candidate))
		{
			continue;
		}

		FPhysicalCarryPlacementTransaction Transaction(Key, *Primitive);
		if (!Transaction.IsValid()
			|| !Key.SetActorTransform(Candidate, false, nullptr, ETeleportType::TeleportPhysics))
		{
			continue;
		}
		const FVector VelocityChange = DropPoint->GetForwardVector()
			* FMath::Max(0.0f, Key.GetThrowImpulseStrength())
			+ FVector::UpVector * FMath::Max(0.0f, Key.GetUpwardThrowImpulseStrength());
		if (!Transaction.ApplyFreeWorld(VelocityChange))
		{
			continue;
		}
		Transaction.Commit();
		return true;
	}
	return false;
}
