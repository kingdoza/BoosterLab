#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CleaningDirectorActor.generated.h"

class AStainSpawnZoneActor;
class AWaterStainActor;

UCLASS(Blueprintable)
class BATHHOUSESIM_API ACleaningDirectorActor : public AActor
{
	GENERATED_BODY()

public:
	ACleaningDirectorActor();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cleaning Spawn", meta = (ClampMin = "0.1"))
	float SpawnIntervalSeconds = 15.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cleaning Spawn", meta = (ClampMin = "1"))
	int32 MaxActiveStains = 20;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cleaning Spawn", meta = (ClampMin = "1"))
	int32 MaxPlacementAttemptsPerInterval = 12;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cleaning Spawn")
	TSubclassOf<AWaterStainActor> StainClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cleaning Spawn", meta = (ClampMin = "0.0"))
	float DefaultStainSpacing = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cleaning Spawn", meta = (ClampMin = "0.0"))
	float DefaultPawnClearance = 80.0f;

private:
	friend class FBathhouseCleaningInteractionTest;

	void TrySpawnStain();
	AStainSpawnZoneActor* SelectZone(const TArray<AStainSpawnZoneActor*>& Zones) const;

	FTimerHandle SpawnTimerHandle;
};
