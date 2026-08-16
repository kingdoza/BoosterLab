#if WITH_DEV_AUTOMATION_TESTS

#include "Tests/BathhouseCleaningTowelTestProbe.h"

#include "Cleaning/CleaningWorldSubsystem.h"
#include "Cleaning/CleaningDirectorActor.h"
#include "Cleaning/StainSpawnZoneActor.h"
#include "Cleaning/WaterStainActor.h"
#include "Cleaning/WetMopActor.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Customer/BathhouseCustomerCharacter.h"
#include "Customer/CustomerRoutineDefinition.h"
#include "Customer/CustomerSessionComponent.h"
#include "Customer/StateTree/CustomerTowelStateTreeTasks.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Facility/BathhouseFacilityActor.h"
#include "Facility/BathhouseFacilitySlotComponent.h"
#include "GameFramework/Pawn.h"
#include "Interaction/BathhouseKeyActor.h"
#include "Interaction/BathhouseKeyHookActor.h"
#include "Interaction/PlayerCarryComponent.h"
#include "Interaction/PlayerInteractionComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/Material.h"
#include "Misc/AutomationTest.h"
#include "Towel/CleanTowelStackActor.h"
#include "Towel/TowelBasketActor.h"
#include "Towel/TowelCirculationSubsystem.h"
#include "Towel/TowelInventoryComponent.h"
#include "Towel/TowelProcessingMachineActor.h"
#include "Towel/TowelTransferSubsystem.h"
#include "Towel/UsedTowelBinActor.h"
#include "Towel/WorldUsedTowelActor.h"
#include "UI/InteractionPromptWidget.h"
#include "UObject/UnrealType.h"

namespace
{
class FScopedBathhouseAutomationWorld
{
public:
	explicit FScopedBathhouseAutomationWorld(const TCHAR* BaseName)
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

