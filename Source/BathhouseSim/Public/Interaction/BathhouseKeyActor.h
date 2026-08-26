#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/PhysicalCarryable.h"
#include "Interaction/PlayerInteractable.h"
#include "BathhouseKeyActor.generated.h"

class ABathhouseCounterActor;
class ABathhouseKeyHookActor;
class UPlayerCarryComponent;
class UBoxComponent;
class USceneComponent;
class UStaticMeshComponent;

UENUM(BlueprintType)
enum class EBathhouseKeyState : uint8
{
	AtHook,
	HeldByPlayer,
	AssignedToCustomer,
	OnCounter,
	Recovering,
	DroppedInWorld
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnBathhouseKeyStateChanged, EBathhouseKeyState, PreviousState, EBathhouseKeyState, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnHeldKeyPresentationChanged, bool, bIsHeld);

UCLASS(Blueprintable)
class BATHHOUSESIM_API ABathhouseKeyActor : public AActor, public IPlayerInteractable, public IPhysicalCarryable
{
	GENERATED_BODY()

public:
	ABathhouseKeyActor();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void FellOutOfWorld(const UDamageType& DamageType) override;

	virtual FPlayerInteractionQuery QueryInteraction(const FPlayerInteractionContext& Context) const override;
	virtual FPlayerInteractionResult ExecuteInteraction(const FPlayerInteractionContext& Context) override;
	virtual EPhysicalCarryKind GetPhysicalCarryKind() const override { return EPhysicalCarryKind::Key; }
	virtual FText GetPhysicalCarryDisplayName() const override;
	virtual FTransform GetHeldTransform() const override;
	virtual bool CanBeTakenBy(const UPlayerCarryComponent& Carry, FText& OutFailureReason) const override;
	virtual bool HandleTakenBy(UPlayerCarryComponent& Carry, USceneComponent* HeldAnchor) override;
	virtual bool CanFreeDrop(FText& OutFailureReason) const override;
	virtual UPrimitiveComponent* GetPhysicalCarryPrimitive() const override;
	virtual float GetThrowSpawnDistance() const override { return ThrowSpawnDistance; }
	virtual float GetThrowImpulseStrength() const override { return ThrowImpulseStrength; }
	virtual float GetUpwardThrowImpulseStrength() const override { return UpwardThrowImpulseStrength; }
	virtual AActor* GetAssignedPhysicalCarryFixedSlot() const override { return FixedSlot.Get(); }
	virtual bool TryBindPhysicalCarryFixedSlot(AActor& SlotActor, FText& OutFailureReason) override;
	virtual void ClearPhysicalCarryFixedSlotBinding(AActor& ExpectedSlot) override;
	virtual void NotifyPhysicalCarryFixedSlotBindingConflict() override { bFixedSlotBindingConflict = true; }
	virtual bool IsStoredInAssignedPhysicalCarryFixedSlot() const override;
	virtual bool NotifyTakenFromFixedSlotCommitted(UPlayerCarryComponent& Carry, AActor& SlotActor) override;
	virtual bool NotifyStoredInFixedSlotCommitted(UPlayerCarryComponent& Carry, AActor& SlotActor) override;
	virtual bool NotifyRecoveredToFixedSlotCommitted(AActor& SlotActor) override;
	virtual void NotifyFixedSlotDestroyed(AActor& SlotActor) override;
	virtual bool NotifyPhysicalDropCommitted(UPlayerCarryComponent& Carry) override;
	virtual void PublishPhysicalCarryCommit(EPhysicalCarryCommitTransition Transition) override;
	virtual void RecoverPhysicalCarryable(UPlayerCarryComponent* PreviousCarry) override;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

	UFUNCTION(BlueprintPure, Category = "Bathhouse Key")
	int32 GetKeyNumber() const { return KeyNumber; }

	UFUNCTION(BlueprintPure, Category = "Bathhouse Key")
	EBathhouseKeyState GetKeyState() const { return KeyState; }

	UFUNCTION(BlueprintPure, Category = "Bathhouse Key")
	ABathhouseKeyHookActor* GetKeyHook() const { return KeyHook; }

	bool InitializeAtHook(ABathhouseKeyHookActor* InHook);
	bool TryTakeFromHook(UPlayerCarryComponent& Carry, ABathhouseKeyHookActor& Hook);
	bool TryReturnToHook(UPlayerCarryComponent& Carry, ABathhouseKeyHookActor& Hook);
	bool TryAssignToCustomer(UPlayerCarryComponent& Carry, AActor& Customer);
	bool TryPlaceOnCounter(AActor& Customer, ABathhouseCounterActor& Counter, int32 ReturnSlotIndex, USceneComponent& ReturnSlot);
	bool TryTakeFromCounter(UPlayerCarryComponent& Carry);
	void RecoverToHook(UObject* ExpectedOwner = nullptr);

	UPROPERTY(BlueprintAssignable, Category = "Bathhouse Key")
	FOnBathhouseKeyStateChanged OnKeyStateChanged;

	UPROPERTY(BlueprintAssignable, Category = "Bathhouse Key")
	FOnHeldKeyPresentationChanged OnHeldPresentationChanged;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Bathhouse Key")
	TObjectPtr<UBoxComponent> KeyPhysicsRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Bathhouse Key")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Bathhouse Key")
	TObjectPtr<UStaticMeshComponent> WorldMesh;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Bathhouse Key", meta = (ClampMin = "0"))
	int32 KeyNumber = 0;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Bathhouse Key")
	TObjectPtr<ABathhouseKeyHookActor> KeyHook = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bathhouse Key|Carry|Presentation")
	FTransform HeldTransform = FTransform::Identity;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bathhouse Key|Carry", meta = (ClampMin = "0.0"))
	float ThrowImpulseStrength = 120.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bathhouse Key|Carry", meta = (ClampMin = "0.0", DeprecatedProperty, DeprecationMessage = "Held-position free drop no longer uses a camera-origin spawn distance."))
	float ThrowSpawnDistance = 70.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bathhouse Key|Carry", meta = (ClampMin = "0.0"))
	float UpwardThrowImpulseStrength = 15.0f;

private:
	friend class FBathhouseKeyRecoveryTest;
	friend class FBathhousePhysicalCarryDropTest;
	friend class FBathhousePhysicalCarryFixedSlotTest;

	bool CommitState(EBathhouseKeyState NewState, UObject* NewOwner, bool bDeferPublication = false);
	void BroadcastStateTransition(EBathhouseKeyState PreviousState, EBathhouseKeyState NewState);
	void ApplyHeldTransform();
	void AttachAtHook();
	void SetWorldPresentation(bool bVisible, bool bCollisionEnabled);
	void SetWorldPhysics(bool bEnabled);

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Bathhouse Key", meta = (AllowPrivateAccess = "true"))
	EBathhouseKeyState KeyState = EBathhouseKeyState::AtHook;

	UPROPERTY(Transient)
	TObjectPtr<UObject> StateOwner = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<ABathhouseCounterActor> CounterOwner = nullptr;

	TWeakObjectPtr<AActor> FixedSlot;

	int32 CounterReturnSlotIndex = INDEX_NONE;
	FTransform InitialTransform;
	FTransform LastSafeTransform;
	EBathhouseKeyState DeferredPreviousState = EBathhouseKeyState::AtHook;
	EBathhouseKeyState DeferredNewState = EBathhouseKeyState::AtHook;
	bool bHasDeferredStatePublication = false;
	bool bEndingPlay = false;
	bool bFixedSlotBindingConflict = false;
};
