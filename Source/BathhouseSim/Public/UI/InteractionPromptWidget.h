#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Interaction/InteractionTypes.h"
#include "TimerManager.h"
#include "InteractionPromptWidget.generated.h"

class UPlayerInteractionComponent;
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

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Interaction Prompt", meta = (ClampMin = "0.1", UIMin = "0.1"))
	float FailureDisplayDurationSeconds = 1.5f;

	UFUNCTION(BlueprintImplementableEvent, Category = "Interaction Prompt")
	void OnInteractionPromptChanged(
		bool bVisible,
		bool bCanInteract,
		const FText& TargetName,
		const FText& ActionName,
		const FText& FailureReason);

private:
	UFUNCTION()
	void HandleInteractionQueryChanged(const FPlayerInteractionQuery& Query);

	void HandleInteractionAttemptFinished(const FPlayerInteractionResult& Result);
	void HandleTransientFailureExpired();
	void BindInteraction();
	void UnbindInteraction();
	void PresentQuery(const FPlayerInteractionQuery& Query, bool bForceRefresh = false);
	void ApplyCurrentPresentation();
	bool ClearTransientFailure(bool bRefreshPresentation);

	UPROPERTY(Transient)
	TObjectPtr<UPlayerInteractionComponent> InteractionComponent = nullptr;

	UPROPERTY(Transient)
	FPlayerInteractionQuery CachedQuery;

	UPROPERTY(Transient)
	FText TransientFailureReason;

	FDelegateHandle InteractionResultHandle;
	FTimerHandle FailureTimerHandle;
	bool bIsQueryBound = false;
	bool bHasPresentedQuery = false;
};