	~FScopedBathhouseAutomationWorld()
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

void BeginActorForTest(AActor* Actor)
{
	if (Actor && !Actor->HasActorBegunPlay())
	{
		Actor->DispatchBeginPlay();
	}
}

bool ConfigureCarryPrimitiveForTest(AActor* Actor, UStaticMesh* Mesh)
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBathhouseTowelTransferTest,
	"BathhouseSim.Towel.AtomicTransferMachineAndRecovery",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBathhouseTowelTransferTest::RunTest(const FString& Parameters)
{
	FScopedBathhouseAutomationWorld TestWorld(TEXT("TowelAutomationWorld"));
	UWorld* World = TestWorld.Get();
	if (!World)
	{
		AddError(TEXT("Failed to create the towel automation world."));
		return false;
	}

	AActor* SourceOwner = World->SpawnActor<AActor>();
	AActor* DestinationOwner = World->SpawnActor<AActor>();
	UTowelInventoryComponent* Source = NewObject<UTowelInventoryComponent>(SourceOwner, TEXT("SourceInventory"));
	UTowelInventoryComponent* Destination = NewObject<UTowelInventoryComponent>(DestinationOwner, TEXT("DestinationInventory"));
	SourceOwner->AddInstanceComponent(Source);
	DestinationOwner->AddInstanceComponent(Destination);
	Source->RegisterComponent();
	Destination->RegisterComponent();
	Source->Capacity = 10;
	Source->State = ETowelState::Used;
	Source->Count = 4;
	Source->Revision = 0;
	Destination->Capacity = 3;
	Destination->State = ETowelState::None;
	Destination->Count = 0;
	Destination->Revision = 0;

	UTowelTransferSubsystem* Transfer = World->GetSubsystem<UTowelTransferSubsystem>();
	TestNotNull(TEXT("The towel transfer subsystem exists"), Transfer);
	UBathhouseTowelAtomicCommitProbe* Probe = NewObject<UBathhouseTowelAtomicCommitProbe>();
	Probe->Bind(Source, Destination);

	FTowelTransferRequest Request;
	Request.Source = Source;
	Request.Destination = Destination;
	Request.RequestedCount = 1;
	Request.ExpectedSourceRevision = 0;
	Request.ExpectedDestinationRevision = 0;
	const FTowelTransferResult PrimaryResult = Transfer->TryTransfer(Request);
	TestTrue(TEXT("Primary transfer succeeds"), PrimaryResult.bSucceeded);
	TestEqual(TEXT("Primary transfer moves exactly one towel"), PrimaryResult.MovedCount, 1);
	TestEqual(TEXT("Source is decremented before notification"), Probe->ObservedAtBroadcast.Count, 3);
	TestEqual(TEXT("Destination is already committed during source notification"), Probe->PeerAtBroadcast.Count, 1);
	TestEqual(TEXT("Both notifications share the committed transaction id"),
		Probe->ObservedTransactionId, PrimaryResult.TransactionId);

	Request.RequestedCount = MAX_int32;
	Request.ExpectedSourceRevision = Source->GetSnapshot().Revision;
	Request.ExpectedDestinationRevision = Destination->GetSnapshot().Revision;
	const FTowelTransferResult SecondaryResult = Transfer->TryTransfer(Request);
	TestTrue(TEXT("Max transfer succeeds"), SecondaryResult.bSucceeded);
	TestEqual(TEXT("Max transfer stops at destination capacity"), SecondaryResult.MovedCount, 2);
	TestEqual(TEXT("Conservation keeps one towel at the source"), Source->GetSnapshot().Count, 1);
	TestEqual(TEXT("Conservation fills the destination with three towels"), Destination->GetSnapshot().Count, 3);
	Probe->Unbind();

	AActor* MixedOwner = World->SpawnActor<AActor>();
	UTowelInventoryComponent* Mixed = NewObject<UTowelInventoryComponent>(MixedOwner, TEXT("MixedInventory"));
	MixedOwner->AddInstanceComponent(Mixed);
	Mixed->RegisterComponent();
	Mixed->Capacity = 5;
	Mixed->State = ETowelState::Clean;
	Mixed->Count = 1;
	const FTowelInventorySnapshot SourceBeforeMixed = Source->GetSnapshot();
	const FTowelInventorySnapshot MixedBefore = Mixed->GetSnapshot();
	Request.Destination = Mixed;
	Request.RequestedCount = 1;
	Request.ExpectedSourceRevision = SourceBeforeMixed.Revision;
	Request.ExpectedDestinationRevision = MixedBefore.Revision;
	const FTowelTransferResult MixedResult = Transfer->TryTransfer(Request);
	TestEqual(TEXT("Mixed-state transfer is rejected"), MixedResult.Failure, ETowelTransferFailure::StateMismatch);
	TestEqual(TEXT("Mixed failure preserves source count"), Source->GetSnapshot().Count, SourceBeforeMixed.Count);
	TestEqual(TEXT("Mixed failure preserves destination count"), Mixed->GetSnapshot().Count, MixedBefore.Count);

	Request.Destination = Destination;
	Request.ExpectedSourceRevision = Source->GetSnapshot().Revision;
	Request.ExpectedDestinationRevision = Destination->GetSnapshot().Revision;
	TestEqual(TEXT("Full destination is rejected"),
		Transfer->TryTransfer(Request).Failure, ETowelTransferFailure::DestinationFull);
	Request.Destination = Mixed;
	Request.ExpectedSourceRevision = Source->GetSnapshot().Revision + 1;
	Request.ExpectedDestinationRevision = Mixed->GetSnapshot().Revision;
	TestEqual(TEXT("Stale revisions are rejected"),
		Transfer->TryTransfer(Request).Failure, ETowelTransferFailure::RevisionMismatch);
	Request.ExpectedSourceRevision = Source->GetSnapshot().Revision;
	Mixed->State = ETowelState::None;
	Mixed->Count = 0;
	TestTrue(TEXT("The reentry test acquires the source transaction guard"), Source->TryBeginTransaction());
	TestEqual(TEXT("An overlapping transfer is rejected as reentry"),
		Transfer->TryTransfer(Request).Failure, ETowelTransferFailure::Reentry);
	Source->EndTransaction();

	ATowelProcessingMachineActor* Machine = World->SpawnActor<ATowelProcessingMachineActor>();
	ATowelBasketActor* InputBasket = World->SpawnActor<ATowelBasketActor>();
	ATowelBasketActor* WrongInputBasket = World->SpawnActor<ATowelBasketActor>();
	ATowelBasketActor* CompleteInputBasket = World->SpawnActor<ATowelBasketActor>();
	ATowelBasketActor* OutputBasket = World->SpawnActor<ATowelBasketActor>();
	BeginActorForTest(Machine);
	BeginActorForTest(InputBasket);
	BeginActorForTest(WrongInputBasket);
	BeginActorForTest(CompleteInputBasket);
	BeginActorForTest(OutputBasket);
	Machine->Inventory->Capacity = 5;
	Machine->Inventory->State = ETowelState::None;
	Machine->Inventory->Count = 0;
	InputBasket->GetInventory()->Capacity = 5;
	InputBasket->GetInventory()->State = ETowelState::Used;
	InputBasket->GetInventory()->Count = 2;
	WrongInputBasket->GetInventory()->Capacity = 5;
	WrongInputBasket->GetInventory()->State = ETowelState::Clean;
	WrongInputBasket->GetInventory()->Count = 1;
	CompleteInputBasket->GetInventory()->Capacity = 5;
	CompleteInputBasket->GetInventory()->State = ETowelState::Wet;
	CompleteInputBasket->GetInventory()->Count = 1;
	OutputBasket->GetInventory()->Capacity = 5;
	OutputBasket->GetInventory()->State = ETowelState::None;
	OutputBasket->GetInventory()->Count = 0;

	Request.Source = WrongInputBasket->GetInventory();
	Request.Destination = Machine->GetInventory();
	Request.RequestedCount = MAX_int32;
	Request.ExpectedSourceRevision = Request.Source->GetSnapshot().Revision;
	Request.ExpectedDestinationRevision = Request.Destination->GetSnapshot().Revision;
	const FTowelInventorySnapshot WrongInputBefore = Request.Source->GetSnapshot();
	const FTowelInventorySnapshot EmptyMachineBefore = Request.Destination->GetSnapshot();
	TestEqual(TEXT("A Waiting washer rejects a wrong-state basket directly"),
		Transfer->TryTransfer(Request).Failure, ETowelTransferFailure::EndpointBlocked);
	TestEqual(TEXT("Wrong-state rejection preserves the basket"),
		Request.Source->GetSnapshot().Count, WrongInputBefore.Count);
	TestEqual(TEXT("Wrong-state rejection preserves the machine"),
		Request.Destination->GetSnapshot().Count, EmptyMachineBefore.Count);
	TestEqual(TEXT("Wrong-state rejection preserves Waiting"),
		Machine->GetMachineState(), ETowelMachineState::Waiting);

	Request.Source = Source;
	Request.Destination = Machine->GetInventory();
	Request.ExpectedSourceRevision = Request.Source->GetSnapshot().Revision;
	Request.ExpectedDestinationRevision = Request.Destination->GetSnapshot().Revision;
	const FTowelInventorySnapshot GenericSourceBefore = Request.Source->GetSnapshot();
	TestEqual(TEXT("A machine rejects a correct-state non-basket endpoint"),
		Transfer->TryTransfer(Request).Failure, ETowelTransferFailure::EndpointBlocked);
	TestEqual(TEXT("Non-basket rejection preserves the source"),
		Request.Source->GetSnapshot().Count, GenericSourceBefore.Count);
	TestEqual(TEXT("Non-basket rejection preserves the machine count"),
		Request.Destination->GetSnapshot().Count, 0);

	Request.Source = InputBasket->GetInventory();
	Request.Destination = Machine->GetInventory();
	Request.ExpectedSourceRevision = Request.Source->GetSnapshot().Revision;
	Request.ExpectedDestinationRevision = Request.Destination->GetSnapshot().Revision;
	const FTowelTransferResult InputResult = Transfer->TryTransfer(Request);
	TestTrue(TEXT("A Waiting washer accepts Used towels from a basket directly"), InputResult.bSucceeded);
	TestEqual(TEXT("The legal direct input fills the machine"), Machine->GetInventory()->GetSnapshot().Count, 2);

	Request.Source = Machine->GetInventory();
	Request.Destination = OutputBasket->GetInventory();
	Request.ExpectedSourceRevision = Request.Source->GetSnapshot().Revision;
	Request.ExpectedDestinationRevision = Request.Destination->GetSnapshot().Revision;
	const FTowelInventorySnapshot WaitingMachineBefore = Request.Source->GetSnapshot();
	TestEqual(TEXT("A Waiting machine cannot be drained directly"),
		Transfer->TryTransfer(Request).Failure, ETowelTransferFailure::EndpointBlocked);
	TestEqual(TEXT("Rejected Waiting drain preserves the machine"),
		Request.Source->GetSnapshot().Count, WaitingMachineBefore.Count);
	TestEqual(TEXT("Rejected Waiting drain preserves the output basket"),
		Request.Destination->GetSnapshot().Count, 0);

	FText FailureReason;
	TestTrue(TEXT("A washer starts only with Used towels"), Machine->StartProcessing(FailureReason));
	Request.ExpectedSourceRevision = Request.Source->GetSnapshot().Revision;
	Request.ExpectedDestinationRevision = Request.Destination->GetSnapshot().Revision;
	TestEqual(TEXT("Processing blocks machine inventory transfer"),
		Transfer->TryTransfer(Request).Failure, ETowelTransferFailure::EndpointBlocked);
	TestEqual(TEXT("Processing rejection preserves both endpoint counts"),
		Request.Source->GetSnapshot().Count + Request.Destination->GetSnapshot().Count, 2);
	Machine->CompleteProcessing();
	TestEqual(TEXT("Washer completion enters Complete"), Machine->GetMachineState(), ETowelMachineState::Complete);
	TestEqual(TEXT("Washer preserves count while converting Used to Wet"),
		Machine->GetInventory()->GetSnapshot().State, ETowelState::Wet);

	Request.Source = CompleteInputBasket->GetInventory();
	Request.Destination = Machine->GetInventory();
	Request.ExpectedSourceRevision = Request.Source->GetSnapshot().Revision;
	Request.ExpectedDestinationRevision = Request.Destination->GetSnapshot().Revision;
	const FTowelInventorySnapshot CompleteMachineBefore = Request.Destination->GetSnapshot();
	TestEqual(TEXT("A Complete machine rejects additional matching output towels"),
		Transfer->TryTransfer(Request).Failure, ETowelTransferFailure::EndpointBlocked);
	TestEqual(TEXT("Complete input rejection preserves its basket"), Request.Source->GetSnapshot().Count, 1);
	TestEqual(TEXT("Complete input rejection preserves machine contents"),
		Request.Destination->GetSnapshot().Count, CompleteMachineBefore.Count);
	TestEqual(TEXT("Complete input rejection preserves machine state"),
		Machine->GetMachineState(), ETowelMachineState::Complete);

	Request.Source = Machine->GetInventory();
	Request.Destination = Mixed;
	Request.ExpectedSourceRevision = Request.Source->GetSnapshot().Revision;
	Request.ExpectedDestinationRevision = Request.Destination->GetSnapshot().Revision;
	TestEqual(TEXT("A Complete machine rejects a non-basket output endpoint"),
		Transfer->TryTransfer(Request).Failure, ETowelTransferFailure::EndpointBlocked);
	TestEqual(TEXT("Rejected non-basket output preserves Complete"),
		Machine->GetMachineState(), ETowelMachineState::Complete);
	TestEqual(TEXT("Rejected non-basket output preserves both counts"),
		Request.Source->GetSnapshot().Count + Request.Destination->GetSnapshot().Count, 2);

	Request.Destination = WrongInputBasket->GetInventory();
	Request.ExpectedSourceRevision = Request.Source->GetSnapshot().Revision;
	Request.ExpectedDestinationRevision = Request.Destination->GetSnapshot().Revision;
	TestEqual(TEXT("A Complete machine rejects an incompatible basket state"),
		Transfer->TryTransfer(Request).Failure, ETowelTransferFailure::StateMismatch);
	TestEqual(TEXT("Incompatible output rejection preserves Complete"),
		Machine->GetMachineState(), ETowelMachineState::Complete);

	Request.Destination = OutputBasket->GetInventory();
	Request.ExpectedSourceRevision = Request.Source->GetSnapshot().Revision;
	Request.ExpectedDestinationRevision = Request.Destination->GetSnapshot().Revision;
	const FTowelTransferResult OutputResult = Transfer->TryTransfer(Request);
	TestTrue(TEXT("Complete output transfers to an empty basket"), OutputResult.bSucceeded);
	TestEqual(TEXT("A final direct output transfer resets the machine automatically"),
		Machine->GetMachineState(), ETowelMachineState::Waiting);
	TestEqual(TEXT("An emptied machine normalizes its inventory state"),
		Machine->GetInventory()->GetSnapshot().State, ETowelState::None);
	TestEqual(TEXT("Machine output remains Wet in the basket"),
		OutputBasket->GetInventory()->GetSnapshot().State, ETowelState::Wet);
	ATowelProcessingMachineActor* Dryer = World->SpawnActor<ATowelProcessingMachineActor>();
	BeginActorForTest(Dryer);
	Dryer->MachineKind = ETowelMachineKind::Dryer;
	Dryer->Inventory->Capacity = 5;
	Dryer->Inventory->State = ETowelState::Wet;
	Dryer->Inventory->Count = 1;
	TestTrue(TEXT("A dryer starts with Wet towels"), Dryer->StartProcessing(FailureReason));
	Dryer->CompleteProcessing();
	TestEqual(TEXT("Dryer preserves count while converting Wet to Clean"),
		Dryer->GetInventory()->GetSnapshot().State, ETowelState::Clean);
	TestEqual(TEXT("Dryer conversion preserves one towel"), Dryer->GetInventory()->GetSnapshot().Count, 1);

	AActor* CollectionOwner = World->SpawnActor<AActor>();
	USceneComponent* CollectionAnchor = NewObject<USceneComponent>(CollectionOwner, TEXT("CollectionAnchor"));
	UPlayerCarryComponent* CollectionCarry = NewObject<UPlayerCarryComponent>(CollectionOwner, TEXT("CollectionCarry"));
	CollectionOwner->SetRootComponent(CollectionAnchor);
	CollectionOwner->AddInstanceComponent(CollectionAnchor);
	CollectionOwner->AddInstanceComponent(CollectionCarry);
	CollectionAnchor->RegisterComponent();
	CollectionCarry->RegisterComponent();
	CollectionCarry->ConfigureHeldAnchor(CollectionAnchor);
	ATowelBasketActor* UsedBasket = World->SpawnActor<ATowelBasketActor>();
	BeginActorForTest(UsedBasket);
	UsedBasket->GetInventory()->Capacity = 5;
	UsedBasket->GetInventory()->State = ETowelState::None;
	UsedBasket->GetInventory()->Count = 0;
	FText CarryFailure;
	TestTrue(TEXT("Collection setup holds an empty basket"),
		CollectionCarry->TryTakePhysicalObject(UsedBasket, CarryFailure));
	FPlayerInteractionContext CollectionContext;
	CollectionContext.Interactor = CollectionOwner;
	CollectionContext.CarryComponent = CollectionCarry;
	AWorldUsedTowelActor* WorldTowel = World->SpawnActor<AWorldUsedTowelActor>();
	BeginActorForTest(WorldTowel);
	WorldTowel->CommitStagedToken();
	const FPlayerInteractionQuery WorldTowelQuery = WorldTowel->QueryInteraction(CollectionContext);
	TestTrue(TEXT("A committed floor towel supports individual primary collection"), WorldTowelQuery.bCanInteract);
	TestFalse(TEXT("A floor towel never exposes bulk secondary interaction"), WorldTowelQuery.bSecondaryVisible);
	TestTrue(TEXT("Individual floor towel collection commits one Used token"),
		WorldTowel->ExecuteInteraction(CollectionContext).bSucceeded);
	TestEqual(TEXT("Floor collection adds one Used towel to the basket"),
		UsedBasket->GetInventory()->GetSnapshot().Count, 1);

	AUsedTowelBinActor* TransferBin = World->SpawnActor<AUsedTowelBinActor>();
	BeginActorForTest(TransferBin);
	TransferBin->GetInventory()->Capacity = 5;
	TransferBin->GetInventory()->State = ETowelState::Used;
	TransferBin->GetInventory()->Count = 3;
	const FPlayerInteractionQuery BinQuery = TransferBin->QueryInteraction(CollectionContext);
	TestTrue(TEXT("Used bin exposes one-towel primary transfer"), BinQuery.bCanInteract);
	TestTrue(TEXT("Used bin exposes max-possible secondary transfer"), BinQuery.bCanSecondaryInteract);
	TestTrue(TEXT("Used bin primary moves one towel"), TransferBin->ExecuteInteraction(CollectionContext).bSucceeded);
	TestEqual(TEXT("Used bin primary decrements container count by one"),
		TransferBin->GetInventory()->GetSnapshot().Count, 2);
	const FPlayerInteractionResult BinSecondaryResult = TransferBin->ExecuteSecondaryInteraction(CollectionContext);
	TestTrue(TEXT("Used bin secondary moves the remaining capacity-limited amount"), BinSecondaryResult.bSucceeded);
	TestEqual(TEXT("Bulk transfer result preserves the F intent"),
		BinSecondaryResult.Intent, EPlayerInteractionIntent::Secondary);
	TestEqual(TEXT("Used bin secondary empties this two-towel remainder"),
		TransferBin->GetInventory()->GetSnapshot().Count, 0);
	TestEqual(TEXT("Container and individual collection conserve four Used towels"),
		UsedBasket->GetInventory()->GetSnapshot().Count, 4);

	AActor* RecoveryOwner = World->SpawnActor<AActor>();
	UTowelInventoryComponent* RecoveryInventory = NewObject<UTowelInventoryComponent>(RecoveryOwner, TEXT("RecoveryInventory"));
	RecoveryOwner->AddInstanceComponent(RecoveryInventory);
	RecoveryInventory->RegisterComponent();
	BeginActorForTest(RecoveryOwner);
	RecoveryInventory->Capacity = 5;
	RecoveryInventory->State = ETowelState::Clean;
	RecoveryInventory->Count = 2;
	const int32 RecoveryBefore = World->GetSubsystem<UTowelCirculationSubsystem>()->GetRecoveryCount(ETowelState::Clean);
	RecoveryOwner->Destroy();
	TestEqual(TEXT("Endpoint EndPlay recovers every remaining token"),
		World->GetSubsystem<UTowelCirculationSubsystem>()->GetRecoveryCount(ETowelState::Clean), RecoveryBefore + 2);
	TestEqual(TEXT("Recovered endpoint is emptied exactly once"), RecoveryInventory->GetSnapshot().Count, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBathhousePhysicalCarryDropTest,
	"BathhouseSim.Interaction.PhysicalCarryDropSweepAndTransaction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBathhousePhysicalCarryDropTest::RunTest(const FString& Parameters)
{
	FScopedBathhouseAutomationWorld TestWorld(TEXT("PhysicalCarryDropAutomationWorld"));
	UWorld* World = TestWorld.Get();
	if (!World)
	{
		AddError(TEXT("Failed to create the physical carry drop automation world."));
		return false;
	}

	UStaticMesh* CarryTestMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (!TestNotNull(TEXT("The Engine cube mesh is available for physical bounds coverage"), CarryTestMesh))
	{
		return false;
	}

	AActor* CarrierOwner = World->SpawnActor<AActor>();
	USceneComponent* HeldAnchor = NewObject<USceneComponent>(CarrierOwner, TEXT("DropHeldAnchor"));
	UPlayerCarryComponent* Carry = NewObject<UPlayerCarryComponent>(CarrierOwner, TEXT("DropCarry"));
	CarrierOwner->SetRootComponent(HeldAnchor);
	CarrierOwner->AddInstanceComponent(HeldAnchor);
	CarrierOwner->AddInstanceComponent(Carry);
	HeldAnchor->RegisterComponent();
	HeldAnchor->SetWorldLocation(FVector(0.0f, 0.0f, 100.0f));
	Carry->RegisterComponent();
	Carry->ConfigureHeldAnchor(HeldAnchor);

	AWetMopActor* Mop = World->SpawnActor<AWetMopActor>();
	ATowelBasketActor* Basket = World->SpawnActor<ATowelBasketActor>();
	BeginActorForTest(Mop);
	BeginActorForTest(Basket);
	TestTrue(TEXT("A carryable actor defaults to the identity held transform"),
		Basket->GetHeldTransform().Equals(FTransform::Identity));
	Mop->HeldTransform = FTransform(
		FRotator(8.0f, 32.0f, -4.0f),
		FVector(12.0f, -7.0f, 5.0f),
		FVector(3.0f, 2.0f, 4.0f));
	Basket->HeldTransform = FTransform(
		FRotator(-6.0f, -25.0f, 9.0f),
		FVector(-10.0f, 6.0f, 3.0f),
		FVector(0.5f));
	TestEqual(TEXT("Mop held contract ignores authored held scale"),
		Mop->GetHeldTransform().GetScale3D(), FVector::OneVector);
	TestEqual(TEXT("Basket held contract ignores authored held scale"),
		Basket->GetHeldTransform().GetScale3D(), FVector::OneVector);
	TestTrue(TEXT("The mop has non-zero physical sweep bounds"),
		ConfigureCarryPrimitiveForTest(Mop, CarryTestMesh));
	TestTrue(TEXT("The basket has non-zero physical sweep bounds"),
		ConfigureCarryPrimitiveForTest(Basket, CarryTestMesh));
	const FVector MopPhysicalScale = Mop->GetActorScale3D();
	const FVector BasketPhysicalScale = Basket->GetActorScale3D();

	AActor* WallActor = World->SpawnActor<AActor>();
	UBoxComponent* Wall = NewObject<UBoxComponent>(WallActor, TEXT("DropWall"));
	WallActor->SetRootComponent(Wall);
	WallActor->AddInstanceComponent(Wall);
	Wall->SetBoxExtent(FVector(5.0f, 100.0f, 100.0f));
	Wall->SetCollisionObjectType(ECC_WorldStatic);
	Wall->SetCollisionResponseToAllChannels(ECR_Block);
	Wall->RegisterComponent();
	Wall->SetWorldLocation(FVector(50.0f, 0.0f, 100.0f));

	FText CarryFailure;
	TestTrue(TEXT("The drop sweep setup takes the wet mop"),
		Carry->TryTakePhysicalObject(Mop, CarryFailure));
	TestEqual(TEXT("Held mop applies its actor-specific local location"),
		Mop->GetRootComponent()->GetRelativeLocation(), Mop->GetHeldTransform().GetLocation());
	TestTrue(TEXT("Held mop applies its actor-specific local rotation"),
		Mop->GetRootComponent()->GetRelativeTransform().GetRotation().Equals(Mop->GetHeldTransform().GetRotation()));
	TestEqual(TEXT("Held mop preserves its physical actor scale"), Mop->GetActorScale3D(), MopPhysicalScale);
	const FPlayerInteractionResult WallDrop = Carry->TryReleaseHeldEquipment(
		HeldAnchor->GetComponentLocation(),
		FVector::ForwardVector);
	TestTrue(TEXT("A wall hit resolves to a safe pre-wall drop"), WallDrop.bSucceeded);
	TestTrue(TEXT("The swept mop remains on the player side of the wall"),
		Mop->GetActorLocation().X < 45.0f);
	TestTrue(TEXT("The common drop path enables physics"),
		Mop->GetPhysicalCarryPrimitive()->IsSimulatingPhysics());
	TestEqual(TEXT("Mop drop keeps the physical scale independent of HeldTransform"),
		Mop->GetActorScale3D(), MopPhysicalScale);
	World->Tick(LEVELTICK_All, 1.0f / 60.0f);
	TestTrue(TEXT("The common drop path applies the authored forward impulse"),
		Mop->GetPhysicalCarryPrimitive()->GetPhysicsLinearVelocity().X > 0.0f);

	Mop->HeldTransform = FTransform::Identity;
	TestTrue(TEXT("The dropped mop can be picked up again"),
		Carry->TryTakePhysicalObject(Mop, CarryFailure));
	UPrimitiveComponent* MopPrimitive = Mop->GetPhysicalCarryPrimitive();
	Wall->SetBoxExtent(FVector(5.0f, 100.0f, 100.0f));
	Wall->SetWorldLocation(HeldAnchor->GetComponentLocation() - FVector(12.0f, 0.0f, 0.0f));
	Wall->UpdateBounds();
	MopPrimitive->UpdateBounds();

	const FVector ThinSweepStart = MopPrimitive->Bounds.Origin;
	const FVector ThinBoundsOffset = ThinSweepStart - Mop->GetActorLocation();
	const FVector ThinSweepEnd = HeldAnchor->GetComponentLocation()
		+ FVector::ForwardVector * Mop->GetThrowSpawnDistance()
		+ ThinBoundsOffset;
	FCollisionQueryParams ThinWallQueryParams(
		SCENE_QUERY_STAT(PhysicalCarryDropThinWallRegression),
		false);
	ThinWallQueryParams.bFindInitialOverlaps = true;
	ThinWallQueryParams.AddIgnoredActor(Mop);
	ThinWallQueryParams.AddIgnoredActor(CarrierOwner);
	FHitResult ThinWallInitialHit;
	TestTrue(TEXT("The thin-wall fixture produces an initial blocking overlap"),
		World->SweepSingleByChannel(
			ThinWallInitialHit,
			ThinSweepStart,
			ThinSweepEnd,
			FQuat::Identity,
			Carry->DropSweepChannel,
			FCollisionShape::MakeBox(MopPrimitive->Bounds.BoxExtent),
			ThinWallQueryParams));
	TestTrue(TEXT("The thin-wall fixture starts penetrating"), ThinWallInitialHit.bStartPenetrating);
	TestTrue(TEXT("The thin-wall fixture exposes an MTD toward the throw target"),
		FVector::DotProduct(ThinWallInitialHit.Normal, FVector::ForwardVector) > 0.0f);

	const FTransform HeldTransformBeforeFailure = Mop->GetActorTransform();
	const FPlayerInteractionResult ThinWallDrop = Carry->TryReleaseHeldEquipment(
		HeldAnchor->GetComponentLocation(),
		FVector::ForwardVector);
	TestFalse(TEXT("A target-side initial-overlap MTD never commits beyond the thin wall"),
		ThinWallDrop.bSucceeded);
	TestTrue(TEXT("The thin-wall rejection preserves the held object"), Carry->GetHeldObject() == Mop);
	TestEqual(TEXT("The thin-wall rejection preserves the held attachment"),
		Mop->GetRootComponent()->GetAttachParent(), HeldAnchor);
	TestFalse(TEXT("The thin-wall rejection keeps held physics disabled"),
		MopPrimitive->IsSimulatingPhysics());
	TestTrue(TEXT("The thin-wall rejection preserves the held transform"),
		Mop->GetActorTransform().Equals(HeldTransformBeforeFailure));
	FPlayerInteractionContext HeldMopContext;
	HeldMopContext.Interactor = CarrierOwner;
	HeldMopContext.CarryComponent = Carry;
	TestFalse(TEXT("The thin-wall rejection preserves the concrete mop carrier"),
		Mop->QueryInteraction(HeldMopContext).bVisible);

	Wall->SetBoxExtent(FVector(100.0f));
	Wall->SetWorldLocation(HeldAnchor->GetComponentLocation());
	Wall->UpdateBounds();
	const FPlayerInteractionResult BlockedDrop = Carry->TryReleaseHeldEquipment(
		HeldAnchor->GetComponentLocation(),
		FVector::ForwardVector);
	TestFalse(TEXT("The existing large start blocker still rejects the drop"), BlockedDrop.bSucceeded);
	TestTrue(TEXT("A rejected drop preserves the held object"), Carry->GetHeldObject() == Mop);
	TestEqual(TEXT("A rejected drop preserves the held attachment"),
		Mop->GetRootComponent()->GetAttachParent(), HeldAnchor);
	TestFalse(TEXT("A rejected drop keeps held physics disabled"),
		Mop->GetPhysicalCarryPrimitive()->IsSimulatingPhysics());
	TestTrue(TEXT("A rejected drop preserves the held transform"),
		Mop->GetActorTransform().Equals(HeldTransformBeforeFailure));

	Wall->SetWorldLocation(FVector(300.0f, 0.0f, 100.0f));
	Wall->UpdateBounds();
	UBathhousePhysicalDropReentryProbe* ReentryProbe = NewObject<UBathhousePhysicalDropReentryProbe>();
	ReentryProbe->Bind(
		Mop,
		Carry,
		HeldAnchor->GetComponentLocation(),
		FVector::ForwardVector);
	TestTrue(TEXT("The same held mop drops after a safe direction becomes available"),
		Carry->TryReleaseHeldEquipment(HeldAnchor->GetComponentLocation(), FVector::ForwardVector).bSucceeded);
	TestEqual(TEXT("A successful drop broadcasts release presentation exactly once"),
		ReentryProbe->ReleasePresentationCount, 1);
	TestEqual(TEXT("The release presentation attempts one nested drop"),
		ReentryProbe->NestedDropAttemptCount, 1);
	TestFalse(TEXT("The physical drop transaction guard rejects the nested drop"),
		ReentryProbe->bNestedDropSucceeded);
	TestFalse(TEXT("The nested drop rejection reports a failure reason"),
		ReentryProbe->NestedDropFailureReason.IsEmpty());
	TestTrue(TEXT("The outer drop clears the authoritative held object exactly once"), Carry->IsHandEmpty());
	TestTrue(TEXT("The outer drop clears the concrete mop carrier"),
		Mop->QueryInteraction(HeldMopContext).bVisible);
	TestTrue(TEXT("Delegate reentry leaves the committed mop simulating physics"),
		MopPrimitive->IsSimulatingPhysics());
	ReentryProbe->Unbind();
	TestTrue(TEXT("The same common path takes a towel basket"),
		Carry->TryTakePhysicalObject(Basket, CarryFailure));
	TestEqual(TEXT("Held basket applies its actor-specific local location"),
		Basket->GetRootComponent()->GetRelativeLocation(), Basket->GetHeldTransform().GetLocation());
	TestTrue(TEXT("Held basket applies its actor-specific local rotation"),
		Basket->GetRootComponent()->GetRelativeTransform().GetRotation().Equals(Basket->GetHeldTransform().GetRotation()));
	TestEqual(TEXT("Held basket preserves its physical actor scale"),
		Basket->GetActorScale3D(), BasketPhysicalScale);
	TestTrue(TEXT("The same common path releases a towel basket"),
		Carry->TryReleaseHeldEquipment(HeldAnchor->GetComponentLocation(), FVector::ForwardVector).bSucceeded);
	TestEqual(TEXT("Basket drop keeps the physical scale independent of HeldTransform"),
		Basket->GetActorScale3D(), BasketPhysicalScale);

	FEnumProperty* FacilityTypeProperty = FindFProperty<FEnumProperty>(
		ABathhouseFacilityActor::StaticClass(),
		TEXT("FacilityType"));
	FIntProperty* FacilityNumberProperty = FindFProperty<FIntProperty>(
		ABathhouseFacilityActor::StaticClass(),
		TEXT("FacilityNumber"));
	const auto SpawnNumberedFacility = [&](const EBathhouseFacilityType FacilityType)
	{
		ABathhouseFacilityActor* Facility = World->SpawnActor<ABathhouseFacilityActor>();
		if (Facility && FacilityTypeProperty && FacilityNumberProperty)
		{
			void* TypeAddress = FacilityTypeProperty->ContainerPtrToValuePtr<void>(Facility);
			FacilityTypeProperty->GetUnderlyingProperty()->SetIntPropertyValue(
				TypeAddress,
				static_cast<int64>(FacilityType));
			FacilityNumberProperty->SetPropertyValue_InContainer(Facility, 0);
			BeginActorForTest(Facility);
		}
		return Facility;
	};
	TestNotNull(TEXT("Facility type reflection is available for key topology setup"), FacilityTypeProperty);
	TestNotNull(TEXT("Facility number reflection is available for key topology setup"), FacilityNumberProperty);
	ABathhouseFacilityActor* ShoeLocker = SpawnNumberedFacility(EBathhouseFacilityType::ShoeLocker);
	ABathhouseFacilityActor* ClothesLocker = SpawnNumberedFacility(EBathhouseFacilityType::ClothesLocker);
	TestTrue(TEXT("Key topology setup registers the numbered shoe locker"),
		ShoeLocker && ShoeLocker->GetFacilityNumber() == 0);
	TestTrue(TEXT("Key topology setup registers the numbered clothes locker"),
		ClothesLocker && ClothesLocker->GetFacilityNumber() == 0);

	ABathhouseKeyHookActor* KeyHook = World->SpawnActor<ABathhouseKeyHookActor>();
	ABathhouseKeyActor* Key = World->SpawnActor<ABathhouseKeyActor>();
	KeyHook->KeyActor = Key;
	BeginActorForTest(KeyHook);
	BeginActorForTest(Key);
	Key->HeldTransform = FTransform(
		FRotator(11.0f, 47.0f, -8.0f),
		FVector(9.0f, 4.0f, -3.0f),
		FVector(2.0f));
	const FVector KeyPhysicalScale(1.2f, 0.8f, 1.1f);
	Key->SetActorScale3D(KeyPhysicalScale);
	TestTrue(TEXT("The carry test key initializes on its registered hook"), Key->InitializeAtHook(KeyHook));
	TestTrue(TEXT("HeldTransform is not applied while the key is on its hook"),
		Key->GetRootComponent()->GetRelativeTransform().GetLocation().IsNearlyZero()
		&& Key->GetRootComponent()->GetRelativeTransform().GetRotation().Equals(FQuat::Identity));
	TestTrue(TEXT("The registered hook transaction takes the key"), Key->TryTakeFromHook(*Carry, *KeyHook));
	TestEqual(TEXT("Held key applies its actor-specific local location"),
		Key->GetRootComponent()->GetRelativeLocation(), Key->GetHeldTransform().GetLocation());
	TestTrue(TEXT("Held key applies its actor-specific local rotation"),
		Key->GetRootComponent()->GetRelativeTransform().GetRotation().Equals(Key->GetHeldTransform().GetRotation()));
	TestEqual(TEXT("Held key preserves physical scale while authored held scale is ignored"),
		Key->GetActorScale3D(), KeyPhysicalScale);
	TestTrue(TEXT("The held key returns through its hook transaction"), Key->TryReturnToHook(*Carry, *KeyHook));
	TestTrue(TEXT("Hook return removes the held offset instead of applying it at the hook"),
		Key->GetRootComponent()->GetRelativeTransform().GetLocation().IsNearlyZero()
		&& Key->GetRootComponent()->GetRelativeTransform().GetRotation().Equals(FQuat::Identity));
	TestEqual(TEXT("Hook return preserves the key physical scale"), Key->GetActorScale3D(), KeyPhysicalScale);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBathhouseCleaningInteractionTest,
	"BathhouseSim.Cleaning.CarryHoldZoneAndRegistry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBathhouseCleaningInteractionTest::RunTest(const FString& Parameters)
{
	FScopedBathhouseAutomationWorld TestWorld(TEXT("CleaningAutomationWorld"));
	UWorld* World = TestWorld.Get();
	if (!World)
	{
		AddError(TEXT("Failed to create the cleaning automation world."));
		return false;
	}

	AActor* CarrierOwner = World->SpawnActor<AActor>();
	USceneComponent* HeldAnchor = NewObject<USceneComponent>(CarrierOwner, TEXT("HeldAnchor"));
	UPlayerCarryComponent* Carry = NewObject<UPlayerCarryComponent>(CarrierOwner, TEXT("Carry"));
	CarrierOwner->SetRootComponent(HeldAnchor);
	CarrierOwner->AddInstanceComponent(HeldAnchor);
	CarrierOwner->AddInstanceComponent(Carry);
	HeldAnchor->RegisterComponent();
	Carry->RegisterComponent();
	Carry->ConfigureHeldAnchor(HeldAnchor);
	AWetMopActor* Mop = World->SpawnActor<AWetMopActor>();
	ATowelBasketActor* Basket = World->SpawnActor<ATowelBasketActor>();
	BeginActorForTest(Mop);
	BeginActorForTest(Basket);
	UStaticMesh* CarryTestMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	TestTrue(TEXT("Cleaning carry test configures mop sweep bounds"),
		ConfigureCarryPrimitiveForTest(Mop, CarryTestMesh));
	TestTrue(TEXT("Cleaning carry test configures basket sweep bounds"),
		ConfigureCarryPrimitiveForTest(Basket, CarryTestMesh));
	FText CarryFailure;
	TestTrue(TEXT("An empty hand takes the wet mop"), Carry->TryTakePhysicalObject(Mop, CarryFailure));
	TestFalse(TEXT("A second physical object cannot auto-swap into the occupied hand"),
		Carry->TryTakePhysicalObject(Basket, CarryFailure));

	AWaterStainActor* Stain = World->SpawnActor<AWaterStainActor>();
	BeginActorForTest(Stain);
	FPlayerInteractionContext Context;
	Context.Interactor = CarrierOwner;
	Context.CarryComponent = Carry;
	const FPlayerInteractionQuery HoldQuery = Stain->QueryInteraction(Context);
	TestEqual(TEXT("Water stain uses Hold activation"),
		HoldQuery.PrimaryActivationMode, EPlayerInteractionActivationMode::Hold);
	TestTrue(TEXT("Held wet mop enables stain cleaning"), HoldQuery.bCanInteract);
	FText HoldFailure;
	TestTrue(TEXT("Hold cleaning begins"), Stain->BeginHoldInteraction(Context, HoldFailure));
	const FPlayerHoldInteractionUpdate Partial = Stain->UpdateHoldInteraction(Context, 1.0f);
	TestEqual(TEXT("Partial cleaning remains running"), Partial.State, EPlayerHoldInteractionState::Running);
	TestTrue(TEXT("Partial cleaning exposes bounded progress"), Partial.Progress > 0.0f && Partial.Progress < 1.0f);
	Stain->CancelHoldInteraction(Context);
	TestEqual(TEXT("Release cancels progress back to zero"), Stain->GetCleaningProgress(), 0.0f);

	APawn* ControlLossPawn = World->SpawnActor<APawn>();
	UPlayerInteractionComponent* ControlLossInteraction = NewObject<UPlayerInteractionComponent>(
		ControlLossPawn,
		TEXT("ControlLossInteraction"));
	ControlLossPawn->AddInstanceComponent(ControlLossInteraction);
	ControlLossInteraction->RegisterComponent();
	FPlayerInteractionContext ControlLossContext;
	ControlLossContext.Interactor = ControlLossPawn;
	ControlLossContext.CarryComponent = Carry;
	TestTrue(TEXT("A fresh cleaner can begin before local-control loss"),
		Stain->BeginHoldInteraction(ControlLossContext, HoldFailure));
	const FPlayerHoldInteractionUpdate ControlLossPartial = Stain->UpdateHoldInteraction(ControlLossContext, 0.5f);
	TestEqual(TEXT("Control-loss setup owns a partial stain hold"),
		ControlLossPartial.State, EPlayerHoldInteractionState::Running);
	ControlLossInteraction->ActiveHoldTarget = Stain;
	ControlLossInteraction->ActiveHoldContext = ControlLossContext;
	ControlLossInteraction->ActiveHoldProgress = ControlLossPartial.Progress;
	ControlLossInteraction->bPrimaryInputHeld = true;
	ControlLossInteraction->CurrentTarget = Stain;
	ControlLossInteraction->CurrentQuery = Stain->QueryInteraction(ControlLossContext);
	UBathhouseCleaningCancelProbe* CancelProbe = NewObject<UBathhouseCleaningCancelProbe>();
	CancelProbe->Bind(Stain);
	int32 ControlLossTerminalResultCount = 0;
	ControlLossInteraction->OnInteractionAttemptFinishedNative.AddLambda(
		[&ControlLossTerminalResultCount](const FPlayerInteractionResult& Result)
		{
			if (!Result.bSucceeded)
			{
				++ControlLossTerminalResultCount;
			}
		});
	TestFalse(TEXT("The control-loss test pawn is not locally controlled"), ControlLossPawn->IsLocallyControlled());
	ControlLossInteraction->TickComponent(0.1f, LEVELTICK_All, nullptr);
	TestEqual(TEXT("Local-control loss cancels the stain exactly once"), CancelProbe->CancelCount, 1);
	TestEqual(TEXT("Local-control loss emits one terminal failure result"),
		ControlLossTerminalResultCount, 1);
	TestEqual(TEXT("Local-control loss returns the stain to Idle"),
		Stain->GetCleaningState(), EStainCleaningState::Idle);
	TestEqual(TEXT("Local-control loss resets stain progress"), Stain->GetCleaningProgress(), 0.0f);
	TestFalse(TEXT("Local-control loss clears the active hold target"),
		ControlLossInteraction->IsPrimaryHoldActive());
	TestFalse(TEXT("Local-control loss clears the primary input latch"),
		ControlLossInteraction->bPrimaryInputHeld);
	TestFalse(TEXT("Local-control loss clears the visible interaction query"),
		ControlLossInteraction->GetCurrentInteractionQuery().bVisible);
	ControlLossInteraction->TickComponent(0.1f, LEVELTICK_All, nullptr);
	ControlLossInteraction->EndPrimaryInteraction();
	TestEqual(TEXT("Later ticks and E release do not cancel the old hold again"), CancelProbe->CancelCount, 1);
	TestEqual(TEXT("Later ticks and E release do not duplicate the terminal result"),
		ControlLossTerminalResultCount, 1);
	CancelProbe->Unbind();

	TestTrue(TEXT("Cleaning can restart after cancellation"), Stain->BeginHoldInteraction(Context, HoldFailure));
	const FPlayerHoldInteractionUpdate Complete = Stain->UpdateHoldInteraction(Context, 3.0f);
	TestEqual(TEXT("A full hold completes exactly once"), Complete.State, EPlayerHoldInteractionState::Succeeded);
	TestEqual(TEXT("Completed stain enters Removed"), Stain->GetCleaningState(), EStainCleaningState::Removed);

	HeldAnchor->SetWorldLocation(FVector(1000.0f, 0.0f, 300.0f));
	Mop->GetPhysicalCarryPrimitive()->UpdateBounds();
	const FPlayerInteractionResult MopDrop = Carry->TryReleaseHeldEquipment(
		HeldAnchor->GetComponentLocation(),
		FVector::ForwardVector);
	TestTrue(TEXT("G releases the wet mop"), MopDrop.bSucceeded);
	TestEqual(TEXT("Mop release result preserves the G intent"), MopDrop.Intent, EPlayerInteractionIntent::DropCarry);
	TestTrue(TEXT("Mop release empties the hand"), Carry->IsHandEmpty());
	HeldAnchor->SetWorldLocation(FVector(1000.0f, 1000.0f, 300.0f));
	TestTrue(TEXT("The empty hand can then take a towel basket"), Carry->TryTakePhysicalObject(Basket, CarryFailure));
	Basket->GetPhysicalCarryPrimitive()->UpdateBounds();
	TestTrue(TEXT("G releases the towel basket"),
		Carry->TryReleaseHeldEquipment(HeldAnchor->GetComponentLocation(), FVector::ForwardVector).bSucceeded);
	ABathhouseKeyActor* Key = World->SpawnActor<ABathhouseKeyActor>();
	TestTrue(TEXT("Legacy key transaction can occupy the generic hand"), Carry->CommitTakeKey(Key));
	const FPlayerInteractionResult KeyDrop = Carry->TryReleaseHeldEquipment(FVector::ZeroVector, FVector::ForwardVector);
	TestFalse(TEXT("G rejects key free-drop"), KeyDrop.bSucceeded);
	TestEqual(TEXT("Rejected key drop preserves held key identity"), Carry->GetHeldKey(), Key);
	Carry->CommitReleaseKey(Key);

	UMaterial* FirstVariationMaterial = NewObject<UMaterial>();
	UMaterial* SecondVariationMaterial = NewObject<UMaterial>();
	AWaterStainActor* NullVariationStain = NewObject<AWaterStainActor>();
	NullVariationStain->MaterialVariants = {nullptr};
	NullVariationStain->MinXYScale = FVector2D(1.4, -2.0);
	NullVariationStain->MaxXYScale = FVector2D(0.6, 0.0);
	NullVariationStain->MinYawDegrees = 30.0f;
	NullVariationStain->MaxYawDegrees = -15.0f;
	const FTransform NullVariationCollisionBefore =
		NullVariationStain->InteractionCollision->GetRelativeTransform();
	NullVariationStain->ConfigureVisualVariationSeed(101);
	NullVariationStain->ResolveAndApplyVisualVariation();
	TestNull(TEXT("A null-only material list preserves the existing Blueprint material"),
		NullVariationStain->SelectedMaterialVariant.Get());
	TestTrue(TEXT("Inverted X scale bounds are normalized"),
		NullVariationStain->SelectedXYScale.X >= 0.6
		&& NullVariationStain->SelectedXYScale.X <= 1.4);
	TestTrue(TEXT("Non-positive Y scale bounds are clamped to a safe positive value"),
		NullVariationStain->SelectedXYScale.Y > 0.0);
	TestTrue(TEXT("Inverted yaw bounds are normalized"),
		NullVariationStain->SelectedYawDegrees >= -15.0f
		&& NullVariationStain->SelectedYawDegrees <= 30.0f);
	TestTrue(TEXT("Visual-root variation leaves the interaction collision transform unchanged"),
		NullVariationStain->InteractionCollision->GetRelativeTransform().Equals(
			NullVariationCollisionBefore));
	TestEqual(TEXT("Water stain visual variation always keeps local Z scale at one"),
		NullVariationStain->StainVisualRoot->GetRelativeScale3D().Z, 1.0);

	AWaterStainActor* SingleVariationStain = NewObject<AWaterStainActor>();
	SingleVariationStain->MaterialVariants = {nullptr, FirstVariationMaterial, nullptr};
	SingleVariationStain->MinXYScale = FVector2D(0.5, 0.75);
	SingleVariationStain->MaxXYScale = FVector2D(1.5, 1.75);
	SingleVariationStain->MinYawDegrees = -20.0f;
	SingleVariationStain->MaxYawDegrees = 35.0f;
	constexpr int32 SingleVariationSeed = 241;
	FRandomStream SingleVariationExpectedStream(SingleVariationSeed);
	const float ExpectedSingleScaleX = SingleVariationExpectedStream.FRandRange(0.5f, 1.5f);
	const float ExpectedSingleScaleY = SingleVariationExpectedStream.FRandRange(0.75f, 1.75f);
	const FVector2D ExpectedSingleScale(
		ExpectedSingleScaleX,
		ExpectedSingleScaleY);
	const float ExpectedSingleYaw = SingleVariationExpectedStream.FRandRange(-20.0f, 35.0f);
	SingleVariationStain->ConfigureVisualVariationSeed(SingleVariationSeed);
	SingleVariationStain->ResolveAndApplyVisualVariation();
	TestTrue(TEXT("A single valid material is selected through null filtering"),
		SingleVariationStain->SelectedMaterialVariant.Get() == FirstVariationMaterial);
	TestTrue(TEXT("A single material candidate does not consume an extra random draw"),
		SingleVariationStain->SelectedXYScale.Equals(ExpectedSingleScale)
		&& FMath::IsNearlyEqual(SingleVariationStain->SelectedYawDegrees, ExpectedSingleYaw));

	AWaterStainActor* FirstSeededVariation = NewObject<AWaterStainActor>();
	AWaterStainActor* SecondSeededVariation = NewObject<AWaterStainActor>();
	for (AWaterStainActor* SeededStain : {FirstSeededVariation, SecondSeededVariation})
	{
		SeededStain->MaterialVariants = {FirstVariationMaterial, nullptr, SecondVariationMaterial};
		SeededStain->MinXYScale = FVector2D(0.7, 0.8);
		SeededStain->MaxXYScale = FVector2D(1.3, 1.4);
		SeededStain->MinYawDegrees = -90.0f;
		SeededStain->MaxYawDegrees = 90.0f;
		SeededStain->ConfigureVisualVariationSeed(1729);
		SeededStain->ResolveAndApplyVisualVariation();
	}
	TestTrue(TEXT("Equal variation seeds reproduce the selected valid material"),
		FirstSeededVariation->SelectedMaterialVariant.Get()
		== SecondSeededVariation->SelectedMaterialVariant.Get());
	TestTrue(TEXT("Equal variation seeds reproduce independent XY scale and local yaw"),
		FirstSeededVariation->SelectedXYScale.Equals(SecondSeededVariation->SelectedXYScale)
		&& FMath::IsNearlyEqual(
			FirstSeededVariation->SelectedYawDegrees,
			SecondSeededVariation->SelectedYawDegrees));
	const FTransform FirstResolvedVisualTransform =
		FirstSeededVariation->StainVisualRoot->GetRelativeTransform();
	FirstSeededVariation->ConfigureVisualVariationSeed(42);
	FirstSeededVariation->MinXYScale = FVector2D(4.0, 4.0);
	FirstSeededVariation->MaxXYScale = FVector2D(4.0, 4.0);
	FirstSeededVariation->ResolveAndApplyVisualVariation();
	TestTrue(TEXT("A stain never rerolls visual variation during its lifetime"),
		FirstSeededVariation->StainVisualRoot->GetRelativeTransform().Equals(
			FirstResolvedVisualTransform));

	AActor* FloorActor = World->SpawnActor<AActor>();
	UBoxComponent* Floor = NewObject<UBoxComponent>(FloorActor, TEXT("CleaningFloor"));
	FloorActor->SetRootComponent(Floor);
	FloorActor->AddInstanceComponent(Floor);
	Floor->SetBoxExtent(FVector(600.0f, 600.0f, 20.0f));
	Floor->SetCollisionObjectType(ECC_WorldStatic);
	Floor->SetCollisionResponseToAllChannels(ECR_Block);
	Floor->ComponentTags.Add(TEXT("CleaningFloor"));
	Floor->RegisterComponent();
	Floor->SetWorldLocation(FVector(0.0f, 0.0f, -20.0f));

	const FTransform ZoneTransform(FRotator::ZeroRotator, FVector(0.0f, 0.0f, 100.0f));
	AStainSpawnZoneActor* DeferredZone = World->SpawnActorDeferred<AStainSpawnZoneActor>(
		AStainSpawnZoneActor::StaticClass(), ZoneTransform);
	DeferredZone->SpawnBounds->SetBoxExtent(FVector(150.0f, 150.0f, 100.0f));
	DeferredZone->RequiredFloorComponentTag = TEXT("CleaningFloor");
	DeferredZone->MaximumFloorSlopeDegrees = 15.0f;
	AStainSpawnZoneActor* Zone = Cast<AStainSpawnZoneActor>(
		UGameplayStatics::FinishSpawningActor(DeferredZone, ZoneTransform));
	BeginActorForTest(Zone);
	UCleaningWorldSubsystem* Cleaning = World->GetSubsystem<UCleaningWorldSubsystem>();
	TestEqual(TEXT("Authored zone registers once"), Cleaning->GetActiveZones().Num(), 1);
	FRandomStream CandidateStream(77);
	FTransform Candidate;
	TestTrue(TEXT("A tagged level floor inside the authored zone is accepted"),
		Zone->FindSpawnTransform(CandidateStream, 80.0f, 40.0f, Candidate));

	AWaterStainActor* SpacingStain = World->SpawnActor<AWaterStainActor>();
	BeginActorForTest(SpacingStain);
	SpacingStain->SetSpawnZone(Zone);
	SpacingStain->SetActorLocation(Candidate.GetLocation());
	if (USphereComponent* StainCollision = SpacingStain->FindComponentByClass<USphereComponent>())
	{
		StainCollision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	FRandomStream RepeatedCandidateStream(77);
	FTransform RepeatedCandidate;
	TestFalse(TEXT("Existing stain spacing rejects the same random candidate"),
		Zone->FindSpawnTransform(RepeatedCandidateStream, 80.0f, 40.0f, RepeatedCandidate));
	SpacingStain->Destroy();

	APawn* Pawn = World->SpawnActor<APawn>();
	USphereComponent* PawnCollision = NewObject<USphereComponent>(Pawn, TEXT("PawnCollision"));
	Pawn->SetRootComponent(PawnCollision);
	Pawn->AddInstanceComponent(PawnCollision);
	PawnCollision->SetSphereRadius(50.0f);
	PawnCollision->SetCollisionObjectType(ECC_Pawn);
	PawnCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	PawnCollision->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
	PawnCollision->RegisterComponent();
	Pawn->SetActorLocation(Candidate.GetLocation());
	FRandomStream PawnCandidateStream(77);
	TestFalse(TEXT("Pawn clearance rejects an otherwise valid candidate"),
		Zone->FindSpawnTransform(PawnCandidateStream, 80.0f, 60.0f, RepeatedCandidate));
	Pawn->Destroy();

	Zone->RequiredFloorComponentTag = TEXT("WrongFloorTag");
	FRandomStream TagCandidateStream(77);
	TestFalse(TEXT("A floor without the required authored tag is rejected"),
		Zone->FindSpawnTransform(TagCandidateStream, 80.0f, 40.0f, RepeatedCandidate));
	Zone->RequiredFloorComponentTag = TEXT("CleaningFloor");
	Floor->SetWorldRotation(FRotator(30.0f, 0.0f, 0.0f));
	Zone->MaximumFloorSlopeDegrees = 10.0f;
	FRandomStream SlopeCandidateStream(77);
	TestFalse(TEXT("A floor steeper than the authored slope limit is rejected"),
		Zone->FindSpawnTransform(SlopeCandidateStream, 80.0f, 40.0f, RepeatedCandidate));
	Floor->SetWorldRotation(FRotator::ZeroRotator);

	AWaterStainActor* FirstZoneStain = World->SpawnActor<AWaterStainActor>();
	AWaterStainActor* SecondZoneStain = World->SpawnActor<AWaterStainActor>();
	BeginActorForTest(FirstZoneStain);
	BeginActorForTest(SecondZoneStain);
	FirstZoneStain->SetSpawnZone(Zone);
	SecondZoneStain->SetSpawnZone(Zone);
	Zone->MaxActiveStainsInZone = 2;
	TestEqual(TEXT("Per-zone registry count tracks active stains"),
		Cleaning->GetActiveStainCountForZone(Zone), 2);
	ACleaningDirectorActor* Director = World->SpawnActor<ACleaningDirectorActor>();
	Director->StainClass = AWaterStainActor::StaticClass();
	Director->MaxActiveStains = 10;
	Director->MaxPlacementAttemptsPerInterval = 2;
	Director->TrySpawnStain();
	TestEqual(TEXT("Director respects the per-zone active stain limit"),
		Cleaning->GetActiveStainCountForZone(Zone), 2);
	FirstZoneStain->Destroy();
	SecondZoneStain->Destroy();
	TestEqual(TEXT("Stain EndPlay removes weak registry entries"),
		Cleaning->GetActiveStainCountForZone(Zone), 0);
	Zone->Destroy();
	TestEqual(TEXT("Zone EndPlay removes the authored zone registry entry"), Cleaning->GetActiveZones().Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBathhouseCustomerTowelTest,
	"BathhouseSim.Customer.TowelAcquireShortageReturnAndCleanup",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBathhouseCustomerTowelTest::RunTest(const FString& Parameters)
{
	FScopedBathhouseAutomationWorld TestWorld(TEXT("CustomerTowelAutomationWorld"));
	UWorld* World = TestWorld.Get();
	if (!World)
	{
		AddError(TEXT("Failed to create the customer towel automation world."));
		return false;
	}

	const FTransform SpawnTransform = FTransform::Identity;
	ABathhouseCustomerCharacter* DeferredCustomer = World->SpawnActorDeferred<ABathhouseCustomerCharacter>(
		ABathhouseCustomerCharacter::StaticClass(), SpawnTransform);
	DeferredCustomer->AutoPossessAI = EAutoPossessAI::Disabled;
	ABathhouseCustomerCharacter* Customer = Cast<ABathhouseCustomerCharacter>(
		UGameplayStatics::FinishSpawningActor(DeferredCustomer, SpawnTransform));
	ACleanTowelStackActor* Stack = World->SpawnActor<ACleanTowelStackActor>();
	AUsedTowelBinActor* Bin = World->SpawnActor<AUsedTowelBinActor>();
	BeginActorForTest(Customer);
	BeginActorForTest(Stack);
	BeginActorForTest(Bin);
	UCustomerRoutineDefinition* Routine = NewObject<UCustomerRoutineDefinition>();
	Routine->TowelAvailabilityWaitSeconds = 1.0f;
	Routine->TowelUnavailableSatisfactionPenalty = 12.0f;
	UCustomerSessionComponent* Session = Customer->GetCustomerSession();
	Session->InitializeSession(Routine, nullptr);
	Stack->GetInventory()->Capacity = 4;
	Stack->GetInventory()->State = ETowelState::Clean;
	Stack->GetInventory()->Count = 2;
	Stack->GetInventory()->Revision = 0;
	Bin->GetInventory()->Capacity = 2;
	Bin->GetInventory()->State = ETowelState::None;
	Bin->GetInventory()->Count = 0;
	Bin->GetInventory()->Revision = 0;

	Session->CurrentFacilityActor = Stack;
	TestTrue(TEXT("Customer acquires one clean towel token"), Session->TryAcquireCleanTowelFromCurrentFacility());
	TestTrue(TEXT("Customer session owns the acquired token"), Session->HasTowelHandle());
	TestEqual(TEXT("Acquisition decrements the stack by one"), Stack->GetInventory()->GetSnapshot().Count, 1);
	TestTrue(TEXT("Drying marks the token Used"), Session->MarkTowelUsed());
	Session->CurrentFacilityActor = Bin;
	TestTrue(TEXT("Return commits the used token to the current bin"), Session->ReturnTowelToCurrentFacility());
	TestFalse(TEXT("Returned token is terminal and leaves the session"), Session->HasTowelHandle());
	TestEqual(TEXT("Used bin receives exactly one token"), Bin->GetInventory()->GetSnapshot().Count, 1);

	Stack->GetInventory()->State = ETowelState::None;
	Stack->GetInventory()->Count = 0;
	Session->CurrentFacilityActor = Stack;
	TestFalse(TEXT("An empty shelf cannot immediately acquire"), Session->TryAcquireCleanTowelFromCurrentFacility());
	const FCustomerAcquireTowelTask AcquireTask;
	TestFalse(TEXT("The acquire task keeps an active wait across StateTree reselection"),
		AcquireTask.bShouldStateChangeOnReselect);
	TestTrue(TEXT("Empty shelf starts the bounded wait"), Session->BeginWaitingForCleanTowel());
	TestTrue(TEXT("The clean inventory delegate is registered for the active wait"),
		Stack->GetInventory()->OnInventoryChanged.IsAlreadyBound(
			Session,
			&UCustomerSessionComponent::HandleCleanTowelInventoryChanged));
	Session->HandleTowelWaitExpired();
	TestTrue(TEXT("Bounded shortage wait reaches its towel-less terminal branch"), Session->IsTowelWaitExpired());
	TestEqual(TEXT("Shortage applies the authored satisfaction penalty once"), Session->GetSatisfaction(), 88.0f);
	TestFalse(TEXT("Expiry removes the clean inventory delegate"),
		Stack->GetInventory()->OnInventoryChanged.IsAlreadyBound(
			Session,
			&UCustomerSessionComponent::HandleCleanTowelInventoryChanged));
	TestTrue(TEXT("A later re-entry can register a fresh bounded wait"), Session->BeginWaitingForCleanTowel());
	TestTrue(TEXT("Repeated begin is idempotent while the fresh wait is active"),
		Session->BeginWaitingForCleanTowel());
	TestTrue(TEXT("The restarted wait still owns exactly the intended delegate binding"),
		Stack->GetInventory()->OnInventoryChanged.IsAlreadyBound(
			Session,
			&UCustomerSessionComponent::HandleCleanTowelInventoryChanged));
	Session->HandleTowelWaitExpired();
	TestEqual(TEXT("A restarted timeout cannot apply the session penalty twice"), Session->GetSatisfaction(), 88.0f);
	TestFalse(TEXT("The restarted expiry also removes its delegate"),
		Stack->GetInventory()->OnInventoryChanged.IsAlreadyBound(
			Session,
			&UCustomerSessionComponent::HandleCleanTowelInventoryChanged));

	Stack->GetInventory()->State = ETowelState::Clean;
	Stack->GetInventory()->Count = 1;
	TestTrue(TEXT("A later clean token can be acquired"), Session->TryAcquireCleanTowelFromCurrentFacility());
	Session->CleanupTowelHandle();
	TestFalse(TEXT("Interruption cleanup removes an unused handle"), Session->HasTowelHandle());
	TestEqual(TEXT("Unused interruption cleanup returns clean token to original stack"),
		Stack->GetInventory()->GetSnapshot().Count, 1);

	TestTrue(TEXT("A clean token can be acquired for used-cleanup coverage"),
		Session->TryAcquireCleanTowelFromCurrentFacility());
	TestTrue(TEXT("Used-cleanup setup marks the token"), Session->MarkTowelUsed());
	Bin->GetInventory()->State = ETowelState::None;
	Bin->GetInventory()->Count = 0;
	UBathhouseFacilitySlotComponent* InterruptionSlot = NewObject<UBathhouseFacilitySlotComponent>(
		Bin,
		TEXT("InterruptionSlot"));
	Bin->AddInstanceComponent(InterruptionSlot);
	InterruptionSlot->SetupAttachment(Bin->GetRootComponent());
	InterruptionSlot->RegisterComponent();
	TestTrue(TEXT("Used-bin interruption setup reserves the facility slot"),
		InterruptionSlot->TryReserve(Customer));
	TestTrue(TEXT("Used-bin interruption setup enters the facility slot"),
		InterruptionSlot->BeginUse(Customer));
	Session->CurrentFacilityActor = Bin;
	Session->CurrentFacilitySlot = InterruptionSlot;
	Session->ReleaseCurrentFacility();
	TestFalse(TEXT("Used-bin facility exit terminalizes the interrupted handle"), Session->HasTowelHandle());
	TestEqual(TEXT("Preferred bin receives the token before its context is cleared"),
		Bin->GetInventory()->GetSnapshot().Count, 1);
	TestNull(TEXT("Used-bin exit clears the current facility actor after cleanup"),
		Session->CurrentFacilityActor.Get());
	TestNull(TEXT("Used-bin exit clears the current facility slot after cleanup"),
		Session->CurrentFacilitySlot.Get());
	TestEqual(TEXT("Used-bin exit releases the slot after token terminalization"),
		InterruptionSlot->GetSlotState(), EBathhouseFacilitySlotState::Available);

	Stack->GetInventory()->State = ETowelState::Clean;
	Stack->GetInventory()->Count = 1;
	Session->CurrentFacilityActor = Stack;
	TestTrue(TEXT("Overflow setup acquires a final clean token"), Session->TryAcquireCleanTowelFromCurrentFacility());
	TestTrue(TEXT("Overflow setup marks the token Used"), Session->MarkTowelUsed());
	Bin->GetInventory()->Capacity = 1;
	Bin->GetInventory()->State = ETowelState::Used;
	Bin->GetInventory()->Count = 1;
	Session->CurrentFacilityActor = Bin;
	const int32 PendingBefore = World->GetSubsystem<UTowelCirculationSubsystem>()->GetPendingSpillCount();
	TestTrue(TEXT("A full bin does not block customer return"), Session->ReturnTowelToCurrentFacility());
	TestEqual(TEXT("Unavailable floor actor staging preserves the token in PendingSpill"),
		World->GetSubsystem<UTowelCirculationSubsystem>()->GetPendingSpillCount(), PendingBefore + 1);
	TestFalse(TEXT("Pending spill commit still terminalizes the customer handle"), Session->HasTowelHandle());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBathhouseInteractionPromptPresentationTest,
	"BathhouseSim.UI.InteractionPromptPrimarySecondaryCapability",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBathhouseInteractionPromptPresentationTest::RunTest(const FString& Parameters)
{
	FPlayerInteractionQuery SecondaryOnlyQuery;
	SecondaryOnlyQuery.bVisible = true;
	SecondaryOnlyQuery.bCanInteract = false;
	SecondaryOnlyQuery.bSecondaryVisible = true;
	SecondaryOnlyQuery.bCanSecondaryInteract = true;
	TestTrue(TEXT("A secondary-only query keeps the native prompt root enabled"),
		UInteractionPromptWidget::IsPromptRootEnabled(SecondaryOnlyQuery));
	TestFalse(TEXT("A secondary-only query remains disabled in the legacy primary hook"),
		UInteractionPromptWidget::IsLegacyPrimaryEnabled(SecondaryOnlyQuery));

	SecondaryOnlyQuery.bCanInteract = true;
	TestTrue(TEXT("An executable primary query enables the legacy primary hook"),
		UInteractionPromptWidget::IsLegacyPrimaryEnabled(SecondaryOnlyQuery));

	SecondaryOnlyQuery.bCanInteract = false;
	SecondaryOnlyQuery.bCanSecondaryInteract = false;
	TestFalse(TEXT("A query with no executable action disables the native prompt root"),
		UInteractionPromptWidget::IsPromptRootEnabled(SecondaryOnlyQuery));
	return true;
}

#endif
