#if WITH_DEV_AUTOMATION_TESTS

#include "Cleaning/WetMopActor.h"
#include "Combat/MonkeyWrenchActor.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Interaction/BathhouseKeyActor.h"
#include "Interaction/PhysicalCarryFixedSlotActor.h"
#include "Interaction/PlayerCarryComponent.h"
#include "Misc/AutomationTest.h"
#include "Misc/DataValidation.h"
#include "Towel/TowelBasketActor.h"
#include "Towel/TowelInventoryComponent.h"
#include "Tests/BathhouseCleaningTowelTestProbe.h"

namespace
{
class FScopedPhysicalCarryWorld
{
public:
	explicit FScopedPhysicalCarryWorld(const TCHAR* BaseName)
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

	~FScopedPhysicalCarryWorld()
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

void BeginPhysicalCarryActor(AActor* Actor)
{
	if (Actor && !Actor->HasActorBegunPlay())
	{
		Actor->DispatchBeginPlay();
	}
}

bool ConfigurePhysicalCarryMesh(AActor* Actor, UStaticMesh* Mesh)
{
	UStaticMeshComponent* Primitive = Actor ? Actor->FindComponentByClass<UStaticMeshComponent>() : nullptr;
	if (!Primitive || !Mesh)
	{
		return false;
	}
	Primitive->SetStaticMesh(Mesh);
	Primitive->SetWorldScale3D(FVector(0.2f));
	Primitive->UpdateBounds();
	return Primitive->Bounds.BoxExtent.GetMin() > KINDA_SMALL_NUMBER;
}
}

#if WITH_EDITOR
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBathhousePhysicalCarryFixedSlotDataValidationTest,
	"BathhouseSim.Interaction.PhysicalCarryFixedSlotDataValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBathhousePhysicalCarryFixedSlotDataValidationTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("Key physical root defaults to CCD"),
		GetDefault<ABathhouseKeyActor>()->GetPhysicalCarryPrimitive()->BodyInstance.bUseCCD);
	TestTrue(TEXT("Wet mop physical root defaults to CCD"),
		GetDefault<AWetMopActor>()->GetPhysicalCarryPrimitive()->BodyInstance.bUseCCD);
	TestTrue(TEXT("Towel basket physical root defaults to CCD"),
		GetDefault<ATowelBasketActor>()->GetPhysicalCarryPrimitive()->BodyInstance.bUseCCD);
	TestTrue(TEXT("Monkey wrench physical root defaults to CCD"),
		GetDefault<AMonkeyWrenchActor>()->GetPhysicalCarryPrimitive()->BodyInstance.bUseCCD);

	const APhysicalCarryFixedSlotActor* SlotCDO = GetDefault<APhysicalCarryFixedSlotActor>();
	if (!TestNotNull(TEXT("Physical carry fixed-slot CDO is available"), SlotCDO))
	{
		return false;
	}
	TestTrue(TEXT("Physical carry fixed-slot CDO is a template"), SlotCDO->IsTemplate());
	FDataValidationContext CDOContext;
	TestEqual(
		TEXT("A thin Blueprint-compatible CDO does not require an instance-only AssignedItem"),
		SlotCDO->IsDataValid(CDOContext),
		EDataValidationResult::Valid);
	TestEqual(TEXT("CDO validation emits no instance-assignment error"), CDOContext.GetNumErrors(), uint32(0));

	FScopedPhysicalCarryWorld TestWorld(TEXT("PhysicalCarrySlotValidationWorld"));
	UWorld* World = TestWorld.Get();
	if (!World)
	{
		AddError(TEXT("Failed to create the physical-carry validation automation world."));
		return false;
	}
	APhysicalCarryFixedSlotActor* UnassignedInstance = World->SpawnActor<APhysicalCarryFixedSlotActor>();
	if (!TestNotNull(TEXT("Unassigned placed-instance fixture is available"), UnassignedInstance))
	{
		return false;
	}
	TestFalse(TEXT("A placed fixed-slot Actor is not a template"), UnassignedInstance->IsTemplate());
	FDataValidationContext InstanceContext;
	TestEqual(
		TEXT("A placed fixed-slot instance still requires one exact AssignedItem"),
		UnassignedInstance->IsDataValid(InstanceContext),
		EDataValidationResult::Invalid);
	bool bFoundExactAssignmentError = false;
	for (const FDataValidationContext::FIssue& Issue : InstanceContext.GetIssues())
	{
		bFoundExactAssignmentError |= Issue.Message.ToString().Contains(
			TEXT("AssignedItem must reference one exact carryable Actor instance."));
	}
	TestTrue(TEXT("Placed-instance validation preserves the exact assignment error"), bFoundExactAssignmentError);
	return true;
}
#endif

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBathhousePhysicalCarryFixedSlotTest,
	"BathhouseSim.Interaction.PhysicalCarryFixedSlotHeldPoseAndRecovery",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBathhousePhysicalCarryFixedSlotTest::RunTest(const FString& Parameters)
{
	FScopedPhysicalCarryWorld TestWorld(TEXT("PhysicalCarryFixedSlotWorld"));
	UWorld* World = TestWorld.Get();
	if (!World)
	{
		AddError(TEXT("Failed to create the physical carry fixed-slot automation world."));
		return false;
	}

	UStaticMesh* CubeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (!TestNotNull(TEXT("Engine cube mesh is available"), CubeMesh))
	{
		return false;
	}

	AActor* CarrierOwner = World->SpawnActor<AActor>();
	USceneComponent* HeldAnchor = NewObject<USceneComponent>(CarrierOwner, TEXT("HeldAnchor"));
	UPlayerCarryComponent* Carry = NewObject<UPlayerCarryComponent>(CarrierOwner, TEXT("Carry"));
	CarrierOwner->SetRootComponent(HeldAnchor);
	CarrierOwner->AddInstanceComponent(HeldAnchor);
	CarrierOwner->AddInstanceComponent(Carry);
	HeldAnchor->RegisterComponent();
	HeldAnchor->SetWorldLocation(FVector(0.0f, 0.0f, 200.0f));
	Carry->RegisterComponent();
	Carry->ConfigureHeldAnchor(HeldAnchor);
	BeginPhysicalCarryActor(CarrierOwner);

	AWetMopActor* Mop = World->SpawnActor<AWetMopActor>();
	ATowelBasketActor* Basket = World->SpawnActor<ATowelBasketActor>();
	ATowelBasketActor* LightBasket = World->SpawnActor<ATowelBasketActor>();
	APhysicalCarryFixedSlotActor* MopSlot = World->SpawnActor<APhysicalCarryFixedSlotActor>();
	MopSlot->SetActorLocation(FVector(400.0f, 0.0f, 100.0f));
	MopSlot->AssignedItem = Mop;
	MopSlot->bStartOccupied = true;
	MopSlot->SlotDisplayName = FText::FromString(TEXT("물걸레 슬롯"));
	TestTrue(TEXT("Mop test mesh has physical bounds"), ConfigurePhysicalCarryMesh(Mop, CubeMesh));
	TestTrue(TEXT("Basket test mesh has physical bounds"), ConfigurePhysicalCarryMesh(Basket, CubeMesh));
	TestTrue(TEXT("Light basket test mesh has physical bounds"), ConfigurePhysicalCarryMesh(LightBasket, CubeMesh));
	BeginPhysicalCarryActor(Mop);
	BeginPhysicalCarryActor(Basket);
	BeginPhysicalCarryActor(LightBasket);
	BeginPhysicalCarryActor(MopSlot);

	FText SlotFailure;
	TestTrue(TEXT("A valid exact-item slot initializes operational"), MopSlot->IsPhysicalCarrySlotOperational(&SlotFailure));
	TestTrue(TEXT("bStartOccupied snaps the exact mop into its anchor"), MopSlot->IsOccupied());
	TestEqual(TEXT("Slotted item uses the slot anchor"), Mop->GetRootComponent()->GetAttachParent(), MopSlot->ItemAnchor.Get());
	TestFalse(TEXT("Slotted item physics is disabled"), Mop->GetPhysicalCarryPrimitive()->IsSimulatingPhysics());

	FPlayerInteractionContext SlotContext;
	SlotContext.Interactor = CarrierOwner;
	SlotContext.CarryComponent = Carry;
	const FPlayerInteractionQuery OccupiedQuery = MopSlot->QueryInteraction(SlotContext);
	TestTrue(TEXT("Occupied slot can be taken by an empty hand"), OccupiedQuery.bCanInteract);
	TestEqual(TEXT("Occupied slot uses the exact take wording"), OccupiedQuery.ActionName.ToString(), FString(TEXT("물건 가져가기")));
	TestTrue(TEXT("E take executes through the carry coordinator"), MopSlot->ExecuteInteraction(SlotContext).bSucceeded);
	TestTrue(TEXT("Slot take commits the exact held object"), Carry->GetHeldObject() == Mop);
	TestFalse(TEXT("Slot becomes empty only after take commit"), MopSlot->IsOccupied());

	const FTransform HeldPoseBeforeDrop = Mop->GetActorTransform();
	const FPlayerInteractionResult MopDrop = Carry->TryReleaseHeldEquipment(
		FVector(50000.0f, 50000.0f, 50000.0f),
		FVector::ForwardVector);
	TestTrue(TEXT("Compatibility wrapper free-drops the mop"), MopDrop.bSucceeded);
	TestTrue(TEXT("Camera origin is ignored and actual held pose is retained"),
		Mop->GetActorLocation().Equals(HeldPoseBeforeDrop.GetLocation(), 0.1f));
	TestEqual(TEXT("Free-world Pawn response remains Ignore"),
		Mop->GetPhysicalCarryPrimitive()->GetCollisionResponseToChannel(ECC_Pawn), ECR_Ignore);
	TestEqual(TEXT("Free-world WorldStatic response remains Block"),
		Mop->GetPhysicalCarryPrimitive()->GetCollisionResponseToChannel(ECC_WorldStatic), ECR_Block);
	TestTrue(TEXT("Common free-world transition forces CCD"),
		Mop->GetPhysicalCarryPrimitive()->BodyInstance.bUseCCD);

	FText CarryFailure;
	TestTrue(TEXT("An unassigned basket can be picked up from free world"),
		Carry->TryTakePhysicalObject(Basket, CarryFailure));
	const FPlayerInteractionQuery WrongItemQuery = MopSlot->QueryInteraction(SlotContext);
	TestFalse(TEXT("An empty exact slot rejects a different held item"), WrongItemQuery.bCanInteract);
	TestEqual(TEXT("Wrong item reports the exact failure wording"),
		WrongItemQuery.FailureReason.ToString(), FString(TEXT("이 슬롯에 놓는 물건이 아닙니다.")));
	TestTrue(TEXT("G remains free drop and never proximity-snaps the wrong item"),
		Carry->TryFreeDropHeldObject(FVector::ForwardVector).bSucceeded);
	TestFalse(TEXT("G near a slot leaves that slot empty"), MopSlot->IsOccupied());

	TestTrue(TEXT("Dropped exact item can be picked up again"), Carry->TryTakePhysicalObject(Mop, CarryFailure));
	TestTrue(TEXT("Exact held item stores through the fixed-slot transaction"),
		Carry->TryStoreHeldObjectInFixedSlot(MopSlot).bSucceeded);
	TestTrue(TEXT("Successful store clears the hand"), Carry->IsHandEmpty());
	TestTrue(TEXT("Successful store occupies the exact slot"), MopSlot->IsOccupied());

	APhysicalCarryFixedSlotActor* BasketSlot = World->SpawnActor<APhysicalCarryFixedSlotActor>();
	BasketSlot->SetActorLocation(FVector(400.0f, 200.0f, 100.0f));
	BasketSlot->AssignedItem = Basket;
	BasketSlot->bStartOccupied = false;
	BeginPhysicalCarryActor(BasketSlot);
	Basket->Inventory->State = ETowelState::Used;
	Basket->Inventory->Count = 3;
	Basket->Inventory->Revision = 7;
	const FTowelInventorySnapshot BasketBeforePlacement = Basket->GetInventory()->GetSnapshot();
	TestTrue(TEXT("Basket can be picked up after its empty slot binds"),
		Carry->TryTakePhysicalObject(Basket, CarryFailure));
	TestTrue(TEXT("Non-empty basket stores in its exact slot"),
		Carry->TryStoreHeldObjectInFixedSlot(BasketSlot).bSucceeded);
	TestTrue(TEXT("Non-empty basket can be taken back from its exact slot"),
		Carry->TryTakeFromFixedSlot(BasketSlot).bSucceeded);
	const FTowelInventorySnapshot BasketAfterPlacement = Basket->GetInventory()->GetSnapshot();
	TestEqual(TEXT("Basket slot round trip preserves towel state"), BasketAfterPlacement.State, BasketBeforePlacement.State);
	TestEqual(TEXT("Basket slot round trip preserves towel count"), BasketAfterPlacement.Count, BasketBeforePlacement.Count);
	TestEqual(TEXT("Basket slot round trip preserves towel revision"), BasketAfterPlacement.Revision, BasketBeforePlacement.Revision);

	HeldAnchor->SetWorldLocation(FVector(0.0f, 300.0f, 200.0f));
	Basket->GetPhysicalCarryPrimitive()->SetMassOverrideInKg(NAME_None, 50.0f, true);
	Basket->GetPhysicalCarryPrimitive()->SetEnableGravity(false);
	TestTrue(TEXT("Heavy basket free drop succeeds"), Carry->TryFreeDropHeldObject(FVector::ForwardVector).bSucceeded);

	TestTrue(TEXT("An independent light basket can be picked up for the mass comparison"),
		Carry->TryTakePhysicalObject(LightBasket, CarryFailure));
	HeldAnchor->SetWorldLocation(FVector(0.0f, 600.0f, 200.0f));
	LightBasket->GetPhysicalCarryPrimitive()->SetMassOverrideInKg(NAME_None, 1.0f, true);
	LightBasket->GetPhysicalCarryPrimitive()->SetEnableGravity(false);
	TestTrue(TEXT("Light basket free drop succeeds"), Carry->TryFreeDropHeldObject(FVector::ForwardVector).bSucceeded);
	World->Tick(LEVELTICK_All, 1.0f / 60.0f);
	const FVector HeavyVelocity = Basket->GetPhysicalCarryPrimitive()->GetPhysicsLinearVelocity();
	const FVector LightVelocity = LightBasket->GetPhysicalCarryPrimitive()->GetPhysicsLinearVelocity();
	TestTrue(TEXT("Heavy basket receives the authored 120/15 velocity change"),
		FMath::IsNearlyEqual(HeavyVelocity.X, 120.0f, 1.0f)
		&& FMath::IsNearlyEqual(HeavyVelocity.Z, 15.0f, 1.0f));
	TestTrue(TEXT("Light basket receives the authored 120/15 velocity change"),
		FMath::IsNearlyEqual(LightVelocity.X, 120.0f, 1.0f)
		&& FMath::IsNearlyEqual(LightVelocity.Z, 15.0f, 1.0f));
	TestTrue(TEXT("Velocity change is mass independent"), HeavyVelocity.Equals(LightVelocity, 1.0f));

	HeldAnchor->SetWorldLocation(FVector(0.0f, 0.0f, 200.0f));
	TestTrue(TEXT("Mop can be taken for overlap rollback"), Carry->TryTakeFromFixedSlot(MopSlot).bSucceeded);
	HeldAnchor->SetWorldLocation(FVector(0.0f, -300.0f, 200.0f));
	AActor* WallActor = World->SpawnActor<AActor>();
	UBoxComponent* Wall = NewObject<UBoxComponent>(WallActor, TEXT("HeldPoseBlocker"));
	WallActor->SetRootComponent(Wall);
	WallActor->AddInstanceComponent(Wall);
	Wall->SetBoxExtent(FVector(40.0f));
	Wall->SetCollisionObjectType(ECC_WorldStatic);
	Wall->SetCollisionResponseToAllChannels(ECR_Block);
	Wall->RegisterComponent();
	Wall->SetWorldLocation(Mop->GetRootComponent()->GetComponentLocation());
	Wall->UpdateBounds();
	Mop->GetPhysicalCarryPrimitive()->UpdateBounds();
	const FTransform BlockedHeldTransform = Mop->GetActorTransform();
	const FPlayerInteractionResult BlockedDrop = Carry->TryFreeDropHeldObject(FVector::ForwardVector);
	TestFalse(TEXT("World blocking overlap rejects free drop"), BlockedDrop.bSucceeded);
	TestTrue(TEXT("Blocked drop retains authoritative held identity"), Carry->GetHeldObject() == Mop);
	TestEqual(TEXT("Blocked drop retains held attachment"), Mop->GetRootComponent()->GetAttachParent(), HeldAnchor);
	TestTrue(TEXT("Blocked drop restores the complete held transform"), Mop->GetActorTransform().Equals(BlockedHeldTransform));
	TestFalse(TEXT("Blocked drop restores disabled physics"), Mop->GetPhysicalCarryPrimitive()->IsSimulatingPhysics());
	Wall->SetWorldLocation(FVector(1000.0f, -300.0f, 200.0f));
	Wall->UpdateBounds();

	APhysicalCarryFixedSlotActor* DuplicateBasketSlot = World->SpawnActor<APhysicalCarryFixedSlotActor>();
	DuplicateBasketSlot->AssignedItem = Basket;
	DuplicateBasketSlot->bStartOccupied = false;
	BeginPhysicalCarryActor(DuplicateBasketSlot);
	FText FirstDuplicateFailure;
	FText SecondDuplicateFailure;
	TestFalse(TEXT("Original slot is disabled by a duplicate exact assignment"),
		BasketSlot->IsPhysicalCarrySlotOperational(&FirstDuplicateFailure));
	TestFalse(TEXT("Duplicate slot is also disabled instead of winning arbitrarily"),
		DuplicateBasketSlot->IsPhysicalCarrySlotOperational(&SecondDuplicateFailure));

	TestFalse(TEXT("Mop slot is empty while its exact item remains held before carrier teardown"), MopSlot->IsOccupied());
	CarrierOwner->Destroy();
	TestTrue(TEXT("Carrier EndPlay recovers the held item into its valid empty slot"), MopSlot->IsOccupied());
	TestTrue(TEXT("Carrier EndPlay leaves no physics enabled in the fixed slot"),
		!Mop->GetPhysicalCarryPrimitive()->IsSimulatingPhysics());

	MopSlot->Destroy();
	TestTrue(TEXT("Runtime slot destruction releases the stored item to free world"),
		Mop->GetPhysicalCarryPrimitive()->IsSimulatingPhysics());
	TestTrue(TEXT("Runtime slot destruction release keeps common free-world CCD"),
		Mop->GetPhysicalCarryPrimitive()->BodyInstance.bUseCCD);
	TestNull(TEXT("Runtime slot destruction clears the item's slot binding"), Mop->GetAssignedPhysicalCarryFixedSlot());
	Mop->GetPhysicalCarryPrimitive()->SetUseCCD(false);
	Mop->RecoverPhysicalCarryable(nullptr);
	TestTrue(TEXT("Last-safe free-world recovery restores the default CCD contract"),
		Mop->GetPhysicalCarryPrimitive()->BodyInstance.bUseCCD);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBathhousePhysicalCarryAtomicCommitTest,
	"BathhouseSim.Interaction.PhysicalCarryAtomicCommitFailureAndReentry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBathhousePhysicalCarryAtomicCommitTest::RunTest(const FString& Parameters)
{
	FScopedPhysicalCarryWorld TestWorld(TEXT("PhysicalCarryAtomicCommitWorld"));
	UWorld* World = TestWorld.Get();
	if (!World)
	{
		AddError(TEXT("Failed to create the physical-carry atomic commit automation world."));
		return false;
	}

	UStaticMesh* CubeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (!TestNotNull(TEXT("Engine cube mesh is available for atomic commit coverage"), CubeMesh))
	{
		return false;
	}

	AActor* CarrierOwner = World->SpawnActor<AActor>();
	USceneComponent* HeldAnchor = NewObject<USceneComponent>(CarrierOwner, TEXT("AtomicHeldAnchor"));
	UPlayerCarryComponent* Carry = NewObject<UPlayerCarryComponent>(CarrierOwner, TEXT("AtomicCarry"));
	CarrierOwner->SetRootComponent(HeldAnchor);
	CarrierOwner->AddInstanceComponent(HeldAnchor);
	CarrierOwner->AddInstanceComponent(Carry);
	HeldAnchor->RegisterComponent();
	HeldAnchor->SetWorldLocation(FVector(0.0f, 0.0f, 200.0f));
	Carry->RegisterComponent();
	Carry->ConfigureHeldAnchor(HeldAnchor);
	BeginPhysicalCarryActor(CarrierOwner);

	ABathhousePhysicalCarryFailureProbeActor* Item =
		World->SpawnActor<ABathhousePhysicalCarryFailureProbeActor>();
	APhysicalCarryFixedSlotActor* Slot = World->SpawnActor<APhysicalCarryFixedSlotActor>();
	Slot->SetActorLocation(FVector(500.0f, 0.0f, 100.0f));
	Slot->AssignedItem = Item;
	Slot->bStartOccupied = true;
	TestTrue(TEXT("Atomic test item has physical bounds"), ConfigurePhysicalCarryMesh(Item, CubeMesh));
	BeginPhysicalCarryActor(Item);
	BeginPhysicalCarryActor(Slot);

	UBathhousePhysicalCarryCommitProbe* Probe = NewObject<UBathhousePhysicalCarryCommitProbe>();
	Probe->Bind(Item, Carry, Slot);
	const FTransform StoredTransform = Item->GetActorTransform();
	USceneComponent* StoredParent = Item->GetRootComponent()->GetAttachParent();
	const ECollisionEnabled::Type StoredCollision = Item->GetPhysicalCarryPrimitive()->GetCollisionEnabled();
	const bool bStoredPhysics = Item->GetPhysicalCarryPrimitive()->IsSimulatingPhysics();
	const FVector StoredLinearVelocity = Item->GetPhysicalCarryPrimitive()->GetPhysicsLinearVelocity();
	const FVector StoredAngularVelocity = Item->GetPhysicalCarryPrimitive()->GetPhysicsAngularVelocityInDegrees();

	Item->SetFailurePoint(ABathhousePhysicalCarryFailureProbeActor::EFailurePoint::FixedTake);
	TestFalse(TEXT("Forced late fixed take failure is rejected"), Carry->TryTakeFromFixedSlot(Slot).bSucceeded);
	TestTrue(TEXT("Failed fixed take restores an empty hand"), Carry->IsHandEmpty());
	TestNull(TEXT("Failed fixed take restores the concrete Carrier"), Item->Carrier.Get());
	TestTrue(TEXT("Failed fixed take restores slot occupancy"), Slot->IsOccupied());
	TestEqual(TEXT("Failed fixed take restores attachment"), Item->GetRootComponent()->GetAttachParent(), StoredParent);
	TestTrue(TEXT("Failed fixed take restores transform"), Item->GetActorTransform().Equals(StoredTransform));
	TestEqual(TEXT("Failed fixed take restores collision"), Item->GetPhysicalCarryPrimitive()->GetCollisionEnabled(), StoredCollision);
	TestEqual(TEXT("Failed fixed take restores physics state"), Item->GetPhysicalCarryPrimitive()->IsSimulatingPhysics(), bStoredPhysics);
	TestTrue(TEXT("Failed fixed take restores linear velocity"),
		Item->GetPhysicalCarryPrimitive()->GetPhysicsLinearVelocity().Equals(StoredLinearVelocity));
	TestTrue(TEXT("Failed fixed take restores angular velocity"),
		Item->GetPhysicalCarryPrimitive()->GetPhysicsAngularVelocityInDegrees().Equals(StoredAngularVelocity));
	TestEqual(TEXT("Failed fixed take emits no item presentation"), Probe->ItemPresentationCount, 0);
	TestEqual(TEXT("Failed fixed take emits no carry presentation"), Probe->CarryPresentationCount, 0);
	TestEqual(TEXT("Failed fixed take emits no slot presentation"), Probe->SlotPresentationCount, 0);

	Probe->bAttemptLowLevelReleaseWhenHeld = true;
	Item->SetFailurePoint(ABathhousePhysicalCarryFailureProbeActor::EFailurePoint::None);
	TestTrue(TEXT("Fixed take commits after all preparation succeeds"), Carry->TryTakeFromFixedSlot(Slot).bSucceeded);
	TestEqual(TEXT("Successful fixed take emits item presentation once"), Probe->ItemPresentationCount, 1);
	TestEqual(TEXT("Successful fixed take emits carry presentation once"), Probe->CarryPresentationCount, 1);
	TestEqual(TEXT("Successful fixed take emits slot presentation once"), Probe->SlotPresentationCount, 1);
	TestEqual(TEXT("Item presentation attempts one low-level held release"), Probe->LowLevelReleaseAttemptCount, 1);
	TestFalse(TEXT("Commit guard rejects synchronous low-level held release"), Probe->bLowLevelReleaseSucceeded);
	TestTrue(TEXT("Rejected low-level reentry leaves the exact item held"), Carry->GetHeldObject() == Item);
	TestTrue(TEXT("Rejected low-level reentry leaves the concrete Carrier committed"), Item->Carrier == Carry);
	TestFalse(TEXT("Successful fixed take leaves the slot empty"), Slot->IsOccupied());

	Probe->bAttemptLowLevelReleaseWhenHeld = false;
	Probe->ResetCounts();
	const FTransform HeldTransform = Item->GetActorTransform();
	USceneComponent* HeldParent = Item->GetRootComponent()->GetAttachParent();
	const ECollisionEnabled::Type HeldCollision = Item->GetPhysicalCarryPrimitive()->GetCollisionEnabled();
	const bool bHeldPhysics = Item->GetPhysicalCarryPrimitive()->IsSimulatingPhysics();
	const FVector HeldLinearVelocity = Item->GetPhysicalCarryPrimitive()->GetPhysicsLinearVelocity();
	const FVector HeldAngularVelocity = Item->GetPhysicalCarryPrimitive()->GetPhysicsAngularVelocityInDegrees();
	Item->SetFailurePoint(ABathhousePhysicalCarryFailureProbeActor::EFailurePoint::FixedStore);
	TestFalse(TEXT("Forced late fixed store failure is rejected"), Carry->TryStoreHeldObjectInFixedSlot(Slot).bSucceeded);
	TestTrue(TEXT("Failed fixed store restores HeldObject"), Carry->GetHeldObject() == Item);
	TestTrue(TEXT("Failed fixed store restores the concrete Carrier"), Item->Carrier == Carry);
	TestFalse(TEXT("Failed fixed store restores empty slot occupancy"), Slot->IsOccupied());
	TestEqual(TEXT("Failed fixed store restores attachment"), Item->GetRootComponent()->GetAttachParent(), HeldParent);
	TestTrue(TEXT("Failed fixed store restores transform"), Item->GetActorTransform().Equals(HeldTransform));
	TestEqual(TEXT("Failed fixed store restores collision"), Item->GetPhysicalCarryPrimitive()->GetCollisionEnabled(), HeldCollision);
	TestEqual(TEXT("Failed fixed store restores physics state"), Item->GetPhysicalCarryPrimitive()->IsSimulatingPhysics(), bHeldPhysics);
	TestTrue(TEXT("Failed fixed store restores linear velocity"),
		Item->GetPhysicalCarryPrimitive()->GetPhysicsLinearVelocity().Equals(HeldLinearVelocity));
	TestTrue(TEXT("Failed fixed store restores angular velocity"),
		Item->GetPhysicalCarryPrimitive()->GetPhysicsAngularVelocityInDegrees().Equals(HeldAngularVelocity));
	TestEqual(TEXT("Failed fixed store emits no item presentation"), Probe->ItemPresentationCount, 0);
	TestEqual(TEXT("Failed fixed store emits no carry presentation"), Probe->CarryPresentationCount, 0);
	TestEqual(TEXT("Failed fixed store emits no slot presentation"), Probe->SlotPresentationCount, 0);

	Item->SetFailurePoint(ABathhousePhysicalCarryFailureProbeActor::EFailurePoint::None);
	TestTrue(TEXT("Fixed store commits after all preparation succeeds"), Carry->TryStoreHeldObjectInFixedSlot(Slot).bSucceeded);
	TestEqual(TEXT("Successful fixed store emits item presentation once"), Probe->ItemPresentationCount, 1);
	TestEqual(TEXT("Successful fixed store emits carry presentation once"), Probe->CarryPresentationCount, 1);
	TestEqual(TEXT("Successful fixed store emits slot presentation once"), Probe->SlotPresentationCount, 1);
	TestTrue(TEXT("Successful fixed store clears the hand"), Carry->IsHandEmpty());
	TestNull(TEXT("Successful fixed store clears the concrete Carrier"), Item->Carrier.Get());
	TestTrue(TEXT("Successful fixed store occupies the slot"), Slot->IsOccupied());

	Probe->ResetCounts();
	TestTrue(TEXT("Item can be taken again for free-drop rollback"), Carry->TryTakeFromFixedSlot(Slot).bSucceeded);
	Probe->ResetCounts();
	Item->GetPhysicalCarryPrimitive()->SetUseCCD(false);
	const FTransform DropHeldTransform = Item->GetActorTransform();
	USceneComponent* DropHeldParent = Item->GetRootComponent()->GetAttachParent();
	const ECollisionEnabled::Type DropHeldCollision = Item->GetPhysicalCarryPrimitive()->GetCollisionEnabled();
	const bool bDropHeldPhysics = Item->GetPhysicalCarryPrimitive()->IsSimulatingPhysics();
	const FVector DropHeldLinearVelocity = Item->GetPhysicalCarryPrimitive()->GetPhysicsLinearVelocity();
	const FVector DropHeldAngularVelocity = Item->GetPhysicalCarryPrimitive()->GetPhysicsAngularVelocityInDegrees();
	const bool bDropHeldUseCCD = Item->GetPhysicalCarryPrimitive()->BodyInstance.bUseCCD;
	TestFalse(TEXT("Late free-drop rollback fixture starts with CCD disabled"), bDropHeldUseCCD);
	Item->SetFailurePoint(ABathhousePhysicalCarryFailureProbeActor::EFailurePoint::FreeDrop);
	TestFalse(TEXT("Forced late free-drop failure is rejected"), Carry->TryFreeDropHeldObject(FVector::ForwardVector).bSucceeded);
	TestTrue(TEXT("Failed free drop restores HeldObject"), Carry->GetHeldObject() == Item);
	TestTrue(TEXT("Failed free drop restores the concrete Carrier"), Item->Carrier == Carry);
	TestFalse(TEXT("Failed free drop keeps the assigned slot empty"), Slot->IsOccupied());
	TestEqual(TEXT("Failed free drop restores attachment"), Item->GetRootComponent()->GetAttachParent(), DropHeldParent);
	TestTrue(TEXT("Failed free drop restores transform"), Item->GetActorTransform().Equals(DropHeldTransform));
	TestEqual(TEXT("Failed free drop restores collision"), Item->GetPhysicalCarryPrimitive()->GetCollisionEnabled(), DropHeldCollision);
	TestEqual(TEXT("Failed free drop restores physics state"), Item->GetPhysicalCarryPrimitive()->IsSimulatingPhysics(), bDropHeldPhysics);
	TestTrue(TEXT("Failed free drop restores linear velocity"),
		Item->GetPhysicalCarryPrimitive()->GetPhysicsLinearVelocity().Equals(DropHeldLinearVelocity));
	TestTrue(TEXT("Failed free drop restores angular velocity"),
		Item->GetPhysicalCarryPrimitive()->GetPhysicsAngularVelocityInDegrees().Equals(DropHeldAngularVelocity));
	TestEqual(TEXT("Failed free drop restores the previous CCD value"),
		static_cast<bool>(Item->GetPhysicalCarryPrimitive()->BodyInstance.bUseCCD), bDropHeldUseCCD);
	TestEqual(TEXT("Failed free drop emits no item presentation"), Probe->ItemPresentationCount, 0);
	TestEqual(TEXT("Failed free drop emits no carry presentation"), Probe->CarryPresentationCount, 0);
	TestEqual(TEXT("Failed free drop emits no slot presentation"), Probe->SlotPresentationCount, 0);

	Item->SetFailurePoint(ABathhousePhysicalCarryFailureProbeActor::EFailurePoint::None);
	TestTrue(TEXT("Free drop commits after all preparation succeeds"), Carry->TryFreeDropHeldObject(FVector::ForwardVector).bSucceeded);
	TestEqual(TEXT("Successful free drop emits item presentation once"), Probe->ItemPresentationCount, 1);
	TestEqual(TEXT("Successful free drop emits carry presentation once"), Probe->CarryPresentationCount, 1);
	TestEqual(TEXT("Successful free drop emits no slot presentation"), Probe->SlotPresentationCount, 0);
	TestTrue(TEXT("Successful free drop clears the hand"), Carry->IsHandEmpty());
	TestNull(TEXT("Successful free drop clears the concrete Carrier"), Item->Carrier.Get());
	TestTrue(TEXT("Successful free drop enables physics"), Item->GetPhysicalCarryPrimitive()->IsSimulatingPhysics());
	TestTrue(TEXT("Successful free drop forces CCD after a previous rollback"),
		Item->GetPhysicalCarryPrimitive()->BodyInstance.bUseCCD);

	AWetMopActor* DestroyedFromPresentation = World->SpawnActor<AWetMopActor>();
	TestTrue(TEXT("Destruction fixture has physical bounds"), ConfigurePhysicalCarryMesh(DestroyedFromPresentation, CubeMesh));
	BeginPhysicalCarryActor(DestroyedFromPresentation);
	UBathhousePhysicalCarryCommitProbe* DestructionProbe = NewObject<UBathhousePhysicalCarryCommitProbe>();
	DestructionProbe->Bind(DestroyedFromPresentation, Carry);
	DestructionProbe->bDestroyItemOnHeldPresentation = true;
	FText TakeFailure;
	TestTrue(TEXT("A committed take tolerates item destruction from item presentation"),
		Carry->TryTakePhysicalObject(DestroyedFromPresentation, TakeFailure));
	TestFalse(TEXT("Presentation destruction invalidates the exact item"), IsValid(DestroyedFromPresentation));
	TestTrue(TEXT("Presentation destruction clears the surviving carry hand"), Carry->IsHandEmpty());
	TestEqual(TEXT("Presentation destruction emits the committed item event once"),
		DestructionProbe->ItemPresentationCount, 1);
	TestEqual(TEXT("Presentation destruction emits held then terminal-null carry events"),
		DestructionProbe->CarryPresentationCount, 2);
	return true;
}

#endif
