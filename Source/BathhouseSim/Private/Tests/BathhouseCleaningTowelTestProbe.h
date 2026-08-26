#pragma once

#include "CoreMinimal.h"
#include "Cleaning/WetMopActor.h"
#include "Interaction/InteractionTypes.h"
#include "Towel/TowelTypes.h"
#include "UObject/Object.h"
#include "BathhouseCleaningTowelTestProbe.generated.h"

class UTowelInventoryComponent;
class AWaterStainActor;
class AWetMopActor;
class UPlayerCarryComponent;
class UPlayerInteractionComponent;
class APhysicalCarryFixedSlotActor;

UCLASS(Transient, NotBlueprintable)
class ABathhousePhysicalCarryFailureProbeActor : public AWetMopActor
{
	GENERATED_BODY()

public:
	enum class EFailurePoint : uint8
	{
		None,
		FixedTake,
		FixedStore,
		FreeDrop
	};

	void SetFailurePoint(const EFailurePoint InFailurePoint) { FailurePoint = InFailurePoint; }

	virtual bool NotifyTakenFromFixedSlotCommitted(
		UPlayerCarryComponent& Carry,
		AActor& SlotActor) override;
	virtual bool NotifyStoredInFixedSlotCommitted(
		UPlayerCarryComponent& Carry,
		AActor& SlotActor) override;
	virtual bool NotifyPhysicalDropCommitted(UPlayerCarryComponent& Carry) override;

private:
	EFailurePoint FailurePoint = EFailurePoint::None;
};

UCLASS(Transient, NotBlueprintable)
class UBathhousePhysicalCarryCommitProbe : public UObject
{
	GENERATED_BODY()

public:
	void Bind(
		AWetMopActor* InMop,
		UPlayerCarryComponent* InCarry,
		APhysicalCarryFixedSlotActor* InSlot = nullptr);
	void Unbind();
	void ResetCounts();

	bool bAttemptLowLevelReleaseWhenHeld = false;
	bool bDestroyItemOnHeldPresentation = false;
	int32 ItemPresentationCount = 0;
	int32 CarryPresentationCount = 0;
	int32 SlotPresentationCount = 0;
	int32 LowLevelReleaseAttemptCount = 0;
	bool bLowLevelReleaseSucceeded = false;

private:
	UFUNCTION()
	void HandleItemPresentationChanged(bool bIsHeld);

	UFUNCTION()
	void HandleCarryPresentationChanged(AActor* HeldObject);

	UFUNCTION()
	void HandleSlotPresentationChanged(bool bIsOccupied);

	UPROPERTY(Transient)
	TObjectPtr<AWetMopActor> Mop = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UPlayerCarryComponent> Carry = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<APhysicalCarryFixedSlotActor> Slot = nullptr;
};

UCLASS(Transient, NotBlueprintable)
class UBathhouseTowelAtomicCommitProbe : public UObject
{
	GENERATED_BODY()

public:
	void Bind(UTowelInventoryComponent* InObserved, UTowelInventoryComponent* InPeer);
	void Unbind();

	int32 BroadcastCount = 0;
	FTowelInventorySnapshot ObservedAtBroadcast;
	FTowelInventorySnapshot PeerAtBroadcast;
	int64 ObservedTransactionId = 0;

private:
	UFUNCTION()
	void HandleInventoryChanged(
		const FTowelInventorySnapshot& Previous,
		const FTowelInventorySnapshot& Current,
		int64 TransactionId);

	UPROPERTY(Transient)
	TObjectPtr<UTowelInventoryComponent> Observed = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UTowelInventoryComponent> Peer = nullptr;
};

UCLASS(Transient, NotBlueprintable)
class UBathhouseCleaningCancelProbe : public UObject
{
	GENERATED_BODY()

public:
	void Bind(AWaterStainActor* InStain);
	void Unbind();

	int32 CancelCount = 0;

private:
	UFUNCTION()
	void HandleCleaningCancelled();

	UPROPERTY(Transient)
	TObjectPtr<AWaterStainActor> Stain = nullptr;
};

UCLASS(Transient, NotBlueprintable)
class UBathhouseInteractionQueryProbe : public UObject
{
	GENERATED_BODY()

public:
	void Bind(UPlayerInteractionComponent* InInteraction);
	void Unbind();

	int32 BroadcastCount = 0;
	FPlayerInteractionQuery LastQuery;

private:
	UFUNCTION()
	void HandleInteractionQueryChanged(const FPlayerInteractionQuery& Query);

	UPROPERTY(Transient)
	TObjectPtr<UPlayerInteractionComponent> Interaction = nullptr;
};

UCLASS(Transient, NotBlueprintable)
class UBathhousePhysicalDropReentryProbe : public UObject
{
	GENERATED_BODY()

public:
	void Bind(
		AWetMopActor* InMop,
		UPlayerCarryComponent* InCarry,
		const FVector& InViewOrigin,
		const FVector& InThrowDirection);
	void Unbind();

	int32 ReleasePresentationCount = 0;
	int32 NestedDropAttemptCount = 0;
	bool bNestedDropSucceeded = false;
	FText NestedDropFailureReason;

private:
	UFUNCTION()
	void HandleHeldPresentationChanged(bool bIsHeld);

	UPROPERTY(Transient)
	TObjectPtr<AWetMopActor> Mop = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UPlayerCarryComponent> Carry = nullptr;

	FVector ViewOrigin = FVector::ZeroVector;
	FVector ThrowDirection = FVector::ForwardVector;
};
