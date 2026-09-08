#include "Placement/PlayerFacilityPlacementComponent.h"

#include "Camera/CameraComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "Interaction/PhysicalCarryable.h"
#include "Interaction/PhysicalCarryPlacementTransaction.h"
#include "Interaction/PlayerCarryComponent.h"
#include "Interaction/PlayerInteractionComponent.h"
#include "Placement/FacilityPlacementComponent.h"
#include "Placement/FacilityPlacementDefinition.h"
#include "Placement/FacilityPlacementPreviewActor.h"
#include "Placement/FacilityPlacementSettings.h"
#include "Placement/FacilityPlacementZoneActor.h"
#include "Placement/PlaceableFacility.h"

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
	IPlaceableFacility* Placeable = Cast<IPlaceableFacility>(NewHeldObject);
	UFacilityPlacementComponent* Placement = Placeable ? Placeable->GetFacilityPlacementComponent() : nullptr;
	if (!Placement || Placement->GetMode() != EPlaceableFacilityMode::Packaged)
	{
		return;
	}
	PreviewFacility = NewHeldObject;
	AccumulatedYaw = 0.0f;
	UFacilityPlacementDefinition* Definition = Placement->GetDefinition();
	if (!Definition || !Definition->PreviewActorClass || !GetWorld())
	{
		SetPreviewFailure(
			EFacilityPlacementFailureCode::InvalidDefinition,
			LOCTEXT("MissingPreview", "설비 배치 미리보기를 만들 수 없습니다."));
		return;
	}
	PreviewActor = GetWorld()->SpawnActor<AFacilityPlacementPreviewActor>(
		Definition->PreviewActorClass,
		NewHeldObject->GetActorTransform());
	if (AFacilityPlacementPreviewActor* SpawnedPreview = PreviewActor.Get())
	{
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
	AActor* FacilityActor = PreviewFacility.Get();
	IPlaceableFacility* Placeable = Cast<IPlaceableFacility>(FacilityActor);
	UFacilityPlacementComponent* Placement = Placeable ? Placeable->GetFacilityPlacementComponent() : nullptr;
	UPrimitiveComponent* PackageRoot = Placement ? Placement->GetPackagePhysicalRoot() : nullptr;
	if (!Query.bSucceeded || !IsValid(PreviewActor.Get()) || !FacilityActor || !Placeable
		|| !IsValid(PackageRoot) || PackageRoot != FacilityActor->GetRootComponent()
		|| !Carry || Carry->GetHeldObject() != FacilityActor)
	{
		const FPlayerInteractionResult Result = FPlayerInteractionResult::Failed(
			Query.FailureReason.IsEmpty() ? LOCTEXT("PlacementUnavailable", "설비를 설치할 수 없습니다.") : Query.FailureReason,
			EPlayerInteractionIntent::PlacementConfirm);
		ReportResult(Result);
		return Result;
	}

	FPhysicalCarryPlacementTransaction Transaction(*FacilityActor, *PackageRoot);
	FText FailureReason;
	const bool bSucceeded = Transaction.IsValid()
		&& Transaction.ApplyPlacedWorld(Candidate)
		&& Carry->CommitReleasePhysicalObjectForPlacement(FacilityActor, [&]()
		{
			return IsValid(FacilityActor)
				&& Placeable->CommitPlaceableFacilityMode(EPlaceableFacilityMode::Placed, FailureReason);
		});
	if (!bSucceeded)
	{
		const FPlayerInteractionResult Result = FPlayerInteractionResult::Failed(
			FailureReason.IsEmpty() ? LOCTEXT("PlacementCommitFailed", "설비 설치 상태를 적용하지 못했습니다.") : FailureReason,
			EPlayerInteractionIntent::PlacementConfirm);
		ReportResult(Result);
		return Result;
	}
	Transaction.Commit();
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
		&& Placeable->CommitPlaceableFacilityMode(EPlaceableFacilityMode::Packaged, FailureReason);
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
