#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Facility/BathhouseFacilityTypes.h"
#include "BathhouseFacilityActor.generated.h"

class UBathhouseFacilitySlotComponent;
class USceneComponent;

UCLASS(Blueprintable)
class BATHHOUSESIM_API ABathhouseFacilityActor : public AActor
{
	GENERATED_BODY()

public:
	ABathhouseFacilityActor();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION(BlueprintPure, Category = "Bathhouse Facility")
	EBathhouseFacilityType GetFacilityType() const { return FacilityType; }

	UFUNCTION(BlueprintPure, Category = "Bathhouse Facility")
	int32 GetFacilityNumber() const { return FacilityNumber; }

	UFUNCTION(BlueprintPure, Category = "Bathhouse Facility")
	float GetSelectionWeight() const { return SelectionWeight; }

	UFUNCTION(BlueprintPure, Category = "Bathhouse Facility")
	bool IsFacilityEnabled() const { return bEnabled; }

	const TArray<TObjectPtr<UBathhouseFacilitySlotComponent>>& GetFacilitySlots() const { return FacilitySlots; }

	UFUNCTION(BlueprintImplementableEvent, Category = "Bathhouse Facility")
	void OnSlotReservationChanged(UBathhouseFacilitySlotComponent* Slot, EBathhouseFacilitySlotState NewState);

	UFUNCTION(BlueprintImplementableEvent, Category = "Bathhouse Facility")
	void OnSlotUseStarted(UBathhouseFacilitySlotComponent* Slot, AActor* User);

	UFUNCTION(BlueprintImplementableEvent, Category = "Bathhouse Facility")
	void OnSlotUseEnded(UBathhouseFacilitySlotComponent* Slot, AActor* User);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Bathhouse Facility")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bathhouse Facility")
	EBathhouseFacilityType FacilityType = EBathhouseFacilityType::Bath;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Bathhouse Facility")
	int32 FacilityNumber = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bathhouse Facility", meta = (ClampMin = "0.0"))
	float SelectionWeight = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bathhouse Facility")
	bool bEnabled = true;

private:
	UFUNCTION()
	void HandleSlotStateChanged(UBathhouseFacilitySlotComponent* Slot, EBathhouseFacilitySlotState PreviousState, EBathhouseFacilitySlotState NewState);

	UPROPERTY(Transient)
	TArray<TObjectPtr<UBathhouseFacilitySlotComponent>> FacilitySlots;
};
