#include "Placement/PlayerFacilityPlacementComponent.h"

#include "Camera/CameraComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "Interaction/PlayerCarryComponent.h"
#include "Interaction/PlayerInteractionComponent.h"
#include "Placement/FacilityPlacementComponent.h"
#include "Placement/FacilityPlacementDefinition.h"
#include "Placement/FacilityPlacementPreviewActor.h"
#include "Placement/FacilityPlacementSettings.h"
#include "Placement/FacilityPlacementZoneActor.h"
#include "Placement/FacilityActorConversionTransaction.h"
#include "Placement/PlaceableFacility.h"
#include "Placement/PlaceableFacilityItemActor.h"

#define LOCTEXT_NAMESPACE "PlayerFacilityPlacementComponent"

UPlayerFacilityPlacementComponent::UPlayerFacilityPlacementComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
}

void UPlayerFacilityPlacementComponent::Configure(
	UCameraComponent* InCamera,
	UPlayerCarryComponent* InCarry,
	UPlayerInteractionComponent* InInteraction)
{
	if (Carry)
	{
		Carry->OnHeldObjectChanged.RemoveDynamic(this, &UPlayerFacilityPlacementComponent::HandleHeldObjectChanged);
	}
	CancelAllSessions();
	Camera = InCamera;
	Carry = InCarry;
	Interaction = InInteraction;
	if (Carry && HasBegunPlay() && CanProcessSession())
	{
		Carry->OnHeldObjectChanged.AddUniqueDynamic(this, &UPlayerFacilityPlacementComponent::HandleHeldObjectChanged);
		HandleHeldObjectChanged(Carry->GetHeldObject());
	}
}

void UPlayerFacilityPlacementComponent::BeginPlay()
{
	Super::BeginPlay();
	if (Carry)
	{
		Carry->OnHeldObjectChanged.AddUniqueDynamic(this, &UPlayerFacilityPlacementComponent::HandleHeldObjectChanged);
		if (CanProcessSession())
		{
			HandleHeldObjectChanged(Carry->GetHeldObject());
		}
	}
	UpdateTickState();
}

void UPlayerFacilityPlacementComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (Carry)
	{
		Carry->OnHeldObjectChanged.RemoveDynamic(this, &UPlayerFacilityPlacementComponent::HandleHeldObjectChanged);
	}
	CancelAllSessions();
	Camera = nullptr;
	Carry = nullptr;
	Interaction = nullptr;
	Super::EndPlay(EndPlayReason);
}

void UPlayerFacilityPlacementComponent::TickComponent(
	const float DeltaTime,
	const ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!CanProcessSession())
	{
		CancelAllSessions();
		return;
	}
	if (PreviewFacility.IsValid())
	{
		RefreshPreview();
	}
	if (RecoveryTarget.IsValid())
	{
		AActor* CurrentTarget = TraceRecoveryTarget();
		IPlaceableFacility* Placeable = Cast<IPlaceableFacility>(RecoveryTarget.Get());
		if (CurrentTarget != RecoveryTarget.Get() || !Placeable)
		{
			CancelRecovery();
		}
		else
		{
			CurrentRecoveryQuery = Placeable->QueryFacilityRecovery();
			if (!CurrentRecoveryQuery.bSucceeded)
			{
				CancelRecovery();
			}
			else
			{
				UpdateRecoveryHold(DeltaTime);
				if (!RecoveryTarget.IsValid())
				{
					return;
				}
			}
		}
	}
	UpdateTickState();
}

