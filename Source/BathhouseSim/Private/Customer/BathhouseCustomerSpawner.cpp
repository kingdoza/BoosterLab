#include "Customer/BathhouseCustomerSpawner.h"

#include "Components/SceneComponent.h"
#include "Customer/BathhouseCustomerCharacter.h"
#include "Customer/CustomerRoutineDefinition.h"
#include "Engine/World.h"
#include "Facility/BathhouseCounterActor.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

ABathhouseCustomerSpawner::ABathhouseCustomerSpawner()
{
	PrimaryActorTick.bCanEverTick = false;
	EntryPoint = CreateDefaultSubobject<USceneComponent>(TEXT("EntryPoint"));
	SetRootComponent(EntryPoint);
	CustomerClass = ABathhouseCustomerCharacter::StaticClass();
}

void ABathhouseCustomerSpawner::BeginPlay()
{
	Super::BeginPlay();
	if (bAutoStart && GetWorld())
	{
		SpawnCustomer();
		GetWorld()->GetTimerManager().SetTimer(
			SpawnTimerHandle,
			this,
			&ABathhouseCustomerSpawner::HandleSpawnTimer,
			FMath::Max(SpawnIntervalSeconds, 0.1f),
			true);
	}
}

void ABathhouseCustomerSpawner::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(SpawnTimerHandle);
	}
	for (ABathhouseCustomerCharacter* Customer : ActiveCustomers)
	{
		if (Customer)
		{
			Customer->OnCustomerFinished.RemoveDynamic(this, &ABathhouseCustomerSpawner::HandleCustomerFinished);
		}
	}
	ActiveCustomers.Reset();
	Super::EndPlay(EndPlayReason);
}

bool ABathhouseCustomerSpawner::SpawnCustomer()
{
	CompactActiveCustomers();
	if (!GetWorld() || !CustomerClass || !RoutineDefinition || !Counter || ActiveCustomers.Num() >= MaxActiveCustomers)
	{
		return false;
	}

	const FTransform SpawnTransform = EntryPoint->GetComponentTransform();
	ABathhouseCustomerCharacter* Customer = GetWorld()->SpawnActorDeferred<ABathhouseCustomerCharacter>(
		CustomerClass,
		SpawnTransform,
		this,
		nullptr,
		ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
	if (!Customer)
	{
		return false;
	}
	Customer->InitializeCustomer(RoutineDefinition, Counter);
	Customer->OnCustomerFinished.AddDynamic(this, &ABathhouseCustomerSpawner::HandleCustomerFinished);
	Customer = Cast<ABathhouseCustomerCharacter>(UGameplayStatics::FinishSpawningActor(Customer, SpawnTransform));
	if (!Customer)
	{
		return false;
	}
	ActiveCustomers.Add(Customer);
	return true;
}

void ABathhouseCustomerSpawner::HandleCustomerFinished(
	ABathhouseCustomerCharacter* Customer,
	const EBathhouseCustomerDepartureReason Reason)
{
	if (Customer)
	{
		Customer->OnCustomerFinished.RemoveDynamic(this, &ABathhouseCustomerSpawner::HandleCustomerFinished);
	}
	ActiveCustomers.Remove(Customer);
}

void ABathhouseCustomerSpawner::HandleSpawnTimer()
{
	SpawnCustomer();
}

void ABathhouseCustomerSpawner::CompactActiveCustomers()
{
	ActiveCustomers.RemoveAll([](const TObjectPtr<ABathhouseCustomerCharacter>& Customer) { return !IsValid(Customer); });
}
