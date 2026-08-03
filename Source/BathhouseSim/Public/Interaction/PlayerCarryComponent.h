#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PlayerCarryComponent.generated.h"

class ABathhouseKeyActor;
class USceneComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnHeldKeyChanged, ABathhouseKeyActor*, HeldKey);

UCLASS(ClassGroup = (Interaction), meta = (BlueprintSpawnableComponent))
class BATHHOUSESIM_API UPlayerCarryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPlayerCarryComponent();

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	void ConfigureHeldAnchor(USceneComponent* InHeldAnchor);

	UFUNCTION(BlueprintPure, Category = "Carry")
	bool IsHandEmpty() const { return HeldKey == nullptr; }

	UFUNCTION(BlueprintPure, Category = "Carry")
	ABathhouseKeyActor* GetHeldKey() const { return HeldKey; }

	UPROPERTY(BlueprintAssignable, Category = "Carry")
	FOnHeldKeyChanged OnHeldKeyChanged;

	bool CommitTakeKey(ABathhouseKeyActor* Key);
	bool CommitReleaseKey(ABathhouseKeyActor* Key);
	USceneComponent* GetHeldAnchor() const { return HeldAnchor; }

private:
	UPROPERTY(Transient)
	TObjectPtr<ABathhouseKeyActor> HeldKey = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> HeldAnchor = nullptr;
};