void UPlayerFacilityPlacementComponent::HandleHeldObjectChanged(AActor* NewHeldObject)
{
	CancelPreview();
	if (!CanProcessSession())
	{
		return;
	}
	APlaceableFacilityItemActor* Item = Cast<APlaceableFacilityItemActor>(NewHeldObject);
	if (!Item || !Item->IsHeldForPlacement())
	{
		return;
	}
	PreviewFacility = Item;
	AccumulatedYaw = 0.0f;
	UFacilityPlacementDefinition* Definition = Item->GetDefinition();
	if (!Definition || !Definition->PlacedFacilityClass || !GetWorld())
	{
		SetPreviewFailure(
			EFacilityPlacementFailureCode::InvalidDefinition,
			LOCTEXT("MissingPreview", "설비 배치 미리보기를 만들 수 없습니다."));
		return;
	}
	PreviewActor = GetWorld()->SpawnActor<AFacilityPlacementPreviewActor>(
		AFacilityPlacementPreviewActor::StaticClass(),
		Item->GetActorTransform());
	if (AFacilityPlacementPreviewActor* SpawnedPreview = PreviewActor.Get())
	{
		FText PreviewFailure;
		if (!SpawnedPreview->InitializeFromPlacedClass(Definition->PlacedFacilityClass, PreviewFailure))
		{
			SpawnedPreview->Destroy();
			PreviewActor.Reset();
			SetPreviewFailure(
				EFacilityPlacementFailureCode::InvalidDefinition,
				PreviewFailure.IsEmpty() ? LOCTEXT("PreviewInitializationFailed", "설비 배치 미리보기를 초기화하지 못했습니다.") : PreviewFailure);
			return;
		}
		SpawnedPreview->SetActorEnableCollision(false);
		SpawnedPreview->OnDestroyed.AddUniqueDynamic(this, &UPlayerFacilityPlacementComponent::HandlePreviewDestroyed);
		RefreshPreview();
		UpdateTickState();
		return;
	}
	SetPreviewFailure(
		EFacilityPlacementFailureCode::InvalidActor,
		LOCTEXT("PreviewSpawnFailed", "설비 배치 미리보기를 생성하지 못했습니다."));
}

void UPlayerFacilityPlacementComponent::RefreshPreview()
{
	if (!CanProcessSession() || !PreviewFacility.IsValid() || !Carry || Carry->GetHeldObject() != PreviewFacility.Get())
	{
		CancelPreview();
		return;
	}
	if (!PreviewActor.IsValid())
	{
		SetPreviewFailure(
			EFacilityPlacementFailureCode::StateChanged,
			LOCTEXT("PreviewLost", "설비 배치 미리보기가 사라졌습니다."));
		return;
	}
	AFacilityPlacementZoneActor* Zone = nullptr;
	CurrentPlacementQuery = ValidateCurrentPlacement(CurrentCandidate, Zone);
	if (PreviewZone.IsValid() && PreviewZone.Get() != Zone)
	{
		PreviewZone->OnGridVisibilityChanged(false);
	}
	PreviewZone = Zone;
	if (AFacilityPlacementPreviewActor* LivePreview = PreviewActor.Get())
	{
		LivePreview->SetActorTransform(CurrentCandidate);
		LivePreview->SetPlacementValidity(CurrentPlacementQuery.bSucceeded, CurrentPlacementQuery.FailureReason);
	}
	if (Zone)
	{
		Zone->OnGridVisibilityChanged(true);
	}
}

void UPlayerFacilityPlacementComponent::CancelPreview()
{
	ClearPreviewVisual();
	PreviewFacility.Reset();
	CurrentPlacementQuery = FFacilityPlacementTransactionResult();
	bSnapHeld = false;
	UpdateTickState();
}

void UPlayerFacilityPlacementComponent::ClearPreviewVisual()
{
	if (AFacilityPlacementPreviewActor* LivePreview = PreviewActor.Get())
	{
		LivePreview->OnDestroyed.RemoveDynamic(this, &UPlayerFacilityPlacementComponent::HandlePreviewDestroyed);
		LivePreview->Destroy();
	}
	PreviewActor.Reset();
	if (PreviewZone.IsValid())
	{
		PreviewZone->OnGridVisibilityChanged(false);
	}
	PreviewZone.Reset();
}

void UPlayerFacilityPlacementComponent::SetPreviewFailure(
	const EFacilityPlacementFailureCode FailureCode,
	const FText& FailureReason)
{
	ClearPreviewVisual();
	CurrentPlacementQuery = FFacilityPlacementTransactionResult::Failed(FailureCode, FailureReason);
	UpdateTickState();
	if (Interaction && CanProcessSession())
	{
		Interaction->RefreshInteractionQuery();
	}
}

