#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "Facility/BathhouseFacilityTypes.h"
#include "BathhouseFacilitySlotComponent.generated.h"

class UBathhouseFacilitySlotComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnFacilitySlotStateChanged, UBathhouseFacilitySlotComponent*, Slot, EBathhouseFacilitySlotState, PreviousState, EBathhouseFacilitySlotState, NewState);

UCLASS(ClassGroup = (Bathhouse), meta = (BlueprintSpawnableComponent))
class BATHHOUSESIM_API UBathhouseFacilitySlotComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UBathhouseFacilitySlotComponent();

	bool TryReserve(AActor* Requestor);
	bool BeginUse(AActor* Requestor);
	bool EndUse(AActor* Requestor);
	bool Release(AActor* Requestor);
	void ForceRelease();

	UFUNCTION(BlueprintPure, Category = "Bathhouse Facility")
	EBathhouseFacilitySlotState GetSlotState() const { return SlotState; }

	UFUNCTION(BlueprintPure, Category = "Bathhouse Facility")
	AActor* GetCurrentUser() const;

	UFUNCTION(BlueprintPure, Category = "Bathhouse Facility")
	bool IsAvailable() const { return bEnabled && SlotState == EBathhouseFacilitySlotState::Available; }

	UFUNCTION(BlueprintPure, Category = "Bathhouse Facility")
	FTransform GetActionTransform() const;

	UFUNCTION(BlueprintPure, Category = "Bathhouse Facility")
	FTransform GetApproachTransform() const;

	UPROPERTY(BlueprintAssignable, Category = "Bathhouse Facility")
	FOnFacilitySlotStateChanged OnSlotStateChanged;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bathhouse Facility")
	FVector ApproachOffset = FVector(-100.0f, 0.0f, 0.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bathhouse Facility")
	FRotator FacingRotation = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bathhouse Facility")
	bool bEnabled = true;

private:
	friend class FBathhouseFacilitySlotExclusivityTest;

	void SetState(EBathhouseFacilitySlotState NewState, AActor* NewReservation, AActor* NewOccupant);

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Bathhouse Facility", meta = (AllowPrivateAccess = "true"))
	EBathhouseFacilitySlotState SlotState = EBathhouseFacilitySlotState::Available;

	UPROPERTY(Transient)
	TObjectPtr<AActor> ReservationOwner = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<AActor> Occupant = nullptr;
};
