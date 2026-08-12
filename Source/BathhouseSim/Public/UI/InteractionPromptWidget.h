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

private:
	friend class FBathhouseInteractionPromptPresentationTest;

	static bool IsPromptRootEnabled(const FPlayerInteractionQuery& Query);
	static bool IsLegacyPrimaryEnabled(const FPlayerInteractionQuery& Query);

	UFUNCTION()
	void HandleInteractionQueryChanged(const FPlayerInteractionQuery& Query);

	void HandleInteractionAttemptFinished(const FPlayerInteractionResult& Result);
	void HandlePrimaryTransientFailureExpired();
	void HandleSecondaryTransientFailureExpired();
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

	FDelegateHandle InteractionResultHandle;
	FTimerHandle PrimaryFailureTimerHandle;
	FTimerHandle SecondaryFailureTimerHandle;
	bool bIsQueryBound = false;
	bool bHasPresentedQuery = false;
};
