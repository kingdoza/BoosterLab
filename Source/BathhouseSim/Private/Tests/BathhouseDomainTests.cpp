#if WITH_DEV_AUTOMATION_TESTS

#include "Tests/BathhouseDomainTestProbe.h"

#include "Animation/AnimMontage.h"
#include "Components/BoxComponent.h"
#include "Facility/BathhouseCounterActor.h"
#include "Facility/BathhouseFacilityActor.h"
#include "Facility/BathhouseFacilitySlotComponent.h"
#include "Interaction/BathhouseKeyActor.h"
#include "Interaction/BathhouseKeyHookActor.h"
#include "Interaction/PlayerCarryComponent.h"
#include "Interaction/PlayerInteractionComponent.h"
#include "Customer/CustomerSessionComponent.h"
#include "Customer/BathhouseCustomerCharacter.h"
#include "Customer/CustomerMontagePlaybackComponent.h"
#include "Customer/StateTree/CustomerStateTreeTasks.h"
#include "Components/SceneComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBathhouseCustomerMontageTest,
	"BathhouseSim.Customer.MontageSelectionAndToken",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBathhouseCustomerMontageTest::RunTest(const FString& Parameters)
{
	UAnimMontage* FirstMontage = NewObject<UAnimMontage>();
	UAnimMontage* SecondMontage = NewObject<UAnimMontage>();
	FRandomStream RandomStream(1729);
	const int32 InitialSeed = RandomStream.GetCurrentSeed();

	AddExpectedError(TEXT("no valid montage candidates"), EAutomationExpectedErrorFlags::Contains, 1);
	TestNull(TEXT("A null-only candidate list is rejected"),
		BathhouseCustomerMontageTasks::SelectMontageCandidate({nullptr}, &RandomStream));
	TestEqual(TEXT("A failed empty selection does not consume random state"), RandomStream.GetCurrentSeed(), InitialSeed);

	TestEqual(TEXT("One valid candidate is selected even among null entries"),
		BathhouseCustomerMontageTasks::SelectMontageCandidate({nullptr, FirstMontage, nullptr}, &RandomStream),
		FirstMontage);
	TestEqual(TEXT("A single valid candidate does not consume random state"), RandomStream.GetCurrentSeed(), InitialSeed);

	UAnimMontage* RandomSelection = BathhouseCustomerMontageTasks::SelectMontageCandidate(
		{nullptr, FirstMontage, SecondMontage},
		&RandomStream);
	TestTrue(TEXT("Multiple valid candidates select exactly one valid montage"),
		RandomSelection == FirstMontage || RandomSelection == SecondMontage);
	TestTrue(TEXT("Multiple candidates consume the supplied random stream"), RandomStream.GetCurrentSeed() != InitialSeed);

	UCustomerMontagePlaybackComponent* Playback = NewObject<UCustomerMontagePlaybackComponent>();
	Playback->CurrentPlaybackToken = 41;
	Playback->CurrentMontage = FirstMontage;
	Playback->CurrentResult = ECustomerMontagePlaybackResult::Playing;
	TestFalse(TEXT("A stale token cannot stop the current montage"), Playback->StopPlayback(40, 0.0f));
	TestEqual(TEXT("A stale stop leaves playback running"),
		Playback->GetPlaybackResult(41), ECustomerMontagePlaybackResult::Playing);
	TestTrue(TEXT("The owning token can interrupt its montage"), Playback->StopPlayback(41, 0.0f));
	TestEqual(TEXT("An owning stop records interruption"),
		Playback->GetPlaybackResult(41), ECustomerMontagePlaybackResult::Interrupted);

	Playback->ArchiveCurrentPlayback();
	Playback->CurrentPlaybackToken = 42;
	Playback->CurrentMontage = FirstMontage;
	Playback->CurrentResult = ECustomerMontagePlaybackResult::Playing;
	TestEqual(TEXT("A replaced token retains its interrupted result"),
		Playback->GetPlaybackResult(41), ECustomerMontagePlaybackResult::Interrupted);
	Playback->HandleMontageEnded(FirstMontage, false, 42);
	TestEqual(TEXT("A natural montage end records success"),
		Playback->GetPlaybackResult(42), ECustomerMontagePlaybackResult::Succeeded);

	Playback->ArchiveCurrentPlayback();
	Playback->CurrentPlaybackToken = 43;
	Playback->CurrentMontage = SecondMontage;
	Playback->CurrentResult = ECustomerMontagePlaybackResult::Playing;
	Playback->HandleMontageEnded(SecondMontage, true, 43);
	TestEqual(TEXT("An interrupted montage end records failure state"),
		Playback->GetPlaybackResult(43), ECustomerMontagePlaybackResult::Interrupted);

	TestEqual(TEXT("One-shot remains running while its token is playing"),
		FPlayCustomerMontageOnceTask::ResolvePlaybackStatus(ECustomerMontagePlaybackResult::Playing),
		EStateTreeRunStatus::Running);
	TestEqual(TEXT("One-shot succeeds only after a natural montage end"),
		FPlayCustomerMontageOnceTask::ResolvePlaybackStatus(ECustomerMontagePlaybackResult::Succeeded),
		EStateTreeRunStatus::Succeeded);
	TestEqual(TEXT("One-shot fails after montage interruption"),
		FPlayCustomerMontageOnceTask::ResolvePlaybackStatus(ECustomerMontagePlaybackResult::Interrupted),
		EStateTreeRunStatus::Failed);
	TestEqual(TEXT("Loop remains running with duration left and owned playback"),
		FPlaySelectedMontageLoopForDurationTask::ResolvePlaybackStatus(
			ECustomerMontagePlaybackResult::Playing, 0.5f),
		EStateTreeRunStatus::Running);
	TestEqual(TEXT("Loop succeeds when duration expires while playback remains owned"),
		FPlaySelectedMontageLoopForDurationTask::ResolvePlaybackStatus(
			ECustomerMontagePlaybackResult::Playing, 0.0f),
		EStateTreeRunStatus::Succeeded);
	TestEqual(TEXT("Loop fails if the montage ends before duration"),
		FPlaySelectedMontageLoopForDurationTask::ResolvePlaybackStatus(
			ECustomerMontagePlaybackResult::Succeeded, 0.5f),
		EStateTreeRunStatus::Failed);
	TestEqual(TEXT("Loop fails if playback is interrupted before duration"),
		FPlaySelectedMontageLoopForDurationTask::ResolvePlaybackStatus(
			ECustomerMontagePlaybackResult::Interrupted, 0.5f),
		EStateTreeRunStatus::Failed);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBathhouseCustomerBathSnapTest,
	"BathhouseSim.Customer.BathSnapCleanup",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBathhouseCustomerBathSnapTest::RunTest(const FString& Parameters)
{
	if (!GEngine)
	{
		AddError(TEXT("GEngine is required for the Bath snap world test."));
		return false;
	}

	const FName WorldName = MakeUniqueObjectName(nullptr, UWorld::StaticClass(), TEXT("BathSnapAutomationWorld"));
	FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, WorldName, GetTransientPackage());
	if (!World)
	{
		GEngine->DestroyWorldContext(World);
		AddError(TEXT("Failed to create the Bath snap automation world."));
		return false;
	}
	World->AddToRoot();
	WorldContext.SetCurrentWorld(World);
	World->InitializeActorsForPlay(FURL());

	const FTransform SpawnTransform(FRotator::ZeroRotator, FVector::ZeroVector);
	ABathhouseCustomerCharacter* Customer = World->SpawnActorDeferred<ABathhouseCustomerCharacter>(
		ABathhouseCustomerCharacter::StaticClass(),
		SpawnTransform,
		nullptr,
		nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	Customer->AutoPossessAI = EAutoPossessAI::Disabled;
	UGameplayStatics::FinishSpawningActor(Customer, SpawnTransform);
	ABathhouseFacilityActor* Facility = World->SpawnActor<ABathhouseFacilityActor>();
	UBathhouseFacilitySlotComponent* Slot = NewObject<UBathhouseFacilitySlotComponent>(Facility, TEXT("BathSnapTestSlot"));
	Facility->AddInstanceComponent(Slot);
	Slot->RegisterComponent();

	UCustomerSessionComponent* Session = Customer->GetCustomerSession();
	struct FFacilitySnapshot
	{
		FTransform Approach;
		FTransform Action;
	};
	const auto ReserveFacility = [&](const FTransform& ReservationTransform)
	{
		Slot->SetWorldTransform(ReservationTransform);
		TestTrue(TEXT("The test customer reserves the facility slot"), Slot->TryReserve(Customer));
		Session->CurrentFacilityActor = Facility;
		Session->CurrentFacilitySlot = Slot;
		Session->CacheCurrentFacilityTransforms();
		const FFacilitySnapshot Snapshot{
			Session->CachedFacilityApproachTransform,
			Session->CachedFacilityActionTransform};
		Customer->SetActorTransform(Snapshot.Approach, false, nullptr, ETeleportType::TeleportPhysics);
		Customer->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
		return Snapshot;
	};
	FEnumProperty* FacilityTypeProperty = FindFProperty<FEnumProperty>(
		ABathhouseFacilityActor::StaticClass(),
		TEXT("FacilityType"));
	TestNotNull(TEXT("Facility type property is available for non-Bath regression setup"), FacilityTypeProperty);
	const auto SetFacilityType = [&](const EBathhouseFacilityType FacilityType)
	{
		if (!FacilityTypeProperty)
		{
			return false;
		}
		void* ValueAddress = FacilityTypeProperty->ContainerPtrToValuePtr<void>(Facility);
		FacilityTypeProperty->GetUnderlyingProperty()->SetIntPropertyValue(
			ValueAddress,
			static_cast<int64>(FacilityType));
		return Facility->GetFacilityType() == FacilityType;
	};

	const FTransform FirstReservationTransform(
		FRotator(0.0f, 35.0f, 0.0f),
		FVector(1000.0f, 0.0f, 100.0f));
	const FTransform FirstLiveMutation(
		FRotator(0.0f, -70.0f, 0.0f),
		FVector(1450.0f, 350.0f, 130.0f));

	const FFacilitySnapshot FirstSnapshot = ReserveFacility(FirstReservationTransform);
	Slot->SetWorldTransform(FirstLiveMutation);
	FTransform NavigationTarget;
	TestTrue(TEXT("Bath approach target resolves after the live slot changes"),
		Session->GetCurrentFacilityTransform(true, NavigationTarget));
	TestTrue(TEXT("Bath navigation keeps the reservation-time approach transform"),
		NavigationTarget.Equals(FirstSnapshot.Approach));
	FTransform ActionTarget;
	TestTrue(TEXT("Bath action target resolves after the live slot changes"),
		Session->GetCurrentFacilityTransform(false, ActionTarget));
	TestTrue(TEXT("Bath action target keeps the reservation-time action transform"),
		ActionTarget.Equals(FirstSnapshot.Action));
	TestTrue(TEXT("The customer snaps from approach to the cached action point"),
		Session->SnapCurrentFacility(ECustomerFacilitySnapTarget::ActionPoint));
	TestTrue(TEXT("Action snap applies the reservation-time action location and rotation"),
		Customer->GetActorTransform().Equals(FirstSnapshot.Action));
	TestEqual(TEXT("Action snap disables character movement"),
		Customer->GetCharacterMovement()->MovementMode, MOVE_None);
	const FRotator SnappedRotation = Customer->GetActorRotation();
	TestTrue(TEXT("The snapped Bath reservation enters occupied use"), Session->BeginUseCurrentFacility());
	TestTrue(TEXT("Begin use does not replace the cached action rotation with the live slot rotation"),
		Customer->GetActorRotation().Equals(SnappedRotation));
	Session->ReleaseCurrentFacility();
	TestTrue(TEXT("Release returns the customer to the original cached approach before clearing the slot"),
		Customer->GetActorTransform().Equals(FirstSnapshot.Approach));
	TestEqual(TEXT("Approach return restores walking movement"),
		Customer->GetCharacterMovement()->MovementMode, MOVE_Walking);
	TestTrue(TEXT("Normal cleanup releases the slot"), Slot->IsAvailable());
	Session->ReleaseCurrentFacility();
	TestTrue(TEXT("Repeated cleanup remains idempotent"), Slot->IsAvailable());

	const FTransform BlockedReservationTransform(
		FRotator(0.0f, 15.0f, 0.0f),
		FVector(2100.0f, 0.0f, 100.0f));
	const FFacilitySnapshot BlockedSnapshot = ReserveFacility(BlockedReservationTransform);
	AActor* Obstacle = World->SpawnActor<AActor>();
	UBoxComponent* BlockingBox = NewObject<UBoxComponent>(Obstacle, TEXT("BathSnapBlockingBox"));
	Obstacle->SetRootComponent(BlockingBox);
	Obstacle->AddInstanceComponent(BlockingBox);
	BlockingBox->SetBoxExtent(FVector(20.0f));
	BlockingBox->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	BlockingBox->RegisterComponent();
	BlockingBox->SetWorldLocation(BlockedSnapshot.Action.GetLocation());
	BlockingBox->UpdateOverlaps();
	const FTransform BeforeBlockedSnap = Customer->GetActorTransform();
	AddExpectedError(TEXT("action-point capsule is blocked"), EAutomationExpectedErrorFlags::Contains, 1);
	TestFalse(TEXT("A blocked action point rejects the snap"),
		Session->SnapCurrentFacility(ECustomerFacilitySnapTarget::ActionPoint));
	TestEqual(TEXT("A rejected snap does not move the customer"), Customer->GetActorTransform(), BeforeBlockedSnap);
	TestEqual(TEXT("A rejected snap leaves walking enabled"),
		Customer->GetCharacterMovement()->MovementMode, MOVE_Walking);
	Session->ReleaseCurrentFacility();
	BlockingBox->DestroyComponent();
	Obstacle->Destroy();

	const FTransform MissingSnapshotReservationTransform(
		FRotator(0.0f, 5.0f, 0.0f),
		FVector(2700.0f, 0.0f, 100.0f));
	ReserveFacility(MissingSnapshotReservationTransform);
	Session->ClearCurrentFacilityTransformCache();
	FTransform MissingSnapshotTarget;
	AddExpectedError(TEXT("cannot resolve a Bath target without its reservation-time facility transform snapshot"),
		EAutomationExpectedErrorFlags::Contains, 1);
	TestFalse(TEXT("A Bath navigation target fails clearly when its reservation snapshot is unavailable"),
		Session->GetCurrentFacilityTransform(true, MissingSnapshotTarget));
	AddExpectedError(TEXT("cannot begin Bath use without its reservation-time facility transform snapshot"),
		EAutomationExpectedErrorFlags::Contains, 1);
	TestFalse(TEXT("Bath use fails before occupancy when its reservation snapshot is unavailable"),
		Session->BeginUseCurrentFacility());
	TestEqual(TEXT("A missing Bath snapshot leaves the slot reserved rather than occupied"),
		Slot->GetSlotState(), EBathhouseFacilitySlotState::Reserved);
	Session->ReleaseCurrentFacility();

	TestTrue(TEXT("Facility switches to a non-Bath type for legacy-path regression"),
		SetFacilityType(EBathhouseFacilityType::Shower));
	const FTransform NonBathReservationTransform(
		FRotator(0.0f, 25.0f, 0.0f),
		FVector(3300.0f, 0.0f, 100.0f));
	const FFacilitySnapshot NonBathSnapshot = ReserveFacility(NonBathReservationTransform);
	const FTransform NonBathLiveMutation(
		FRotator(0.0f, 105.0f, 0.0f),
		FVector(3650.0f, -250.0f, 100.0f));
	Slot->SetWorldTransform(NonBathLiveMutation);
	FTransform NonBathTarget;
	TestTrue(TEXT("A non-Bath facility target still resolves from the live slot"),
		Session->GetCurrentFacilityTransform(true, NonBathTarget));
	TestTrue(TEXT("The non-Bath path preserves its prior live-transform behavior"),
		NonBathTarget.Equals(Slot->GetApproachTransform()));
	TestFalse(TEXT("The non-Bath target does not use the reservation-time snapshot"),
		NonBathTarget.Equals(NonBathSnapshot.Approach));
	Customer->SetActorTransform(NonBathTarget, false, nullptr, ETeleportType::TeleportPhysics);
	const FVector NonBathUseLocation = Customer->GetActorLocation();
	TestTrue(TEXT("The unsnapped non-Bath reservation enters occupied use"), Session->BeginUseCurrentFacility());
	TestFalse(TEXT("The non-Bath activity path remains unsnapped"), Session->IsSnappedToFacilityActionPoint());
	TestTrue(TEXT("Non-Bath begin use keeps the live action facing"),
		Customer->GetActorRotation().Equals(Slot->GetActionTransform().Rotator()));
	TestEqual(TEXT("Non-Bath begin use does not teleport the customer"),
		Customer->GetActorLocation(), NonBathUseLocation);
	TestEqual(TEXT("Non-Bath begin use leaves walking enabled"),
		Customer->GetCharacterMovement()->MovementMode, MOVE_Walking);
	Session->ReleaseCurrentFacility();
	TestEqual(TEXT("Non-Bath release does not apply the Bath return path"),
		Customer->GetActorLocation(), NonBathUseLocation);
	TestTrue(TEXT("Non-Bath cleanup releases the slot"), Slot->IsAvailable());
	TestTrue(TEXT("Facility switches back to Bath for technical-abort regression"),
		SetFacilityType(EBathhouseFacilityType::Bath));

	const FTransform AbortReservationTransform(
		FRotator(0.0f, -30.0f, 0.0f),
		FVector(4200.0f, 0.0f, 100.0f));
	const FFacilitySnapshot AbortSnapshot = ReserveFacility(AbortReservationTransform);
	Slot->SetWorldTransform(FTransform(
		FRotator(0.0f, 160.0f, 0.0f),
		FVector(4700.0f, 500.0f, 140.0f)));
	TestTrue(TEXT("Technical-abort setup reaches the action point"),
		Session->SnapCurrentFacility(ECustomerFacilitySnapTarget::ActionPoint));
	TestTrue(TEXT("Technical-abort setup begins Bath use"), Session->BeginUseCurrentFacility());
	AddExpectedError(TEXT("technical abort: automation cleanup"), EAutomationExpectedErrorFlags::Contains, 1);
	Session->TechnicalAbort(TEXT("automation cleanup"));
	TestTrue(TEXT("Technical abort returns to the original cached approach transform"),
		Customer->GetActorTransform().Equals(AbortSnapshot.Approach));
	TestEqual(TEXT("Technical abort restores walking movement"),
		Customer->GetCharacterMovement()->MovementMode, MOVE_Walking);
	TestTrue(TEXT("Technical abort releases the facility slot"), Slot->IsAvailable());

	World->DestroyWorld(false);
	GEngine->DestroyWorldContext(World);
	World->RemoveFromRoot();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBathhouseInteractionAttemptResultTest,
	"BathhouseSim.Interaction.AttemptResult",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBathhouseInteractionAttemptResultTest::RunTest(const FString& Parameters)
{
	UPlayerInteractionComponent* Interaction = NewObject<UPlayerInteractionComponent>();
	int32 BroadcastCount = 0;
	FPlayerInteractionResult ObservedResult;
	const FDelegateHandle ResultHandle = Interaction->OnInteractionAttemptFinishedNative.AddLambda(
		[&](const FPlayerInteractionResult& Result)
		{
			++BroadcastCount;
			ObservedResult = Result;
		});

	const FPlayerInteractionResult NoTargetResult = Interaction->TryInteract();
	TestEqual(TEXT("A no-target attempt broadcasts exactly once"), BroadcastCount, 1);
	TestFalse(TEXT("A no-target attempt remains failed"), NoTargetResult.bSucceeded);
	TestTrue(TEXT("The delegate preserves the no-target failure reason"),
		ObservedResult.FailureReason.EqualTo(NoTargetResult.FailureReason));

	BroadcastCount = 0;
	const FPlayerInteractionResult SuccessfulResult = Interaction->FinishInteractionAttempt(FPlayerInteractionResult::Succeeded());
	TestEqual(TEXT("A completed success broadcasts exactly once"), BroadcastCount, 1);
	TestTrue(TEXT("The completion helper preserves success"), SuccessfulResult.bSucceeded);
	TestTrue(TEXT("The delegate receives the successful result"), ObservedResult.bSucceeded);

	Interaction->OnInteractionAttemptFinishedNative.Remove(ResultHandle);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBathhouseSingleKeyCarryTest,
	"BathhouseSim.Interaction.SingleKeyCarry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBathhouseSingleKeyCarryTest::RunTest(const FString& Parameters)
{
	UPlayerCarryComponent* Carry = NewObject<UPlayerCarryComponent>();
	ABathhouseKeyActor* FirstKey = NewObject<ABathhouseKeyActor>();
	ABathhouseKeyActor* SecondKey = NewObject<ABathhouseKeyActor>();

	TestTrue(TEXT("An empty hand accepts the first key"), Carry->CommitTakeKey(FirstKey));
	TestFalse(TEXT("A hand already holding a key rejects another key"), Carry->CommitTakeKey(SecondKey));
	TestFalse(TEXT("A mismatched release cannot clear the held key"), Carry->CommitReleaseKey(SecondKey));
	TestEqual(TEXT("The original key remains held"), Carry->GetHeldKey(), FirstKey);
	TestTrue(TEXT("The matching key can be released"), Carry->CommitReleaseKey(FirstKey));
	TestTrue(TEXT("The hand is empty after the matching release"), Carry->IsHandEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBathhouseKeyRecoveryTest,
	"BathhouseSim.Interaction.KeyRecovery",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBathhouseKeyRecoveryTest::RunTest(const FString& Parameters)
{
	ABathhouseKeyHookActor* AssignedHook = NewObject<ABathhouseKeyHookActor>();
	ABathhouseKeyActor* AssignedKey = NewObject<ABathhouseKeyActor>();
	AActor* Customer = NewObject<AActor>();
	AActor* UnexpectedOwner = NewObject<AActor>();
	TestTrue(TEXT("Assigned key initializes at its hook"), AssignedKey->InitializeAtHook(AssignedHook));
	AssignedKey->CommitState(EBathhouseKeyState::AssignedToCustomer, Customer);
	AssignedKey->RecoverToHook(UnexpectedOwner);
	TestEqual(TEXT("A mismatched recovery guard preserves assigned state"), AssignedKey->GetKeyState(), EBathhouseKeyState::AssignedToCustomer);
	AssignedKey->RecoverToHook(Customer);
	TestEqual(TEXT("Assigned customer key recovers to its hook"), AssignedKey->GetKeyState(), EBathhouseKeyState::AtHook);

	ABathhouseKeyHookActor* CounterHook = NewObject<ABathhouseKeyHookActor>();
	ABathhouseKeyActor* CounterKey = NewObject<ABathhouseKeyActor>();
	ABathhouseCounterActor* Counter = NewObject<ABathhouseCounterActor>();
	USceneComponent* ReturnPoint = NewObject<USceneComponent>(Counter);
	TestTrue(TEXT("Counter key initializes at its hook"), CounterKey->InitializeAtHook(CounterHook));
	FBathhouseReturnedObjectSlot& ReturnedSlot = Counter->RuntimeReturnedSlots.AddDefaulted_GetRef();
	ReturnedSlot.Point = ReturnPoint;
	ReturnedSlot.ReservationOwner = Customer;
	ReturnedSlot.ReturnedObject = CounterKey;
	CounterKey->CounterOwner = Counter;
	CounterKey->CounterReturnSlotIndex = 0;
	CounterKey->CommitState(EBathhouseKeyState::OnCounter, Counter);
	CounterKey->RecoverToHook();
	TestEqual(TEXT("Counter key recovers to its hook"), CounterKey->GetKeyState(), EBathhouseKeyState::AtHook);
	TestNull(TEXT("Recovery clears the returned object slot"), ReturnedSlot.ReturnedObject.Get());
	TestNull(TEXT("Recovery clears the returned slot reservation"), ReturnedSlot.ReservationOwner.Get());

	ABathhouseKeyHookActor* HeldHook = NewObject<ABathhouseKeyHookActor>();
	ABathhouseKeyActor* HeldKey = NewObject<ABathhouseKeyActor>();
	UPlayerCarryComponent* Carry = NewObject<UPlayerCarryComponent>();
	TestTrue(TEXT("Held key initializes at its hook"), HeldKey->InitializeAtHook(HeldHook));
	TestTrue(TEXT("Carry accepts the recovery test key"), Carry->CommitTakeKey(HeldKey));
	HeldKey->CommitState(EBathhouseKeyState::HeldByPlayer, Carry);
	HeldKey->RecoverToHook();
	TestTrue(TEXT("Held-key recovery clears the actual carry owner"), Carry->IsHandEmpty());
	TestEqual(TEXT("Held key recovers to its hook"), HeldKey->GetKeyState(), EBathhouseKeyState::AtHook);
	HeldKey->RecoverToHook();
	TestEqual(TEXT("Repeated recovery is idempotent"), HeldKey->GetKeyState(), EBathhouseKeyState::AtHook);

	ABathhouseKeyHookActor* EndingHook = NewObject<ABathhouseKeyHookActor>();
	ABathhouseKeyActor* EndingKey = NewObject<ABathhouseKeyActor>();
	UPlayerCarryComponent* EndingCarry = NewObject<UPlayerCarryComponent>();
	TestTrue(TEXT("Ending key initializes at its hook"), EndingKey->InitializeAtHook(EndingHook));
	TestTrue(TEXT("Carry accepts the ending key"), EndingCarry->CommitTakeKey(EndingKey));
	EndingKey->CommitState(EBathhouseKeyState::HeldByPlayer, EndingCarry);
	EndingKey->EndPlay(EEndPlayReason::Destroyed);
	TestTrue(TEXT("Key EndPlay clears the carry reference"), EndingCarry->IsHandEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBathhouseFacilitySlotExclusivityTest,
	"BathhouseSim.Facility.SlotExclusivity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBathhouseFacilitySlotExclusivityTest::RunTest(const FString& Parameters)
{
	UBathhouseFacilitySlotComponent* Slot = NewObject<UBathhouseFacilitySlotComponent>();
	AActor* FirstCustomer = NewObject<AActor>();
	AActor* SecondCustomer = NewObject<AActor>();

	TestTrue(TEXT("First customer reserves the slot"), Slot->TryReserve(FirstCustomer));
	TestTrue(TEXT("Reservation is idempotent for the same customer"), Slot->TryReserve(FirstCustomer));
	TestFalse(TEXT("Second customer cannot reserve the slot"), Slot->TryReserve(SecondCustomer));
	TestTrue(TEXT("Reservation owner begins use"), Slot->BeginUse(FirstCustomer));
	TestFalse(TEXT("Second customer cannot end another customer's use"), Slot->EndUse(SecondCustomer));
	TestTrue(TEXT("Occupant ends use"), Slot->EndUse(FirstCustomer));
	TestTrue(TEXT("Reservation owner releases the slot"), Slot->Release(FirstCustomer));
	TestTrue(TEXT("Slot becomes available"), Slot->IsAvailable());

	Slot->ApproachOffset = FVector(-125.0f, 20.0f, 0.0f);
	Slot->FacingRotation = FRotator(0.0f, 35.0f, 0.0f);
	const FTransform ActionTransform = Slot->GetActionTransform();
	const FTransform ApproachTransform = Slot->GetApproachTransform();
	TestEqual(TEXT("Action transform applies authored facing once"), ActionTransform.Rotator().Yaw, 35.0);
	TestEqual(TEXT("Approach transform shares the single authored facing"), ApproachTransform.Rotator().Yaw, 35.0);
	TestEqual(TEXT("Approach offset remains component-local"), ApproachTransform.GetLocation(), FVector(-125.0f, 20.0f, 0.0f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBathhouseCounterLaneIsolationTest,
	"BathhouseSim.Facility.CounterLaneIsolation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBathhouseCounterLaneIsolationTest::RunTest(const FString& Parameters)
{
	ABathhouseCounterActor* Counter = NewObject<ABathhouseCounterActor>();
	AActor* CheckInCustomer = NewObject<AActor>();
	AActor* CheckoutCustomer = NewObject<AActor>();

	TestTrue(TEXT("Check-in customer joins check-in lane"), Counter->EnqueueActor(EBathhouseCounterLane::CheckIn, CheckInCustomer));
	TestTrue(TEXT("Checkout customer joins checkout lane"), Counter->EnqueueActor(EBathhouseCounterLane::Checkout, CheckoutCustomer));
	TestTrue(TEXT("Check-in front is independent"), Counter->IsFront(EBathhouseCounterLane::CheckIn, CheckInCustomer));
	TestTrue(TEXT("Checkout front is independent"), Counter->IsFront(EBathhouseCounterLane::Checkout, CheckoutCustomer));
	TestFalse(TEXT("Check-in customer is not checkout front"), Counter->IsFront(EBathhouseCounterLane::Checkout, CheckInCustomer));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBathhouseCounterPointReferenceTest,
	"BathhouseSim.Facility.CounterPointReferences",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBathhouseCounterPointReferenceTest::RunTest(const FString& Parameters)
{
	ABathhouseCounterActor* Counter = NewObject<ABathhouseCounterActor>();
	USceneComponent* CheckInPoint = NewObject<USceneComponent>(Counter, TEXT("AuthoredCheckInPoint"));
	USceneComponent* CheckoutPoint = NewObject<USceneComponent>(Counter, TEXT("AuthoredCheckoutPoint"));
	USceneComponent* ReturnedPoint = NewObject<USceneComponent>(Counter, TEXT("AuthoredReturnedPoint"));
	UBathhouseNonSceneTestComponent* NonSceneComponent =
		NewObject<UBathhouseNonSceneTestComponent>(Counter, TEXT("InvalidNonScenePoint"));
	AActor* OtherActor = NewObject<AActor>();
	USceneComponent* OtherActorPoint = NewObject<USceneComponent>(OtherActor, TEXT("InvalidOtherActorPoint"));

	const auto MakeReference = [](UActorComponent* Component)
	{
		FComponentReference Reference;
		Reference.OverrideComponent = Component;
		return Reference;
	};

	Counter->CheckInQueuePointReferences = {
		MakeReference(CheckInPoint),
		MakeReference(CheckInPoint)};
	Counter->CheckoutQueuePointReferences = {
		MakeReference(CheckInPoint),
		MakeReference(CheckoutPoint),
		MakeReference(OtherActorPoint)};
	Counter->ReturnedKeyPointReferences = {
		MakeReference(NonSceneComponent),
		FComponentReference(),
		MakeReference(ReturnedPoint),
		MakeReference(CheckoutPoint)};

	AddExpectedError(TEXT("Counter point reference"), EAutomationExpectedErrorFlags::Contains, 6);
	Counter->ResolveConfiguredPoints();

	TestEqual(TEXT("Only the valid check-in point is resolved"), Counter->ResolvedCheckInQueuePoints.Num(), 1);
	TestEqual(TEXT("Check-in ordering preserves the first valid authored point"),
		Counter->ResolvedCheckInQueuePoints[0].Get(), CheckInPoint);
	TestEqual(TEXT("Only the valid checkout point is resolved"), Counter->ResolvedCheckoutQueuePoints.Num(), 1);
	TestEqual(TEXT("Checkout ordering preserves its valid authored point"),
		Counter->ResolvedCheckoutQueuePoints[0].Get(), CheckoutPoint);
	TestEqual(TEXT("Only the valid returned-key point is resolved"), Counter->ResolvedReturnedKeyPoints.Num(), 1);
	TestEqual(TEXT("Returned-key ordering preserves its valid authored point"),
		Counter->ResolvedReturnedKeyPoints[0].Get(), ReturnedPoint);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBathhouseQueueCleanupTest,
	"BathhouseSim.Customer.QueueCleanup",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBathhouseQueueCleanupTest::RunTest(const FString& Parameters)
{
	ABathhouseCounterActor* Counter = NewObject<ABathhouseCounterActor>();
	AActor* LeavingOwner = NewObject<AActor>();
	AActor* RemainingOwner = NewObject<AActor>();
	UCustomerSessionComponent* LeavingSession = NewObject<UCustomerSessionComponent>(LeavingOwner);
	UCustomerSessionComponent* RemainingSession = NewObject<UCustomerSessionComponent>(RemainingOwner);
	LeavingSession->InitializeSession(nullptr, Counter);
	RemainingSession->InitializeSession(nullptr, Counter);
	TestTrue(TEXT("Leaving customer joins check-in"), LeavingSession->JoinQueue(EBathhouseCounterLane::CheckIn));
	TestTrue(TEXT("Remaining customer joins check-in"), RemainingSession->JoinQueue(EBathhouseCounterLane::CheckIn));

	bool bObservedRemovalBroadcast = false;
	bool bLeavingLaneWasClearedBeforeBroadcast = false;
	const FDelegateHandle ObservationHandle = Counter->OnQueueChangedNative.AddLambda(
		[&](const EBathhouseCounterLane Lane)
		{
			if (Lane == EBathhouseCounterLane::CheckIn)
			{
				bObservedRemovalBroadcast = true;
				bLeavingLaneWasClearedBeforeBroadcast = LeavingSession->GetQueueLane() == EBathhouseCounterLane::None;
			}
		});

	LeavingSession->LeaveQueue();
	Counter->OnQueueChangedNative.Remove(ObservationHandle);
	TestTrue(TEXT("Queue removal still broadcasts to remaining customers"), bObservedRemovalBroadcast);
	TestTrue(TEXT("Leaving membership is cleared before the synchronous broadcast"), bLeavingLaneWasClearedBeforeBroadcast);
	TestTrue(TEXT("Remaining customer becomes queue front"), RemainingSession->IsQueueFront());
	TestTrue(TEXT("A movable remaining customer forwards the lane update"),
		RemainingSession->ShouldForwardQueueChangedEvent(EBathhouseCounterLane::CheckIn));

	RemainingSession->bWaitingForCheckIn = true;
	TestFalse(TEXT("An active check-in wait ignores unrelated lane updates"),
		RemainingSession->ShouldForwardQueueChangedEvent(EBathhouseCounterLane::CheckIn));
	RemainingSession->bWaitingForCheckIn = false;

	AActor* CheckoutOwner = NewObject<AActor>();
	UCustomerSessionComponent* CheckoutSession = NewObject<UCustomerSessionComponent>(CheckoutOwner);
	CheckoutSession->InitializeSession(nullptr, Counter);
	CheckoutSession->bCheckInTerminalCommitted = true;
	TestTrue(TEXT("Checkout customer joins checkout"), CheckoutSession->JoinQueue(EBathhouseCounterLane::Checkout));
	TestTrue(TEXT("Completed check-in does not suppress checkout positioning updates"),
		CheckoutSession->ShouldForwardQueueChangedEvent(EBathhouseCounterLane::Checkout));
	TestTrue(TEXT("Checkout offer guard starts only for the lane front"), CheckoutSession->BeginCheckoutOffer());
	TestFalse(TEXT("An active checkout offer ignores unrelated lane updates"),
		CheckoutSession->ShouldForwardQueueChangedEvent(EBathhouseCounterLane::Checkout));
	CheckoutSession->EndCheckoutOffer();
	return true;
}

#endif
