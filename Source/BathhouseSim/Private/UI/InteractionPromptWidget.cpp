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
	const bool bEquipment = Result.Intent == EPlayerInteractionIntent::EquipmentUse;
	const bool bPlacement = Result.Intent == EPlayerInteractionIntent::PlacementConfirm;
	const bool bRecovery = Result.Intent == EPlayerInteractionIntent::FacilityRecovery;
	FText& FailureReason = bRecovery
		? RecoveryTransientFailureReason
		: bPlacement
		? PlacementTransientFailureReason
		: bEquipment
		? EquipmentTransientFailureReason
		: (bSecondary ? SecondaryTransientFailureReason : PrimaryTransientFailureReason);
	FTimerHandle& TimerHandle = bRecovery
		? RecoveryFailureTimerHandle
		: bPlacement
		? PlacementFailureTimerHandle
		: bEquipment
		? EquipmentFailureTimerHandle
		: (bSecondary ? SecondaryFailureTimerHandle : PrimaryFailureTimerHandle);
	FailureReason = Result.FailureReason;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			TimerHandle,
			this,
			bRecovery
				? &UInteractionPromptWidget::HandleRecoveryTransientFailureExpired
				: bPlacement
				? &UInteractionPromptWidget::HandlePlacementTransientFailureExpired
				: bEquipment
				? &UInteractionPromptWidget::HandleEquipmentTransientFailureExpired
				: bSecondary
				? &UInteractionPromptWidget::HandleSecondaryTransientFailureExpired
				: &UInteractionPromptWidget::HandlePrimaryTransientFailureExpired,
			FMath::Max(0.1f, FailureDisplayDurationSeconds),
			false);
	}
	ApplyCurrentPresentation();
}

void UInteractionPromptWidget::HandlePlacementTransientFailureExpired()
{
	ClearTransientFailure(EPlayerInteractionIntent::PlacementConfirm, true);
}

void UInteractionPromptWidget::HandleRecoveryTransientFailureExpired()
{
	ClearTransientFailure(EPlayerInteractionIntent::FacilityRecovery, true);
}

void UInteractionPromptWidget::HandlePrimaryTransientFailureExpired()
{
	ClearTransientFailure(EPlayerInteractionIntent::Primary, true);
}

void UInteractionPromptWidget::HandleSecondaryTransientFailureExpired()
{
	ClearTransientFailure(EPlayerInteractionIntent::Secondary, true);
}

