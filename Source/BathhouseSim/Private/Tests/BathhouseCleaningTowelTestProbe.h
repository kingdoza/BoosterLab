#pragma once

#include "CoreMinimal.h"
#include "Interaction/InteractionTypes.h"
#include "Towel/TowelTypes.h"
#include "UObject/Object.h"
#include "BathhouseCleaningTowelTestProbe.generated.h"

class UTowelInventoryComponent;
class AWaterStainActor;
class AWetMopActor;
class UPlayerCarryComponent;
class UPlayerInteractionComponent;

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
