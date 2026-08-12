#include "UI/InteractionPromptWidget.h"

#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "Engine/World.h"
#include "Interaction/PlayerInteractionComponent.h"

void UInteractionPromptWidget::SetInteractionComponent(UPlayerInteractionComponent* InInteractionComponent)
{
	if (InteractionComponent == InInteractionComponent)
	{
		return;
	}
	UnbindInteraction();
	ClearAllTransientFailures(false);
	InteractionComponent = InInteractionComponent;
	BindInteraction();
	if (IsConstructed() && !InteractionComponent)
	{
		PresentQuery(FPlayerInteractionQuery(), true);
	}
}

void UInteractionPromptWidget::NativeConstruct()
{
	Super::NativeConstruct();
	bHasPresentedQuery = false;
	BindInteraction();
	if (!InteractionComponent)
	{
		PresentQuery(FPlayerInteractionQuery());
	}
}

void UInteractionPromptWidget::NativeDestruct()
{
	UnbindInteraction();
	InteractionComponent = nullptr;
	ClearAllTransientFailures(false);
	PresentQuery(FPlayerInteractionQuery(), true);
	Super::NativeDestruct();
}

void UInteractionPromptWidget::HandleInteractionQueryChanged(const FPlayerInteractionQuery& Query)
{
	ClearAllTransientFailures(false);
	PresentQuery(Query, true);
}

void UInteractionPromptWidget::HandleInteractionAttemptFinished(const FPlayerInteractionResult& Result)
{
	if (Result.bSucceeded || Result.FailureReason.IsEmpty())
	{
		ClearTransientFailure(Result.Intent, true);
		return;
	}

	ClearTransientFailure(Result.Intent, false);
	const bool bSecondary = Result.Intent == EPlayerInteractionIntent::Secondary;
	FText& FailureReason = bSecondary ? SecondaryTransientFailureReason : PrimaryTransientFailureReason;
	FTimerHandle& TimerHandle = bSecondary ? SecondaryFailureTimerHandle : PrimaryFailureTimerHandle;
	FailureReason = Result.FailureReason;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			TimerHandle,
			this,
			bSecondary
				? &UInteractionPromptWidget::HandleSecondaryTransientFailureExpired
				: &UInteractionPromptWidget::HandlePrimaryTransientFailureExpired,
			FMath::Max(0.1f, FailureDisplayDurationSeconds),
			false);
	}
	ApplyCurrentPresentation();
}

void UInteractionPromptWidget::HandlePrimaryTransientFailureExpired()
{
	ClearTransientFailure(EPlayerInteractionIntent::Primary, true);
}

void UInteractionPromptWidget::HandleSecondaryTransientFailureExpired()
{
	ClearTransientFailure(EPlayerInteractionIntent::Secondary, true);
}

void UInteractionPromptWidget::BindInteraction()
{
	if (bIsQueryBound || InteractionResultHandle.IsValid() || !IsConstructed() || !InteractionComponent)
	{
		return;
	}
	InteractionComponent->OnInteractionQueryChanged.AddDynamic(this, &UInteractionPromptWidget::HandleInteractionQueryChanged);
	bIsQueryBound = true;
	InteractionResultHandle = InteractionComponent->OnInteractionAttemptFinishedNative.AddUObject(
		this,
		&UInteractionPromptWidget::HandleInteractionAttemptFinished);
	PresentQuery(InteractionComponent->GetCurrentInteractionQuery(), true);
}

void UInteractionPromptWidget::UnbindInteraction()
{
	if (InteractionComponent)
	{
		if (bIsQueryBound)
		{
			InteractionComponent->OnInteractionQueryChanged.RemoveDynamic(this, &UInteractionPromptWidget::HandleInteractionQueryChanged);
		}
		if (InteractionResultHandle.IsValid())
		{
			InteractionComponent->OnInteractionAttemptFinishedNative.Remove(InteractionResultHandle);
		}
	}
	bIsQueryBound = false;
	InteractionResultHandle.Reset();
}

void UInteractionPromptWidget::PresentQuery(const FPlayerInteractionQuery& Query, const bool bForceRefresh)
{
	if (!bForceRefresh && bHasPresentedQuery && CachedQuery.Equals(Query))
	{
		return;
	}
	CachedQuery = Query;
	bHasPresentedQuery = true;
	ApplyCurrentPresentation();
}

