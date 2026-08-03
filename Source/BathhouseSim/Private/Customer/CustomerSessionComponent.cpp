#include "Customer/CustomerSessionComponent.h"

#include "AIController.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StateTreeAIComponent.h"
#include "Customer/BathhouseCustomerAIController.h"
#include "Customer/BathhouseCustomerCharacter.h"
#include "Customer/CustomerRoutineDefinition.h"
#include "Economy/BathhouseCashPaymentActor.h"
#include "Engine/World.h"
#include "Facility/BathhouseCounterActor.h"
#include "Facility/BathhouseFacilityActor.h"
#include "Facility/BathhouseFacilitySlotComponent.h"
#include "Facility/BathhouseFacilitySubsystem.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Interaction/BathhouseKeyActor.h"
#include "Interaction/BathhouseKeyHookActor.h"
#include "Interaction/PlayerCarryComponent.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

#define LOCTEXT_NAMESPACE "CustomerSessionComponent"

UCustomerSessionComponent::UCustomerSessionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UCustomerSessionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(CheckInTimeoutHandle);
		World->GetTimerManager().ClearTimer(BathStayTimerHandle);
	}
	StopWaitingForFacility();
	ReleaseCurrentFacility();
	LeaveQueue();
	if (Counter && QueueChangedHandle.IsValid())
	{
		Counter->OnQueueChangedNative.Remove(QueueChangedHandle);
		QueueChangedHandle.Reset();
	}

	if (!bFinished)
	{
		if (CashOffer && !CashOffer->IsClaimed())
		{
			CashOffer->Destroy();
		}
		if (AssignedKey && AssignedKey->GetKeyState() != EBathhouseKeyState::OnCounter)
		{
			AssignedKey->RecoverToHook();
		}
	}
	Super::EndPlay(EndPlayReason);
}

void UCustomerSessionComponent::InitializeSession(
	UCustomerRoutineDefinition* InRoutineDefinition,
	ABathhouseCounterActor* InCounter)
{
	if (Counter && QueueChangedHandle.IsValid())
	{
		Counter->OnQueueChangedNative.Remove(QueueChangedHandle);
	}
	RoutineDefinition = InRoutineDefinition;
	Counter = InCounter;
	if (Counter)
	{
		QueueChangedHandle = Counter->OnQueueChangedNative.AddUObject(this, &UCustomerSessionComponent::HandleQueueChanged);
	}
}

FPlayerInteractionQuery UCustomerSessionComponent::QueryCheckInInteraction(const FPlayerInteractionContext& Context) const
{
	FPlayerInteractionQuery Query;
	if (!bWaitingForCheckIn || bCheckInTerminalCommitted || !Counter || !Counter->IsFront(EBathhouseCounterLane::CheckIn, GetOwner()))
	{
		return Query;
	}

	Query.bVisible = true;
	Query.TargetName = LOCTEXT("CustomerTarget", "입장 손님");
	Query.ActionName = LOCTEXT("GiveKey", "키 전달하기");
	if (!Context.CarryComponent)
	{
		Query.FailureReason = LOCTEXT("MissingCarry", "키 소지 상태를 확인할 수 없습니다.");
		return Query;
	}

	ABathhouseKeyActor* HeldKey = Context.CarryComponent->GetHeldKey();
	if (!HeldKey || HeldKey->GetKeyState() != EBathhouseKeyState::HeldByPlayer)
	{
		Query.FailureReason = LOCTEXT("NoHeldKey", "전달할 번호 키를 들고 있지 않습니다.");
		return Query;
	}

	FText TopologyFailure;
	const ABathhouseKeyHookActor* Hook = HeldKey->GetKeyHook();
	const UBathhouseFacilitySubsystem* Subsystem = GetWorld() ? GetWorld()->GetSubsystem<UBathhouseFacilitySubsystem>() : nullptr;
	Query.bCanInteract = Hook && Subsystem && Subsystem->ValidateKeyNumber(HeldKey->GetKeyNumber(), Hook, &TopologyFailure);
	if (!Query.bCanInteract)
	{
		Query.FailureReason = TopologyFailure.IsEmpty()
			? LOCTEXT("InvalidKey", "이 번호 키에 필요한 시설이 올바르지 않습니다.")
			: TopologyFailure;
	}
	return Query;
}

