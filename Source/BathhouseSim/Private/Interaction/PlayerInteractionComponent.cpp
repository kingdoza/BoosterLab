#include "Interaction/PlayerInteractionComponent.h"

#include "Camera/CameraComponent.h"
#include "GameFramework/Pawn.h"
#include "Interaction/PlayerCarryComponent.h"
#include "Interaction/PlayerEquipmentUseComponent.h"
#include "Interaction/PlayerInteractable.h"

UPlayerInteractionComponent::UPlayerInteractionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
}

void UPlayerInteractionComponent::BeginPlay()
{
	Super::BeginPlay();
	RefreshInteractionQuery();
}

void UPlayerInteractionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CancelActiveHold(false, FText::GetEmpty(), false);
	ClearInteractionQuery();
	OnInteractionQueryChanged.Clear();
	OnInteractionAttemptFinishedNative.Clear();
	Camera = nullptr;
	CarryComponent = nullptr;
	EquipmentUseComponent = nullptr;
	Super::EndPlay(EndPlayReason);
}

void UPlayerInteractionComponent::TickComponent(
	const float DeltaTime,
	const ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (bInteractionSuppressed)
	{
		return;
	}

	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (OwnerPawn && OwnerPawn->IsLocallyControlled())
	{
		if (ActiveHoldTarget)
		{
			TickActiveHold(DeltaTime);
		}
		else
		{
			RefreshInteractionQuery();
		}
	}
	else
	{
		const bool bHadActiveHold = ActiveHoldTarget != nullptr;
		if (ActiveHoldTarget)
		{
			CancelActiveHold(false, FText::GetEmpty(), false);
		}
		else
		{
			ClearActiveHoldState();
		}
		ClearInteractionQuery();
		if (bHadActiveHold)
		{
			FinishInteractionAttempt(FPlayerInteractionResult::Failed(
				NSLOCTEXT(
					"BathhouseInteraction",
					"HoldLocalControlLost",
					"로컬 제어권을 잃어 상호작용이 취소되었습니다.")));
		}
	}
}

void UPlayerInteractionComponent::Configure(UCameraComponent* InCamera, UPlayerCarryComponent* InCarryComponent)
{
	Camera = InCamera;
	CarryComponent = InCarryComponent;
	RefreshInteractionQuery();
}

void UPlayerInteractionComponent::ConfigureEquipmentUse(UPlayerEquipmentUseComponent* InEquipmentUseComponent)
{
	EquipmentUseComponent = InEquipmentUseComponent;
	RefreshInteractionQuery();
}

void UPlayerInteractionComponent::SetInteractionSuppressed(const bool bSuppressed)
{
	if (bInteractionSuppressed == bSuppressed)
	{
		return;
	}

	bInteractionSuppressed = bSuppressed;
	if (bInteractionSuppressed)
	{
		if (EquipmentUseComponent)
		{
			EquipmentUseComponent->CancelEquipmentUse();
		}
		if (ActiveHoldTarget)
		{
			CancelActiveHold(false, FText::GetEmpty(), false);
		}
		else
		{
			ClearActiveHoldState();
		}
		ClearInteractionQuery();
		return;
	}

	RefreshInteractionQuery();
}

FPlayerInteractionResult UPlayerInteractionComponent::TryInteract()
{
	return BeginPrimaryInteraction();
}

