#include "Cleaning/CleaningDirectorActor.h"

#include "Cleaning/CleaningWorldSubsystem.h"
#include "Cleaning/StainSpawnZoneActor.h"
#include "Cleaning/WaterStainActor.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

ACleaningDirectorActor::ACleaningDirectorActor()
{
	PrimaryActorTick.bCanEverTick = false;
}

void ACleaningDirectorActor::BeginPlay()
{
	Super::BeginPlay();
	GetWorldTimerManager().SetTimer(
		SpawnTimerHandle,
		this,
		&ACleaningDirectorActor::TrySpawnStain,
		FMath::Max(0.1f, SpawnIntervalSeconds),
		true);
}

void ACleaningDirectorActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(SpawnTimerHandle);
	Super::EndPlay(EndPlayReason);
}

void ACleaningDirectorActor::TrySpawnStain()
{
	UWorld* World = GetWorld();
	UCleaningWorldSubsystem* Subsystem = World ? World->GetSubsystem<UCleaningWorldSubsystem>() : nullptr;
	if (!World || !Subsystem || !StainClass || Subsystem->GetActiveStainCount() >= MaxActiveStains)
	{
		return;
	}
	TArray<AStainSpawnZoneActor*> EligibleZones = Subsystem->GetActiveZones().FilterByPredicate(
		[Subsystem](const AStainSpawnZoneActor* Zone)
		{
			return IsValid(Zone)
				&& Subsystem->GetActiveStainCountForZone(Zone) < Zone->GetMaxActiveStains();
		});
	FRandomStream RandomStream(FMath::Rand());
	for (int32 Attempt = 0; Attempt < MaxPlacementAttemptsPerInterval && !EligibleZones.IsEmpty(); ++Attempt)
	{
		AStainSpawnZoneActor* Zone = SelectZone(EligibleZones, RandomStream);
		FTransform SpawnTransform;
		if (!Zone || !Zone->FindSpawnTransform(
			RandomStream,
			DefaultStainSpacing,
			DefaultPawnClearance,
			SpawnTransform))
		{
			continue;
		}
		AWaterStainActor* Stain = World->SpawnActorDeferred<AWaterStainActor>(
			StainClass,
			SpawnTransform,
			this,
			nullptr,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (Stain)
		{
			Stain->ConfigureVisualVariationSeed(RandomStream.RandHelper(MAX_int32));
			Stain->SetSpawnZone(Zone);
			UGameplayStatics::FinishSpawningActor(Stain, SpawnTransform);
		}
		return;
	}
}

AStainSpawnZoneActor* ACleaningDirectorActor::SelectZone(
	const TArray<AStainSpawnZoneActor*>& Zones,
	FRandomStream& RandomStream) const
{
	float TotalWeight = 0.0f;
	for (const AStainSpawnZoneActor* Zone : Zones)
	{
		TotalWeight += FMath::Max(0.0f, Zone->GetSelectionWeight());
	}
	if (TotalWeight <= 0.0f)
	{
		return Zones.IsEmpty() ? nullptr : Zones[0];
	}
	float Choice = RandomStream.FRandRange(0.0f, TotalWeight);
	for (AStainSpawnZoneActor* Zone : Zones)
	{
		Choice -= FMath::Max(0.0f, Zone->GetSelectionWeight());
		if (Choice <= 0.0f)
		{
			return Zone;
		}
	}
	return Zones.Last();
}
