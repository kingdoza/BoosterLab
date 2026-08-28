#include "Customer/CustomerQueueNavigationComponent.h"

#include "AIController.h"
#include "Customer/BathhouseCustomerCharacter.h"
#include "Customer/CustomerRoutineDefinition.h"
#include "Customer/CustomerSessionComponent.h"
#include "Facility/BathhouseCounterActor.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Tasks/AITask_MoveTo.h"

DEFINE_LOG_CATEGORY_STATIC(LogCustomerQueueNavigation, Log, All);

namespace
{
constexpr float QueueLocationMaterialTolerance = 1.0f;
constexpr float QueueYawMaterialTolerance = 0.25f;
}

UCustomerQueueNavigationComponent::UCustomerQueueNavigationComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UCustomerQueueNavigationComponent::BeginPlay()
{
	Super::BeginPlay();
	SetComponentTickEnabled(ActiveExecutionToken != 0 && !bSuspended);
}

void UCustomerQueueNavigationComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	RecoveryCompletion.Unbind();
	CleanupExecution();
	Super::EndPlay(EndPlayReason);
}

uint64 UCustomerQueueNavigationComponent::BeginQueueNavigation(const EBathhouseCounterLane ExpectedLane)
{
	if (ActiveExecutionToken != 0)
	{
		return ActiveLane == ExpectedLane && !bRecoveryOnlyExecution ? ActiveExecutionToken : 0;
	}
	if (!InitializeExecution(ExpectedLane, false) || !RefreshAssignment(true))
	{
		CleanupExecution();
		return 0;
	}
	return ActiveExecutionToken;
}

void UCustomerQueueNavigationComponent::CancelQueueNavigation(const uint64 ExecutionToken)
{
	if (ExecutionToken != 0 && ExecutionToken == ActiveExecutionToken && !bRecoveryGate)
	{
		CleanupExecution();
	}
}

void UCustomerQueueNavigationComponent::CancelForIntentionalQueueLeave()
{
	CleanupExecution();
}

ECustomerQueueNavigationStatus UCustomerQueueNavigationComponent::GetQueueNavigationStatus(
	const uint64 ExecutionToken) const
{
	return ExecutionToken != 0 && ExecutionToken == ActiveExecutionToken
		? Status
		: ECustomerQueueNavigationStatus::Inactive;
}

void UCustomerQueueNavigationComponent::SuspendForKnockdown()
{
	if (ActiveExecutionToken == 0 || bSuspended)
	{
		return;
	}
	CancelActiveMove();
	Mode = EExecutionMode::None;
	bSuspended = true;
	Status = ECustomerQueueNavigationStatus::Suspended;
	SetComponentTickEnabled(false);
}

void UCustomerQueueNavigationComponent::ResumeQueueNavigationAfterOverflowInterruption()
{
	if (ActiveExecutionToken == 0)
	{
		return;
	}
	bSuspended = false;
	CurrentAssignment = FBathhouseQueueAssignment();
	RefreshAssignment(true);
}

bool UCustomerQueueNavigationComponent::BeginQueuePoseRecovery(FOnCustomerQueueRecoveryFinished Completion)
{
	ABathhouseCustomerCharacter* Customer = Cast<ABathhouseCustomerCharacter>(GetOwner());
	UCustomerSessionComponent* Session = Customer ? Customer->GetCustomerSession() : nullptr;
	if (!Session || Session->GetQueueLane() == EBathhouseCounterLane::None || bRecoveryGate)
	{
		return false;
	}
	if (ActiveExecutionToken == 0 && !InitializeExecution(Session->GetQueueLane(), true))
	{
		return false;
	}
	if (ActiveLane != Session->GetQueueLane())
	{
		return false;
	}

	RecoveryCompletion = MoveTemp(Completion);
	bRecoveryGate = true;
	bSuspended = false;
	CurrentAssignment = FBathhouseQueueAssignment();
	if (!RefreshAssignment(true))
	{
		CompleteRecovery(false);
		return false;
	}
	return true;
}

void UCustomerQueueNavigationComponent::CancelQueuePoseRecovery()
{
	if (!bRecoveryGate)
	{
		return;
	}
	RecoveryCompletion.Unbind();
	bRecoveryGate = false;
	if (bRecoveryOnlyExecution)
	{
		CleanupExecution();
	}
	else
	{
		SuspendForKnockdown();
	}
}

