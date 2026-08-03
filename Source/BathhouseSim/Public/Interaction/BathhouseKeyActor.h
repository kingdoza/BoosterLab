#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
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
class BATHHOUSESIM_API ABathhouseKeyActor : public AActor, public IPlayerInteractable
{
	GENERATED_BODY()

public:
	ABathhouseKeyActor();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	virtual FPlayerInteractionQuery QueryInteraction(const FPlayerInteractionContext& Context) const override;
	virtual FPlayerInteractionResult ExecuteInteraction(const FPlayerInteractionContext& Context) override;

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

private:
	friend class FBathhouseKeyRecoveryTest;

	bool CommitState(EBathhouseKeyState NewState, UObject* NewOwner);
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