FPlayerInteractionResult UCustomerSessionComponent::ExecuteCheckInInteraction(const FPlayerInteractionContext& Context)
{
	const FPlayerInteractionQuery Query = QueryCheckInInteraction(Context);
	if (!Query.bCanInteract || !Context.CarryComponent || bCheckInTerminalCommitted)
	{
		return FPlayerInteractionResult::Failed(Query.FailureReason);
	}

	ABathhouseKeyActor* Key = Context.CarryComponent->GetHeldKey();
	if (!Key || !Key->TryAssignToCustomer(*Context.CarryComponent, *GetOwner()))
	{
		return FPlayerInteractionResult::Failed(LOCTEXT("KeyCommitFailed", "키 상태가 변경되어 전달에 실패했습니다."));
	}

	bCheckInTerminalCommitted = true;
	bWaitingForCheckIn = false;
	AssignedKey = Key;
	KeyNumber = Key->GetKeyNumber();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(CheckInTimeoutHandle);
	}
	SendCustomerEvent(TAG_Customer_Event_KeyReceived);
	return FPlayerInteractionResult::Succeeded();
}

bool UCustomerSessionComponent::JoinQueue(const EBathhouseCounterLane Lane)
{
	if (!Counter || Lane == EBathhouseCounterLane::None)
	{
		return false;
	}
	if (QueueLane == Lane)
	{
		return true;
	}
	if (QueueLane != EBathhouseCounterLane::None)
	{
		return false;
	}
	if (!Counter->EnqueueActor(Lane, GetOwner()))
	{
		return false;
	}
	QueueLane = Lane;
	SetPresentationState(Lane == EBathhouseCounterLane::CheckIn
		? EBathhouseCustomerPresentationState::QueueingCheckIn
		: EBathhouseCustomerPresentationState::QueueingCheckout);
	return true;
}

void UCustomerSessionComponent::LeaveQueue()
{
	const EBathhouseCounterLane PreviousLane = QueueLane;
	QueueLane = EBathhouseCounterLane::None;
	CancelCheckInWait();
	EndCheckoutOffer();
	if (Counter && PreviousLane != EBathhouseCounterLane::None)
	{
		Counter->DequeueActor(PreviousLane, GetOwner());
	}
}

bool UCustomerSessionComponent::IsQueueFront() const
{
	return Counter && QueueLane != EBathhouseCounterLane::None && Counter->IsFront(QueueLane, GetOwner());
}

bool UCustomerSessionComponent::GetQueueTargetTransform(FTransform& OutTransform) const
{
	return Counter && QueueLane != EBathhouseCounterLane::None
		&& Counter->GetQueueTargetTransform(QueueLane, GetOwner(), OutTransform);
}

void UCustomerSessionComponent::BeginWaitingForCheckIn()
{
	if (bWaitingForCheckIn || bCheckInTerminalCommitted || QueueLane != EBathhouseCounterLane::CheckIn
		|| !IsQueueFront() || !RoutineDefinition || !GetWorld())
	{
		return;
	}
	bWaitingForCheckIn = true;
	SetPresentationState(EBathhouseCustomerPresentationState::WaitingForKey);
	GetWorld()->GetTimerManager().SetTimer(
		CheckInTimeoutHandle,
		this,
		&UCustomerSessionComponent::HandleCheckInTimeout,
		FMath::Max(RoutineDefinition->CheckInTimeoutSeconds, 0.1f),
		false);
}

void UCustomerSessionComponent::CancelCheckInWait()
{
	bWaitingForCheckIn = false;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(CheckInTimeoutHandle);
	}
}

bool UCustomerSessionComponent::TryReserveFacility(const EBathhouseFacilityType FacilityType, const bool bExcludeLastBath)
{
	if (CurrentFacilitySlot)
	{
		if (!bHasCachedFacilityTransforms)
		{
			CacheCurrentFacilityTransforms();
		}
		return CurrentFacilityActor && CurrentFacilityActor->GetFacilityType() == FacilityType;
	}
	UBathhouseFacilitySubsystem* Subsystem = GetWorld() ? GetWorld()->GetSubsystem<UBathhouseFacilitySubsystem>() : nullptr;
	if (!Subsystem)
	{
		return false;
	}

	const int32 Number = FacilityType == EBathhouseFacilityType::ShoeLocker || FacilityType == EBathhouseFacilityType::ClothesLocker
		? KeyNumber
		: INDEX_NONE;
	ABathhouseFacilityActor* Facility = nullptr;
	UBathhouseFacilitySlotComponent* Slot = nullptr;
	if (!Subsystem->TryReserveRandomSlot(
		FacilityType,
		GetOwner(),
		Facility,
		Slot,
		Number,
		bExcludeLastBath ? LastBathActor.Get() : nullptr))
	{
		WaitForFacility(FacilityType);
		return false;
	}

	StopWaitingForFacility();
	CurrentFacilityActor = Facility;
	CurrentFacilitySlot = Slot;
	CacheCurrentFacilityTransforms();
	return true;
}

