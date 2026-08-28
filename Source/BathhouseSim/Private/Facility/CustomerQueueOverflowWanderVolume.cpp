#include "Facility/CustomerQueueOverflowWanderVolume.h"

#include "AIController.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "GameFramework/Pawn.h"
#include "NavigationData.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"

ACustomerQueueOverflowWanderVolume::ACustomerQueueOverflowWanderVolume()
{
	PrimaryActorTick.bCanEverTick = false;
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
	WanderBounds = CreateDefaultSubobject<UBoxComponent>(TEXT("WanderBounds"));
	WanderBounds->SetupAttachment(SceneRoot);
	WanderBounds->SetBoxExtent(FVector(300.0f, 300.0f, 100.0f));
	WanderBounds->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

bool ACustomerQueueOverflowWanderVolume::TrySampleReachablePoint(
	const AActor& Requestor,
	FVector& OutPoint) const
{
	UWorld* World = GetWorld();
	UNavigationSystemV1* Navigation = World
		? FNavigationSystem::GetCurrent<UNavigationSystemV1>(World)
		: nullptr;
	const ANavigationData* NavigationData = Navigation
		? Navigation->GetDefaultNavDataInstance(FNavigationSystem::DontCreate)
		: nullptr;
	if (!Navigation || !NavigationData || !WanderBounds)
	{
		return false;
	}

	const FVector LocalExtent = WanderBounds->GetUnscaledBoxExtent();
	const FTransform BoundsTransform = WanderBounds->GetComponentTransform();
	UObject* QueryOwner = const_cast<AActor*>(&Requestor);
	if (const APawn* Pawn = Cast<APawn>(&Requestor); Pawn && Pawn->GetController())
	{
		QueryOwner = Pawn->GetController();
	}
	for (int32 Attempt = 0; Attempt < FMath::Max(1, SampleAttemptCount); ++Attempt)
	{
		const FVector LocalSample(
			FMath::FRandRange(-LocalExtent.X, LocalExtent.X),
			FMath::FRandRange(-LocalExtent.Y, LocalExtent.Y),
			FMath::FRandRange(-LocalExtent.Z, LocalExtent.Z));
		FNavLocation Projected;
		if (!Navigation->ProjectPointToNavigation(
			BoundsTransform.TransformPosition(LocalSample),
			Projected,
			NavigationProjectionExtent,
			NavigationData))
		{
			continue;
		}
		if (!ContainsWorldPoint(Projected.Location))
		{
			continue;
		}
		FPathFindingQuery Query(QueryOwner, *NavigationData, Requestor.GetActorLocation(), Projected.Location);
		if (!Navigation->TestPathSync(Query, EPathFindingMode::Regular))
		{
			continue;
		}
		OutPoint = Projected.Location;
		return true;
	}
	return false;
}

bool ACustomerQueueOverflowWanderVolume::ContainsWorldPoint(const FVector& Point) const
{
	if (!WanderBounds)
	{
		return false;
	}
	const FVector LocalPoint = WanderBounds->GetComponentTransform().InverseTransformPosition(Point);
	const FVector Extent = WanderBounds->GetUnscaledBoxExtent();
	return FMath::Abs(LocalPoint.X) <= Extent.X
		&& FMath::Abs(LocalPoint.Y) <= Extent.Y
		&& FMath::Abs(LocalPoint.Z) <= Extent.Z;
}
