#include "Placement/FacilityPlacementZoneActor.h"

#include "Components/BoxComponent.h"
#include "Placement/FacilityPlacementDefinition.h"
#include "Placement/FacilityPlacementSettings.h"

AFacilityPlacementZoneActor::AFacilityPlacementZoneActor()
{
	PrimaryActorTick.bCanEverTick = false;
	ZoneBounds = CreateDefaultSubobject<UBoxComponent>(TEXT("ZoneBounds"));
	SetRootComponent(ZoneBounds);
	ZoneBounds->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	ZoneBounds->SetCollisionResponseToAllChannels(ECR_Ignore);
	ZoneBounds->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	ZoneBounds->SetCanEverAffectNavigation(false);
}

bool AFacilityPlacementZoneActor::IsDefinitionAllowed(const UFacilityPlacementDefinition& Definition) const
{
	return AllowedFacilityTags.IsEmpty() || Definition.FacilityTags.HasAny(AllowedFacilityTags);
}

FTransform AFacilityPlacementZoneActor::MakeCandidateTransform(
	const FVector& WorldPoint,
	const float YawDegrees,
	const bool bSnap) const
{
	FVector Local = GetActorTransform().InverseTransformPosition(WorldPoint);
	Local.Z = ZoneBounds ? ZoneBounds->GetUnscaledBoxExtent().Z : 0.0f;
	if (bSnap)
	{
		const float Grid = GetDefault<UFacilityPlacementSettings>()->GetGridSizeCm();
		Local.X = QuantizeLocalCoordinate(Local.X, Grid);
		Local.Y = QuantizeLocalCoordinate(Local.Y, Grid);
	}
	const FQuat LocalYaw(FVector::UpVector, FMath::DegreesToRadians(NormalizePlacementYaw(YawDegrees)));
	return FTransform(GetActorQuat() * LocalYaw, GetActorTransform().TransformPosition(Local));
}

bool AFacilityPlacementZoneActor::ContainsFootprint(
	const FTransform& CandidateTransform,
	const FVector& WorldHalfExtent) const
{
	if (!ZoneBounds || WorldHalfExtent.ContainsNaN())
	{
		return false;
	}
	const FTransform ZoneTransform = ZoneBounds->GetComponentTransform();
	const FVector Bounds = ZoneBounds->GetUnscaledBoxExtent();
	const FVector AxisX = CandidateTransform.GetUnitAxis(EAxis::X);
	const FVector AxisY = CandidateTransform.GetUnitAxis(EAxis::Y);
	for (const FVector2D Corner : {
		FVector2D(-1.0f, -1.0f),
		FVector2D(-1.0f, 1.0f),
		FVector2D(1.0f, -1.0f),
		FVector2D(1.0f, 1.0f) })
	{
		const FVector WorldCorner = CandidateTransform.GetLocation()
			+ AxisX * (Corner.X * WorldHalfExtent.X)
			+ AxisY * (Corner.Y * WorldHalfExtent.Y);
		const FVector LocalCorner = ZoneTransform.InverseTransformPosition(WorldCorner);
		if (FMath::Abs(LocalCorner.X) > Bounds.X + KINDA_SMALL_NUMBER
			|| FMath::Abs(LocalCorner.Y) > Bounds.Y + KINDA_SMALL_NUMBER)
		{
			return false;
		}
	}
	return true;
}

float AFacilityPlacementZoneActor::QuantizeLocalCoordinate(const float Value, const float GridSize)
{
	return FMath::GridSnap(Value, FMath::Max(1.0f, GridSize));
}

float AFacilityPlacementZoneActor::NormalizePlacementYaw(const float YawDegrees)
{
	return FRotator::NormalizeAxis(YawDegrees);
}