bool UCustomerQueueNavigationComponent::InitializeExecution(
	const EBathhouseCounterLane ExpectedLane,
	const bool bRecoveryOnly)
{
	ABathhouseCustomerCharacter* Customer = Cast<ABathhouseCustomerCharacter>(GetOwner());
	UCustomerSessionComponent* Session = Customer ? Customer->GetCustomerSession() : nullptr;
	ABathhouseCounterActor* Counter = Session ? Session->GetCounter() : nullptr;
	if (!Customer || !Session || !Counter || ExpectedLane == EBathhouseCounterLane::None
		|| Session->GetQueueLane() != ExpectedLane)
	{
		return false;
	}

	ActiveExecutionToken = AllocateNonZeroToken(NextExecutionToken);
	ActiveLane = ExpectedLane;
	BoundCounter = Counter;
	bRecoveryOnlyExecution = bRecoveryOnly;
	QueueChangedHandle = Counter->OnQueueChangedNative.AddUObject(
		this,
		&UCustomerQueueNavigationComponent::HandleQueueChanged);
	SnapshotMovementFlags();
	return true;
}

bool UCustomerQueueNavigationComponent::RefreshAssignment(const bool bForceRestart)
{
	ABathhouseCustomerCharacter* Customer = Cast<ABathhouseCustomerCharacter>(GetOwner());
	UCustomerSessionComponent* Session = Customer ? Customer->GetCustomerSession() : nullptr;
	ABathhouseCounterActor* Counter = BoundCounter.Get();
	FBathhouseQueueAssignment NewAssignment;
	if (!Customer || !Session || !Counter || Session->GetQueueLane() != ActiveLane
		|| !Counter->ResolveQueueAssignment(ActiveLane, Customer, NewAssignment))
	{
		Status = ECustomerQueueNavigationStatus::Failed;
		Mode = EExecutionMode::None;
		CancelActiveMove();
		if (Session && !Session->IsFinished())
		{
			Session->TechnicalAbort(TEXT("Queue assignment became invalid during native queue navigation."));
		}
		if (bRecoveryGate)
		{
			CompleteRecovery(false);
		}
		return false;
	}

	const bool bMaterialChange = bForceRestart || IsMaterialAssignmentChange(NewAssignment);
	CurrentAssignment = NewAssignment;
	if (!bMaterialChange)
	{
		return true;
	}

	CancelActiveMove();
	OverflowPauseRemaining = 0.0f;
	Mode = EExecutionMode::None;
	Status = ECustomerQueueNavigationStatus::Running;
	const UCustomerRoutineDefinition* Definition = Session->GetRoutineDefinition();
	if (NewAssignment.Type == EBathhouseQueueAssignmentType::OverflowWander)
	{
		if (bRecoveryGate)
		{
			CompleteRecovery(true);
			return true;
		}
		FVector WanderPoint;
		if (!Counter->TrySampleCheckoutOverflowPoint(*Customer, WanderPoint))
		{
			const float MinPause = Definition ? FMath::Max(0.0f, Definition->OverflowPauseMinSeconds) : 1.0f;
			const float MaxPause = Definition ? FMath::Max(MinPause, Definition->OverflowPauseMaxSeconds) : MinPause;
			OverflowPauseRemaining = FMath::FRandRange(MinPause, MaxPause);
			Mode = EExecutionMode::OverflowPause;
			Status = ECustomerQueueNavigationStatus::Waiting;
			SetComponentTickEnabled(true);
			return true;
		}
		const float Acceptance = Definition
			? FMath::Max(1.0f, Definition->OverflowWanderAcceptanceRadius)
			: 50.0f;
		return StartMove(WanderPoint, Acceptance, EExecutionMode::MovingOverflow);
	}

	const float Acceptance = Definition ? FMath::Max(1.0f, Definition->QueueAcceptanceRadius) : 10.0f;
	if (FVector::DistSquared2D(Customer->GetActorLocation(), NewAssignment.TargetTransform.GetLocation())
		<= FMath::Square(Acceptance))
	{
		StartFacing();
		return true;
	}
	return StartMove(NewAssignment.TargetTransform.GetLocation(), Acceptance, EExecutionMode::MovingVisible);
}