bool UCustomerSessionComponent::BeginUseCurrentFacility()
{
	if (!CurrentFacilitySlot)
	{
		return false;
	}

	const bool bIsBathTransaction = CurrentFacilityActor
		&& CurrentFacilityActor->GetFacilityType() == EBathhouseFacilityType::Bath;
	if (bIsBathTransaction && !bHasCachedFacilityTransforms)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("Bathhouse customer %s cannot begin Bath use without its reservation-time facility transform snapshot."),
			*GetNameSafe(GetOwner()));
		return false;
	}
	if (!CurrentFacilitySlot->BeginUse(GetOwner()))
	{
		return false;
	}

	if (AActor* Owner = GetOwner(); Owner && !bSnappedToFacilityActionPoint)
	{
		const FRotator ActionRotation = bIsBathTransaction
			? CachedFacilityActionTransform.Rotator()
			: CurrentFacilitySlot->GetActionTransform().Rotator();
		Owner->SetActorRotation(ActionRotation);
	}
	return true;
}

bool UCustomerSessionComponent::SnapCurrentFacility(const ECustomerFacilitySnapTarget Target)
{
	return Target == ECustomerFacilitySnapTarget::ActionPoint
		? SnapToCurrentFacilityActionPoint()
		: ReturnToCurrentFacilityApproachPoint();
}

void UCustomerSessionComponent::ReleaseCurrentFacility()
{
	if (bSnappedToFacilityActionPoint && !ReturnToCurrentFacilityApproachPoint())
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("Bathhouse customer %s could not return to the cached facility approach point before release."),
			*GetNameSafe(GetOwner()));
		if (ACharacter* Character = Cast<ACharacter>(GetOwner()))
		{
			RestoreSavedMovementMode(*Character);
		}
	}
	if (CurrentFacilitySlot)
	{
		CurrentFacilitySlot->EndUse(GetOwner());
		CurrentFacilitySlot->Release(GetOwner());
	}
	CurrentFacilitySlot = nullptr;
	CurrentFacilityActor = nullptr;
	ClearCurrentFacilityTransformCache();
}

bool UCustomerSessionComponent::GetCurrentFacilityTransform(const bool bApproach, FTransform& OutTransform) const
{
	if (!CurrentFacilitySlot)
	{
		return false;
	}

	const bool bIsBathTransaction = CurrentFacilityActor
		&& CurrentFacilityActor->GetFacilityType() == EBathhouseFacilityType::Bath;
	if (bIsBathTransaction)
	{
		if (!bHasCachedFacilityTransforms)
		{
			UE_LOG(
				LogTemp,
				Error,
				TEXT("Bathhouse customer %s cannot resolve a Bath target without its reservation-time facility transform snapshot."),
				*GetNameSafe(GetOwner()));
			return false;
		}
		OutTransform = bApproach ? CachedFacilityApproachTransform : CachedFacilityActionTransform;
		return true;
	}

	OutTransform = bApproach ? CurrentFacilitySlot->GetApproachTransform() : CurrentFacilitySlot->GetActionTransform();
	return true;
}

void UCustomerSessionComponent::CacheCurrentFacilityTransforms()
{
	bHasCachedFacilityTransforms = CurrentFacilitySlot != nullptr;
	if (!bHasCachedFacilityTransforms)
	{
		CachedFacilityApproachTransform = FTransform::Identity;
		CachedFacilityActionTransform = FTransform::Identity;
		return;
	}
	CachedFacilityApproachTransform = CurrentFacilitySlot->GetApproachTransform();
	CachedFacilityActionTransform = CurrentFacilitySlot->GetActionTransform();
}

