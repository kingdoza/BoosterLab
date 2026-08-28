#if WITH_DEV_AUTOMATION_TESTS

#include "Tests/BathhouseDomainTestProbe.h"

#include "AIController.h"
#include "Animation/AnimMontage.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Facility/BathhouseCounterActor.h"
#include "Facility/BathhouseFacilityActor.h"
#include "Facility/BathhouseFacilitySlotComponent.h"
#include "Facility/CustomerQueueOverflowWanderVolume.h"
#include "Interaction/BathhouseKeyActor.h"
#include "Interaction/BathhouseKeyHookActor.h"
#include "Interaction/PlayerCarryComponent.h"
#include "Interaction/PlayerInteractionComponent.h"
#include "Customer/CustomerSessionComponent.h"
#include "Customer/BathhouseCustomerCharacter.h"
#include "Customer/CustomerMontagePlaybackComponent.h"
#include "Customer/CustomerQueueNavigationComponent.h"
#include "Customer/CustomerRoutineDefinition.h"
#include "Customer/CustomerRoutineInterruptionComponent.h"
#include "Economy/BathhouseCashPaymentActor.h"
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
	const auto MakeCharacterTransformAtFacilityPoint = [Customer](const FTransform& FacilityPointTransform)
	{
		FTransform CharacterTransform = FacilityPointTransform;
		CharacterTransform.AddToTranslation(
			FVector::UpVector * Customer->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
		return CharacterTransform;
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
		Customer->SetActorTransform(
			MakeCharacterTransformAtFacilityPoint(Snapshot.Approach),
			false,
			nullptr,
			ETeleportType::TeleportPhysics);
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
	TestTrue(TEXT("Action snap places the character feet at the reservation-time action point"),
		Customer->GetActorTransform().Equals(MakeCharacterTransformAtFacilityPoint(FirstSnapshot.Action)));
	TestTrue(TEXT("Action snap preserves the authored facility point as the character feet location"),
		Customer->GetCharacterMovement()->GetActorFeetLocation().Equals(FirstSnapshot.Action.GetLocation()));
	TestEqual(TEXT("Action snap disables character movement"),
		Customer->GetCharacterMovement()->MovementMode, MOVE_None);
	const FRotator SnappedRotation = Customer->GetActorRotation();
	TestTrue(TEXT("The snapped Bath reservation enters occupied use"), Session->BeginUseCurrentFacility());
	TestTrue(TEXT("Begin use does not replace the cached action rotation with the live slot rotation"),
		Customer->GetActorRotation().Equals(SnappedRotation));
	Session->ReleaseCurrentFacility();
	TestTrue(TEXT("Release returns the customer feet to the original cached approach before clearing the slot"),
		Customer->GetActorTransform().Equals(MakeCharacterTransformAtFacilityPoint(FirstSnapshot.Approach)));
	TestTrue(TEXT("Approach return preserves the authored facility point as the character feet location"),
		Customer->GetCharacterMovement()->GetActorFeetLocation().Equals(FirstSnapshot.Approach.GetLocation()));
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
	BlockingBox->SetWorldLocation(
		MakeCharacterTransformAtFacilityPoint(BlockedSnapshot.Action).GetLocation());
	BlockingBox->UpdateOverlaps();
	const ECollisionEnabled::Type CapsuleCollisionBeforeSnap =
		Customer->GetCapsuleComponent()->GetCollisionEnabled();
	const FCollisionResponseContainer CapsuleResponsesBeforeSnap =
		Customer->GetCapsuleComponent()->GetCollisionResponseToChannels();
	const bool bActorCollisionBeforeSnap = Customer->GetActorEnableCollision();
	const int32 NavigationFailuresBeforeSnap = Session->NavigationFailureCount;
	TestTrue(TEXT("A blocked action point still permits the authoritative unswept snap"),
		Session->SnapCurrentFacility(ECustomerFacilitySnapTarget::ActionPoint));
	TestTrue(TEXT("Blocked action snap still places the character feet at the cached action point"),
		Customer->GetActorTransform().Equals(MakeCharacterTransformAtFacilityPoint(BlockedSnapshot.Action)));
	TestEqual(TEXT("Blocked action snap disables character movement"),
		Customer->GetCharacterMovement()->MovementMode, MOVE_None);
	TestEqual(TEXT("Blocked action snap preserves capsule collision enabled state"),
		Customer->GetCapsuleComponent()->GetCollisionEnabled(), CapsuleCollisionBeforeSnap);
	TestTrue(TEXT("Blocked action snap preserves every capsule collision response"),
		Customer->GetCapsuleComponent()->GetCollisionResponseToChannels() == CapsuleResponsesBeforeSnap);
	TestEqual(TEXT("Blocked action snap preserves actor collision state"),
		Customer->GetActorEnableCollision(), bActorCollisionBeforeSnap);
	TestEqual(TEXT("Blocked action snap does not consume a navigation retry"),
		Session->NavigationFailureCount, NavigationFailuresBeforeSnap);
	TestTrue(TEXT("Blocked action snap can enter occupied Bath use"), Session->BeginUseCurrentFacility());
	Session->ReleaseCurrentFacility();
	TestTrue(TEXT("Blocked action cleanup returns to the cached approach point"),
		Customer->GetActorTransform().Equals(MakeCharacterTransformAtFacilityPoint(BlockedSnapshot.Approach)));
	TestEqual(TEXT("Blocked action cleanup restores walking movement"),
		Customer->GetCharacterMovement()->MovementMode, MOVE_Walking);
	TestTrue(TEXT("Blocked action cleanup releases the slot"), Slot->IsAvailable());
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
	TestTrue(TEXT("Technical abort returns the customer feet to the original cached approach transform"),
		Customer->GetActorTransform().Equals(MakeCharacterTransformAtFacilityPoint(AbortSnapshot.Approach)));
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
	TestTrue(TEXT("Counter key initializes at its hook"), CounterKey->InitializeAtHook(CounterHook));
	CounterKey->CounterOwner = Counter;
	CounterKey->CommitState(EBathhouseKeyState::OnCounter, Counter);
	CounterKey->HeldTransform = FTransform(
		FRotator(-12.0f, 63.0f, 7.0f),
		FVector(-8.0f, 5.0f, 6.0f),
		FVector(3.0f));
	AActor* CounterCarryOwner = NewObject<AActor>();
	USceneComponent* CounterHeldAnchor = NewObject<USceneComponent>(CounterCarryOwner);
	UPlayerCarryComponent* CounterCarry = NewObject<UPlayerCarryComponent>(CounterCarryOwner);
	CounterCarryOwner->SetRootComponent(CounterHeldAnchor);
	CounterCarry->ConfigureHeldAnchor(CounterHeldAnchor);
	TestTrue(TEXT("The counter transaction takes the returned key"),
		CounterKey->TryTakeFromCounter(*CounterCarry));
	TestEqual(TEXT("Counter take applies the held key local location"),
		CounterKey->GetRootComponent()->GetRelativeLocation(), CounterKey->GetHeldTransform().GetLocation());
	TestTrue(TEXT("Counter take applies the held key local rotation"),
		CounterKey->GetRootComponent()->GetRelativeTransform().GetRotation().Equals(
			CounterKey->GetHeldTransform().GetRotation()));
	TestEqual(TEXT("Counter take ignores authored HeldTransform scale"),
		CounterKey->GetHeldTransform().GetScale3D(), FVector::OneVector);
	CounterKey->RecoverToHook();
	TestEqual(TEXT("Counter key recovers to its hook"), CounterKey->GetKeyState(), EBathhouseKeyState::AtHook);
	TestTrue(TEXT("Counter recovery removes the held offset at the hook"),
		CounterKey->GetRootComponent()->GetRelativeTransform().GetLocation().IsNearlyZero()
		&& CounterKey->GetRootComponent()->GetRelativeTransform().GetRotation().Equals(FQuat::Identity));
	TestTrue(TEXT("Counter recovery clears the carry owner"), CounterCarry->IsHandEmpty());

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
	FBathhouseKeyTopologyInitializationTest,
	"BathhouseSim.Interaction.KeyTopologyInitializationOrder",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBathhouseKeyTopologyInitializationTest::RunTest(const FString& Parameters)
{
	if (!GEngine)
	{
		AddError(TEXT("GEngine is required for the key topology initialization test."));
		return false;
	}
	const FName WorldName = MakeUniqueObjectName(nullptr, UWorld::StaticClass(), TEXT("KeyTopologyAutomationWorld"));
	FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, WorldName, GetTransientPackage());
	if (!World)
	{
		GEngine->DestroyWorldContext(World);
		AddError(TEXT("Failed to create the key topology automation world."));
		return false;
	}
	World->AddToRoot();
	WorldContext.SetCurrentWorld(World);
	World->InitializeActorsForPlay(FURL());

	const FTransform Identity = FTransform::Identity;
	const auto BeginActor = [](AActor* Actor)
	{
		if (Actor && !Actor->HasActorBegunPlay())
		{
			Actor->DispatchBeginPlay();
		}
	};
	ABathhouseKeyHookActor* Hook = World->SpawnActorDeferred<ABathhouseKeyHookActor>(
		ABathhouseKeyHookActor::StaticClass(),
		Identity,
		nullptr,
		nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	ABathhouseKeyActor* Key = World->SpawnActorDeferred<ABathhouseKeyActor>(
		ABathhouseKeyActor::StaticClass(),
		Identity,
		nullptr,
		nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	Hook->KeyNumber = 2;
	Hook->KeyActor = Key;
	Key->KeyNumber = 2;
	Key->KeyHook = Hook;
	UGameplayStatics::FinishSpawningActor(Hook, Identity);
	UGameplayStatics::FinishSpawningActor(Key, Identity);
	BeginActor(Hook);
	BeginActor(Key);

	FText FailureReason;
	TestFalse(TEXT("A key hook that begins before its numbered facilities is initially unavailable"),
		Hook->IsPhysicalCarrySlotOperational(&FailureReason));
	TestTrue(TEXT("The early failure identifies the missing numbered facility"), !FailureReason.IsEmpty());

	ABathhouseFacilityActor* ShoeLocker = World->SpawnActorDeferred<ABathhouseFacilityActor>(
		ABathhouseFacilityActor::StaticClass(),
		Identity,
		nullptr,
		nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	ShoeLocker->FacilityType = EBathhouseFacilityType::ShoeLocker;
	ShoeLocker->FacilityNumber = 2;
	ShoeLocker->bEnabled = true;
	UGameplayStatics::FinishSpawningActor(ShoeLocker, Identity);
	BeginActor(ShoeLocker);
	TestFalse(TEXT("One numbered facility is not enough to activate the key hook"),
		Hook->IsPhysicalCarrySlotOperational());

	ABathhouseFacilityActor* ClothesLocker = World->SpawnActorDeferred<ABathhouseFacilityActor>(
		ABathhouseFacilityActor::StaticClass(),
		Identity,
		nullptr,
		nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	ClothesLocker->FacilityType = EBathhouseFacilityType::ClothesLocker;
	ClothesLocker->FacilityNumber = 2;
	ClothesLocker->bEnabled = true;
	UGameplayStatics::FinishSpawningActor(ClothesLocker, Identity);
	BeginActor(ClothesLocker);

	TestTrue(TEXT("The final numbered facility registration reactivates the existing key hook"),
		Hook->IsPhysicalCarrySlotOperational(&FailureReason));
	TestEqual(TEXT("The reactivated hook stores its exact authored key"),
		Hook->GetStoredPhysicalCarryItem(), static_cast<AActor*>(Key));
	TestEqual(TEXT("Reactivation preserves the key's AtHook domain state"),
		Key->GetKeyState(), EBathhouseKeyState::AtHook);

	World->DestroyWorld(false);
	GEngine->DestroyWorldContext(World);
	World->RemoveFromRoot();
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
	Counter->CheckInServicePoint->SetRelativeLocation(FVector(0.0f, -100.0f, 0.0f));
	Counter->CheckoutServicePoint->SetRelativeLocation(FVector(0.0f, 100.0f, 0.0f));
	CheckInPoint->SetRelativeLocation(FVector(-100.0f, -100.0f, 0.0f));
	CheckoutPoint->SetRelativeLocation(FVector(-100.0f, 100.0f, 0.0f));

	const auto MakeReference = [](UActorComponent* Component)
	{
		FComponentReference Reference;
		Reference.OverrideComponent = Component;
		return Reference;
	};

	Counter->CheckInQueuePointReferences = {
		MakeReference(CheckInPoint),
		MakeReference(Counter->CheckInServicePoint),
		MakeReference(CheckInPoint)};
	Counter->CheckoutQueuePointReferences = {
		MakeReference(Counter->CheckoutServicePoint),
		MakeReference(CheckInPoint),
		MakeReference(CheckoutPoint),
		MakeReference(OtherActorPoint)};
	Counter->ReturnedKeyPointReferences = {
		MakeReference(NonSceneComponent),
		FComponentReference(),
		MakeReference(ReturnedPoint),
		MakeReference(CheckoutPoint)};

	AddExpectedError(TEXT("Counter point reference"), EAutomationExpectedErrorFlags::Contains, 3);
	AddExpectedError(TEXT("uses native service point"), EAutomationExpectedErrorFlags::Contains, 2);
	Counter->ResolveConfiguredPoints();

	TestEqual(TEXT("Only the valid check-in point is resolved"), Counter->ResolvedCheckInQueuePoints.Num(), 1);
	TestEqual(TEXT("Check-in ordering preserves the first valid authored point"),
		Counter->ResolvedCheckInQueuePoints[0].Get(), CheckInPoint);
	TestEqual(TEXT("Only the valid checkout point is resolved"), Counter->ResolvedCheckoutQueuePoints.Num(), 1);
	TestEqual(TEXT("Checkout ordering preserves its valid authored point"),
		Counter->ResolvedCheckoutQueuePoints[0].Get(), CheckoutPoint);
	TestFalse(TEXT("Check-in queue points cannot reuse the native service point"),
		Counter->ResolvedCheckInQueuePoints.Contains(Counter->CheckInServicePoint));
	TestFalse(TEXT("Checkout queue points cannot reuse the native service point"),
		Counter->ResolvedCheckoutQueuePoints.Contains(Counter->CheckoutServicePoint));
	AActor* CheckInFront = NewObject<AActor>();
	AActor* CheckInQueued = NewObject<AActor>();
	Counter->EnqueueActor(EBathhouseCounterLane::CheckIn, CheckInFront);
	Counter->EnqueueActor(EBathhouseCounterLane::CheckIn, CheckInQueued);
	FBathhouseQueueAssignment ServiceAssignment;
	FBathhouseQueueAssignment QueueAssignment;
	TestTrue(TEXT("Validated service assignment resolves"),
		Counter->ResolveQueueAssignment(EBathhouseCounterLane::CheckIn, CheckInFront, ServiceAssignment));
	TestTrue(TEXT("Validated queue-point assignment resolves"),
		Counter->ResolveQueueAssignment(EBathhouseCounterLane::CheckIn, CheckInQueued, QueueAssignment));
	TestFalse(TEXT("Distinct visible indices use distinct accepted component transforms"),
		ServiceAssignment.TargetTransform.Equals(QueueAssignment.TargetTransform));
	TestNotNull(TEXT("The native returned-key drop point remains stable"), Counter->GetReturnedKeyDropPoint());
	TestEqual(TEXT("Deprecated returned-key point references remain serialized but are not canonical"),
		Counter->ReturnedKeyPointReferences.Num(), 4);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBathhouseCounterAssignmentTest,
	"BathhouseSim.Facility.CounterAssignmentAndOverflow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBathhouseCounterAssignmentTest::RunTest(const FString& Parameters)
{
	ABathhouseCounterActor* Counter = NewObject<ABathhouseCounterActor>();
	Counter->CheckInServicePoint->SetRelativeLocationAndRotation(FVector(10.0f, 20.0f, 30.0f), FRotator(5.0f, 25.0f, 7.0f));
	Counter->CheckoutServicePoint->SetRelativeLocationAndRotation(FVector(40.0f, 50.0f, 60.0f), FRotator(3.0f, 90.0f, 4.0f));
	USceneComponent* CheckInPoint = NewObject<USceneComponent>(Counter);
	CheckInPoint->SetRelativeLocationAndRotation(FVector(100.0f, 0.0f, 0.0f), FRotator(12.0f, 35.0f, 2.0f));
	USceneComponent* CheckoutPoint = NewObject<USceneComponent>(Counter);
	CheckoutPoint->SetRelativeLocationAndRotation(FVector(200.0f, 0.0f, 0.0f), FRotator(8.0f, 125.0f, 1.0f));
	Counter->ResolvedCheckInQueuePoints = {CheckInPoint};
	Counter->ResolvedCheckoutQueuePoints = {CheckoutPoint};

	AActor* First = NewObject<AActor>();
	AActor* Second = NewObject<AActor>();
	AActor* Third = NewObject<AActor>();
	const int64 InitialRevision = Counter->GetQueueRevision(EBathhouseCounterLane::Checkout);
	TestTrue(TEXT("Checkout service entry enqueues"), Counter->EnqueueActor(EBathhouseCounterLane::Checkout, First));
	TestTrue(TEXT("Checkout visible queue entry enqueues"), Counter->EnqueueActor(EBathhouseCounterLane::Checkout, Second));
	TestTrue(TEXT("Checkout overflow entry stays in the same FIFO"), Counter->EnqueueActor(EBathhouseCounterLane::Checkout, Third));
	TestTrue(TEXT("Checkout mutation advances a nonzero revision"),
		Counter->GetQueueRevision(EBathhouseCounterLane::Checkout) > InitialRevision);

	FBathhouseQueueAssignment Assignment;
	TestTrue(TEXT("Service assignment resolves"),
		Counter->ResolveQueueAssignment(EBathhouseCounterLane::Checkout, First, Assignment));
	TestEqual(TEXT("Index zero is the service point"), Assignment.Type, EBathhouseQueueAssignmentType::ServicePoint);
	TestTrue(TEXT("Service assignment preserves the full component transform"),
		Assignment.TargetTransform.Equals(Counter->CheckoutServicePoint->GetComponentTransform()));
	TestTrue(TEXT("Visible queue assignment resolves"),
		Counter->ResolveQueueAssignment(EBathhouseCounterLane::Checkout, Second, Assignment));
	TestEqual(TEXT("Index one maps to queue point zero"), Assignment.QueuePointIndex, 0);
	TestTrue(TEXT("Queue assignment preserves authored pitch/yaw/roll"),
		Assignment.TargetTransform.GetRotation().Equals(CheckoutPoint->GetComponentQuat()));
	TestTrue(TEXT("Overflow assignment resolves without clamping"),
		Counter->ResolveQueueAssignment(EBathhouseCounterLane::Checkout, Third, Assignment));
	TestEqual(TEXT("Index after visible capacity is overflow"), Assignment.Type, EBathhouseQueueAssignmentType::OverflowWander);

	TestTrue(TEXT("Dequeuing front promotes the same FIFO entries"), Counter->DequeueActor(EBathhouseCounterLane::Checkout, First));
	TestTrue(TEXT("Former queue point becomes service"),
		Counter->ResolveQueueAssignment(EBathhouseCounterLane::Checkout, Second, Assignment));
	TestEqual(TEXT("FIFO promotion reaches service"), Assignment.Type, EBathhouseQueueAssignmentType::ServicePoint);
	TestTrue(TEXT("Earliest overflow entry promotes to the exact visible point"),
		Counter->ResolveQueueAssignment(EBathhouseCounterLane::Checkout, Third, Assignment));
	TestEqual(TEXT("Overflow promotion reaches queue point zero"), Assignment.Type, EBathhouseQueueAssignmentType::QueuePoint);
	TestEqual(TEXT("Promoted entry receives point zero"), Assignment.QueuePointIndex, 0);

	AActor* CheckInFirst = NewObject<AActor>();
	AActor* CheckInSecond = NewObject<AActor>();
	AActor* CheckInBeyondCapacity = NewObject<AActor>();
	Counter->EnqueueActor(EBathhouseCounterLane::CheckIn, CheckInFirst);
	Counter->EnqueueActor(EBathhouseCounterLane::CheckIn, CheckInSecond);
	Counter->EnqueueActor(EBathhouseCounterLane::CheckIn, CheckInBeyondCapacity);
	TestFalse(TEXT("Check-in beyond visible capacity is invalid instead of clamped"),
		Counter->ResolveQueueAssignment(EBathhouseCounterLane::CheckIn, CheckInBeyondCapacity, Assignment));
	TestEqual(TEXT("Invalid check-in assignment remains explicitly invalid"),
		Assignment.Type, EBathhouseQueueAssignmentType::Invalid);

	ACustomerQueueOverflowWanderVolume* Volume = NewObject<ACustomerQueueOverflowWanderVolume>();
	TestTrue(TEXT("Overflow volume contains its local origin"), Volume->ContainsWorldPoint(Volume->GetActorLocation()));
	TestFalse(TEXT("Overflow volume rejects a point outside its bounds"),
		Volume->ContainsWorldPoint(Volume->GetActorLocation() + FVector(10000.0f, 0.0f, 0.0f)));
	const int64 RevisionBeforeFailedSample = Counter->GetQueueRevision(EBathhouseCounterLane::Checkout);
	FVector Sample;
	TestFalse(TEXT("Sampling without a NavMesh fails without a global fallback"),
		Volume->TrySampleReachablePoint(*Third, Sample));
	TestEqual(TEXT("Overflow sample failure cannot mutate the FIFO revision"),
		Counter->GetQueueRevision(EBathhouseCounterLane::Checkout), RevisionBeforeFailedSample);

	AActor* InvalidEntry = NewObject<AActor>();
	Counter->EnqueueActor(EBathhouseCounterLane::Checkout, InvalidEntry);
	InvalidEntry->MarkAsGarbage();
	int32 CompactMutationBroadcasts = 0;
	const FDelegateHandle CompactHandle = Counter->OnQueueChangedNative.AddLambda(
		[&CompactMutationBroadcasts](const EBathhouseCounterLane Lane)
		{
			if (Lane == EBathhouseCounterLane::Checkout)
			{
				++CompactMutationBroadcasts;
			}
		});
	AActor* PostCompactEntry = NewObject<AActor>();
	TestTrue(TEXT("A new mutation compacts invalid weak entries before enqueue"),
		Counter->EnqueueActor(EBathhouseCounterLane::Checkout, PostCompactEntry));
	Counter->OnQueueChangedNative.Remove(CompactHandle);
	TestEqual(TEXT("Compaction plus enqueue broadcasts the lane once"), CompactMutationBroadcasts, 1);
	TestTrue(TEXT("Post-compaction entry remains resolvable"),
		Counter->ResolveQueueAssignment(EBathhouseCounterLane::Checkout, PostCompactEntry, Assignment));
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBathhouseQueueNavigationFacingTest,
	"BathhouseSim.Customer.QueueNavigationFacingAndRecoveryGate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBathhouseQueueNavigationFacingTest::RunTest(const FString& Parameters)
{
	if (!GEngine)
	{
		AddError(TEXT("GEngine is required for the queue navigation world test."));
		return false;
	}
	const FName WorldName = MakeUniqueObjectName(nullptr, UWorld::StaticClass(), TEXT("QueueNavigationAutomationWorld"));
	FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, WorldName, GetTransientPackage());
	if (!World)
	{
		GEngine->DestroyWorldContext(World);
		AddError(TEXT("Failed to create queue navigation automation world."));
		return false;
	}
	World->AddToRoot();
	WorldContext.SetCurrentWorld(World);
	World->InitializeActorsForPlay(FURL());

	ABathhouseCounterActor* Counter = World->SpawnActor<ABathhouseCounterActor>();
	ABathhouseCustomerCharacter* Customer = World->SpawnActorDeferred<ABathhouseCustomerCharacter>(
		ABathhouseCustomerCharacter::StaticClass(),
		FTransform::Identity,
		nullptr,
		nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	Customer->AutoPossessAI = EAutoPossessAI::Disabled;
	UGameplayStatics::FinishSpawningActor(Customer, FTransform::Identity);
	UCustomerRoutineDefinition* Definition = NewObject<UCustomerRoutineDefinition>();
	TestEqual(TEXT("Queue acceptance radius defaults to 10 cm"), Definition->QueueAcceptanceRadius, 10.0f);
	Definition->QueueFacingRotationSpeedDegrees = 90.0f;
	Definition->QueueFacingToleranceDegrees = 1.0f;
	Customer->InitializeCustomer(Definition, Counter);
	UCustomerSessionComponent* Session = Customer->GetCustomerSession();
	UCustomerQueueNavigationComponent* Navigation = Customer->GetCustomerQueueNavigation();
	TestTrue(TEXT("Customer joins the checkout service assignment"), Session->JoinQueue(EBathhouseCounterLane::Checkout));
	Counter->SetActorRotation(FRotator(0.0f, 90.0f, 0.0f));
	Customer->SetActorRotation(FRotator::ZeroRotator);
	Customer->GetCharacterMovement()->bOrientRotationToMovement = true;
	Customer->GetCharacterMovement()->bUseControllerDesiredRotation = true;
	Customer->bUseControllerRotationYaw = true;
	const uint64 Token = Navigation->BeginQueueNavigation(EBathhouseCounterLane::Checkout);
	TestTrue(TEXT("Native queue navigation starts"), Token != 0);
	Navigation->TickComponent(0.1f, LEVELTICK_All, nullptr);
	TestFalse(TEXT("Service does not complete after position alone"),
		Navigation->GetQueueNavigationStatus(Token) == ECustomerQueueNavigationStatus::ServiceReady);
	for (int32 Step = 0; Step < 20
		&& Navigation->GetQueueNavigationStatus(Token) != ECustomerQueueNavigationStatus::ServiceReady; ++Step)
	{
		Navigation->TickComponent(0.1f, LEVELTICK_All, nullptr);
	}
	TestEqual(TEXT("Service completes after yaw alignment"),
		Navigation->GetQueueNavigationStatus(Token), ECustomerQueueNavigationStatus::ServiceReady);
	TestTrue(TEXT("Character uses target yaw without pitch or roll"),
		Customer->GetActorRotation().Equals(FRotator(0.0f, 90.0f, 0.0f), 1.0f));

	UCustomerRoutineInterruptionComponent* Interruption = Customer->GetCustomerRoutineInterruption();
	TestTrue(TEXT("Knockdown suspends active queue navigation"), Interruption->BeginSoftInterruption());
	Counter->SetActorRotation(FRotator(0.0f, 150.0f, 0.0f));
	AActor* LaterCustomer = NewObject<AActor>();
	Counter->EnqueueActor(EBathhouseCounterLane::Checkout, LaterCustomer);
	TestTrue(TEXT("Recovery gate request is accepted"), Interruption->EndSoftInterruption());
	TestTrue(TEXT("Routine remains paused before latest yaw recovery"), Interruption->IsSoftInterrupted());
	for (int32 Step = 0; Step < 20 && Interruption->IsSoftInterrupted(); ++Step)
	{
		Navigation->TickComponent(0.1f, LEVELTICK_All, nullptr);
	}
	TestFalse(TEXT("Routine resumes exactly after the latest assignment pose"), Interruption->IsSoftInterrupted());
	TestTrue(TEXT("Recovery uses the post-revision service yaw"),
		Customer->GetActorRotation().Equals(FRotator(0.0f, 150.0f, 0.0f), 1.0f));

	ABathhouseCounterActor* PreBeginPlayCounter = World->SpawnActor<ABathhouseCounterActor>(
		FVector(2000.0f, 0.0f, 0.0f), FRotator(0.0f, 90.0f, 0.0f));
	ABathhouseCustomerCharacter* PreBeginPlayCustomer = World->SpawnActorDeferred<ABathhouseCustomerCharacter>(
		ABathhouseCustomerCharacter::StaticClass(),
		FTransform(FVector(2000.0f, 0.0f, 0.0f)),
		nullptr,
		nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	PreBeginPlayCustomer->AutoPossessAI = EAutoPossessAI::Disabled;
	PreBeginPlayCustomer->InitializeCustomer(Definition, PreBeginPlayCounter);
	UCustomerSessionComponent* PreBeginPlaySession = PreBeginPlayCustomer->GetCustomerSession();
	UCustomerQueueNavigationComponent* PreBeginPlayNavigation =
		PreBeginPlayCustomer->GetCustomerQueueNavigation();
	TestTrue(TEXT("Pre-BeginPlay customer joins checkout"),
		PreBeginPlaySession->JoinQueue(EBathhouseCounterLane::Checkout));
	const uint64 PreBeginPlayToken =
		PreBeginPlayNavigation->BeginQueueNavigation(EBathhouseCounterLane::Checkout);
	TestTrue(TEXT("Queue navigation can start before component BeginPlay"), PreBeginPlayToken != 0);
	UGameplayStatics::FinishSpawningActor(
		PreBeginPlayCustomer,
		FTransform(FVector(2000.0f, 0.0f, 0.0f)));
	TestTrue(TEXT("Component BeginPlay preserves an already-active queue Tick"),
		PreBeginPlayNavigation->IsComponentTickEnabled());
	for (int32 Step = 0; Step < 20
		&& PreBeginPlayNavigation->GetQueueNavigationStatus(PreBeginPlayToken)
			!= ECustomerQueueNavigationStatus::ServiceReady; ++Step)
	{
		PreBeginPlayNavigation->TickComponent(0.1f, LEVELTICK_All, nullptr);
	}
	TestEqual(TEXT("Pre-BeginPlay queue execution completes its initial facing"),
		PreBeginPlayNavigation->GetQueueNavigationStatus(PreBeginPlayToken),
		ECustomerQueueNavigationStatus::ServiceReady);
	TestTrue(TEXT("Pre-BeginPlay customer reaches the authored service yaw"),
		PreBeginPlayCustomer->GetActorRotation().Equals(FRotator(0.0f, 90.0f, 0.0f), 1.0f));
	PreBeginPlaySession->LeaveQueue();

	int32 IntentionalLeaveBroadcasts = 0;
	const FDelegateHandle IntentionalLeaveHandle = Counter->OnQueueChangedNative.AddLambda(
		[&IntentionalLeaveBroadcasts](const EBathhouseCounterLane Lane)
		{
			if (Lane == EBathhouseCounterLane::Checkout)
			{
				++IntentionalLeaveBroadcasts;
			}
		});
	const int64 RevisionBeforeIntentionalLeave = Counter->GetQueueRevision(EBathhouseCounterLane::Checkout);
	Session->LeaveQueue();
	Counter->OnQueueChangedNative.Remove(IntentionalLeaveHandle);
	TestFalse(TEXT("Intentional queue leave is not a technical abort"), Session->IsTechnicalAbort());
	TestEqual(TEXT("Intentional queue leave broadcasts exactly once"), IntentionalLeaveBroadcasts, 1);
	TestEqual(TEXT("Intentional queue leave advances the lane revision exactly once"),
		Counter->GetQueueRevision(EBathhouseCounterLane::Checkout), RevisionBeforeIntentionalLeave + 1);
	TestTrue(TEXT("The remaining FIFO entry promotes to the service point"),
		Counter->IsFront(EBathhouseCounterLane::Checkout, LaterCustomer));
	TestEqual(TEXT("Intentional leave invalidates the execution token"),
		Navigation->GetQueueNavigationStatus(Token), ECustomerQueueNavigationStatus::Inactive);
	TestNull(TEXT("Intentional leave releases the active move task"), Navigation->ActiveMoveTask.Get());
	TestFalse(TEXT("Intentional leave removes the Counter delegate"), Navigation->QueueChangedHandle.IsValid());
	TestEqual(TEXT("Intentional leave clears the active execution token"), Navigation->ActiveExecutionToken, uint64(0));
	TestEqual(TEXT("Intentional leave clears the active move token"), Navigation->ActiveMoveToken, uint64(0));
	TestFalse(TEXT("Intentional leave disables navigation Tick"), Navigation->IsComponentTickEnabled());
	TestFalse(TEXT("Intentional leave consumes the movement flag snapshot"), Navigation->bMovementFlagsSnapshotted);
	TestTrue(TEXT("Intentional leave restores orient-to-movement"),
		Customer->GetCharacterMovement()->bOrientRotationToMovement);
	TestTrue(TEXT("Intentional leave restores controller-desired rotation"),
		Customer->GetCharacterMovement()->bUseControllerDesiredRotation);
	TestTrue(TEXT("Intentional leave restores controller yaw"), Customer->bUseControllerRotationYaw);
	const uint64 MoveGenerationAfterIntentionalLeave = Navigation->NextMoveToken;
	const int64 RevisionAfterIntentionalLeave = Counter->GetQueueRevision(EBathhouseCounterLane::Checkout);
	Session->LeaveQueue();
	TestEqual(TEXT("Repeated intentional leave does not advance the move generation"),
		Navigation->NextMoveToken, MoveGenerationAfterIntentionalLeave);
	TestEqual(TEXT("Repeated intentional leave does not mutate the Counter revision"),
		Counter->GetQueueRevision(EBathhouseCounterLane::Checkout), RevisionAfterIntentionalLeave);

	ABathhouseCounterActor* OverflowCounter = World->SpawnActor<ABathhouseCounterActor>();
	ABathhouseCustomerCharacter* OverflowCustomer = World->SpawnActorDeferred<ABathhouseCustomerCharacter>(
		ABathhouseCustomerCharacter::StaticClass(),
		FTransform(FVector(1000.0f, 0.0f, 0.0f)),
		nullptr,
		nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	OverflowCustomer->AutoPossessAI = EAutoPossessAI::Disabled;
	UGameplayStatics::FinishSpawningActor(OverflowCustomer, FTransform(FVector(1000.0f, 0.0f, 0.0f)));
	OverflowCounter->SetActorLocation(OverflowCustomer->GetActorLocation());
	OverflowCustomer->InitializeCustomer(Definition, OverflowCounter);
	AActor* OverflowFront = NewObject<AActor>();
	OverflowCounter->EnqueueActor(EBathhouseCounterLane::Checkout, OverflowFront);
	UCustomerSessionComponent* OverflowSession = OverflowCustomer->GetCustomerSession();
	TestTrue(TEXT("Overflow customer keeps normal checkout membership"),
		OverflowSession->JoinQueue(EBathhouseCounterLane::Checkout));
	UCustomerQueueNavigationComponent* OverflowNavigation = OverflowCustomer->GetCustomerQueueNavigation();
	const uint64 OverflowToken = OverflowNavigation->BeginQueueNavigation(EBathhouseCounterLane::Checkout);
	TestTrue(TEXT("Overflow execution starts even when sampling must locally retry"), OverflowToken != 0);
	const FVector PositionBeforeInterruption = OverflowCustomer->GetActorLocation();
	TestTrue(TEXT("Overflow knockdown suspends the queue task"),
		OverflowCustomer->GetCustomerRoutineInterruption()->BeginSoftInterruption());
	TestTrue(TEXT("Overflow recovery resumes without a fixed-pose gate"),
		OverflowCustomer->GetCustomerRoutineInterruption()->EndSoftInterruption());
	TestFalse(TEXT("Overflow recovery does not keep the routine paused"),
		OverflowCustomer->GetCustomerRoutineInterruption()->IsSoftInterrupted());
	TestEqual(TEXT("Overflow recovery never teleports to an obsolete wander point"),
		OverflowCustomer->GetActorLocation(), PositionBeforeInterruption);
	TestEqual(TEXT("Failed local sample waits without consuming the queue task"),
		OverflowNavigation->GetQueueNavigationStatus(OverflowToken), ECustomerQueueNavigationStatus::Waiting);
	OverflowNavigation->CancelQueueNavigation(OverflowToken);
	OverflowSession->LeaveQueue();

	ABathhouseCounterActor* MovingCounter = World->SpawnActor<ABathhouseCounterActor>(
		FVector(3000.0f, 0.0f, 0.0f), FRotator::ZeroRotator);
	ABathhouseCustomerCharacter* MovingCustomer = World->SpawnActorDeferred<ABathhouseCustomerCharacter>(
		ABathhouseCustomerCharacter::StaticClass(),
		FTransform(FVector(4000.0f, 0.0f, 0.0f)),
		nullptr,
		nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	MovingCustomer->AutoPossessAI = EAutoPossessAI::Disabled;
	UGameplayStatics::FinishSpawningActor(MovingCustomer, FTransform(FVector(4000.0f, 0.0f, 0.0f)));
	MovingCustomer->AIControllerClass = AAIController::StaticClass();
	MovingCustomer->SpawnDefaultController();
	MovingCustomer->InitializeCustomer(Definition, MovingCounter);
	UCustomerSessionComponent* MovingSession = MovingCustomer->GetCustomerSession();
	UCustomerQueueNavigationComponent* MovingNavigation = MovingCustomer->GetCustomerQueueNavigation();
	TestNotNull(TEXT("Move teardown fixture owns an AI controller"), MovingCustomer->GetController());
	TestTrue(TEXT("Move teardown fixture joins checkout"),
		MovingSession->JoinQueue(EBathhouseCounterLane::Checkout));
	const uint64 MovingToken = MovingNavigation->BeginQueueNavigation(EBathhouseCounterLane::Checkout);
	TestTrue(TEXT("Move teardown fixture starts a queue execution"), MovingToken != 0);
	TestNotNull(TEXT("A native MoveTo task is owned before intentional leave"), MovingNavigation->ActiveMoveTask.Get());
	MovingSession->LeaveQueue();
	TestFalse(TEXT("MoveTo teardown does not technical-abort"), MovingSession->IsTechnicalAbort());
	TestNull(TEXT("Intentional leave releases the native MoveTo task"), MovingNavigation->ActiveMoveTask.Get());
	TestFalse(TEXT("MoveTo teardown removes the Counter delegate"), MovingNavigation->QueueChangedHandle.IsValid());
	TestEqual(TEXT("MoveTo teardown invalidates the execution token"),
		MovingNavigation->GetQueueNavigationStatus(MovingToken), ECustomerQueueNavigationStatus::Inactive);

	ABathhouseCounterActor* DestructionCounter = World->SpawnActor<ABathhouseCounterActor>(
		FVector(5000.0f, 0.0f, 0.0f), FRotator::ZeroRotator);
	ABathhouseCustomerCharacter* DestructionCustomer = World->SpawnActorDeferred<ABathhouseCustomerCharacter>(
		ABathhouseCustomerCharacter::StaticClass(),
		FTransform(FVector(5000.0f, 0.0f, 0.0f)),
		nullptr,
		nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	DestructionCustomer->AutoPossessAI = EAutoPossessAI::Disabled;
	UGameplayStatics::FinishSpawningActor(DestructionCustomer, FTransform(FVector(5000.0f, 0.0f, 0.0f)));
	DestructionCustomer->InitializeCustomer(Definition, DestructionCounter);
	DestructionCustomer->DispatchBeginPlay();
	UCustomerSessionComponent* DestructionSession = DestructionCustomer->GetCustomerSession();
	UCustomerQueueNavigationComponent* DestructionNavigation = DestructionCustomer->GetCustomerQueueNavigation();
	TestTrue(TEXT("Destruction fixture joins checkout"),
		DestructionSession->JoinQueue(EBathhouseCounterLane::Checkout));
	const uint64 DestructionToken = DestructionNavigation->BeginQueueNavigation(EBathhouseCounterLane::Checkout);
	TestTrue(TEXT("Destruction fixture starts a queue execution"), DestructionToken != 0);
	AActor* DestructionRemaining = World->SpawnActor<AActor>();
	DestructionCounter->EnqueueActor(EBathhouseCounterLane::Checkout, DestructionRemaining);
	int32 DestructionBroadcasts = 0;
	const FDelegateHandle DestructionHandle = DestructionCounter->OnQueueChangedNative.AddLambda(
		[&DestructionBroadcasts](const EBathhouseCounterLane Lane)
		{
			if (Lane == EBathhouseCounterLane::Checkout)
			{
				++DestructionBroadcasts;
			}
		});
	const int64 RevisionBeforeDestruction =
		DestructionCounter->GetQueueRevision(EBathhouseCounterLane::Checkout);
	TestTrue(TEXT("Destroy routes the active-customer EndPlay path"), DestructionCustomer->Destroy());
	DestructionCounter->OnQueueChangedNative.Remove(DestructionHandle);
	TestFalse(TEXT("Active queue destruction is not a technical abort"), DestructionSession->IsTechnicalAbort());
	TestEqual(TEXT("Active queue destruction broadcasts exactly once"), DestructionBroadcasts, 1);
	TestEqual(TEXT("Active queue destruction advances the revision exactly once"),
		DestructionCounter->GetQueueRevision(EBathhouseCounterLane::Checkout), RevisionBeforeDestruction + 1);
	TestTrue(TEXT("Destruction promotes the remaining FIFO entry"),
		DestructionCounter->IsFront(EBathhouseCounterLane::Checkout, DestructionRemaining));
	TestEqual(TEXT("Destruction invalidates the execution token"),
		DestructionNavigation->GetQueueNavigationStatus(DestructionToken),
		ECustomerQueueNavigationStatus::Inactive);
	TestNull(TEXT("Destruction releases the active move task"), DestructionNavigation->ActiveMoveTask.Get());
	TestFalse(TEXT("Destruction removes the Counter delegate"), DestructionNavigation->QueueChangedHandle.IsValid());
	TestFalse(TEXT("Destruction disables navigation Tick"), DestructionNavigation->IsComponentTickEnabled());
	TestFalse(TEXT("Destruction consumes the movement flag snapshot"),
		DestructionNavigation->bMovementFlagsSnapshotted);

	ABathhouseCounterActor* CorruptionCounter = World->SpawnActor<ABathhouseCounterActor>(
		FVector(6000.0f, 0.0f, 0.0f), FRotator::ZeroRotator);
	ABathhouseCustomerCharacter* CorruptionCustomer = World->SpawnActorDeferred<ABathhouseCustomerCharacter>(
		ABathhouseCustomerCharacter::StaticClass(),
		FTransform(FVector(6000.0f, 0.0f, 0.0f)),
		nullptr,
		nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	CorruptionCustomer->AutoPossessAI = EAutoPossessAI::Disabled;
	UGameplayStatics::FinishSpawningActor(CorruptionCustomer, FTransform(FVector(6000.0f, 0.0f, 0.0f)));
	CorruptionCustomer->InitializeCustomer(Definition, CorruptionCounter);
	UCustomerSessionComponent* CorruptionSession = CorruptionCustomer->GetCustomerSession();
	UCustomerQueueNavigationComponent* CorruptionNavigation = CorruptionCustomer->GetCustomerQueueNavigation();
	TestTrue(TEXT("Corruption fixture joins checkout"),
		CorruptionSession->JoinQueue(EBathhouseCounterLane::Checkout));
	const uint64 CorruptionToken = CorruptionNavigation->BeginQueueNavigation(EBathhouseCounterLane::Checkout);
	TestTrue(TEXT("Corruption fixture starts a queue execution"), CorruptionToken != 0);
	int32 CorruptionBroadcasts = 0;
	const FDelegateHandle CorruptionHandle = CorruptionCounter->OnQueueChangedNative.AddLambda(
		[&CorruptionBroadcasts](const EBathhouseCounterLane Lane)
		{
			if (Lane == EBathhouseCounterLane::Checkout)
			{
				++CorruptionBroadcasts;
			}
		});
	AddExpectedError(
		TEXT("technical abort: Queue assignment became invalid during native queue navigation."),
		EAutomationExpectedErrorFlags::Contains,
		1);
	TestTrue(TEXT("External membership corruption removes the active FIFO entry"),
		CorruptionCounter->DequeueActor(EBathhouseCounterLane::Checkout, CorruptionCustomer));
	CorruptionCounter->OnQueueChangedNative.Remove(CorruptionHandle);
	TestTrue(TEXT("Unexpected missing active assignment remains a technical failure"),
		CorruptionSession->IsTechnicalAbort());
	TestEqual(TEXT("External corruption broadcasts its logical dequeue exactly once"), CorruptionBroadcasts, 1);
	TestEqual(TEXT("Technical-abort cleanup invalidates the execution token"),
		CorruptionNavigation->GetQueueNavigationStatus(CorruptionToken),
		ECustomerQueueNavigationStatus::Inactive);
	TestFalse(TEXT("Technical-abort cleanup removes the Counter delegate"),
		CorruptionNavigation->QueueChangedHandle.IsValid());
	World->DestroyWorld(false);
	GEngine->DestroyWorldContext(World);
	World->RemoveFromRoot();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBathhouseCheckoutKeyDropTest,
	"BathhouseSim.Interaction.CheckoutPhysicalKeyDrop",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBathhouseCheckoutKeyDropTest::RunTest(const FString& Parameters)
{
	if (!GEngine)
	{
		AddError(TEXT("GEngine is required for the checkout key drop world test."));
		return false;
	}
	const FName WorldName = MakeUniqueObjectName(nullptr, UWorld::StaticClass(), TEXT("CheckoutKeyDropAutomationWorld"));
	FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, WorldName, GetTransientPackage());
	if (!World)
	{
		GEngine->DestroyWorldContext(World);
		AddError(TEXT("Failed to create checkout key drop automation world."));
		return false;
	}
	World->AddToRoot();
	WorldContext.SetCurrentWorld(World);
	World->InitializeActorsForPlay(FURL());

	ABathhouseCounterActor* Counter = World->SpawnActor<ABathhouseCounterActor>(
		FVector(0.0f, 0.0f, 300.0f), FRotator(0.0f, 35.0f, 0.0f));
	Counter->GetReturnedKeyDropPoint()->SetRelativeRotation(FRotator(0.0f, 70.0f, 0.0f));
	Counter->ReturnedKeyDropLocalXYExtent = FVector2D(250.0f, 250.0f);
	Counter->ReturnedKeyDropAttemptCount = 32;
	int32 DeprecatedSlotBroadcastCount = 0;
	const FDelegateHandle DeprecatedSlotHandle = Counter->OnReturnedKeySlotsChangedNative.AddLambda(
		[&DeprecatedSlotBroadcastCount]() { ++DeprecatedSlotBroadcastCount; });

	AActor* FirstCustomer = World->SpawnActor<AActor>();
	ABathhouseKeyHookActor* FirstHook = World->SpawnActor<ABathhouseKeyHookActor>();
	ABathhouseKeyActor* FirstKey = World->SpawnActor<ABathhouseKeyActor>();
	FirstKey->KeyHook = FirstHook;
	TestEqual(TEXT("First checkout key retains its original hook identity"), FirstKey->GetKeyHook(), FirstHook);
	FirstKey->CommitState(EBathhouseKeyState::AssignedToCustomer, FirstCustomer);
	FirstKey->SetWorldPresentation(false, false);
	FirstKey->KeyPhysicsRoot->SetMassOverrideInKg(NAME_None, 50.0f, true);
	FirstKey->KeyPhysicsRoot->SetEnableGravity(false);
	const int32 ActorCountBeforeDrop = World->GetCurrentLevel()->Actors.Num();
	TestTrue(TEXT("Assigned key physically drops on the Counter"), FirstKey->TryPlaceOnCounter(*FirstCustomer, *Counter));
	TestEqual(TEXT("Checkout drop reuses the same Actor without spawning"),
		World->GetCurrentLevel()->Actors.Num(), ActorCountBeforeDrop);
	TestEqual(TEXT("Same key commits OnCounter"), FirstKey->GetKeyState(), EBathhouseKeyState::OnCounter);
	TestTrue(TEXT("Returned key simulates physics"), FirstKey->KeyPhysicsRoot->IsSimulatingPhysics());
	TestEqual(TEXT("Returned key ignores Pawn collision"),
		FirstKey->KeyPhysicsRoot->GetCollisionResponseToChannel(ECC_Pawn), ECR_Ignore);
	TestTrue(TEXT("Returned key enables CCD"), FirstKey->KeyPhysicsRoot->BodyInstance.bUseCCD);
	TestFalse(TEXT("Drop-point forward deliberately differs from Counter forward"),
		Counter->GetReturnedKeyDropPoint()->GetForwardVector().Equals(Counter->GetActorForwardVector(), 0.001f));
	const FVector ExpectedVelocity = Counter->GetReturnedKeyDropPoint()->GetForwardVector()
		* FirstKey->GetThrowImpulseStrength()
		+ FVector::UpVector * FirstKey->GetUpwardThrowImpulseStrength();
	const FVector FirstDropLocation = FirstKey->GetActorLocation();
	World->Tick(LEVELTICK_All, 1.0f / 60.0f);
	TestTrue(TEXT("Heavy returned key follows drop-point forward plus authored world-up velocity change"),
		FirstKey->KeyPhysicsRoot->GetPhysicsLinearVelocity().Equals(ExpectedVelocity, 1.0f));
	FirstKey->KeyPhysicsRoot->SetPhysicsLinearVelocity(FVector::ZeroVector);
	FirstKey->KeyPhysicsRoot->SetSimulatePhysics(false);
	FirstKey->SetActorLocation(FirstDropLocation, false, nullptr, ETeleportType::TeleportPhysics);
	FirstKey->KeyPhysicsRoot->UpdateOverlaps();
	TestTrue(TEXT("Repeated OnCounter placement is idempotent"), FirstKey->TryPlaceOnCounter(*FirstCustomer, *Counter));
	TestEqual(TEXT("Idempotent placement does not redrop the key"), FirstKey->GetActorLocation(), FirstDropLocation);

	AActor* SecondCustomer = World->SpawnActor<AActor>();
	ABathhouseKeyHookActor* SecondHook = World->SpawnActor<ABathhouseKeyHookActor>();
	ABathhouseKeyActor* SecondKey = World->SpawnActor<ABathhouseKeyActor>();
	SecondKey->KeyHook = SecondHook;
	SecondKey->CommitState(EBathhouseKeyState::AssignedToCustomer, SecondCustomer);
	SecondKey->SetWorldPresentation(false, false);
	SecondKey->KeyPhysicsRoot->SetMassOverrideInKg(NAME_None, 1.0f, true);
	SecondKey->KeyPhysicsRoot->SetEnableGravity(false);
	TestTrue(TEXT("A second key finds a collision-free candidate"), SecondKey->TryPlaceOnCounter(*SecondCustomer, *Counter));
	TestFalse(TEXT("Multiple returned keys do not occupy the same candidate"),
		SecondKey->GetActorLocation().Equals(FirstDropLocation, 0.1f));
	TestTrue(TEXT("The collision-free second key also enters physics"),
		SecondKey->KeyPhysicsRoot->IsSimulatingPhysics());

	AActor* CarryOwner = NewObject<AActor>();
	USceneComponent* HeldAnchor = NewObject<USceneComponent>(CarryOwner);
	CarryOwner->SetRootComponent(HeldAnchor);
	UPlayerCarryComponent* Carry = NewObject<UPlayerCarryComponent>(CarryOwner);
	Carry->ConfigureHeldAnchor(HeldAnchor);
	TestTrue(TEXT("Player pickup no longer needs a Counter slot release"), FirstKey->TryTakeFromCounter(*Carry));
	TestEqual(TEXT("Deprecated slot delegate is never broadcast by drop or pickup"), DeprecatedSlotBroadcastCount, 0);

	AActor* Blocker = World->SpawnActor<AActor>();
	UBoxComponent* BlockerRoot = NewObject<UBoxComponent>(Blocker, TEXT("ReturnedKeyBlocker"));
	Blocker->SetRootComponent(BlockerRoot);
	Blocker->AddInstanceComponent(BlockerRoot);
	BlockerRoot->SetBoxExtent(FVector(1000.0f));
	BlockerRoot->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	BlockerRoot->RegisterComponent();
	Blocker->SetActorLocation(Counter->GetActorLocation());
	AActor* BlockedCustomer = World->SpawnActor<AActor>();
	UCustomerSessionComponent* BlockedSession = NewObject<UCustomerSessionComponent>(BlockedCustomer);
	BlockedCustomer->AddInstanceComponent(BlockedSession);
	BlockedSession->RegisterComponent();
	BlockedSession->InitializeSession(NewObject<UCustomerRoutineDefinition>(), Counter);
	ABathhouseKeyHookActor* BlockedHook = World->SpawnActor<ABathhouseKeyHookActor>();
	ABathhouseKeyActor* BlockedKey = World->SpawnActor<ABathhouseKeyActor>();
	BlockedKey->KeyHook = BlockedHook;
	BlockedKey->CommitState(EBathhouseKeyState::AssignedToCustomer, BlockedCustomer);
	BlockedKey->SetWorldPresentation(false, false);
	BlockedSession->AssignedKey = BlockedKey;
	const FTransform BeforeBlockedPlacement = BlockedKey->GetActorTransform();
	const ECollisionEnabled::Type BeforeBlockedCollision = BlockedKey->KeyPhysicsRoot->GetCollisionEnabled();
	TestFalse(TEXT("Blocked exact and random candidates fail atomically"), BlockedSession->TryPlaceCheckoutKey());
	TestEqual(TEXT("Blocked placement preserves AssignedToCustomer"),
		BlockedKey->GetKeyState(), EBathhouseKeyState::AssignedToCustomer);
	TestTrue(TEXT("Blocked placement restores the exact transform"),
		BlockedKey->GetActorTransform().Equals(BeforeBlockedPlacement));
	TestTrue(TEXT("Blocked placement preserves the hidden customer-owned presentation"), BlockedKey->IsHidden());
	TestEqual(TEXT("Blocked placement preserves collision state"),
		BlockedKey->KeyPhysicsRoot->GetCollisionEnabled(), BeforeBlockedCollision);
	TestFalse(TEXT("Blocked placement never enables physics"), BlockedKey->KeyPhysicsRoot->IsSimulatingPhysics());
	TestEqual(TEXT("Blocked placement preserves the expected customer owner"),
		BlockedKey->StateOwner.Get(), static_cast<UObject*>(BlockedCustomer));
	TestFalse(TEXT("Cash cannot be created before physical OnCounter commit"),
		BlockedSession->TryCreateCashOffer(ABathhouseCashPaymentActor::StaticClass()));

	Counter->OnReturnedKeySlotsChangedNative.Remove(DeprecatedSlotHandle);
	World->DestroyWorld(false);
	GEngine->DestroyWorldContext(World);
	World->RemoveFromRoot();
	return true;
}

#endif