FPlayerInteractionResult UPlayerInteractionComponent::BeginPrimaryInteraction()
{
	if (bInteractionSuppressed)
	{
		return FPlayerInteractionResult::Failed(
			NSLOCTEXT("BathhouseInteraction", "InteractionSuppressed", "현재 상호작용을 사용할 수 없습니다."));
	}

	if (bPrimaryInputHeld)
	{
		RefreshInteractionQuery();
		return FinishInteractionAttempt(FPlayerInteractionResult::Failed(
			NSLOCTEXT("BathhouseInteraction", "PrimaryAlreadyHeld", "이미 상호작용 중입니다.")));
	}
	bPrimaryInputHeld = true;

	FPlayerInteractionContext Context;
	IPlayerInteractable* Interactable = nullptr;
	UObject* TargetObject = nullptr;
	if (!BuildInteraction(Context, Interactable, TargetObject) || !Interactable)
	{
		bPrimaryInputHeld = false;
		RefreshInteractionQuery();
		return FinishInteractionAttempt(
			FPlayerInteractionResult::Failed(NSLOCTEXT("BathhouseInteraction", "NoTarget", "상호작용 대상이 없습니다.")));
	}

	const FPlayerInteractionQuery Query = Interactable->QueryInteraction(Context);
	CommitQuery(TargetObject, Query);
	if (!Query.bVisible || !Query.bCanInteract)
	{
		bPrimaryInputHeld = false;
		return FinishInteractionAttempt(FPlayerInteractionResult::Failed(Query.FailureReason));
	}
	if (Query.PrimaryActivationMode == EPlayerInteractionActivationMode::Hold)
	{
		FText FailureReason;
		if (!Interactable->BeginHoldInteraction(Context, FailureReason))
		{
			bPrimaryInputHeld = false;
			RefreshInteractionQuery();
			return FinishInteractionAttempt(FPlayerInteractionResult::Failed(FailureReason));
		}
		ActiveHoldTarget = TargetObject;
		ActiveHoldContext = Context;
		ActiveHoldProgress = 0.0f;
		FPlayerInteractionQuery HoldQuery = Interactable->QueryInteraction(Context);
		HoldQuery.HoldProgress = 0.0f;
		CommitQuery(TargetObject, HoldQuery);
		return FPlayerInteractionResult::Succeeded();
	}

	const FPlayerInteractionResult Result = Interactable->ExecuteInteraction(Context);
	bPrimaryInputHeld = false;
	RefreshInteractionQuery();
	return FinishInteractionAttempt(Result);
}

void UPlayerInteractionComponent::EndPrimaryInteraction()
{
	bPrimaryInputHeld = false;
	if (bInteractionSuppressed)
	{
		return;
	}
	if (ActiveHoldTarget)
	{
		CancelActiveHold(
			true,
			NSLOCTEXT("BathhouseInteraction", "HoldCancelled", "상호작용이 취소되었습니다."));
	}
}

FPlayerInteractionResult UPlayerInteractionComponent::TrySecondaryInteract()
{
	if (bInteractionSuppressed)
	{
		return FPlayerInteractionResult::Failed(
			NSLOCTEXT("BathhouseInteraction", "SecondaryInteractionSuppressed", "현재 상호작용을 사용할 수 없습니다."),
			EPlayerInteractionIntent::Secondary);
	}

	FPlayerInteractionContext Context;
	IPlayerInteractable* Interactable = nullptr;
	UObject* TargetObject = nullptr;
	if (!BuildInteraction(Context, Interactable, TargetObject) || !Interactable)
	{
		RefreshInteractionQuery();
		return FinishInteractionAttempt(FPlayerInteractionResult::Failed(
			NSLOCTEXT("BathhouseInteraction", "NoSecondaryTarget", "보조 상호작용 대상이 없습니다."),
			EPlayerInteractionIntent::Secondary));
	}
	const FPlayerInteractionQuery Query = Interactable->QueryInteraction(Context);
	CommitQuery(TargetObject, Query);
	if (!Query.bSecondaryVisible || !Query.bCanSecondaryInteract)
	{
		return FinishInteractionAttempt(FPlayerInteractionResult::Failed(
			Query.SecondaryFailureReason.IsEmpty()
				? NSLOCTEXT("BathhouseInteraction", "SecondaryUnavailable", "이 대상에는 보조 상호작용이 없습니다.")
				: Query.SecondaryFailureReason,
			EPlayerInteractionIntent::Secondary));
	}
	FPlayerInteractionResult Result = Interactable->ExecuteSecondaryInteraction(Context);
	Result.Intent = EPlayerInteractionIntent::Secondary;
	RefreshInteractionQuery();
	return FinishInteractionAttempt(Result);
}

