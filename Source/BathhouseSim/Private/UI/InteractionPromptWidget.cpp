#include "UI/InteractionPromptWidget.h"

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
	ClearTransientFailure(false);
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
	ClearTransientFailure(false);
	PresentQuery(FPlayerInteractionQuery(), true);
	Super::NativeDestruct();
}

void UInteractionPromptWidget::HandleInteractionQueryChanged(const FPlayerInteractionQuery& Query)
{
	ClearTransientFailure(false);
	PresentQuery(Query, true);
}

void UInteractionPromptWidget::HandleInteractionAttemptFinished(const FPlayerInteractionResult& Result)
{
	if (Result.bSucceeded || Result.FailureReason.IsEmpty())
	{
		ClearTransientFailure(true);
		return;
	}

	ClearTransientFailure(false);
	TransientFailureReason = Result.FailureReason;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			FailureTimerHandle,
			this,
			&UInteractionPromptWidget::HandleTransientFailureExpired,
			FMath::Max(0.1f, FailureDisplayDurationSeconds),
			false);
	}
	ApplyCurrentPresentation();
}

void UInteractionPromptWidget::HandleTransientFailureExpired()
{
	ClearTransientFailure(true);
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
		PromptRoot && TargetNameText && ActionNameText && FailureReasonText,
		TEXT("InteractionPromptWidget is missing one or more required BindWidget fields.")))
	{
		return;
	}

	const FText EmptyText = FText::GetEmpty();
	const bool bHasVisibleQuery = CachedQuery.bVisible;
	const bool bHasTransientFailure = !TransientFailureReason.IsEmpty();
	const bool bShowPrompt = bHasVisibleQuery || bHasTransientFailure;
	const bool bPromptEnabled = bHasVisibleQuery && CachedQuery.bCanInteract;
	const FText& TargetName = bHasVisibleQuery ? CachedQuery.TargetName : EmptyText;
	const FText& ActionName = bHasVisibleQuery ? CachedQuery.ActionName : EmptyText;
	const FText& EffectiveFailureReason = bHasTransientFailure
		? TransientFailureReason
		: (bHasVisibleQuery ? CachedQuery.FailureReason : EmptyText);
	const bool bShowFailure = bShowPrompt && !EffectiveFailureReason.IsEmpty();

	PromptRoot->SetVisibility(bShowPrompt ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	PromptRoot->SetIsEnabled(bPromptEnabled);
	TargetNameText->SetText(TargetName);
	ActionNameText->SetText(ActionName);
	FailureReasonText->SetText(bShowFailure ? EffectiveFailureReason : EmptyText);
	FailureReasonText->SetVisibility(bShowFailure ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);

	OnInteractionPromptChanged(
		bShowPrompt,
		bPromptEnabled,
		TargetName,
		ActionName,
		EffectiveFailureReason);
}

bool UInteractionPromptWidget::ClearTransientFailure(const bool bRefreshPresentation)
{
	const bool bHadTransientFailure = !TransientFailureReason.IsEmpty() || FailureTimerHandle.IsValid();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(FailureTimerHandle);
	}
	FailureTimerHandle.Invalidate();
	TransientFailureReason = FText::GetEmpty();
	if (bRefreshPresentation && bHadTransientFailure && bHasPresentedQuery)
	{
		ApplyCurrentPresentation();
	}
	return bHadTransientFailure;
}