void UCustomerSessionComponent::ClearCurrentFacilityTransformCache()
{
	CachedFacilityApproachTransform = FTransform::Identity;
	CachedFacilityActionTransform = FTransform::Identity;
	bHasCachedFacilityTransforms = false;
	bHasSavedMovementMode = false;
	bSnappedToFacilityActionPoint = false;
}

bool UCustomerSessionComponent::SnapToCurrentFacilityActionPoint()
{
	if (bSnappedToFacilityActionPoint)
	{
		return true;
	}
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	UCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement() : nullptr;
	if (!CurrentFacilitySlot || !bHasCachedFacilityTransforms || !Character || !Movement)
	{
		UE_LOG(LogTemp, Error, TEXT("Bathhouse customer %s cannot snap without a cached facility and character movement."), *GetNameSafe(GetOwner()));
		return false;
	}
	if (!IsActionTransformClear(*Character))
	{
		UE_LOG(LogTemp, Error, TEXT("Bathhouse customer %s action-point capsule is blocked; snap was rejected."), *GetNameSafe(GetOwner()));
		return false;
	}

	if (AAIController* AIController = Cast<AAIController>(Character->GetController()))
	{
		AIController->StopMovement();
	}
	Movement->StopMovementImmediately();
	SavedMovementMode = Movement->MovementMode;
	SavedCustomMovementMode = Movement->CustomMovementMode;
	bHasSavedMovementMode = true;
	Movement->DisableMovement();

	const FTransform PreviousTransform = Character->GetActorTransform();
	if (!Character->SetActorLocationAndRotation(
		CachedFacilityActionTransform.GetLocation(),
		CachedFacilityActionTransform.Rotator(),
		false,
		nullptr,
		ETeleportType::TeleportPhysics))
	{
		Character->SetActorTransform(PreviousTransform, false, nullptr, ETeleportType::TeleportPhysics);
		RestoreSavedMovementMode(*Character);
		return false;
	}
	bSnappedToFacilityActionPoint = true;
	return true;
}

bool UCustomerSessionComponent::ReturnToCurrentFacilityApproachPoint()
{
	if (!bSnappedToFacilityActionPoint)
	{
		return true;
	}
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (!bHasCachedFacilityTransforms || !Character)
	{
		return false;
	}
	const bool bMoved = Character->SetActorLocationAndRotation(
		CachedFacilityApproachTransform.GetLocation(),
		CachedFacilityApproachTransform.Rotator(),
		false,
		nullptr,
		ETeleportType::TeleportPhysics);
	RestoreSavedMovementMode(*Character);
	if (bMoved)
	{
		bSnappedToFacilityActionPoint = false;
	}
	return bMoved;
}

bool UCustomerSessionComponent::IsActionTransformClear(const ACharacter& Character) const
{
	const UWorld* World = Character.GetWorld();
	const UCapsuleComponent* Capsule = Character.GetCapsuleComponent();
	if (!World || !Capsule)
	{
		return false;
	}
	const FCollisionShape CapsuleShape = FCollisionShape::MakeCapsule(
		Capsule->GetScaledCapsuleRadius(),
		Capsule->GetScaledCapsuleHalfHeight());
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(CustomerFacilityActionSnap), false, &Character);
	return !World->OverlapBlockingTestByProfile(
		CachedFacilityActionTransform.GetLocation(),
		CachedFacilityActionTransform.GetRotation(),
		Capsule->GetCollisionProfileName(),
		CapsuleShape,
		QueryParams);
}

void UCustomerSessionComponent::RestoreSavedMovementMode(ACharacter& Character)
{
	if (UCharacterMovementComponent* Movement = Character.GetCharacterMovement())
	{
		const EMovementMode MovementMode = bHasSavedMovementMode && SavedMovementMode != MOVE_None
			? SavedMovementMode.GetValue()
			: MOVE_Walking;
		const uint8 CustomMode = MovementMode == MOVE_Custom ? SavedCustomMovementMode : 0;
		Movement->SetMovementMode(MovementMode, CustomMode);
	}
	bHasSavedMovementMode = false;
}

