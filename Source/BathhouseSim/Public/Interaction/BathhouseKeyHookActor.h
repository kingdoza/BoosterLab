#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/PlayerInteractable.h"
#include "BathhouseKeyHookActor.generated.h"

class ABathhouseKeyActor;
class UBoxComponent;
class USceneComponent;

UCLASS(Blueprintable)
class BATHHOUSESIM_API ABathhouseKeyHookActor : public AActor, public IPlayerInteractable
{
	GENERATED_BODY()

public:
	ABathhouseKeyHookActor();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	virtual FPlayerInteractionQuery QueryInteraction(const FPlayerInteractionContext& Context) const override;
	virtual FPlayerInteractionResult ExecuteInteraction(const FPlayerInteractionContext& Context) override;

	UFUNCTION(BlueprintPure, Category = "Bathhouse Key")
	int32 GetKeyNumber() const { return KeyNumber; }

	UFUNCTION(BlueprintPure, Category = "Bathhouse Key")
	ABathhouseKeyActor* GetKeyActor() const { return KeyActor; }

	USceneComponent* GetKeyAnchor() const { return KeyAnchor; }
	bool IsNumberTopologyValid(FText* OutFailureReason = nullptr) const;

protected:
	friend class FBathhousePhysicalCarryDropTest;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Bathhouse Key")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Bathhouse Key")
	TObjectPtr<UBoxComponent> InteractionCollision;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Bathhouse Key")
	TObjectPtr<USceneComponent> KeyAnchor;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Bathhouse Key", meta = (ClampMin = "0"))
	int32 KeyNumber = 0;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Bathhouse Key")
	TObjectPtr<ABathhouseKeyActor> KeyActor = nullptr;
};