bool UCustomerQueueNavigationComponent::StartMove(
	const FVector& Destination,
	const float AcceptanceRadius,
	const EExecutionMode MoveMode)
{
	ABathhouseCustomerCharacter* Customer = Cast<ABathhouseCustomerCharacter>(GetOwner());
	AAIController* Controller = Customer ? Cast<AAIController>(Customer->GetController()) : nullptr;
	if (!Controller)
	{
		HandleNavigationFailure();
		return false;
	}
	ApplyMovementFlagsForMove();
	ActiveMoveToken = AllocateNonZeroToken(NextMoveToken);
	ActiveMoveTask = UAITask_MoveTo::AIMoveTo(
		Controller,
		Destination,
		nullptr,
		FMath::Max(1.0f, AcceptanceRadius),
		EAIOptionFlag::Enable,
		EAIOptionFlag::Enable,
		true,
		false,
		false,
		EAIOptionFlag::Enable,
		EAIOptionFlag::Enable);
	if (!ActiveMoveTask)
	{
		ActiveMoveToken = 0;
		HandleNavigationFailure();
		return false;
	}
	Mode = MoveMode;
	Status = ECustomerQueueNavigationStatus::Running;
	ActiveMoveTask->ReadyForActivation();
	SetComponentTickEnabled(true);
	return true;
}

void UCustomerQueueNavigationComponent::StartFacing()
{
	ApplyMovementFlagsForFacing();
	Mode = EExecutionMode::FacingVisible;
	Status = ECustomerQueueNavigationStatus::Running;
	SetComponentTickEnabled(true);
}

void UCustomerQueueNavigationComponent::TickComponent(
	const float DeltaTime,
	const ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (ActiveExecutionToken == 0 || bSuspended)
	{
		SetComponentTickEnabled(false);
		return;
	}

	if ((Mode == EExecutionMode::MovingVisible || Mode == EExecutionMode::MovingOverflow)
		&& ActiveMoveTask && !ActiveMoveTask->IsActive())
	{
		const uint64 CompletedToken = ActiveMoveToken;
		const bool bSucceeded = ActiveMoveTask->WasMoveSuccessful();
		ActiveMoveTask = nullptr;
		HandleMoveFinished(bSucceeded, CompletedToken);
		return;
	}

	if (Mode == EExecutionMode::FacingVisible)
	{
		ABathhouseCustomerCharacter* Customer = Cast<ABathhouseCustomerCharacter>(GetOwner());
		UCustomerSessionComponent* Session = Customer ? Customer->GetCustomerSession() : nullptr;
		const UCustomerRoutineDefinition* Definition = Session ? Session->GetRoutineDefinition() : nullptr;
		if (!Customer)
		{
			HandleNavigationFailure();
			return;
		}
		const float TargetYaw = CurrentAssignment.TargetTransform.Rotator().Yaw;
		const float Tolerance = Definition ? FMath::Clamp(Definition->QueueFacingToleranceDegrees, 0.1f, 180.0f) : 2.0f;
		const float CurrentYaw = Customer->GetActorRotation().Yaw;
		if (FMath::Abs(FMath::FindDeltaAngleDegrees(CurrentYaw, TargetYaw)) <= Tolerance)
		{
			Customer->SetActorRotation(FRotator(0.0f, TargetYaw, 0.0f));
			CompleteVisibleTarget();
			return;
		}
		const float Speed = Definition ? FMath::Max(1.0f, Definition->QueueFacingRotationSpeedDegrees) : 360.0f;
		const float NewYaw = FMath::FixedTurn(CurrentYaw, TargetYaw, Speed * FMath::Max(0.0f, DeltaTime));
		Customer->SetActorRotation(FRotator(0.0f, NewYaw, 0.0f));
		return;
	}

	if (Mode == EExecutionMode::OverflowPause)
	{
		OverflowPauseRemaining -= FMath::Max(0.0f, DeltaTime);
		if (OverflowPauseRemaining <= 0.0f)
		{
			RefreshAssignment(true);
		}
	}
}