void UPlayerFacilityPlacementComponent::HandlePreviewDestroyed(AActor* DestroyedActor)
{
	if (DestroyedActor)
	{
		PreviewActor.Reset();
		SetPreviewFailure(
			EFacilityPlacementFailureCode::StateChanged,
			LOCTEXT("PreviewDestroyed", "설비 배치 미리보기가 제거되었습니다."));
	}
}

void UPlayerFacilityPlacementComponent::SetSnapHeld(const bool bHeld)
{
	bSnapHeld = bHeld;
	RefreshPreview();
}

void UPlayerFacilityPlacementComponent::AddRotationInput(const float ActionValue)
{
	if (!IsPlacementActive() || FMath::IsNearlyZero(ActionValue)) return;
	AccumulatedYaw = AFacilityPlacementZoneActor::NormalizePlacementYaw(
		AccumulatedYaw + ActionValue * GetDefault<UFacilityPlacementSettings>()->GetRotationStepDegrees());
	RefreshPreview();
}

FPlayerInteractionResult UPlayerFacilityPlacementComponent::ConfirmPlacement()
{
	if (!CanProcessSession())
	{
		CancelPreview();
		const FPlayerInteractionResult Result = FPlayerInteractionResult::Failed(
			LOCTEXT("PlacementSuppressed", "현재는 설비를 설치할 수 없습니다."),
			EPlayerInteractionIntent::PlacementConfirm);
		ReportResult(Result);
		return Result;
	}
	FTransform Candidate;
	AFacilityPlacementZoneActor* Zone = nullptr;
	const FFacilityPlacementTransactionResult Query = ValidateCurrentPlacement(Candidate, Zone);
	APlaceableFacilityItemActor* Item = PreviewFacility.Get();
	if (!Query.bSucceeded || !IsValid(PreviewActor.Get()) || !Item
		|| !Carry || Carry->GetHeldObject() != Item || !Zone)
	{
		const FPlayerInteractionResult Result = FPlayerInteractionResult::Failed(
			Query.FailureReason.IsEmpty() ? LOCTEXT("PlacementUnavailable", "설비를 설치할 수 없습니다.") : Query.FailureReason,
			EPlayerInteractionIntent::PlacementConfirm);
		ReportResult(Result);
		return Result;
	}

	FText FailureReason;
	const bool bSucceeded = FFacilityActorConversionTransaction::PlaceItemAsFacility(
		*Item,
		Candidate,
		*Zone,
		*Carry,
		FailureReason) != nullptr;
	if (!bSucceeded)
	{
		const FPlayerInteractionResult Result = FPlayerInteractionResult::Failed(
			FailureReason.IsEmpty() ? LOCTEXT("PlacementCommitFailed", "설비 설치 상태를 적용하지 못했습니다.") : FailureReason,
			EPlayerInteractionIntent::PlacementConfirm);
		ReportResult(Result);
		return Result;
	}
	const FPlayerInteractionResult Result = FPlayerInteractionResult::Succeeded(EPlayerInteractionIntent::PlacementConfirm);
	ReportResult(Result);
	return Result;
}

bool UPlayerFacilityPlacementComponent::BeginRecoveryHold()
{
	CancelRecovery();
	if (!CanProcessSession()) return false;
	AActor* Candidate = TraceRecoveryTarget();
	IPlaceableFacility* Placeable = Cast<IPlaceableFacility>(Candidate);
	if (!Candidate || !Placeable) return false;
	CurrentRecoveryQuery = Placeable->QueryFacilityRecovery();
	if (!CurrentRecoveryQuery.bSucceeded)
	{
		ReportResult(FPlayerInteractionResult::Failed(CurrentRecoveryQuery.FailureReason, EPlayerInteractionIntent::FacilityRecovery));
		return false;
	}
	RecoveryTarget = Candidate;
	Candidate->OnDestroyed.AddUniqueDynamic(this, &UPlayerFacilityPlacementComponent::HandleRecoveryTargetDestroyed);
	RecoveryElapsed = 0.0f;
	bRecoveryCommittedThisPress = false;
	if (Interaction)
	{
		// The focused facility contributes a visible recovery row with zero progress.
		// Reassert this live component as the final query provider when the hold begins
		// so the row receives RecoveryElapsed even after Blueprint instance reinstancing.
		Interaction->ConfigureSupplementalIntentSource(this);
	}
	UpdateTickState();
	return true;
}

