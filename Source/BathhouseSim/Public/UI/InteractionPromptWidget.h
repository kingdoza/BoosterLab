#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Interaction/InteractionTypes.h"
#include "TimerManager.h"
#include "InteractionPromptWidget.generated.h"

class UPlayerInteractionComponent;
class UProgressBar;
class UTextBlock;
class UWidget;

UCLASS(Abstract, Blueprintable)
class BATHHOUSESIM_API UInteractionPromptWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetInteractionComponent(UPlayerInteractionComponent* InInteractionComponent);

	UFUNCTION(BlueprintPure, Category = "Interaction Prompt")
	FPlayerInteractionQuery GetCachedQuery() const { return CachedQuery; }

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidget> PromptRoot = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> TargetNameText = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> ActionNameText = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> FailureReasonText = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> SecondaryActionNameText = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> SecondaryFailureReasonText = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UProgressBar> InteractionProgressBar = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> EquipmentActionNameText = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> EquipmentFailureReasonText = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UProgressBar> EquipmentProgressBar = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> PlacementActionNameText = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> PlacementFailureReasonText = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> RecoveryActionNameText = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> RecoveryFailureReasonText = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UProgressBar> RecoveryProgressBar = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Interaction Prompt", meta = (ClampMin = "0.1", UIMin = "0.1"))
	float FailureDisplayDurationSeconds = 1.5f;

	UFUNCTION(BlueprintImplementableEvent, Category = "Interaction Prompt")
	void OnInteractionPromptChanged(
		bool bVisible,
		bool bCanInteract,
		const FText& TargetName,
		const FText& ActionName,
		const FText& FailureReason);

	UFUNCTION(BlueprintImplementableEvent, Category = "Interaction Prompt")
	void OnInteractionPromptDetailsChanged(
		bool bSecondaryVisible,
		bool bCanSecondaryInteract,
		const FText& SecondaryActionName,
		const FText& SecondaryFailureReason,
		bool bHoldVisible,
		float HoldProgress);

	UFUNCTION(BlueprintImplementableEvent, Category = "Interaction Prompt")
	void OnEquipmentUsePromptChanged(
		bool bVisible,
		bool bCanUse,
		const FText& ActionName,
		const FText& FailureReason,
		bool bHoldVisible,
		float Progress);

	UFUNCTION(BlueprintImplementableEvent, Category = "Interaction Prompt")
	void OnFacilityPlacementPromptChanged(
		bool bPlacementVisible,
		bool bCanPlace,
		const FText& PlacementFailureReason,
		bool bRecoveryVisible,
		bool bCanRecover,
		float RecoveryProgress);

private:
	friend class FBathhouseInteractionPromptPresentationTest;

	static bool IsPromptRootEnabled(const FPlayerInteractionQuery& Query);
	static bool IsLegacyPrimaryEnabled(const FPlayerInteractionQuery& Query);

	UFUNCTION()
	void HandleInteractionQueryChanged(const FPlayerInteractionQuery& Query);

	void HandleInteractionAttemptFinished(const FPlayerInteractionResult& Result);
	void HandlePrimaryTransientFailureExpired();
	void HandleSecondaryTransientFailureExpired();
	void HandleEquipmentTransientFailureExpired();
	void HandlePlacementTransientFailureExpired();
	void HandleRecoveryTransientFailureExpired();
	void BindInteraction();
	void UnbindInteraction();
	void PresentQuery(const FPlayerInteractionQuery& Query, bool bForceRefresh = false);
	void ApplyCurrentPresentation();
	bool ClearTransientFailure(EPlayerInteractionIntent Intent, bool bRefreshPresentation);
	bool ClearAllTransientFailures(bool bRefreshPresentation);

	UPROPERTY(Transient)
	TObjectPtr<UPlayerInteractionComponent> InteractionComponent = nullptr;

	UPROPERTY(Transient)
	FPlayerInteractionQuery CachedQuery;

	UPROPERTY(Transient)
	FText PrimaryTransientFailureReason;

	UPROPERTY(Transient)
	FText SecondaryTransientFailureReason;

	UPROPERTY(Transient)
	FText EquipmentTransientFailureReason;

	UPROPERTY(Transient)
	FText PlacementTransientFailureReason;

	UPROPERTY(Transient)
	FText RecoveryTransientFailureReason;

	FDelegateHandle InteractionResultHandle;
	FTimerHandle PrimaryFailureTimerHandle;
	FTimerHandle SecondaryFailureTimerHandle;
	FTimerHandle EquipmentFailureTimerHandle;
	FTimerHandle PlacementFailureTimerHandle;
	FTimerHandle RecoveryFailureTimerHandle;
	bool bIsQueryBound = false;
	bool bHasPresentedQuery = false;
};