void UCustomerSessionComponent::WaitForFacility(const EBathhouseFacilityType FacilityType)
{
	WaitingFacilityType = FacilityType;
	if (FacilityAvailabilityHandle.IsValid())
	{
		return;
	}
	if (UBathhouseFacilitySubsystem* Subsystem = GetWorld() ? GetWorld()->GetSubsystem<UBathhouseFacilitySubsystem>() : nullptr)
	{
		FacilityAvailabilityHandle = Subsystem->OnFacilityAvailabilityChanged.AddUObject(
			this,
			&UCustomerSessionComponent::HandleFacilityAvailabilityChanged);
	}
}

void UCustomerSessionComponent::StopWaitingForFacility()
{
	if (!FacilityAvailabilityHandle.IsValid())
	{
		return;
	}
	if (UBathhouseFacilitySubsystem* Subsystem = GetWorld() ? GetWorld()->GetSubsystem<UBathhouseFacilitySubsystem>() : nullptr)
	{
		Subsystem->OnFacilityAvailabilityChanged.Remove(FacilityAvailabilityHandle);
	}
	FacilityAvailabilityHandle.Reset();
}

float UCustomerSessionComponent::BeginActivity(const EBathhouseCustomerActivity Activity)
{
	if (!RoutineDefinition || CurrentActivity != EBathhouseCustomerActivity::None || !BeginUseCurrentFacility())
	{
		return -1.0f;
	}
	CurrentActivity = Activity;
	if (ABathhouseCustomerCharacter* Customer = Cast<ABathhouseCustomerCharacter>(GetOwner()))
	{
		Customer->NotifyActivityStarted(Activity);
	}
	SetPresentationState(Activity == EBathhouseCustomerActivity::BathDwell
		? EBathhouseCustomerPresentationState::Bathing
		: EBathhouseCustomerPresentationState::UsingFacility);
	return FMath::Max(0.0f, RoutineDefinition->GetActivityDuration(Activity));
}

void UCustomerSessionComponent::FinishActivity(const EBathhouseCustomerActivity Activity)
{
	if (CurrentActivity != Activity)
	{
		return;
	}
	if (ABathhouseCustomerCharacter* Customer = Cast<ABathhouseCustomerCharacter>(GetOwner()))
	{
		Customer->NotifyActivityFinished(Activity);
	}
	if (Activity == EBathhouseCustomerActivity::BathDwell)
	{
		LastBathActor = CurrentFacilityActor;
	}
	CurrentActivity = EBathhouseCustomerActivity::None;
	if (Activity == EBathhouseCustomerActivity::PreShower)
	{
		StartBathStay();
	}
}

void UCustomerSessionComponent::AbortActivity(
	const EBathhouseCustomerActivity Activity,
	const bool bReleaseFacility)
{
	if (CurrentActivity == Activity)
	{
		CurrentActivity = EBathhouseCustomerActivity::None;
	}
	if (bReleaseFacility)
	{
		ReleaseCurrentFacility();
	}
}

bool UCustomerSessionComponent::StartBathStay()
{
	if (bBathStayStarted || !RoutineDefinition || !GetWorld())
	{
		return bBathStayStarted;
	}
	bBathStayStarted = true;
	bBathStayExpired = false;
	const float Duration = FMath::Max(RoutineDefinition->BathStayDurationSeconds, 0.1f);
	BathStayEndTime = GetWorld()->GetTimeSeconds() + Duration;
	GetWorld()->GetTimerManager().SetTimer(
		BathStayTimerHandle,
		this,
		&UCustomerSessionComponent::HandleBathStayExpired,
		Duration,
		false);
	return true;
}

float UCustomerSessionComponent::GetRemainingBathStaySeconds() const
{
	return bBathStayStarted && !bBathStayExpired && GetWorld()
		? FMath::Max(0.0, BathStayEndTime - GetWorld()->GetTimeSeconds())
		: 0.0f;
}

bool UCustomerSessionComponent::BeginCheckoutOffer()
{
	if (bCheckoutOfferActive)
	{
		return true;
	}
	if (bFinished || bCleanupInProgress || bCashClaimed || QueueLane != EBathhouseCounterLane::Checkout || !IsQueueFront())
	{
		return false;
	}
	bCheckoutOfferActive = true;
	return true;
}

void UCustomerSessionComponent::EndCheckoutOffer()
{
	bCheckoutOfferActive = false;
}

