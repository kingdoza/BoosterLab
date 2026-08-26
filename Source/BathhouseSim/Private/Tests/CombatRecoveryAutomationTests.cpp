#if WITH_DEV_AUTOMATION_TESTS

#include "AIController.h"
#include "Camera/CameraComponent.h"
#include "Character/FirstPersonCharacter.h"
#include "Cleaning/WaterStainActor.h"
#include "Cleaning/WetMopActor.h"
#include "Combat/CombatTypes.h"
#include "Combat/HealthComponent.h"
#include "Combat/MeleeAttackComponent.h"
#include "Combat/MonkeyWrenchActor.h"
#include "Components/SceneComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Customer/BathhouseCustomerCharacter.h"
#include "Customer/CustomerKnockdownComponent.h"
#include "Customer/CustomerRoutineDefinition.h"
#include "Customer/CustomerRoutineInterruptionComponent.h"
#include "Customer/CustomerSessionComponent.h"
#include "Customer/StateTree/CustomerStateTreeTasks.h"
#include "Curves/CurveVector.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "EnhancedInputComponent.h"
#include "Facility/BathhouseFacilityActor.h"
#include "Facility/BathhouseFacilitySlotComponent.h"
#include "Facility/BathhouseCounterActor.h"
#include "GameFramework/DamageType.h"
#include "InputAction.h"
#include "Interaction/PlayerCarryComponent.h"
#include "Interaction/PlayerEquipmentUseComponent.h"
#include "Interaction/HeldEquipmentMotionComponent.h"
#include "Interaction/PhysicalCarryFixedSlotActor.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"
#include "Tests/BathhouseCleaningTowelTestProbe.h"
#include "UObject/GarbageCollection.h"
#include "UObject/UnrealType.h"

namespace
{
class FScopedCombatAutomationWorld
{
public:
	explicit FScopedCombatAutomationWorld(const TCHAR* BaseName)
	{
		if (!GEngine)
		{
			return;
		}
		const FName WorldName = MakeUniqueObjectName(nullptr, UWorld::StaticClass(), BaseName);
		FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
		World = UWorld::CreateWorld(EWorldType::Game, false, WorldName, GetTransientPackage());
		if (!World)
		{
			GEngine->DestroyWorldContext(World);
			return;
		}
		World->AddToRoot();
		WorldContext.SetCurrentWorld(World);
		World->InitializeActorsForPlay(FURL());
	}

	~FScopedCombatAutomationWorld()
	{
		if (World && GEngine)
		{
			World->DestroyWorld(false);
			GEngine->DestroyWorldContext(World);
			World->RemoveFromRoot();
		}
	}

