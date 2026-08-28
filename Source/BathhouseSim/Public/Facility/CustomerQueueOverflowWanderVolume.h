#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CustomerQueueOverflowWanderVolume.generated.h"

class AActor;
class UBoxComponent;
class USceneComponent;

UCLASS(Blueprintable)
class BATHHOUSESIM_API ACustomerQueueOverflowWanderVolume : public AActor
{
	GENERATED_BODY()

public:
	ACustomerQueueOverflowWanderVolume();

	bool TrySampleReachablePoint(const AActor& Requestor, FVector& OutPoint) const;
	bool ContainsWorldPoint(const FVector& Point) const;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Queue Overflow")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Queue Overflow")
	TObjectPtr<UBoxComponent> WanderBounds;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Queue Overflow", meta = (ClampMin = "1.0"))
	FVector NavigationProjectionExtent = FVector(75.0f, 75.0f, 150.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Queue Overflow", meta = (ClampMin = "1", UIMin = "1"))
	int32 SampleAttemptCount = 12;
};