bool UCustomerSessionComponent::TryPlaceCheckoutKey()
{
	if (!Counter || !AssignedKey)
	{
		return false;
	}
	if (AssignedKey->GetKeyState() == EBathhouseKeyState::OnCounter)
	{
		return true;
	}

	FTransform SlotTransform;
	if (!Counter->TryReserveReturnedObjectSlot(GetOwner(), ReturnSlotIndex, SlotTransform))
	{
		return false;
	}
	USceneComponent* ReturnSlot = Counter->GetReturnSlotComponent(ReturnSlotIndex);
	if (!ReturnSlot || !AssignedKey->TryPlaceOnCounter(*GetOwner(), *Counter, ReturnSlotIndex, *ReturnSlot))
	{
		Counter->ReleaseReturnedObjectReservation(GetOwner(), ReturnSlotIndex);
		ReturnSlotIndex = INDEX_NONE;
		return false;
	}
	return true;
}

bool UCustomerSessionComponent::TryCreateCashOffer(TSubclassOf<ABathhouseCashPaymentActor> CashClass)
{
	if (bCashClaimed)
	{
		return true;
	}
	if (CashOffer)
	{
		return true;
	}
	if (!Counter || !AssignedKey || AssignedKey->GetKeyState() != EBathhouseKeyState::OnCounter || !CashClass || !GetWorld())
	{
		return false;
	}

	const USceneComponent* OfferPoint = Counter->GetCashOfferPoint();
	const FTransform SpawnTransform = OfferPoint ? OfferPoint->GetComponentTransform() : Counter->GetActorTransform();
	ABathhouseCashPaymentActor* SpawnedCash = GetWorld()->SpawnActorDeferred<ABathhouseCashPaymentActor>(
		CashClass,
		SpawnTransform,
		GetOwner(),
		nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!SpawnedCash)
	{
		return false;
	}
	SpawnedCash->ConfigurePaymentAmount(RoutineDefinition ? RoutineDefinition->UsageFee : 10000);
	SpawnedCash->OnCashClaimed.AddDynamic(this, &UCustomerSessionComponent::HandleCashClaimed);
	CashOffer = Cast<ABathhouseCashPaymentActor>(UGameplayStatics::FinishSpawningActor(SpawnedCash, SpawnTransform));
	SetPresentationState(EBathhouseCustomerPresentationState::OfferingPayment);
	return CashOffer != nullptr;
}

void UCustomerSessionComponent::CancelCashOffer()
{
	if (CashOffer && !CashOffer->IsClaimed())
	{
		CashOffer->OnCashClaimed.RemoveDynamic(this, &UCustomerSessionComponent::HandleCashClaimed);
		CashOffer->Destroy();
	}
	CashOffer = nullptr;
}

bool UCustomerSessionComponent::RegisterNavigationFailure()
{
	++NavigationFailureCount;
	const int32 MaxRetries = RoutineDefinition ? RoutineDefinition->MaxNavigationRetries : 0;
	if (NavigationFailureCount >= FMath::Max(1, MaxRetries))
	{
		TechnicalAbort(FString::Printf(TEXT("Navigation retry exhaustion after %d failures."), NavigationFailureCount));
		return true;
	}
	return false;
}

void UCustomerSessionComponent::FinishSession(const EBathhouseCustomerDepartureReason Reason)
{
	if (bFinished || bCleanupInProgress)
	{
		return;
	}
	bCleanupInProgress = true;
	DepartureReason = Reason == EBathhouseCustomerDepartureReason::None ? DepartureReason : Reason;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(CheckInTimeoutHandle);
		World->GetTimerManager().ClearTimer(BathStayTimerHandle);
	}
	StopWaitingForFacility();
	ReleaseCurrentFacility();
	LeaveQueue();
	if (CashOffer && !CashOffer->IsClaimed())
	{
		CancelCashOffer();
	}
	CashOffer = nullptr;
	if (DepartureReason == EBathhouseCustomerDepartureReason::TechnicalAbort && AssignedKey
		&& AssignedKey->GetKeyState() != EBathhouseKeyState::AtHook)
	{
		AssignedKey->RecoverToHook();
	}
	bFinished = true;
	bCleanupInProgress = false;
	SetPresentationState(EBathhouseCustomerPresentationState::Leaving);
	if (ABathhouseCustomerCharacter* Customer = Cast<ABathhouseCustomerCharacter>(GetOwner()))
	{
		Customer->NotifyCustomerFinished(DepartureReason);
		Customer->SetLifeSpan(0.1f);
	}
}

