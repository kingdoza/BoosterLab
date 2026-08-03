#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/EngineTypes.h"
#include "Interaction/InteractionTypes.h"
#include "PlayerInteractionComponent.generated.h"

class IPlayerInteractable;
class UCameraComponent;
class UPlayerCarryComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnInteractionQueryChanged, const FPlayerInteractionQuery&, Query);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnInteractionAttemptFinishedNative, const FPlayerInteractionResult&);

UCLASS(ClassGroup = (Interaction), meta = (BlueprintSpawnableComponent))
class BATHHOUSESIM_API UPlayerInteractionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPlayerInteractionComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	void Configure(UCameraComponent* InCamera, UPlayerCarryComponent* InCarryComponent);

	UFUNCTION(BlueprintPure, Category = "Interaction")
	FPlayerInteractionQuery GetCurrentInteractionQuery() const { return CurrentQuery; }

	UFUNCTION(BlueprintCallable, Category = "Interaction")
	FPlayerInteractionResult TryInteract();

	void RefreshInteractionQuery();
	void ClearInteractionQuery();

	UPROPERTY(BlueprintAssignable, Category = "Interaction")
	FOnInteractionQueryChanged OnInteractionQueryChanged;

	FOnInteractionAttemptFinishedNative OnInteractionAttemptFinishedNative;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction", meta = (ClampMin = "1.0"))
	float TraceDistance = 300.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction")
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Visibility;

private:
	friend class FBathhouseInteractionAttemptResultTest;

	bool BuildInteraction(FPlayerInteractionContext& OutContext, IPlayerInteractable*& OutInteractable, UObject*& OutTargetObject) const;
	FPlayerInteractionResult FinishInteractionAttempt(const FPlayerInteractionResult& Result);
	void CommitQuery(UObject* TargetObject, const FPlayerInteractionQuery& NewQuery);

	UPROPERTY(Transient)
	TObjectPtr<UCameraComponent> Camera = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UPlayerCarryComponent> CarryComponent = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UObject> CurrentTarget = nullptr;

	UPROPERTY(Transient)
	FPlayerInteractionQuery CurrentQuery;
};