void UInteractionPromptWidget::ApplyCurrentPresentation()
{
	if (!ensureMsgf(
		PromptRoot && TargetNameText && ActionNameText && FailureReasonText
			&& SecondaryActionNameText && SecondaryFailureReasonText && InteractionProgressBar,
		TEXT("InteractionPromptWidget is missing one or more required BindWidget fields.")))
	{
		return;
	}

	const FText EmptyText = FText::GetEmpty();
	const bool bHasVisibleQuery = CachedQuery.bVisible;
	const bool bHasPrimaryTransientFailure = !PrimaryTransientFailureReason.IsEmpty();
	const bool bHasSecondaryTransientFailure = !SecondaryTransientFailureReason.IsEmpty();
	const bool bHasTransientFailure = bHasPrimaryTransientFailure || bHasSecondaryTransientFailure;
	const bool bShowPrompt = bHasVisibleQuery || bHasTransientFailure;
	const bool bSecondaryVisible = bHasVisibleQuery && CachedQuery.bSecondaryVisible;
	const bool bPromptEnabled = IsPromptRootEnabled(CachedQuery);
	const bool bPrimaryEnabled = IsLegacyPrimaryEnabled(CachedQuery);
	const FText& TargetName = bHasVisibleQuery ? CachedQuery.TargetName : EmptyText;
	const FText& ActionName = bHasVisibleQuery ? CachedQuery.ActionName : EmptyText;
	const FText& EffectiveFailureReason = bHasPrimaryTransientFailure
		? PrimaryTransientFailureReason
		: (bHasVisibleQuery ? CachedQuery.FailureReason : EmptyText);
	const FText& SecondaryActionName = bSecondaryVisible ? CachedQuery.SecondaryActionName : EmptyText;
	const FText& EffectiveSecondaryFailureReason = bHasSecondaryTransientFailure
		? SecondaryTransientFailureReason
		: (bSecondaryVisible ? CachedQuery.SecondaryFailureReason : EmptyText);
	const bool bShowFailure = bShowPrompt && !EffectiveFailureReason.IsEmpty();
	const bool bShowSecondaryFailure = (bSecondaryVisible || bHasSecondaryTransientFailure)
		&& !EffectiveSecondaryFailureReason.IsEmpty();
	const bool bShowHold = bHasVisibleQuery
		&& CachedQuery.PrimaryActivationMode == EPlayerInteractionActivationMode::Hold;

	PromptRoot->SetVisibility(bShowPrompt ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	PromptRoot->SetIsEnabled(bPromptEnabled);
	TargetNameText->SetText(TargetName);
	ActionNameText->SetText(ActionName);
	ActionNameText->SetIsEnabled(bHasVisibleQuery && CachedQuery.bCanInteract);
	FailureReasonText->SetText(bShowFailure ? EffectiveFailureReason : EmptyText);
	FailureReasonText->SetVisibility(bShowFailure ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	SecondaryActionNameText->SetText(SecondaryActionName);
	SecondaryActionNameText->SetIsEnabled(bSecondaryVisible && CachedQuery.bCanSecondaryInteract);
	SecondaryActionNameText->SetVisibility(bSecondaryVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	SecondaryFailureReasonText->SetText(
		bShowSecondaryFailure ? EffectiveSecondaryFailureReason : EmptyText);
	SecondaryFailureReasonText->SetVisibility(
		bShowSecondaryFailure ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	InteractionProgressBar->SetPercent(FMath::Clamp(CachedQuery.HoldProgress, 0.0f, 1.0f));
	InteractionProgressBar->SetVisibility(bShowHold ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);

	OnInteractionPromptChanged(
		bShowPrompt,
		bPrimaryEnabled,
		TargetName,
		ActionName,
		EffectiveFailureReason);
	OnInteractionPromptDetailsChanged(
		bSecondaryVisible,
		bSecondaryVisible && CachedQuery.bCanSecondaryInteract,
		SecondaryActionName,
		EffectiveSecondaryFailureReason,
		bShowHold,
		FMath::Clamp(CachedQuery.HoldProgress, 0.0f, 1.0f));
}

bool UInteractionPromptWidget::IsPromptRootEnabled(const FPlayerInteractionQuery& Query)
{
	return Query.bVisible
		&& (Query.bCanInteract || (Query.bSecondaryVisible && Query.bCanSecondaryInteract));
}

bool UInteractionPromptWidget::IsLegacyPrimaryEnabled(const FPlayerInteractionQuery& Query)
{
	return Query.bVisible && Query.bCanInteract;
}

bool UInteractionPromptWidget::ClearTransientFailure(
	const EPlayerInteractionIntent Intent,
	const bool bRefreshPresentation)
{
	const bool bSecondary = Intent == EPlayerInteractionIntent::Secondary;
	FText& FailureReason = bSecondary ? SecondaryTransientFailureReason : PrimaryTransientFailureReason;
	FTimerHandle& TimerHandle = bSecondary ? SecondaryFailureTimerHandle : PrimaryFailureTimerHandle;
	const bool bHadTransientFailure = !FailureReason.IsEmpty() || TimerHandle.IsValid();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TimerHandle);
	}
	TimerHandle.Invalidate();
	FailureReason = FText::GetEmpty();
	if (bRefreshPresentation && bHadTransientFailure && bHasPresentedQuery)
	{
		ApplyCurrentPresentation();
	}
	return bHadTransientFailure;
}

bool UInteractionPromptWidget::ClearAllTransientFailures(const bool bRefreshPresentation)
{
	const bool bClearedPrimary = ClearTransientFailure(EPlayerInteractionIntent::Primary, false);
	const bool bClearedSecondary = ClearTransientFailure(EPlayerInteractionIntent::Secondary, false);
	if (bRefreshPresentation && (bClearedPrimary || bClearedSecondary) && bHasPresentedQuery)
	{
		ApplyCurrentPresentation();
	}
	return bClearedPrimary || bClearedSecondary;
}