	UWorld* Get() const { return World; }

private:
	UWorld* World = nullptr;
};

void BeginCombatTestActor(AActor* Actor)
{
	if (Actor && !Actor->HasActorBegunPlay())
	{
		Actor->DispatchBeginPlay();
	}
}

ABathhouseCustomerCharacter* SpawnRecoveryTestCustomer(UWorld* World, const FTransform& SpawnTransform = FTransform::Identity)
{
	if (!World)
	{
		return nullptr;
	}
	ABathhouseCustomerCharacter* DeferredCustomer = World->SpawnActorDeferred<ABathhouseCustomerCharacter>(
		ABathhouseCustomerCharacter::StaticClass(),
		SpawnTransform);
	if (!DeferredCustomer)
	{
		return nullptr;
	}
	DeferredCustomer->AutoPossessAI = EAutoPossessAI::Disabled;
	ABathhouseCustomerCharacter* Customer = Cast<ABathhouseCustomerCharacter>(
		UGameplayStatics::FinishSpawningActor(DeferredCustomer, SpawnTransform));
	BeginCombatTestActor(Customer);
	return Customer;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBathhouseEquipmentUseRoutingTest,
	"BathhouseSim.Interaction.PrimaryUseBindingEquipmentQueryAndDropCancel",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBathhouseEquipmentUseRoutingTest::RunTest(const FString& Parameters)
{
	FScopedCombatAutomationWorld TestWorld(TEXT("EquipmentUseAutomationWorld"));
	UWorld* World = TestWorld.Get();
	if (!World)
	{
		AddError(TEXT("Failed to create the equipment-use automation world."));
		return false;
	}

	AFirstPersonCharacter* Character = World->SpawnActor<AFirstPersonCharacter>();
	BeginCombatTestActor(Character);
	const FStructProperty* ActiveContextProperty = FindFProperty<FStructProperty>(
		UPlayerEquipmentUseComponent::StaticClass(),
		TEXT("ActiveContext"));
	TestNotNull(TEXT("The retained equipment context is reflected for GC traversal"), ActiveContextProperty);
	if (ActiveContextProperty)
	{
		TestTrue(TEXT("The retained equipment context is transient runtime state"),
			ActiveContextProperty->HasAnyPropertyFlags(CPF_Transient));
	}
	UInputAction* PrimaryAction = NewObject<UInputAction>();
	UInputAction* FallbackAction = NewObject<UInputAction>();
	Character->PrimaryUseAction = PrimaryAction;
	Character->ComputerClickAction = FallbackAction;
	UEnhancedInputComponent* PreferredInput = NewObject<UEnhancedInputComponent>(Character);
	Character->SetupPlayerInputComponent(PreferredInput);
	int32 PrimaryBindingCount = 0;
	int32 FallbackBindingCount = 0;
	for (const TUniquePtr<FEnhancedInputActionEventBinding>& Binding : PreferredInput->GetActionEventBindings())
	{
		PrimaryBindingCount += Binding->GetAction() == PrimaryAction ? 1 : 0;
		FallbackBindingCount += Binding->GetAction() == FallbackAction ? 1 : 0;
	}
	TestEqual(TEXT("Configured PrimaryUse binds Started/Triggered/Completed/Canceled exactly once"), PrimaryBindingCount, 4);
	TestEqual(TEXT("Configured PrimaryUse suppresses deprecated fallback bindings"), FallbackBindingCount, 0);

	AFirstPersonCharacter* FallbackCharacter = World->SpawnActor<AFirstPersonCharacter>();
	BeginCombatTestActor(FallbackCharacter);
	FallbackCharacter->PrimaryUseAction = nullptr;
	FallbackCharacter->ComputerClickAction = FallbackAction;
	UEnhancedInputComponent* FallbackInput = NewObject<UEnhancedInputComponent>(FallbackCharacter);
	FallbackCharacter->SetupPlayerInputComponent(FallbackInput);
	int32 FallbackOnlyBindingCount = 0;
	for (const TUniquePtr<FEnhancedInputActionEventBinding>& Binding : FallbackInput->GetActionEventBindings())
	{
		FallbackOnlyBindingCount += Binding->GetAction() == FallbackAction ? 1 : 0;
	}
	TestEqual(TEXT("Missing PrimaryUse binds only the deprecated fallback"), FallbackOnlyBindingCount, 4);

	UStaticMesh* CubeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	AWetMopActor* Mop = World->SpawnActor<AWetMopActor>();
	BeginCombatTestActor(Mop);
	UStaticMeshComponent* MopMesh = Mop->FindComponentByClass<UStaticMeshComponent>();
	MopMesh->SetStaticMesh(CubeMesh);
	MopMesh->SetWorldScale3D(FVector(0.2f));
	MopMesh->UpdateBounds();
	FText CarryFailure;
	TestTrue(TEXT("The generic hand takes the wet mop"),
		Character->GetPlayerCarry()->TryTakePhysicalObject(Mop, CarryFailure));
	const FPlayerInteractionQuery MopQuery = Character->GetPlayerEquipmentUse()->MergeEquipmentQuery(FPlayerInteractionQuery());
	TestTrue(TEXT("Held usable is the authoritative LMB row"), MopQuery.bEquipmentUseVisible);
	TestTrue(TEXT("Held mop exposes executable Hold use"), MopQuery.bCanEquipmentUse);
	TestEqual(TEXT("Held mop query uses Hold activation"),
		MopQuery.EquipmentActivationMode, EPlayerInteractionActivationMode::Hold);
	Character->PrimaryUseStartInput();
	TestEqual(TEXT("Equipment mode owns the press"),
		Character->PrimaryUsePressOwner,
		AFirstPersonCharacter::EPrimaryUsePressOwner::Equipment);
	TestTrue(TEXT("LMB Started begins no-target mopping"), Mop->IsMopping());
	CollectGarbage(RF_NoFlags);
	TestTrue(TEXT("Forced GC preserves the reflected active equipment use"),
		Character->GetPlayerEquipmentUse()->IsEquipmentUseInputActive());
	TestTrue(TEXT("Forced GC preserves the active mop domain state"), Mop->IsMopping());
	Character->PrimaryUseTriggeredInput();
	Character->GetFirstPersonCamera()->SetWorldLocation(FVector(1000.0f, 0.0f, 300.0f));
	MopMesh->UpdateBounds();
	const FPlayerInteractionResult MopDrop = Character->GetPlayerCarry()->TryReleaseHeldEquipment(
		Character->GetFirstPersonCamera()->GetComponentLocation(),
		FVector::ForwardVector);
	TestTrue(TEXT("G uses the held-position free-drop transaction"), MopDrop.bSucceeded);
	TestFalse(TEXT("G cancels active mop use before the drop commit"), Mop->IsMopping());
	Mop->SetActorEnableCollision(false);
	Character->PrimaryUseEndInput();
	TestEqual(TEXT("Release clears the original equipment press owner"),
		Character->PrimaryUsePressOwner,
		AFirstPersonCharacter::EPrimaryUsePressOwner::None);

	AMonkeyWrenchActor* Wrench = World->SpawnActor<AMonkeyWrenchActor>();
	BeginCombatTestActor(Wrench);
	UStaticMeshComponent* WrenchMesh = Wrench->FindComponentByClass<UStaticMeshComponent>();
	WrenchMesh->SetStaticMesh(CubeMesh);
	WrenchMesh->SetWorldScale3D(FVector(0.2f));
	WrenchMesh->UpdateBounds();
	TestTrue(TEXT("The empty generic hand takes the wrench"),
		Character->GetPlayerCarry()->TryTakePhysicalObject(Wrench, CarryFailure));
	TestEqual(TEXT("Wrench appends its carry kind without changing existing ordinals"),
		Character->GetPlayerCarry()->GetHeldKind(), EPhysicalCarryKind::MonkeyWrench);
	Character->PrimaryUseStartInput();
	Character->PrimaryUseEndInput();
	TestTrue(TEXT("Wrench attack continues after the instant press is released"), Wrench->GetMeleeAttack()->IsAttacking());
	WrenchMesh->UpdateBounds();
	const FPlayerInteractionResult WrenchDrop = Character->GetPlayerCarry()->TryReleaseHeldEquipment(
		Character->GetFirstPersonCamera()->GetComponentLocation(),
		FVector::ForwardVector);
	TestTrue(TEXT("Wrench supports rollback-safe held-position G drop"), WrenchDrop.bSucceeded);
	TestFalse(TEXT("G cancels an in-flight instant attack before commit"), Wrench->GetMeleeAttack()->IsAttacking());

	AMonkeyWrenchActor* EndingWrench = World->SpawnActor<AMonkeyWrenchActor>();
	BeginCombatTestActor(EndingWrench);
	TestTrue(TEXT("The hand can take another wrench after the committed drop"),
		Character->GetPlayerCarry()->TryTakePhysicalObject(EndingWrench, CarryFailure));
	Character->PrimaryUseStartInput();
	TestTrue(TEXT("A held Actor can end while its equipment press is active"),
		Character->GetPlayerEquipmentUse()->IsEquipmentUseInputActive());
	EndingWrench->Destroy();
	TestTrue(TEXT("Held Actor EndPlay clears the carry reference"), Character->GetPlayerCarry()->IsHandEmpty());
	TestFalse(TEXT("Held Actor EndPlay cancels the equipment-use owner"),
		Character->GetPlayerEquipmentUse()->IsEquipmentUseInputActive());
	Character->PrimaryUseEndInput();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBathhousePhysicalCarryFallRecoveryTest,
	"BathhouseSim.Interaction.PhysicalCarryHeldMopFallRecovery",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBathhousePhysicalCarryFallRecoveryTest::RunTest(const FString& Parameters)
{
	FScopedCombatAutomationWorld TestWorld(TEXT("PhysicalCarryFallRecoveryWorld"));
	UWorld* World = TestWorld.Get();
	if (!World)
	{
		AddError(TEXT("Failed to create the physical-carry fall recovery automation world."));
		return false;
	}

	AFirstPersonCharacter* Character = World->SpawnActor<AFirstPersonCharacter>();
	BeginCombatTestActor(Character);
	UHeldEquipmentMotionComponent* Motion = Character->FindComponentByClass<UHeldEquipmentMotionComponent>();
	if (!TestNotNull(TEXT("First-person fixture owns held-equipment motion"), Motion))
	{
		return false;
	}

	UStaticMesh* CubeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	AWetMopActor* Mop = World->SpawnActor<AWetMopActor>();
	UStaticMeshComponent* MopMesh = Mop ? Mop->FindComponentByClass<UStaticMeshComponent>() : nullptr;
	if (!TestNotNull(TEXT("Fall recovery mop owns its physical mesh"), MopMesh)
		|| !TestNotNull(TEXT("Fall recovery cube mesh is available"), CubeMesh))
	{
		return false;
	}
	MopMesh->SetStaticMesh(CubeMesh);
	MopMesh->SetWorldScale3D(FVector(0.2f));
	MopMesh->UpdateBounds();
	UCurveVector* MoppingCurve = NewObject<UCurveVector>(Mop, TEXT("FallRecoveryMoppingCurve"));
	MoppingCurve->FloatCurves[0].AddKey(0.0f, 0.0f);
	MoppingCurve->FloatCurves[0].AddKey(0.4f, 10.0f);
	Mop->MoppingPositionCurve = MoppingCurve;
	BeginCombatTestActor(Mop);

	APhysicalCarryFixedSlotActor* Slot = World->SpawnActor<APhysicalCarryFixedSlotActor>();
	Slot->SetActorLocation(FVector(600.0f, 0.0f, 100.0f));
	Slot->AssignedItem = Mop;
	Slot->bStartOccupied = false;
	BeginCombatTestActor(Slot);
	FText CarryFailure;
	TestTrue(TEXT("Character takes the exact mop assigned to an empty recovery slot"),
		Character->GetPlayerCarry()->TryTakePhysicalObject(Mop, CarryFailure));

	Character->PrimaryUseStartInput();
	TestTrue(TEXT("Hold mopping owns equipment input before the fall"),
		Character->GetPlayerEquipmentUse()->IsEquipmentUseInputActive());
	TestTrue(TEXT("Hold mopping activates mop domain state before the fall"), Mop->IsMopping());
	TestTrue(TEXT("Hold mopping activates held-equipment motion before the fall"), Motion->IsMotionActive());

	AWaterStainActor* Stain = World->SpawnActor<AWaterStainActor>();
	BeginCombatTestActor(Stain);
	FText StainFailure;
	TestTrue(TEXT("Fall fixture starts a stain cleaning session with the same player"),
		Stain->BeginMopCleaning(Character, StainFailure));
	Mop->ActiveStain = Stain;
	UBathhouseCleaningCancelProbe* CancelProbe = NewObject<UBathhouseCleaningCancelProbe>();
	CancelProbe->Bind(Stain);

	Mop->FellOutOfWorld(*GetDefault<UDamageType>());
	TestFalse(TEXT("Held mop fall clears the equipment input owner"),
		Character->GetPlayerEquipmentUse()->IsEquipmentUseInputActive());
	TestFalse(TEXT("Held mop fall stops the retained motion target"), Motion->IsMotionActive());
	TestTrue(TEXT("Held mop fall clears the authoritative carry hand"), Character->GetPlayerCarry()->IsHandEmpty());
	TestFalse(TEXT("Held mop fall cancels mopping domain state"), Mop->IsMopping());
	TestEqual(TEXT("Held mop fall cancels the active stain exactly once"), CancelProbe->CancelCount, 1);
	TestEqual(TEXT("Held mop fall resets stain state"), Stain->GetCleaningState(), EStainCleaningState::Idle);
	TestTrue(TEXT("Held mop fall resets stain progress"), FMath::IsNearlyZero(Stain->GetCleaningProgress()));
	TestTrue(TEXT("Held mop fall recovers into the exact empty fixed slot"), Slot->IsOccupied());
	TestEqual(TEXT("Recovered mop is attached to the exact slot anchor"),
		Mop->GetRootComponent()->GetAttachParent(), Slot->GetPhysicalCarryItemAnchor());
	TestTrue(TEXT("Recovered mop matches the exact slot location"),
		Mop->GetActorLocation().Equals(Slot->GetPhysicalCarryItemAnchor()->GetComponentLocation(), 0.01f));
	TestTrue(TEXT("Recovered mop matches the exact slot rotation"),
		Mop->GetActorQuat().Equals(Slot->GetPhysicalCarryItemAnchor()->GetComponentQuat(), KINDA_SMALL_NUMBER));
	TestFalse(TEXT("Recovered mop keeps physics disabled in the slot"), MopMesh->IsSimulatingPhysics());

	const FTransform RecoveredTransform = Mop->GetActorTransform();
	World->Tick(LEVELTICK_All, 1.0f / 30.0f);
	Character->PrimaryUseEndInput();
	World->Tick(LEVELTICK_All, 1.0f / 30.0f);
	TestTrue(TEXT("Post-fall motion tick and later input release cannot overwrite the slot pose"),
		Mop->GetActorTransform().Equals(RecoveredTransform));
	TestTrue(TEXT("Post-fall input release preserves exact slot occupancy"), Slot->IsOccupied());
	TestFalse(TEXT("Post-fall input release preserves disabled slot physics"), MopMesh->IsSimulatingPhysics());

	TestTrue(TEXT("The same recovered mop can be taken again"),
		Character->GetPlayerCarry()->TryTakeFromFixedSlot(Slot).bSucceeded);
	Character->PrimaryUseStartInput();
	TestTrue(TEXT("The same recovered mop can own a later equipment press"),
		Character->GetPlayerEquipmentUse()->IsEquipmentUseInputActive());
	TestTrue(TEXT("The same recovered mop can start mopping again"), Mop->IsMopping());
	TestTrue(TEXT("The same recovered mop can restart held motion"), Motion->IsMotionActive());
	Character->PrimaryUseEndInput();
	TestFalse(TEXT("Later use releases normally"), Character->GetPlayerEquipmentUse()->IsEquipmentUseInputActive());
	CancelProbe->Unbind();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBathhouseHealthComponentTest,
	"BathhouseSim.Combat.HealthClampDepletionAndRecovery",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBathhouseHealthComponentTest::RunTest(const FString& Parameters)
{
	FScopedCombatAutomationWorld TestWorld(TEXT("HealthAutomationWorld"));
	UWorld* World = TestWorld.Get();
	if (!World)
	{
		AddError(TEXT("Failed to create the combat automation world."));
		return false;
	}

	AActor* Owner = World->SpawnActor<AActor>();
	UHealthComponent* Health = NewObject<UHealthComponent>(Owner, TEXT("Health"));
	Owner->AddInstanceComponent(Health);
	Health->MaxHealth = 100.0f;
	Health->RegisterComponent();
	BeginCombatTestActor(Owner);
	TestEqual(TEXT("Health initializes to authored max"), Health->GetCurrentHealth(), 100.0f);

	FCombatDamageContext DamageContext;
	DamageContext.Damage = 25.0f;
	TestTrue(TEXT("Positive nonlethal damage commits"), Health->ApplyDamage(DamageContext));
	TestEqual(TEXT("Nonlethal damage clamps current health"), Health->GetCurrentHealth(), 75.0f);
	TestTrue(TEXT("Nonlethal health remains active"), Health->IsHealthActive());

	DamageContext.Damage = 1000.0f;
	TestTrue(TEXT("Lethal damage commits once"), Health->ApplyDamage(DamageContext));
	TestEqual(TEXT("Lethal damage clamps at zero"), Health->GetCurrentHealth(), 0.0f);
	TestTrue(TEXT("Zero health enters the depleted guard"), Health->IsDepleted());
	TestFalse(TEXT("Repeated damage while depleted is ignored"), Health->ApplyDamage(DamageContext));
	TestTrue(TEXT("Configured recovery restores health"), Health->RestoreHealthToRatio(0.5f));
	TestEqual(TEXT("Recovery ratio is clamped against max"), Health->GetCurrentHealth(), 50.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBathhouseMeleeAttackTest,
	"BathhouseSim.Combat.CameraSphereSingleAttackAndActorDedupe",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBathhouseMeleeAttackTest::RunTest(const FString& Parameters)
{
	FScopedCombatAutomationWorld TestWorld(TEXT("MeleeAutomationWorld"));
	UWorld* World = TestWorld.Get();
	if (!World)
	{
		AddError(TEXT("Failed to create the melee automation world."));
		return false;
	}

	AActor* User = World->SpawnActor<AActor>();
	UCameraComponent* Camera = NewObject<UCameraComponent>(User, TEXT("AttackCamera"));
	User->SetRootComponent(Camera);
	User->AddInstanceComponent(Camera);
	Camera->RegisterComponent();
	Camera->SetWorldLocationAndRotation(FVector::ZeroVector, FRotator::ZeroRotator);
	BeginCombatTestActor(User);

	AActor* Weapon = World->SpawnActor<AActor>();
	USceneComponent* WeaponRoot = NewObject<USceneComponent>(Weapon, TEXT("WeaponRoot"));
	UMeleeAttackComponent* Attack = NewObject<UMeleeAttackComponent>(Weapon, TEXT("MeleeAttack"));
	Weapon->SetRootComponent(WeaponRoot);
	Weapon->AddInstanceComponent(WeaponRoot);
	Weapon->AddInstanceComponent(Attack);
	WeaponRoot->RegisterComponent();
	Attack->RegisterComponent();
	BeginCombatTestActor(Weapon);

	AActor* Target = World->SpawnActor<AActor>();
	USphereComponent* FirstBody = NewObject<USphereComponent>(Target, TEXT("FirstBody"));
	USphereComponent* SecondBody = NewObject<USphereComponent>(Target, TEXT("SecondBody"));
	UHealthComponent* TargetHealth = NewObject<UHealthComponent>(Target, TEXT("TargetHealth"));
	Target->SetRootComponent(FirstBody);
	SecondBody->SetupAttachment(FirstBody);
	Target->AddInstanceComponent(FirstBody);
	Target->AddInstanceComponent(SecondBody);
	Target->AddInstanceComponent(TargetHealth);
	for (USphereComponent* Body : {FirstBody, SecondBody})
	{
		Body->InitSphereRadius(20.0f);
		Body->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		Body->SetCollisionResponseToAllChannels(ECR_Ignore);
		Body->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
		Body->RegisterComponent();
	}
	SecondBody->SetRelativeLocation(FVector(10.0f, 0.0f, 0.0f));
	TargetHealth->RegisterComponent();
	Target->SetActorLocation(FVector(200.0f, 0.0f, 0.0f));
	BeginCombatTestActor(Target);
	World->UpdateWorldComponents(true, false);

	Attack->AttackDistance = 200.0f;
	Attack->AttackRadius = 50.0f;
	Attack->TraceChannel = ECC_Pawn;
	Attack->Damage = 40.0f;
	Attack->HitTimeSeconds = 0.2f;
	Attack->AttackDurationSeconds = 0.5f;
	TestTrue(TEXT("First LMB Started begins one attack"), Attack->StartAttack(User, Weapon, Camera, nullptr));
	TestFalse(TEXT("Another Started during the attack is ignored"), Attack->StartAttack(User, Weapon, Camera, nullptr));
	Attack->TickComponent(0.25f, LEVELTICK_All, nullptr);
	TestEqual(TEXT("Multiple hit components on one Actor receive damage once"),
		TargetHealth->GetCurrentHealth(), 60.0f);
	Attack->TickComponent(0.30f, LEVELTICK_All, nullptr);
	TestFalse(TEXT("Attack ends after its authored duration"), Attack->IsAttacking());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBathhouseCustomerInterruptionTest,
	"BathhouseSim.CustomerRecovery.SoftInterruptionSerialAndBathTimer",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBathhouseCustomerInterruptionTest::RunTest(const FString& Parameters)
{
	FScopedCombatAutomationWorld TestWorld(TEXT("RecoveryAutomationWorld"));
	UWorld* World = TestWorld.Get();
	if (!World)
	{
		AddError(TEXT("Failed to create the recovery automation world."));
		return false;
	}

	ABathhouseCustomerCharacter* Customer = SpawnRecoveryTestCustomer(World);
	if (!Customer)
	{
		AddError(TEXT("Failed to spawn the production customer recovery fixture."));
		return false;
	}
	UCustomerSessionComponent* Session = Customer->GetCustomerSession();
	UCustomerRoutineInterruptionComponent* Interruption = Customer->GetCustomerRoutineInterruption();
	UCustomerRoutineDefinition* Definition = NewObject<UCustomerRoutineDefinition>();
	Definition->BathStayDurationSeconds = 10.0f;
	Session->InitializeSession(Definition, nullptr);
	TestTrue(TEXT("Total bath timer starts"), Session->StartBathStay());
	const float BeforePause = Session->GetRemainingBathStaySeconds();
	TestTrue(TEXT("First knockdown request soft-pauses the routine"), Interruption->BeginSoftInterruption());
	TestTrue(TEXT("The production interruption path pauses session timers automatically"),
		Session->AreRoutineTimersPaused());
	const uint64 FirstSerial = Interruption->GetInterruptionSerial();
	TestFalse(TEXT("Repeated soft-pause does not increment the serial"), Interruption->BeginSoftInterruption());
	TestEqual(TEXT("Repeated soft-pause preserves the serial"), Interruption->GetInterruptionSerial(), FirstSerial);
	World->Tick(LEVELTICK_All, 0.5f);
	TestTrue(TEXT("Paused total bath time preserves its remaining value"),
		FMath::IsNearlyEqual(Session->GetRemainingBathStaySeconds(), BeforePause, 0.05f));

	TestTrue(TEXT("Recovery resumes the same routine instance"), Interruption->EndSoftInterruption());
	TestFalse(TEXT("The production interruption path resumes session timers automatically"),
		Session->AreRoutineTimersPaused());
	World->Tick(LEVELTICK_All, 0.25f);
	TestTrue(TEXT("Resumed total bath timer continues from saved remaining time"),
		Session->GetRemainingBathStaySeconds() < BeforePause);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBathhouseCustomerKnockdownTest,
	"BathhouseSim.CustomerRecovery.CollisionAndCheckInInteractionRestore",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBathhouseCustomerKnockdownTest::RunTest(const FString& Parameters)
{
	FScopedCombatAutomationWorld TestWorld(TEXT("KnockdownCollisionAutomationWorld"));
	UWorld* World = TestWorld.Get();
	if (!World)
	{
		AddError(TEXT("Failed to create the knockdown collision automation world."));
		return false;
	}

	ABathhouseCustomerCharacter* Customer = SpawnRecoveryTestCustomer(World);
	ABathhouseCounterActor* Counter = World->SpawnActor<ABathhouseCounterActor>();
	BeginCombatTestActor(Counter);
	if (!Customer || !Counter)
	{
		AddError(TEXT("Failed to spawn the check-in recovery fixture."));
		return false;
	}

	UCustomerRoutineDefinition* Definition = NewObject<UCustomerRoutineDefinition>();
	UCustomerSessionComponent* Session = Customer->GetCustomerSession();
	UCustomerKnockdownComponent* Knockdown = Customer->GetCustomerKnockdown();
	UCapsuleComponent* Capsule = Customer->GetCapsuleComponent();
	USkeletalMeshComponent* Mesh = Customer->GetMesh();
	Session->InitializeSession(Definition, Counter);
	TestTrue(TEXT("The customer joins the check-in queue"), Session->JoinQueue(EBathhouseCounterLane::CheckIn));
	Session->BeginWaitingForCheckIn();

	const FPlayerInteractionContext InteractionContext;
	TestTrue(TEXT("A standing front customer exposes the check-in prompt"),
		Customer->QueryInteraction(InteractionContext).bVisible);
	Knockdown->bKnockedDown = true;
	TestFalse(TEXT("A knocked-down customer hides the check-in prompt"),
		Customer->QueryInteraction(InteractionContext).bVisible);
	const FPlayerInteractionResult KnockedDownResult = Customer->ExecuteInteraction(InteractionContext);
	TestFalse(TEXT("A direct interaction attempt cannot bypass the knockdown guard"), KnockedDownResult.bSucceeded);
	TestTrue(TEXT("The knockdown guard returns its explicit failure reason"),
		KnockedDownResult.FailureReason.ToString().Contains(TEXT("쓰러진 손님")));

	Mesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Mesh->SetCollisionObjectType(ECC_Pawn);
	Mesh->SetCollisionResponseToAllChannels(ECR_Block);
	Mesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	Mesh->SetCollisionResponseToChannel(ECC_Vehicle, ECR_Ignore);
	Capsule->SetCollisionProfileName(TEXT("Pawn"));
	Capsule->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

	Knockdown->SavedCapsuleCollisionProfile = Capsule->GetCollisionProfileName();
	Knockdown->SavedMeshCollisionProfile = Mesh->GetCollisionProfileName();
	Knockdown->SavedCapsuleCollisionEnabled = Capsule->GetCollisionEnabled();
	Knockdown->SavedMeshCollisionEnabled = Mesh->GetCollisionEnabled();
	Knockdown->SavedMeshCollisionObjectType = Mesh->GetCollisionObjectType();
	Knockdown->SavedMeshCollisionResponses = Mesh->GetCollisionResponseToChannels();

	Mesh->SetCollisionProfileName(TEXT("Ragdoll"));
	Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	TestEqual(TEXT("Ragdoll suppresses the interaction trace"),
		Mesh->GetCollisionResponseToChannel(ECC_Visibility), ECR_Ignore);

	Knockdown->RestoreCollisionState(*Mesh, *Capsule);
	TestEqual(TEXT("Recovery restores the custom mesh profile"), Mesh->GetCollisionProfileName(), FName(TEXT("Custom")));
	TestEqual(TEXT("Recovery restores mesh query collision"),
		Mesh->GetCollisionEnabled(), ECollisionEnabled::QueryOnly);
	TestEqual(TEXT("Recovery restores the mesh object type"), Mesh->GetCollisionObjectType(), ECC_Pawn);
	TestEqual(TEXT("Recovery restores the interaction trace response"),
		Mesh->GetCollisionResponseToChannel(ECC_Visibility), ECR_Block);
	TestEqual(TEXT("Recovery preserves the authored pawn response"),
		Mesh->GetCollisionResponseToChannel(ECC_Pawn), ECR_Ignore);
	TestEqual(TEXT("Recovery restores capsule collision"),
		Capsule->GetCollisionEnabled(), ECollisionEnabled::QueryAndPhysics);

	Knockdown->bKnockedDown = false;
	TestTrue(TEXT("The recovered customer exposes the check-in prompt again"),
		Customer->QueryInteraction(InteractionContext).bVisible);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBathhouseCustomerRecoveryFacilityAndOperationTest,
	"BathhouseSim.CustomerRecovery.FacilityAndOperationInvalidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBathhouseCustomerRecoveryFacilityAndOperationTest::RunTest(const FString& Parameters)
{
	FScopedCombatAutomationWorld TestWorld(TEXT("RecoveryFacilityAutomationWorld"));
	UWorld* World = TestWorld.Get();
	if (!World)
	{
		AddError(TEXT("Failed to create the facility recovery automation world."));
		return false;
	}

	ABathhouseCustomerCharacter* Customer = SpawnRecoveryTestCustomer(World);
	if (!Customer)
	{
		AddError(TEXT("Failed to spawn the facility recovery customer."));
		return false;
	}
	UCustomerSessionComponent* Session = Customer->GetCustomerSession();
	UCustomerRoutineInterruptionComponent* Interruption = Customer->GetCustomerRoutineInterruption();
	UCustomerRoutineDefinition* Definition = NewObject<UCustomerRoutineDefinition>();
	Session->InitializeSession(Definition, nullptr);

	const FTransform FacilityTransform(FRotator(0.0f, 25.0f, 0.0f), FVector(600.0f, 0.0f, 0.0f));
	ABathhouseFacilityActor* DeferredFacility = World->SpawnActorDeferred<ABathhouseFacilityActor>(
		ABathhouseFacilityActor::StaticClass(),
		FacilityTransform);
	if (!DeferredFacility)
	{
		AddError(TEXT("Failed to spawn the recovery facility."));
		return false;
	}
	UBathhouseFacilitySlotComponent* Slot = NewObject<UBathhouseFacilitySlotComponent>(
		DeferredFacility,
		TEXT("RecoverySlot"));
	DeferredFacility->AddInstanceComponent(Slot);
	Slot->SetupAttachment(DeferredFacility->GetRootComponent());
	ABathhouseFacilityActor* Facility = Cast<ABathhouseFacilityActor>(
		UGameplayStatics::FinishSpawningActor(DeferredFacility, FacilityTransform));
	if (!Slot->IsRegistered())
	{
		Slot->RegisterComponent();
	}
	BeginCombatTestActor(Facility);
	TestTrue(TEXT("Deferred facility discovers its authored recovery slot"),
		Facility && Facility->GetFacilitySlots().Contains(Slot));

	TestTrue(TEXT("The production session reserves the Bath facility"),
		Session->TryReserveFacility(EBathhouseFacilityType::Bath));
	FTransform CachedApproach;
	FTransform CachedAction;
	TestTrue(TEXT("The reservation caches the Bath approach transform"),
		Session->GetCurrentFacilityTransform(true, CachedApproach));
	TestTrue(TEXT("The reservation caches the Bath action transform"),
		Session->GetCurrentFacilityTransform(false, CachedAction));
	TestTrue(TEXT("The customer reaches the cached Bath action point"),
		Session->SnapCurrentFacility(ECustomerFacilitySnapTarget::ActionPoint));
	TestTrue(TEXT("The reserved slot enters occupied use"), Session->BeginUseCurrentFacility());
	TestEqual(TEXT("The facility is occupied before interruption"),
		Slot->GetSlotState(), EBathhouseFacilitySlotState::Occupied);

	TestTrue(TEXT("Production interruption suspends an occupied facility use"),
		Interruption->BeginSoftInterruption());
	TestEqual(TEXT("Suspended facility use returns Occupied to Reserved"),
		Slot->GetSlotState(), EBathhouseFacilitySlotState::Reserved);
	TestTrue(TEXT("Suspension preserves the reservation owner"), Slot->GetCurrentUser() == Customer);
	TestTrue(TEXT("The session retains its current facility while suspended"), Session->HasCurrentFacility());
	TestTrue(TEXT("The session records that facility use needs recovery"),
		Session->IsCurrentFacilityUseSuspendedForKnockdown());
	Slot->SetWorldTransform(FTransform(FRotator(0.0f, -80.0f, 0.0f), FVector(1400.0f, 500.0f, 0.0f)));
	FTransform PreservedAction;
	TestTrue(TEXT("Suspended Bath use still resolves its reservation-time cache"),
		Session->GetCurrentFacilityTransform(false, PreservedAction));
	TestTrue(TEXT("Live facility mutation cannot replace the cached action transform"),
		PreservedAction.Equals(CachedAction));
	TestTrue(TEXT("Routine recovery resumes after the facility suspension"),
		Interruption->EndSoftInterruption());
	TestTrue(TEXT("Successful local recovery returns the reservation to occupied"),
		Session->ResumeCurrentFacilityUseAfterKnockdown());
	TestEqual(TEXT("Recovered facility is occupied by the same customer"),
		Slot->GetSlotState(), EBathhouseFacilitySlotState::Occupied);
	TestTrue(TEXT("Recovered facility keeps the same owner"), Slot->GetCurrentUser() == Customer);
	TestFalse(TEXT("Successful recovery clears the facility suspension flag"),
		Session->IsCurrentFacilityUseSuspendedForKnockdown());

	TestTrue(TEXT("A second interruption can suspend the recovered use"),
		Interruption->BeginSoftInterruption());
	TestEqual(TEXT("The second suspension is also Reserved"),
		Slot->GetSlotState(), EBathhouseFacilitySlotState::Reserved);
	Slot->ForceRelease();
	TestTrue(TEXT("The routine itself still resumes after external facility invalidation"),
		Interruption->EndSoftInterruption());
	TestFalse(TEXT("An invalidated reservation cannot be promoted to occupied"),
		Session->ResumeCurrentFacilityUseAfterKnockdown());
	TestEqual(TEXT("Failed recovery does not recreate or duplicate the reservation"),
		Slot->GetSlotState(), EBathhouseFacilitySlotState::Available);
	TestNull(TEXT("Failed recovery leaves no facility owner"), Slot->GetCurrentUser());
	Session->ReleaseCurrentFacility();
	TestFalse(TEXT("Existing cleanup clears the invalidated current facility"), Session->HasCurrentFacility());
	TestFalse(TEXT("Existing cleanup clears the suspension flag"),
		Session->IsCurrentFacilityUseSuspendedForKnockdown());
	TestEqual(TEXT("Cleanup keeps the invalidated slot available"),
		Slot->GetSlotState(), EBathhouseFacilitySlotState::Available);

	const uint64 SupersededToken = Interruption->RegisterRestartableOperation();
	const uint64 ReplacementToken = Interruption->RegisterRestartableOperation();
	TestFalse(TEXT("The first operation token is superseded"),
		Interruption->IsRestartableOperationCurrent(SupersededToken));
	TestTrue(TEXT("The replacement operation token is current"),
		Interruption->IsRestartableOperationCurrent(ReplacementToken));
	FCustomerRestartableMoveToTaskInstanceData MoveData;
	MoveData.Customer = Customer;
	MoveData.OperationToken = SupersededToken;
	MoveData.bMoveSucceeded = true;
	TestEqual(TEXT("A superseded MoveTo operation fails instead of remaining Running"),
		FCustomerRestartableMoveToTask::InvalidateSupersededOperation(MoveData),
		EStateTreeRunStatus::Failed);
	TestEqual(TEXT("Invalidation clears the stale local operation token"), MoveData.OperationToken, uint64(0));
	TestFalse(TEXT("An old completion cannot report success after replacement"), MoveData.bMoveSucceeded);
	TestTrue(TEXT("Invalidating the old operation cannot clear the replacement token"),
		Interruption->IsRestartableOperationCurrent(ReplacementToken));
	Interruption->ClearRestartableOperation(ReplacementToken);
	TestFalse(TEXT("Focused cleanup leaves no active replacement token"),
		Interruption->IsRestartableOperationCurrent(ReplacementToken));
	return true;
}

#endif