void UCustomerQueueNavigationComponent::HandleMoveFinished(
	const bool bSucceeded,
	const uint64 CompletedMoveToken)
{
	if (CompletedMoveToken == 0 || CompletedMoveToken != ActiveMoveToken)
	{
		return;
	}
	ActiveMoveToken = 0;
	if (!bSucceeded)
	{
		HandleNavigationFailure();
		return;
	}
	if (ABathhouseCustomerCharacter* Customer = Cast<ABathhouseCustomerCharacter>(GetOwner()))
	{
		if (UCustomerSessionComponent* Session = Customer->GetCustomerSession())
		{
			Session->ResetNavigationFailures();
		}
	}
	if (Mode == EExecutionMode::MovingVisible)
	{
		StartFacing();
		return;
	}
	if (Mode == EExecutionMode::MovingOverflow)
	{
		ABathhouseCustomerCharacter* Customer = Cast<ABathhouseCustomerCharacter>(GetOwner());
		const UCustomerSessionComponent* Session = Customer ? Customer->GetCustomerSession() : nullptr;
		const UCustomerRoutineDefinition* Definition = Session ? Session->GetRoutineDefinition() : nullptr;
		const float MinPause = Definition ? FMath::Max(0.0f, Definition->OverflowPauseMinSeconds) : 1.0f;
		const float MaxPause = Definition ? FMath::Max(MinPause, Definition->OverflowPauseMaxSeconds) : MinPause;
		OverflowPauseRemaining = FMath::FRandRange(MinPause, MaxPause);
		Mode = EExecutionMode::OverflowPause;
		Status = ECustomerQueueNavigationStatus::Waiting;
	}
}

void UCustomerQueueNavigationComponent::HandleNavigationFailure()
{
	ABathhouseCustomerCharacter* Customer = Cast<ABathhouseCustomerCharacter>(GetOwner());
	UCustomerSessionComponent* Session = Customer ? Customer->GetCustomerSession() : nullptr;
	CancelActiveMove();
	if (!Session || Session->RegisterNavigationFailure())
	{
		Status = ECustomerQueueNavigationStatus::Failed;
		Mode = EExecutionMode::None;
		SetComponentTickEnabled(false);
		if (bRecoveryGate)
		{
			CompleteRecovery(false);
		}
		return;
	}
	RefreshAssignment(true);
}

void UCustomerQueueNavigationComponent::CompleteVisibleTarget()
{
	Mode = EExecutionMode::None;
	Status = CurrentAssignment.Type == EBathhouseQueueAssignmentType::ServicePoint
		? ECustomerQueueNavigationStatus::ServiceReady
		: ECustomerQueueNavigationStatus::Waiting;
	SetComponentTickEnabled(false);
	if (bRecoveryGate)
	{
		CompleteRecovery(true);
	}
}

void UCustomerQueueNavigationComponent::CompleteRecovery(const bool bSucceeded)
{
	if (!bRecoveryGate)
	{
		return;
	}
	bRecoveryGate = false;
	FOnCustomerQueueRecoveryFinished Completion = MoveTemp(RecoveryCompletion);
	RecoveryCompletion.Unbind();
	if (bRecoveryOnlyExecution)
	{
		CleanupExecution();
	}
	Completion.ExecuteIfBound(bSucceeded);
}

void UCustomerQueueNavigationComponent::HandleQueueChanged(const EBathhouseCounterLane ChangedLane)
{
	if (ChangedLane == ActiveLane && ActiveExecutionToken != 0 && !bSuspended)
	{
		RefreshAssignment(false);
	}
}

void UCustomerQueueNavigationComponent::CancelActiveMove()
{
	if (ActiveMoveToken != 0 || ActiveMoveTask)
	{
		++NextMoveToken;
	}
	ActiveMoveToken = 0;
	if (ActiveMoveTask && ActiveMoveTask->IsActive())
	{
		ActiveMoveTask->ExternalCancel();
	}
	ActiveMoveTask = nullptr;
}

void UCustomerQueueNavigationComponent::CleanupExecution()
{
	CancelActiveMove();
	if (ABathhouseCounterActor* Counter = BoundCounter.Get(); Counter && QueueChangedHandle.IsValid())
	{
		Counter->OnQueueChangedNative.Remove(QueueChangedHandle);
	}
	QueueChangedHandle.Reset();
	BoundCounter.Reset();
	RestoreMovementFlags();
	CurrentAssignment = FBathhouseQueueAssignment();
	RecoveryCompletion.Unbind();
	ActiveExecutionToken = 0;
	ActiveLane = EBathhouseCounterLane::None;
	Status = ECustomerQueueNavigationStatus::Inactive;
	Mode = EExecutionMode::None;
	OverflowPauseRemaining = 0.0f;
	bSuspended = false;
	bRecoveryGate = false;
	bRecoveryOnlyExecution = false;
	SetComponentTickEnabled(false);
}

