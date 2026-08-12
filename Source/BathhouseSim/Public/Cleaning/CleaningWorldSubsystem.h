#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "CleaningWorldSubsystem.generated.h"

class AStainSpawnZoneActor;
class AWaterStainActor;

UCLASS()
class BATHHOUSESIM_API UCleaningWorldSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	void RegisterZone(AStainSpawnZoneActor* Zone);
	void UnregisterZone(AStainSpawnZoneActor* Zone);
	void RegisterStain(AWaterStainActor* Stain);
	void UnregisterStain(AWaterStainActor* Stain);

	TArray<AStainSpawnZoneActor*> GetActiveZones();
	int32 GetActiveStainCount();
	int32 GetActiveStainCountForZone(const AStainSpawnZoneActor* Zone);
	bool IsStainLocationClear(const FVector& Location, float MinimumSpacing);

private:
	void Compact();

	TArray<TWeakObjectPtr<AStainSpawnZoneActor>> Zones;
	TArray<TWeakObjectPtr<AWaterStainActor>> Stains;
};
