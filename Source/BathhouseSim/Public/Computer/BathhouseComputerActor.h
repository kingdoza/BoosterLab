#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/PlayerInteractable.h"
#include "BathhouseComputerActor.generated.h"

class UCameraComponent;
class UPlayerComputerUseComponent;
class UStaticMeshComponent;
class UWidgetComponent;

UCLASS()
class BATHHOUSESIM_API ABathhouseComputerActor : public AActor, public IPlayerInteractable
{
	GENERATED_BODY()

public:
	ABathhouseComputerActor();

	virtual FPlayerInteractionQuery QueryInteraction(const FPlayerInteractionContext& Context) const override;
	virtual FPlayerInteractionResult ExecuteInteraction(const FPlayerInteractionContext& Context) override;

	bool TryReserveFor(UPlayerComputerUseComponent* PlayerComputerUse);
	void ReleaseReservation(UPlayerComputerUseComponent* PlayerComputerUse);
	bool IsReservedBy(const UPlayerComputerUseComponent* PlayerComputerUse) const;

	UWidgetComponent* GetScreenWidget() const { return ScreenWidget; }
	UCameraComponent* GetFocusCamera() const { return FocusCamera; }
	float GetFocusBlendInSeconds() const { return FocusBlendInSeconds; }
	float GetFocusBlendOutSeconds() const { return FocusBlendOutSeconds; }
	bool IsScreenReady() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Computer")
	TObjectPtr<UStaticMeshComponent> ComputerMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Computer")
	TObjectPtr<UWidgetComponent> ScreenWidget;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Computer")
	TObjectPtr<UCameraComponent> FocusCamera;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Computer|Focus", meta = (ClampMin = "0.0"))
	float FocusBlendInSeconds = 0.35f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Computer|Focus", meta = (ClampMin = "0.0"))
	float FocusBlendOutSeconds = 0.25f;

private:
	friend class FBathhouseComputerSessionTest;

	UPlayerComputerUseComponent* ResolvePlayerComputerUse(const FPlayerInteractionContext& Context) const;

	UPROPERTY(Transient)
	TWeakObjectPtr<UPlayerComputerUseComponent> CurrentUser;
};
