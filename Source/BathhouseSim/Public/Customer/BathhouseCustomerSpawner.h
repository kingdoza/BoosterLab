#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Customer/BathhouseCustomerTypes.h"
#include "BathhouseCustomerSpawner.generated.h"

class ABathhouseCounterActor;
class ABathhouseCustomerCharacter;
class UCustomerRoutineDefinition;
class USceneComponent;

UCLASS(Blueprintable)
class BATHHOUSESIM_API ABathhouseCustomerSpawner : public AActor
{
	GENERATED_BODY()

public:
	ABathhouseCustomerSpawner();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	bool SpawnCustomer();

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Customer Spawner")
	TObjectPtr<USceneComponent> EntryPoint;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Customer Spawner")
	TSubclassOf<ABathhouseCustomerCharacter> CustomerClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Customer Spawner")
	TObjectPtr<UCustomerRoutineDefinition> RoutineDefinition = nullptr;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Customer Spawner")
	TObjectPtr<ABathhouseCounterActor> Counter = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Customer Spawner", meta = (ClampMin = "0.1"))
	float SpawnIntervalSeconds = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Customer Spawner", meta = (ClampMin = "1"))
	int32 MaxActiveCustomers = 3;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Customer Spawner")
	bool bAutoStart = true;

private:
	UFUNCTION()
	void HandleCustomerFinished(ABathhouseCustomerCharacter* Customer, EBathhouseCustomerDepartureReason Reason);

	void HandleSpawnTimer();
	void CompactActiveCustomers();

	UPROPERTY(Transient)
	TArray<TObjectPtr<ABathhouseCustomerCharacter>> ActiveCustomers;

	FTimerHandle SpawnTimerHandle;
};