void UCustomerSessionComponent::TechnicalAbort(const FString& ErrorMessage)
{
	if (DepartureReason == EBathhouseCustomerDepartureReason::TechnicalAbort || bFinished)
	{
		return;
	}
	UE_LOG(LogTemp, Error, TEXT("Bathhouse customer %s technical abort: %s"), *GetNameSafe(GetOwner()), *ErrorMessage);
	DepartureReason = EBathhouseCustomerDepartureReason::TechnicalAbort;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(CheckInTimeoutHandle);
		World->GetTimerManager().ClearTimer(BathStayTimerHandle);
	}
	StopWaitingForFacility();
	ReleaseCurrentFacility();
	LeaveQueue();
	if (CashOffer && !CashOffer->IsClaimed())
	{
		CancelCashOffer();
	}
	CashOffer = nullptr;
	if (AssignedKey && AssignedKey->GetKeyState() != EBathhouseKeyState::AtHook)
	{
		AssignedKey->RecoverToHook();
	}
	SetPresentationState(EBathhouseCustomerPresentationState::Leaving);
}

void UCustomerSessionComponent::SendCustomerEvent(const FGameplayTag& EventTag) const
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	const AController* Controller = Pawn ? Pawn->GetController() : nullptr;
	if (UStateTreeAIComponent* StateTree = Controller ? Controller->FindComponentByClass<UStateTreeAIComponent>() : nullptr)
	{
		StateTree->SendStateTreeEvent(EventTag);
	}
}

void UCustomerSessionComponent::SetPresentationState(const EBathhouseCustomerPresentationState NewState) const
{
	if (ABathhouseCustomerCharacter* Customer = Cast<ABathhouseCustomerCharacter>(GetOwner()))
	{
		Customer->NotifyPresentationState(NewState);
	}
}

void UCustomerSessionComponent::HandleCheckInTimeout()
{
	if (bCheckInTerminalCommitted || !bWaitingForCheckIn)
	{
		return;
	}
	bCheckInTerminalCommitted = true;
	bWaitingForCheckIn = false;
	bCheckInTimedOut = true;
	DepartureReason = EBathhouseCustomerDepartureReason::CheckInTimedOut;
	LeaveQueue();
	SetPresentationState(EBathhouseCustomerPresentationState::Leaving);
	SendCustomerEvent(TAG_Customer_Event_CheckInTimedOut);
}

void UCustomerSessionComponent::HandleBathStayExpired()
{
	if (bBathStayExpired)
	{
		return;
	}
	bBathStayExpired = true;
	SendCustomerEvent(TAG_Customer_Event_BathStayExpired);
}

void UCustomerSessionComponent::HandleFacilityAvailabilityChanged(const EBathhouseFacilityType FacilityType)
{
	if (FacilityType == WaitingFacilityType)
	{
		SendCustomerEvent(TAG_Customer_Event_FacilityAvailable);
	}
}

void UCustomerSessionComponent::HandleQueueChanged(const EBathhouseCounterLane ChangedLane)
{
	if (ShouldForwardQueueChangedEvent(ChangedLane))
	{
		SendCustomerEvent(TAG_Customer_Event_QueueChanged);
	}
}

bool UCustomerSessionComponent::ShouldForwardQueueChangedEvent(const EBathhouseCounterLane ChangedLane) const
{
	return ChangedLane != EBathhouseCounterLane::None
		&& ChangedLane == QueueLane
		&& !bWaitingForCheckIn
		&& (ChangedLane != EBathhouseCounterLane::CheckIn || !bCheckInTerminalCommitted)
		&& !bCheckoutOfferActive
		&& (ChangedLane != EBathhouseCounterLane::Checkout || !bCashClaimed)
		&& !bCleanupInProgress
		&& !bFinished;
}

void UCustomerSessionComponent::HandleCashClaimed(ABathhouseCashPaymentActor* CashActor)
{
	if (bCashClaimed || CashActor != CashOffer)
	{
		return;
	}
	bCashClaimed = true;
	CashOffer = nullptr;
	EndCheckoutOffer();
	LeaveQueue();
	SetPresentationState(EBathhouseCustomerPresentationState::Leaving);
	SendCustomerEvent(TAG_Customer_Event_CashClaimed);
}

#undef LOCTEXT_NAMESPACE