FPlayerInteractionResult UPlayerInteractionComponent::TryDropCarry(
	const FVector& ViewDirection)
{
	if (bInteractionSuppressed)
	{
		return FPlayerInteractionResult::Failed(
			NSLOCTEXT("BathhouseInteraction", "DropCarrySuppressed", "현재 장비를 내려놓을 수 없습니다."),
			EPlayerInteractionIntent::DropCarry);
	}

	if (ActiveHoldTarget)
	{
		CancelActiveHold(
			true,
			NSLOCTEXT("BathhouseInteraction", "HoldInterruptedByDrop", "장비를 내려놓아 상호작용이 취소되었습니다."));
	}
	FPlayerInteractionResult Result = CarryComponent
		? CarryComponent->TryFreeDropHeldObject(ViewDirection)
		: FPlayerInteractionResult::Failed(
			NSLOCTEXT("BathhouseInteraction", "MissingCarry", "소지 상태를 확인할 수 없습니다."),
			EPlayerInteractionIntent::DropCarry);
	Result.Intent = EPlayerInteractionIntent::DropCarry;
	RefreshInteractionQuery();
	return FinishInteractionAttempt(Result);
}

FPlayerInteractionResult UPlayerInteractionComponent::TryDropCarry(
	const FVector& ViewOrigin,
	const FVector& ViewDirection)
{
	return TryDropCarry(ViewDirection);
}

void UPlayerInteractionComponent::RefreshInteractionQuery()
{
	if (bInteractionSuppressed)
	{
		return;
	}

	FPlayerInteractionContext Context;
	IPlayerInteractable* Interactable = nullptr;
	UObject* TargetObject = nullptr;
	FPlayerInteractionQuery Query;
	if (BuildInteraction(Context, Interactable, TargetObject) && Interactable)
	{
		Query = Interactable->QueryInteraction(Context);
		if (TargetObject == ActiveHoldTarget)
		{
			Query.HoldProgress = ActiveHoldProgress;
		}
	}
	if (EquipmentUseComponent)
	{
		Query = EquipmentUseComponent->MergeEquipmentQuery(Query);
	}
	CommitQuery(TargetObject, Query);
}

void UPlayerInteractionComponent::ClearInteractionQuery()
{
	CommitQuery(nullptr, FPlayerInteractionQuery());
}

bool UPlayerInteractionComponent::BuildInteraction(
	FPlayerInteractionContext& OutContext,
	IPlayerInteractable*& OutInteractable,
	UObject*& OutTargetObject) const
{
	OutInteractable = nullptr;
	OutTargetObject = nullptr;
	FHitResult Hit;
	if (!TraceFocus(Hit))
	{
		return false;
	}

	UObject* Candidate = Hit.GetComponent();
	if (!Candidate || !Candidate->GetClass()->ImplementsInterface(UPlayerInteractable::StaticClass()))
	{
		Candidate = Hit.GetActor();
	}
	if (!Candidate || !Candidate->GetClass()->ImplementsInterface(UPlayerInteractable::StaticClass()))
	{
		return false;
	}

	OutInteractable = Cast<IPlayerInteractable>(Candidate);
	OutTargetObject = Candidate;
	OutContext.Interactor = GetOwner();
	OutContext.CarryComponent = CarryComponent;
	OutContext.HitActor = Hit.GetActor();
	OutContext.HitComponent = Hit.GetComponent();
	OutContext.HitResult = Hit;
	return OutInteractable != nullptr;
}

bool UPlayerInteractionComponent::TraceFocus(FHitResult& OutHit) const
{
	OutHit = FHitResult();
	if (bInteractionSuppressed || !Camera || !GetWorld() || !GetOwner())
	{
		return false;
	}
	const FVector Start = Camera->GetComponentLocation();
	const FVector End = Start + Camera->GetForwardVector() * TraceDistance;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(PlayerInteraction), true, GetOwner());
	return GetWorld()->LineTraceSingleByChannel(OutHit, Start, End, TraceChannel, QueryParams);
}

bool UPlayerInteractionComponent::GetCurrentFocusHit(FHitResult& OutHit) const
{
	return TraceFocus(OutHit);
}

