#include "Cleaning/StainSpawnZoneActor.h"

#include "Cleaning/CleaningWorldSubsystem.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"

AStainSpawnZoneActor::AStainSpawnZoneActor()
{
	PrimaryActorTick.bCanEverTick = false;
	SpawnBounds = CreateDefaultSubobject<UBoxComponent>(TEXT("SpawnBounds"));
	SetRootComponent(SpawnBounds);
	SpawnBounds->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SpawnBounds->InitBoxExtent(FVector(300.0f, 300.0f, 100.0f));
}

void AStainSpawnZoneActor::BeginPlay()
{
	Super::BeginPlay();
	if (UCleaningWorldSubsystem* Subsystem = GetWorld()->GetSubsystem<UCleaningWorldSubsystem>())
	{
		Subsystem->RegisterZone(this);
	}
}

void AStainSpawnZoneActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		if (UCleaningWorldSubsystem* Subsystem = World->GetSubsystem<UCleaningWorldSubsystem>())
		{
			Subsystem->UnregisterZone(this);
		}
	}
	Super::EndPlay(EndPlayReason);
}

bool AStainSpawnZoneActor::FindSpawnTransform(
	FRandomStream& RandomStream,
	const float DefaultStainSpacing,
	const float DefaultPawnClearance,
	FTransform& OutTransform) const
{
	const UWorld* World = GetWorld();
	if (!World || !SpawnBounds)
	{
		return false;
	}
	const FVector Extent = SpawnBounds->GetUnscaledBoxExtent();
	const FVector LocalCandidate(
		RandomStream.FRandRange(-Extent.X, Extent.X),
		RandomStream.FRandRange(-Extent.Y, Extent.Y),
		Extent.Z);
	const FVector TraceStart = SpawnBounds->GetComponentTransform().TransformPosition(LocalCandidate);
	const FVector TraceEnd = TraceStart - FVector::UpVector * (Extent.Z * 2.0f + FloorTraceDistance);
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(CleaningFloorPlacement), true, this);
	FHitResult FloorHit;
	if (!World->LineTraceSingleByChannel(FloorHit, TraceStart, TraceEnd, FloorTraceChannel, QueryParams)
		|| !FloorHit.Component.IsValid())
	{
		return false;
	}
	if (!RequiredFloorComponentTag.IsNone() && !FloorHit.Component->ComponentHasTag(RequiredFloorComponentTag))
	{
		return false;
	}
	const float MinimumUpDot = FMath::Cos(FMath::DegreesToRadians(MaximumFloorSlopeDegrees));
	if (FVector::DotProduct(FloorHit.ImpactNormal.GetSafeNormal(), FVector::UpVector) < MinimumUpDot)
	{
		return false;
	}
	const FVector HitLocal = SpawnBounds->GetComponentTransform().InverseTransformPosition(FloorHit.ImpactPoint);
	if (FMath::Abs(HitLocal.X) > Extent.X || FMath::Abs(HitLocal.Y) > Extent.Y)
	{
		return false;
	}
	UCleaningWorldSubsystem* Subsystem = World->GetSubsystem<UCleaningWorldSubsystem>();
	const float Spacing = StainSpacingOverride > 0.0f ? StainSpacingOverride : DefaultStainSpacing;
	if (!Subsystem || !Subsystem->IsStainLocationClear(FloorHit.ImpactPoint, Spacing))
	{
		return false;
	}
	const float PawnClearance = PawnClearanceOverride > 0.0f ? PawnClearanceOverride : DefaultPawnClearance;
	FCollisionObjectQueryParams PawnObjects;
	PawnObjects.AddObjectTypesToQuery(ECC_Pawn);
	if (PawnClearance > 0.0f && World->OverlapAnyTestByObjectType(
		FloorHit.ImpactPoint,
		FQuat::Identity,
		PawnObjects,
		FCollisionShape::MakeSphere(PawnClearance),
		QueryParams))
	{
		return false;
	}
	OutTransform = FTransform(FRotationMatrix::MakeFromZ(FloorHit.ImpactNormal).ToQuat(), FloorHit.ImpactPoint);
	return true;
}