void UCustomerQueueNavigationComponent::SnapshotMovementFlags()
{
	ABathhouseCustomerCharacter* Customer = Cast<ABathhouseCustomerCharacter>(GetOwner());
	UCharacterMovementComponent* Movement = Customer ? Customer->GetCharacterMovement() : nullptr;
	if (!Customer || !Movement || bMovementFlagsSnapshotted)
	{
		return;
	}
	bSavedOrientRotationToMovement = Movement->bOrientRotationToMovement;
	bSavedUseControllerDesiredRotation = Movement->bUseControllerDesiredRotation;
	bSavedUseControllerRotationYaw = Customer->bUseControllerRotationYaw;
	bMovementFlagsSnapshotted = true;
}

void UCustomerQueueNavigationComponent::ApplyMovementFlagsForMove()
{
	ABathhouseCustomerCharacter* Customer = Cast<ABathhouseCustomerCharacter>(GetOwner());
	UCharacterMovementComponent* Movement = Customer ? Customer->GetCharacterMovement() : nullptr;
	if (Customer && Movement && bMovementFlagsSnapshotted)
	{
		Movement->bOrientRotationToMovement = bSavedOrientRotationToMovement;
		Movement->bUseControllerDesiredRotation = bSavedUseControllerDesiredRotation;
		Customer->bUseControllerRotationYaw = bSavedUseControllerRotationYaw;
	}
}

void UCustomerQueueNavigationComponent::ApplyMovementFlagsForFacing()
{
	ABathhouseCustomerCharacter* Customer = Cast<ABathhouseCustomerCharacter>(GetOwner());
	UCharacterMovementComponent* Movement = Customer ? Customer->GetCharacterMovement() : nullptr;
	if (Customer && Movement)
	{
		Movement->bOrientRotationToMovement = false;
		Movement->bUseControllerDesiredRotation = false;
		Customer->bUseControllerRotationYaw = false;
	}
}

void UCustomerQueueNavigationComponent::RestoreMovementFlags()
{
	ABathhouseCustomerCharacter* Customer = Cast<ABathhouseCustomerCharacter>(GetOwner());
	UCharacterMovementComponent* Movement = Customer ? Customer->GetCharacterMovement() : nullptr;
	if (Customer && Movement && bMovementFlagsSnapshotted)
	{
		Movement->bOrientRotationToMovement = bSavedOrientRotationToMovement;
		Movement->bUseControllerDesiredRotation = bSavedUseControllerDesiredRotation;
		Customer->bUseControllerRotationYaw = bSavedUseControllerRotationYaw;
	}
	bMovementFlagsSnapshotted = false;
}

uint64 UCustomerQueueNavigationComponent::AllocateNonZeroToken(uint64& Counter)
{
	uint64 Token = ++Counter;
	if (Token == 0)
	{
		Token = ++Counter;
	}
	return Token;
}

bool UCustomerQueueNavigationComponent::IsMaterialAssignmentChange(
	const FBathhouseQueueAssignment& NewAssignment) const
{
	if (CurrentAssignment.Type != NewAssignment.Type
		|| CurrentAssignment.LogicalIndex != NewAssignment.LogicalIndex
		|| CurrentAssignment.QueuePointIndex != NewAssignment.QueuePointIndex)
	{
		return true;
	}
	if (!NewAssignment.IsVisibleAssignment())
	{
		return false;
	}
	return !CurrentAssignment.TargetTransform.GetLocation().Equals(
			NewAssignment.TargetTransform.GetLocation(),
			QueueLocationMaterialTolerance)
		|| FMath::Abs(FMath::FindDeltaAngleDegrees(
			CurrentAssignment.TargetTransform.Rotator().Yaw,
			NewAssignment.TargetTransform.Rotator().Yaw)) > QueueYawMaterialTolerance;
}