void UPlayerFacilityPlacementComponent::UpdateRecoveryHold(const float DeltaTime)
{
	if (!CanProcessSession() || !RecoveryTarget.IsValid() || bRecoveryCommittedThisPress)
	{
		CancelRecovery();
		return;
	}
	RecoveryElapsed = FMath::Min(
		RecoveryElapsed + FMath::Max(0.0f, DeltaTime),
		GetDefault<UFacilityPlacementSettings>()->GetRecoveryHoldSeconds());
	if (Interaction)
	{
		Interaction->RefreshInteractionQuery();
	}
	if (RecoveryElapsed >= GetDefault<UFacilityPlacementSettings>()->GetRecoveryHoldSeconds())
	{
		CompleteRecoveryHold();
	}
}

void UPlayerFacilityPlacementComponent::CompleteRecoveryHold()
{
	if (!CanProcessSession() || !RecoveryTarget.IsValid() || bRecoveryCommittedThisPress)
	{
		CancelRecovery();
		return;
	}
	const float Required = GetDefault<UFacilityPlacementSettings>()->GetRecoveryHoldSeconds();
	if (RecoveryElapsed < Required)
	{
		CancelRecovery();
		return;
	}
	AActor* Target = TraceRecoveryTarget();
	IPlaceableFacility* Placeable = Target == RecoveryTarget.Get() ? Cast<IPlaceableFacility>(Target) : nullptr;
	CurrentRecoveryQuery = Placeable ? Placeable->QueryFacilityRecovery()
		: FFacilityPlacementTransactionResult::Failed(EFacilityPlacementFailureCode::StateChanged, LOCTEXT("RecoveryTargetChanged", "회수 대상에서 시선이 벗어났습니다."));
	FText FailureReason;
	const bool bSucceeded = CurrentRecoveryQuery.bSucceeded
		&& FFacilityActorConversionTransaction::RecoverFacilityToItem(*Target, FailureReason) != nullptr;
	bRecoveryCommittedThisPress = bSucceeded;
	ReportResult(bSucceeded
		? FPlayerInteractionResult::Succeeded(EPlayerInteractionIntent::FacilityRecovery)
		: FPlayerInteractionResult::Failed(FailureReason.IsEmpty() ? CurrentRecoveryQuery.FailureReason : FailureReason, EPlayerInteractionIntent::FacilityRecovery));
	CancelRecovery();
}

void UPlayerFacilityPlacementComponent::CancelRecoveryHold()
{
	CancelRecovery();
}

void UPlayerFacilityPlacementComponent::CancelRecovery()
{
	if (AActor* Target = RecoveryTarget.Get())
	{
		Target->OnDestroyed.RemoveDynamic(this, &UPlayerFacilityPlacementComponent::HandleRecoveryTargetDestroyed);
	}
	RecoveryTarget.Reset();
	RecoveryElapsed = 0.0f;
	CurrentRecoveryQuery = FFacilityPlacementTransactionResult();
	bRecoveryCommittedThisPress = false;
	UpdateTickState();
}

void UPlayerFacilityPlacementComponent::HandleRecoveryTargetDestroyed(AActor* DestroyedActor)
{
	(void)DestroyedActor;
	CancelRecovery();
}

void UPlayerFacilityPlacementComponent::CancelAllSessions()
{
	CancelPreview();
	CancelRecovery();
}

void UPlayerFacilityPlacementComponent::ReportResult(const FPlayerInteractionResult& Result) const
{
	if (Interaction) Interaction->ReportExternalInteractionAttempt(Result);
}

#undef LOCTEXT_NAMESPACE
