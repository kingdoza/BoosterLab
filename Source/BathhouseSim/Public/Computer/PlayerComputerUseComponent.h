#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "PlayerComputerUseComponent.generated.h"

class ABathhouseComputerActor;
class APlayerController;
class UCameraComponent;
class UFirstPersonMovementComponent;
class UPlayerCarryComponent;
class UPlayerInteractionComponent;
class UWidgetInteractionComponent;

enum class EPlayerComputerUsePhase : uint8
{
	Inactive,
	FocusingIn,
	Active,
	FocusingOut
};

UCLASS(ClassGroup = (Computer), meta = (BlueprintSpawnableComponent))
class BATHHOUSESIM_API UPlayerComputerUseComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPlayerComputerUseComponent();

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	void Configure(
		UCameraComponent* InCamera,
		UFirstPersonMovementComponent* InMovement,
		UPlayerInteractionComponent* InInteraction,
		UPlayerCarryComponent* InCarry,
		UWidgetInteractionComponent* InWidgetInteraction);

	bool BeginComputerUse(ABathhouseComputerActor* Computer);
	void RequestEndComputerUse();
	void HandleComputerUnavailable(ABathhouseComputerActor* Computer);

	bool PressPointer();
	void ReleasePointer();

	bool IsCapturingInput() const { return Phase != EPlayerComputerUsePhase::Inactive; }
	bool IsActive() const { return Phase == EPlayerComputerUsePhase::Active; }
	EPlayerComputerUsePhase GetPhase() const { return Phase; }
	ABathhouseComputerActor* GetActiveComputer() const { return ActiveComputer.Get(); }

private:
	friend class FBathhouseComputerSessionTest;

	APlayerController* ResolvePlayerController() const;
	AActor* ResolveReturnViewTarget(APlayerController* PlayerController) const;
	void CompleteFocusIn();
	void CompleteFocusOut();
	void ForceCleanup(bool bReleaseReservation, bool bRestoreView);
	void ReleasePointerIfNeeded();
	void RestoreMovement();
	void RestorePlayerInput(APlayerController* PlayerController);
	void ApplyFocusedAntiAliasingOverride();
	void RestoreFocusedAntiAliasingOverride();
	void ClearBlendTimer();
	bool HasValidConfiguredContext() const;

	UPROPERTY(Transient)
	TObjectPtr<UCameraComponent> Camera = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UFirstPersonMovementComponent> Movement = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UPlayerInteractionComponent> Interaction = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UPlayerCarryComponent> Carry = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UWidgetInteractionComponent> WidgetInteraction = nullptr;

	UPROPERTY(Transient)
	TWeakObjectPtr<ABathhouseComputerActor> ActiveComputer;

	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> PreviousViewTarget;

	UPROPERTY(Transient)
	TWeakObjectPtr<APlayerController> SessionPlayerController;

	FTimerHandle BlendTimer;
	TEnumAsByte<EMovementMode> SavedMovementMode = MOVE_Walking;
	uint8 SavedCustomMovementMode = 0;
	EPlayerComputerUsePhase Phase = EPlayerComputerUsePhase::Inactive;
	bool bSavedCursorVisible = false;
	bool bSnapshotValid = false;
	bool bPointerDown = false;
};
