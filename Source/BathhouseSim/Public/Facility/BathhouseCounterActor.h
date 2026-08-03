#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "GameFramework/Actor.h"
#include "Facility/BathhouseFacilityTypes.h"
#include "BathhouseCounterActor.generated.h"

class USceneComponent;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnCounterQueueChangedNative, EBathhouseCounterLane);
DECLARE_MULTICAST_DELEGATE(FOnReturnedKeySlotsChangedNative);

USTRUCT()
struct FBathhouseQueueEntry
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	TObjectPtr<AActor> Actor = nullptr;

	uint64 Sequence = 0;
};

USTRUCT()
struct FBathhouseReturnedObjectSlot
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> Point = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<AActor> ReservationOwner = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<AActor> ReturnedObject = nullptr;
};

UCLASS(Blueprintable)
class BATHHOUSESIM_API ABathhouseCounterActor : public AActor
{
	GENERATED_BODY()

public:
	ABathhouseCounterActor();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	bool EnqueueActor(EBathhouseCounterLane Lane, AActor* Actor);
	bool DequeueActor(EBathhouseCounterLane Lane, AActor* Actor);
	bool IsFront(EBathhouseCounterLane Lane, const AActor* Actor) const;
	bool GetQueueTargetTransform(EBathhouseCounterLane Lane, const AActor* Actor, FTransform& OutTransform) const;

	bool TryReserveReturnedObjectSlot(AActor* Requestor, int32& OutSlotIndex, FTransform& OutTransform);
	bool PlaceReturnedObject(AActor* Requestor, int32 SlotIndex, AActor* ReturnedObject);
	bool TakeReturnedObject(AActor* ReturnedObject);
	bool ReleaseReturnedObjectReservation(AActor* Requestor, int32 SlotIndex);
	USceneComponent* GetReturnSlotComponent(int32 SlotIndex) const;
	USceneComponent* GetCashOfferPoint() const { return CashOfferPoint; }

	UFUNCTION(BlueprintImplementableEvent, Category = "Bathhouse Counter")
	void OnQueueChanged(EBathhouseCounterLane Lane);

	UFUNCTION(BlueprintImplementableEvent, Category = "Bathhouse Counter")
	void OnReturnedKeySlotsChanged();

	FOnCounterQueueChangedNative OnQueueChangedNative;
	FOnReturnedKeySlotsChangedNative OnReturnedKeySlotsChangedNative;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Bathhouse Counter")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Bathhouse Counter")
	TObjectPtr<USceneComponent> CheckInServicePoint;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Bathhouse Counter")
	TObjectPtr<USceneComponent> CheckoutServicePoint;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Bathhouse Counter")
	TObjectPtr<USceneComponent> CashOfferPoint;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Bathhouse Counter", meta = (UseComponentPicker, AllowedClasses = "/Script/Engine.SceneComponent"))
	TArray<FComponentReference> CheckInQueuePointReferences;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Bathhouse Counter", meta = (UseComponentPicker, AllowedClasses = "/Script/Engine.SceneComponent"))
	TArray<FComponentReference> CheckoutQueuePointReferences;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Bathhouse Counter", meta = (UseComponentPicker, AllowedClasses = "/Script/Engine.SceneComponent"))
	TArray<FComponentReference> ReturnedKeyPointReferences;

private:
	friend class FBathhouseKeyRecoveryTest;
	friend class FBathhouseCounterPointReferenceTest;

	TArray<FBathhouseQueueEntry>& GetMutableQueue(EBathhouseCounterLane Lane);
	const TArray<FBathhouseQueueEntry>& GetQueue(EBathhouseCounterLane Lane) const;
	USceneComponent* GetServicePoint(EBathhouseCounterLane Lane) const;
	const TArray<TObjectPtr<USceneComponent>>& GetQueuePoints(EBathhouseCounterLane Lane) const;
	void ResolveConfiguredPoints();
	void ResolvePointReferences(
		const TCHAR* RoleName,
		const TArray<FComponentReference>& References,
		TArray<TObjectPtr<USceneComponent>>& OutPoints,
		TSet<USceneComponent*>& UsedAcrossRoles);
	void BroadcastQueueChanged(EBathhouseCounterLane Lane);

	UPROPERTY(Transient)
	TArray<TObjectPtr<USceneComponent>> ResolvedCheckInQueuePoints;

	UPROPERTY(Transient)
	TArray<TObjectPtr<USceneComponent>> ResolvedCheckoutQueuePoints;

	UPROPERTY(Transient)
	TArray<TObjectPtr<USceneComponent>> ResolvedReturnedKeyPoints;

	UPROPERTY(Transient)
	TArray<FBathhouseQueueEntry> CheckInQueue;

	UPROPERTY(Transient)
	TArray<FBathhouseQueueEntry> CheckoutQueue;

	UPROPERTY(Transient)
	TArray<FBathhouseReturnedObjectSlot> RuntimeReturnedSlots;

	uint64 NextQueueSequence = 1;
};
