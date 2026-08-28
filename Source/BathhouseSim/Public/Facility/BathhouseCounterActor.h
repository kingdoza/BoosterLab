#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "GameFramework/Actor.h"
#include "Facility/BathhouseFacilityTypes.h"
#include "BathhouseCounterActor.generated.h"

class USceneComponent;
class ACustomerQueueOverflowWanderVolume;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnCounterQueueChangedNative, EBathhouseCounterLane);
DECLARE_MULTICAST_DELEGATE(FOnReturnedKeySlotsChangedNative);

USTRUCT()
struct FBathhouseQueueEntry
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> Actor;

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
	bool ResolveQueueAssignment(EBathhouseCounterLane Lane, const AActor* Actor, FBathhouseQueueAssignment& OutAssignment) const;
	int64 GetQueueRevision(EBathhouseCounterLane Lane) const;
	UE_DEPRECATED(5.8, "Use ResolveQueueAssignment and handle OverflowWander explicitly.")
	bool GetQueueTargetTransform(EBathhouseCounterLane Lane, const AActor* Actor, FTransform& OutTransform) const;
	bool TrySampleCheckoutOverflowPoint(const AActor& Requestor, FVector& OutPoint) const;

	UE_DEPRECATED(5.8, "Returned keys now use key-owned free-world placement at ReturnedKeyDropPoint.")
	bool TryReserveReturnedObjectSlot(AActor* Requestor, int32& OutSlotIndex, FTransform& OutTransform);
	UE_DEPRECATED(5.8, "Returned keys no longer occupy Counter runtime slots.")
	bool PlaceReturnedObject(AActor* Requestor, int32 SlotIndex, AActor* ReturnedObject);
	UE_DEPRECATED(5.8, "Player pickup no longer releases a Counter runtime slot.")
	bool TakeReturnedObject(AActor* ReturnedObject);
	UE_DEPRECATED(5.8, "Returned keys no longer reserve Counter runtime slots.")
	bool ReleaseReturnedObjectReservation(AActor* Requestor, int32 SlotIndex);
	UE_DEPRECATED(5.8, "Use GetReturnedKeyDropPoint for authoring only; runtime placement is key-owned.")
	USceneComponent* GetReturnSlotComponent(int32 SlotIndex) const;
	USceneComponent* GetCashOfferPoint() const { return CashOfferPoint; }
	USceneComponent* GetReturnedKeyDropPoint() const { return ReturnedKeyDropPoint; }
	FVector2D GetReturnedKeyDropLocalXYExtent() const { return ReturnedKeyDropLocalXYExtent; }
	int32 GetReturnedKeyDropAttemptCount() const { return FMath::Max(1, ReturnedKeyDropAttemptCount); }
	void NotifyReturnedKeyDropped(AActor* ReturnedKey);

	UFUNCTION(BlueprintImplementableEvent, Category = "Bathhouse Counter")
	void OnQueueChanged(EBathhouseCounterLane Lane);

	UFUNCTION(BlueprintImplementableEvent, Category = "Bathhouse Counter", meta = (DeprecatedFunction, DeprecationMessage = "Returned keys now use ReturnedKeyDropPoint and OnReturnedKeyDropped."))
	void OnReturnedKeySlotsChanged();

	UFUNCTION(BlueprintImplementableEvent, Category = "Bathhouse Counter")
	void OnReturnedKeyDropped(AActor* ReturnedKey);

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

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Bathhouse Counter|Returned Key")
	TObjectPtr<USceneComponent> ReturnedKeyDropPoint;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Bathhouse Counter", meta = (UseComponentPicker, AllowedClasses = "/Script/Engine.SceneComponent"))
	TArray<FComponentReference> CheckInQueuePointReferences;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Bathhouse Counter", meta = (UseComponentPicker, AllowedClasses = "/Script/Engine.SceneComponent"))
	TArray<FComponentReference> CheckoutQueuePointReferences;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Bathhouse Counter", meta = (UseComponentPicker, AllowedClasses = "/Script/Engine.SceneComponent", DeprecatedProperty, DeprecationMessage = "Use ReturnedKeyDropPoint and its search settings."))
	TArray<FComponentReference> ReturnedKeyPointReferences;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Bathhouse Counter|Queue")
	TArray<TObjectPtr<ACustomerQueueOverflowWanderVolume>> CheckoutOverflowVolumes;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bathhouse Counter|Returned Key", meta = (ClampMin = "0.0"))
	FVector2D ReturnedKeyDropLocalXYExtent = FVector2D(35.0f, 20.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bathhouse Counter|Returned Key", meta = (ClampMin = "1", UIMin = "1"))
	int32 ReturnedKeyDropAttemptCount = 8;

private:
	friend class FBathhouseKeyRecoveryTest;
	friend class FBathhouseCounterPointReferenceTest;
	friend class FBathhouseCounterAssignmentTest;
	friend class FBathhouseCheckoutKeyDropTest;

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
	bool CompactInvalidEntries(EBathhouseCounterLane Lane);
	void AdvanceQueueRevision(EBathhouseCounterLane Lane);

	UPROPERTY(Transient)
	TArray<TObjectPtr<USceneComponent>> ResolvedCheckInQueuePoints;

	UPROPERTY(Transient)
	TArray<TObjectPtr<USceneComponent>> ResolvedCheckoutQueuePoints;

	UPROPERTY(Transient)
	TArray<FBathhouseQueueEntry> CheckInQueue;

	UPROPERTY(Transient)
	TArray<FBathhouseQueueEntry> CheckoutQueue;

	uint64 NextQueueSequence = 1;
	int64 CheckInQueueRevision = 1;
	int64 CheckoutQueueRevision = 1;
};
