#include "Cleaning/CleaningWorldSubsystem.h"

#include "Cleaning/StainSpawnZoneActor.h"
#include "Cleaning/WaterStainActor.h"

void UCleaningWorldSubsystem::RegisterZone(AStainSpawnZoneActor* Zone)
{
	Compact();
	if (IsValid(Zone) && !Zones.Contains(Zone))
	{
		Zones.Add(Zone);
	}
}

void UCleaningWorldSubsystem::UnregisterZone(AStainSpawnZoneActor* Zone)
{
	Zones.RemoveAll([Zone](const TWeakObjectPtr<AStainSpawnZoneActor>& Entry)
	{
		return !Entry.IsValid() || Entry.Get() == Zone;
	});
}

void UCleaningWorldSubsystem::RegisterStain(AWaterStainActor* Stain)
{
	Compact();
	if (IsValid(Stain) && !Stains.Contains(Stain))
	{
		Stains.Add(Stain);
	}
}

void UCleaningWorldSubsystem::UnregisterStain(AWaterStainActor* Stain)
{
	Stains.RemoveAll([Stain](const TWeakObjectPtr<AWaterStainActor>& Entry)
	{
		return !Entry.IsValid() || Entry.Get() == Stain;
	});
}

TArray<AStainSpawnZoneActor*> UCleaningWorldSubsystem::GetActiveZones()
{
	Compact();
	TArray<AStainSpawnZoneActor*> Result;
	Result.Reserve(Zones.Num());
	for (const TWeakObjectPtr<AStainSpawnZoneActor>& Zone : Zones)
	{
		Result.Add(Zone.Get());
	}
	return Result;
}

int32 UCleaningWorldSubsystem::GetActiveStainCount()
{
	Compact();
	return Stains.Num();
}

int32 UCleaningWorldSubsystem::GetActiveStainCountForZone(const AStainSpawnZoneActor* Zone)
{
	Compact();
	int32 Count = 0;
	for (const TWeakObjectPtr<AWaterStainActor>& Stain : Stains)
	{
		if (Stain.IsValid() && Stain->GetSpawnZone() == Zone)
		{
			++Count;
		}
	}
	return Count;
}

bool UCleaningWorldSubsystem::IsStainLocationClear(const FVector& Location, const float MinimumSpacing)
{
	Compact();
	const float MinimumSpacingSquared = FMath::Square(FMath::Max(0.0f, MinimumSpacing));
	for (const TWeakObjectPtr<AWaterStainActor>& Stain : Stains)
	{
		if (Stain.IsValid() && FVector::DistSquared(Stain->GetActorLocation(), Location) < MinimumSpacingSquared)
		{
			return false;
		}
	}
	return true;
}

void UCleaningWorldSubsystem::Compact()
{
	Zones.RemoveAll([](const TWeakObjectPtr<AStainSpawnZoneActor>& Entry) { return !Entry.IsValid(); });
	Stains.RemoveAll([](const TWeakObjectPtr<AWaterStainActor>& Entry) { return !Entry.IsValid(); });
}