FPlayerInteractionResult UPlayerInteractionComponent::ReportExternalInteractionAttempt(
	const FPlayerInteractionResult& Result)
{
	return FinishInteractionAttempt(Result);
}

FPlayerInteractionResult UPlayerInteractionComponent::FinishInteractionAttempt(const FPlayerInteractionResult& Result)
{
	OnInteractionAttemptFinishedNative.Broadcast(Result);
	return Result;
}

void UPlayerInteractionComponent::TickActiveHold(const float DeltaTime)
{
	if (!bPrimaryInputHeld || !ActiveHoldTarget)
	{
		CancelActiveHold(
			true,
			NSLOCTEXT("BathhouseInteraction", "HoldInputLost", "상호작용 입력이 유지되지 않았습니다."));
		return;
	}

	FPlayerInteractionContext Context;
	IPlayerInteractable* Interactable = nullptr;
	UObject* TargetObject = nullptr;
	if (!BuildInteraction(Context, Interactable, TargetObject) || !Interactable || TargetObject != ActiveHoldTarget)
	{
		CancelActiveHold(
			true,
			NSLOCTEXT("BathhouseInteraction", "HoldFocusLost", "대상에서 시선을 떼어 상호작용이 취소되었습니다."));
		return;
	}
	const FPlayerInteractionQuery Query = Interactable->QueryInteraction(Context);
	if (!Query.bVisible || !Query.bCanInteract
		|| Query.PrimaryActivationMode != EPlayerInteractionActivationMode::Hold)
	{
		CancelActiveHold(
			true,
			Query.FailureReason.IsEmpty()
				? NSLOCTEXT("BathhouseInteraction", "HoldInvalidated", "상호작용 조건이 유지되지 않았습니다.")
				: Query.FailureReason);
		return;
	}

	ActiveHoldContext = Context;
	const FPlayerHoldInteractionUpdate Update = Interactable->UpdateHoldInteraction(Context, DeltaTime);
	ActiveHoldProgress = FMath::Clamp(Update.Progress, 0.0f, 1.0f);
	if (Update.State == EPlayerHoldInteractionState::Running)
	{
		FPlayerInteractionQuery ProgressQuery = Interactable->QueryInteraction(Context);
		ProgressQuery.HoldProgress = ActiveHoldProgress;
		CommitQuery(TargetObject, ProgressQuery);
		return;
	}

	const bool bSucceeded = Update.State == EPlayerHoldInteractionState::Succeeded;
	ClearActiveHoldState();
	RefreshInteractionQuery();
	FinishInteractionAttempt(bSucceeded
		? FPlayerInteractionResult::Succeeded()
		: FPlayerInteractionResult::Failed(Update.FailureReason));
}

void UPlayerInteractionComponent::CancelActiveHold(
	const bool bBroadcastFailure,
	const FText& FailureReason,
	const bool bRefreshQuery)
{
	UObject* TargetObject = ActiveHoldTarget.Get();
	const FPlayerInteractionContext Context = ActiveHoldContext;
	ClearActiveHoldState();
	if (IPlayerInteractable* Interactable = Cast<IPlayerInteractable>(TargetObject))
	{
		Interactable->CancelHoldInteraction(Context);
	}
	if (bRefreshQuery)
	{
		RefreshInteractionQuery();
	}
	if (bBroadcastFailure)
	{
		FinishInteractionAttempt(FPlayerInteractionResult::Failed(FailureReason));
	}
}

void UPlayerInteractionComponent::ClearActiveHoldState()
{
	ActiveHoldTarget = nullptr;
	ActiveHoldContext = FPlayerInteractionContext();
	ActiveHoldProgress = 0.0f;
	bPrimaryInputHeld = false;
}

void UPlayerInteractionComponent::CommitQuery(UObject* TargetObject, const FPlayerInteractionQuery& NewQuery)
{
	if (CurrentTarget == TargetObject && CurrentQuery.Equals(NewQuery))
	{
		return;
	}

	CurrentTarget = TargetObject;
	CurrentQuery = NewQuery;
	OnInteractionQueryChanged.Broadcast(CurrentQuery);
}