void UInteractionPromptWidget::HandleEquipmentTransientFailureExpired()
{
	ClearTransientFailure(EPlayerInteractionIntent::EquipmentUse, true);
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
			&& SecondaryActionNameText && SecondaryFailureReasonText && InteractionProgressBar
			&& EquipmentActionNameText && EquipmentFailureReasonText && EquipmentProgressBar
			&& PlacementActionNameText && PlacementFailureReasonText
			&& RecoveryActionNameText && RecoveryFailureReasonText && RecoveryProgressBar,
		TEXT("InteractionPromptWidget is missing one or more required BindWidget fields.")))
	{
		return;
	}

	const FText EmptyText = FText::GetEmpty();
	const bool bHasVisibleQuery = CachedQuery.bVisible;
	const bool bHasPersistentTarget = CachedQuery.bVisible || CachedQuery.bEquipmentUseVisible;
	const bool bHasPrimaryTransientFailure = !PrimaryTransientFailureReason.IsEmpty();
	const bool bHasSecondaryTransientFailure = !SecondaryTransientFailureReason.IsEmpty();
	const bool bHasEquipmentTransientFailure = !EquipmentTransientFailureReason.IsEmpty();
	const bool bHasPlacementTransientFailure = !PlacementTransientFailureReason.IsEmpty();
	const bool bHasRecoveryTransientFailure = !RecoveryTransientFailureReason.IsEmpty();
	const bool bHasTransientFailure = bHasPrimaryTransientFailure || bHasSecondaryTransientFailure
		|| bHasEquipmentTransientFailure || bHasPlacementTransientFailure || bHasRecoveryTransientFailure;
	const bool bShowPrompt = bHasVisibleQuery || CachedQuery.bEquipmentUseVisible
		|| CachedQuery.bPlacementVisible || CachedQuery.bRecoveryVisible || bHasTransientFailure;
	const bool bSecondaryVisible = bHasVisibleQuery && CachedQuery.bSecondaryVisible;
	const bool bPromptEnabled = IsPromptRootEnabled(CachedQuery);
	const bool bPrimaryEnabled = IsLegacyPrimaryEnabled(CachedQuery);
	const FText& TargetName = bHasPersistentTarget ? CachedQuery.TargetName : EmptyText;
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
	const bool bEquipmentVisible = CachedQuery.bEquipmentUseVisible || bHasEquipmentTransientFailure;
	const FText& EquipmentActionName = CachedQuery.bEquipmentUseVisible ? CachedQuery.EquipmentActionName : EmptyText;
	const FText& EffectiveEquipmentFailureReason = bHasEquipmentTransientFailure
		? EquipmentTransientFailureReason
		: (CachedQuery.bEquipmentUseVisible ? CachedQuery.EquipmentFailureReason : EmptyText);
	const bool bShowEquipmentFailure = bEquipmentVisible && !EffectiveEquipmentFailureReason.IsEmpty();
	const bool bShowEquipmentHold = CachedQuery.bEquipmentUseVisible
		&& CachedQuery.EquipmentActivationMode == EPlayerInteractionActivationMode::Hold;
	const bool bPlacementVisible = CachedQuery.bPlacementVisible || bHasPlacementTransientFailure;
	const FText& EffectivePlacementFailure = bHasPlacementTransientFailure
		? PlacementTransientFailureReason
		: CachedQuery.PlacementFailureReason;
	const bool bRecoveryVisible = CachedQuery.bRecoveryVisible || bHasRecoveryTransientFailure;
	const FText& EffectiveRecoveryFailure = bHasRecoveryTransientFailure
		? RecoveryTransientFailureReason
		: CachedQuery.RecoveryFailureReason;

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
	EquipmentActionNameText->SetText(EquipmentActionName);
	EquipmentActionNameText->SetIsEnabled(CachedQuery.bEquipmentUseVisible && CachedQuery.bCanEquipmentUse);
	EquipmentActionNameText->SetVisibility(bEquipmentVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	EquipmentFailureReasonText->SetText(bShowEquipmentFailure ? EffectiveEquipmentFailureReason : EmptyText);
	EquipmentFailureReasonText->SetVisibility(bShowEquipmentFailure ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	EquipmentProgressBar->SetPercent(FMath::Clamp(CachedQuery.EquipmentUseProgress, 0.0f, 1.0f));
	EquipmentProgressBar->SetVisibility(bShowEquipmentHold ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	PlacementActionNameText->SetText(CachedQuery.bPlacementVisible ? CachedQuery.PlacementActionName : EmptyText);
	PlacementActionNameText->SetIsEnabled(CachedQuery.bPlacementVisible && CachedQuery.bCanPlace);
	PlacementActionNameText->SetVisibility(bPlacementVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	PlacementFailureReasonText->SetText(EffectivePlacementFailure);
	PlacementFailureReasonText->SetVisibility(bPlacementVisible && !EffectivePlacementFailure.IsEmpty()
		? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	RecoveryActionNameText->SetText(CachedQuery.bRecoveryVisible ? CachedQuery.RecoveryActionName : EmptyText);
	RecoveryActionNameText->SetIsEnabled(CachedQuery.bRecoveryVisible && CachedQuery.bCanRecover);
	RecoveryActionNameText->SetVisibility(bRecoveryVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	RecoveryFailureReasonText->SetText(EffectiveRecoveryFailure);
	RecoveryFailureReasonText->SetVisibility(bRecoveryVisible && !EffectiveRecoveryFailure.IsEmpty()
		? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	RecoveryProgressBar->SetPercent(FMath::Clamp(CachedQuery.RecoveryProgress, 0.0f, 1.0f));
	RecoveryProgressBar->SetVisibility(CachedQuery.bRecoveryVisible
		? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);

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
	OnEquipmentUsePromptChanged(
		bEquipmentVisible,
		CachedQuery.bEquipmentUseVisible && CachedQuery.bCanEquipmentUse,
		EquipmentActionName,
		EffectiveEquipmentFailureReason,
		bShowEquipmentHold,
		FMath::Clamp(CachedQuery.EquipmentUseProgress, 0.0f, 1.0f));
	OnFacilityPlacementPromptChanged(
		bPlacementVisible,
		CachedQuery.bPlacementVisible && CachedQuery.bCanPlace,
		EffectivePlacementFailure,
		bRecoveryVisible,
		CachedQuery.bRecoveryVisible && CachedQuery.bCanRecover,
		FMath::Clamp(CachedQuery.RecoveryProgress, 0.0f, 1.0f));
}

bool UInteractionPromptWidget::IsPromptRootEnabled(const FPlayerInteractionQuery& Query)
{
	return (Query.bVisible && Query.bCanInteract)
		|| (Query.bSecondaryVisible && Query.bCanSecondaryInteract)
		|| (Query.bEquipmentUseVisible && Query.bCanEquipmentUse)
		|| (Query.bPlacementVisible && Query.bCanPlace)
		|| (Query.bRecoveryVisible && Query.bCanRecover);
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
	const bool bEquipment = Intent == EPlayerInteractionIntent::EquipmentUse;
	const bool bPlacement = Intent == EPlayerInteractionIntent::PlacementConfirm;
	const bool bRecovery = Intent == EPlayerInteractionIntent::FacilityRecovery;
	FText& FailureReason = bRecovery
		? RecoveryTransientFailureReason
		: bPlacement
		? PlacementTransientFailureReason
		: bEquipment
		? EquipmentTransientFailureReason
		: (bSecondary ? SecondaryTransientFailureReason : PrimaryTransientFailureReason);
	FTimerHandle& TimerHandle = bRecovery
		? RecoveryFailureTimerHandle
		: bPlacement
		? PlacementFailureTimerHandle
		: bEquipment
		? EquipmentFailureTimerHandle
		: (bSecondary ? SecondaryFailureTimerHandle : PrimaryFailureTimerHandle);
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
	const bool bClearedEquipment = ClearTransientFailure(EPlayerInteractionIntent::EquipmentUse, false);
	const bool bClearedPlacement = ClearTransientFailure(EPlayerInteractionIntent::PlacementConfirm, false);
	const bool bClearedRecovery = ClearTransientFailure(EPlayerInteractionIntent::FacilityRecovery, false);
	if (bRefreshPresentation && (bClearedPrimary || bClearedSecondary || bClearedEquipment
		|| bClearedPlacement || bClearedRecovery) && bHasPresentedQuery)
	{
		ApplyCurrentPresentation();
	}
	return bClearedPrimary || bClearedSecondary || bClearedEquipment || bClearedPlacement || bClearedRecovery;
}
