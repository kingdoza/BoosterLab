#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/PhysicalCarryable.h"
#include "Interaction/PlayerInteractable.h"
#include "BathhouseKeyActor.generated.h"

class ABathhouseCounterActor;
class ABathhouseKeyHookActor;
class UPlayerCarryComponent;
class USceneComponent;
class UStaticMeshComponent;

UENUM(BlueprintType)
enum class EBathhouseKeyState : uint8
{
	AtHook,
	HeldByPlayer,
	AssignedToCustomer,
	OnCounter,
	Recovering
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

	virtual FPlayerInteractionQuery QueryInteraction(const FPlayerInteractionContext& Context) const override;
	virtual FPlayerInteractionResult ExecuteInteraction(const FPlayerInteractionContext& Context) override;
	virtual EPhysicalCarryKind GetPhysicalCarryKind() const override { return EPhysicalCarryKind::Key; }
	virtual FText GetPhysicalCarryDisplayName() const override;
	virtual FTransform GetHeldTransform() const override;
	virtual bool CanBeTakenBy(const UPlayerCarryComponent& Carry, FText& OutFailureReason) const override;
	virtual bool HandleTakenBy(UPlayerCarryComponent& Carry, USceneComponent* HeldAnchor) override;
	virtual bool CanFreeDrop(FText& OutFailureReason) const override;
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
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Bathhouse Key")
	TObjectPtr<UStaticMeshComponent> WorldMesh;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Bathhouse Key", meta = (ClampMin = "0"))
	int32 KeyNumber = 0;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Bathhouse Key")
	TObjectPtr<ABathhouseKeyHookActor> KeyHook = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bathhouse Key|Carry|Presentation")
	FTransform HeldTransform = FTransform::Identity;

private:
	friend class FBathhouseKeyRecoveryTest;
	friend class FBathhousePhysicalCarryDropTest;

	bool CommitState(EBathhouseKeyState NewState, UObject* NewOwner);
	void ApplyHeldTransform();
	void AttachAtHook();
	void SetWorldPresentation(bool bVisible, bool bCollisionEnabled);

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Bathhouse Key", meta = (AllowPrivateAccess = "true"))
	EBathhouseKeyState KeyState = EBathhouseKeyState::AtHook;

	UPROPERTY(Transient)
	TObjectPtr<UObject> StateOwner = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<ABathhouseCounterActor> CounterOwner = nullptr;

	int32 CounterReturnSlotIndex = INDEX_NONE;
};
