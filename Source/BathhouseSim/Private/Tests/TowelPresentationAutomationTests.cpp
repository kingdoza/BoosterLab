#if WITH_DEV_AUTOMATION_TESTS

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Towel/CleanTowelStackActor.h"
#include "Towel/Presentation/TowelPileVisualComponent.h"
#include "Towel/Presentation/TowelSlotVisualComponent.h"
#include "Towel/Presentation/TowelStackVisualComponent.h"
#include "Towel/Presentation/TowelVisualMeshProfile.h"
#include "Towel/TowelBasketActor.h"
#include "Towel/TowelInventoryComponent.h"
#include "Towel/TowelProcessingMachineActor.h"
#include "Towel/UsedTowelBinActor.h"

namespace
{
class FScopedTowelPresentationWorld
{
public:
	explicit FScopedTowelPresentationWorld(const TCHAR* BaseName)
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

	~FScopedTowelPresentationWorld()
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

AActor* CreatePresentationOwner(UWorld& World, const TCHAR* RootName)
{
	AActor* Owner = World.SpawnActor<AActor>();
	USceneComponent* Root = NewObject<USceneComponent>(Owner, RootName);
	Owner->SetRootComponent(Root);
	Owner->AddInstanceComponent(Root);
	Root->RegisterComponent();
	return Owner;
}

void BeginActorForPresentationTest(AActor* Actor)
{
	if (Actor && !Actor->HasActorBegunPlay())
	{
		Actor->DispatchBeginPlay();
	}
}

template <typename TVisual>
TVisual* AddVisualComponent(AActor& Owner, const TCHAR* Name)
{
	TVisual* Visual = NewObject<TVisual>(&Owner, Name);
	Owner.AddInstanceComponent(Visual);
	Visual->SetupAttachment(Owner.GetRootComponent());
	Visual->RegisterComponent();
	return Visual;
}

FTowelStateMeshVariants MakeVariants(
	const ETowelState State,
	std::initializer_list<UStaticMesh*> Meshes)
{
	FTowelStateMeshVariants Entry;
	Entry.State = State;
	for (UStaticMesh* Mesh : Meshes)
	{
		Entry.MeshVariants.Add(Mesh);
	}
	return Entry;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBathhouseTowelPresentationTest,
	"BathhouseSim.Towel.Presentation.StackPileSlotAndLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBathhouseTowelPresentationTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FScopedTowelPresentationWorld TestWorld(TEXT("TowelPresentationAutomationWorld"));
	UWorld* World = TestWorld.Get();
	if (!World)
	{
		AddError(TEXT("Failed to create the towel presentation automation world."));
		return false;
	}
	const auto CommitInventoryForPresentation = [](
		UTowelInventoryComponent& Inventory,
		const ETowelState State,
		const int32 Count,
		const int64 TransactionId)
	{
		const FTowelInventorySnapshot Previous = Inventory.GetSnapshot();
		if (Inventory.TryBeginTransaction())
		{
			Inventory.CommitInternal(State, Count);
			Inventory.EndTransaction();
			Inventory.BroadcastCommit(Previous, TransactionId);
		}
	};

	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	UStaticMesh* Sphere = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	UStaticMesh* Cylinder = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (!TestNotNull(TEXT("Cube mesh is available"), Cube)
		|| !TestNotNull(TEXT("Sphere mesh is available"), Sphere)
		|| !TestNotNull(TEXT("Cylinder mesh is available"), Cylinder))
	{
		return false;
	}

	UTowelVisualMeshProfile* InvalidProfile = NewObject<UTowelVisualMeshProfile>();
	InvalidProfile->StateVariants.Add(MakeVariants(ETowelState::None, {Cube}));
	InvalidProfile->StateVariants.Add(MakeVariants(ETowelState::Used, {nullptr}));
	InvalidProfile->StateVariants.Add(MakeVariants(ETowelState::Clean, {Cube, Cube, Sphere}));
	InvalidProfile->StateVariants.Add(MakeVariants(ETowelState::Clean, {Cylinder}));
	TArray<FText> ProfileErrors;
	TArray<FText> ProfileWarnings;
	TestFalse(TEXT("None and duplicate state entries invalidate a profile"),
		InvalidProfile->ValidateProfile(ProfileErrors, ProfileWarnings));
	TestEqual(TEXT("Profile reports the None and duplicate state errors"), ProfileErrors.Num(), 2);
	TestEqual(TEXT("Profile reports the null-only state as a no-visual warning"), ProfileWarnings.Num(), 1);
	TestEqual(TEXT("Null candidates are removed from runtime lookup"),
		InvalidProfile->GetValidVariants(ETowelState::Used).Num(), 0);
	TestEqual(TEXT("Weighted duplicate meshes remain separate candidates"),
		InvalidProfile->GetValidVariants(ETowelState::Clean).Num(), 3);

	UTowelVisualMeshProfile* Profile = NewObject<UTowelVisualMeshProfile>();
	Profile->StateVariants.Add(MakeVariants(ETowelState::Clean, {Cube, Cube, Sphere}));
	Profile->StateVariants.Add(MakeVariants(ETowelState::Used, {Cylinder}));
	Profile->StateVariants.Add(MakeVariants(ETowelState::Wet, {Sphere}));
	FRandomStream SingleCandidateRandom(91);
	const int32 SingleCandidateSeedBefore = SingleCandidateRandom.GetCurrentSeed();
	TestEqual(TEXT("A one-candidate state returns that mesh"),
		Profile->SelectMesh(ETowelState::Wet, SingleCandidateRandom), Sphere);
	TestEqual(TEXT("A one-candidate state does not consume random state"),
		SingleCandidateRandom.GetCurrentSeed(), SingleCandidateSeedBefore);
	FRandomStream WeightedRandom(17);
	int32 CubeSelections = 0;
	int32 SphereSelections = 0;
	for (int32 Draw = 0; Draw < 600; ++Draw)
	{
		UStaticMesh* Selected = Profile->SelectMesh(ETowelState::Clean, WeightedRandom);
		CubeSelections += Selected == Cube ? 1 : 0;
		SphereSelections += Selected == Sphere ? 1 : 0;
	}
	TestTrue(TEXT("Duplicate candidate entries provide measurable selection weight"),
		CubeSelections > SphereSelections);

	AActor* StackOwner = CreatePresentationOwner(*World, TEXT("StackRoot"));
	UTowelStackVisualComponent* Stack = AddVisualComponent<UTowelStackVisualComponent>(
		*StackOwner, TEXT("TestStackVisual"));
	Stack->MeshProfile = Profile;
	Stack->RandomSeed = 27;
	Stack->RandomStream.Initialize(Stack->RandomSeed);
	Stack->BaseLocalOffset = FVector(2.0f, 3.0f, 4.0f);
	Stack->ZSpacing = 6.0f;
	Stack->SetTargetPresentation(ETowelState::Clean, 3, 0, false);
	TestEqual(TEXT("Stack synchronizes the requested count"), Stack->DisplayedCount, 3);
	for (int32 Index = 0; Index < Stack->LayerRecords.Num(); ++Index)
	{
		TestEqual(
			FString::Printf(TEXT("Stack index %d uses pivot local +Z spacing"), Index),
			Stack->LayerRecords[Index].LocalTransform.GetLocation(),
			Stack->BaseLocalOffset + FVector::UpVector * Stack->ZSpacing * Index);
	}
	const TObjectPtr<UStaticMesh> StableMesh0 = Stack->LayerRecords[0].Mesh;
	const TObjectPtr<UStaticMesh> StableMesh1 = Stack->LayerRecords[1].Mesh;
	Stack->SetTargetPresentation(ETowelState::Clean, 4, 1, false);
	TestEqual(TEXT("An unrelated count increase preserves existing mesh zero"), Stack->LayerRecords[0].Mesh, StableMesh0);
	TestEqual(TEXT("An unrelated count increase preserves existing mesh one"), Stack->LayerRecords[1].Mesh, StableMesh1);
	Stack->SetTargetPresentation(ETowelState::Clean, 2, 2, false);
	TestEqual(TEXT("Stack decreases from the global top"), Stack->LayerRecords.Num(), 2);
	const int32 RandomSeedBeforeReAdd = Stack->RandomStream.GetCurrentSeed();
	Stack->SetTargetPresentation(ETowelState::Clean, 3, 3, false);
	TestNotEqual(TEXT("A removed and re-added index performs a fresh mesh draw"),
		Stack->RandomStream.GetCurrentSeed(), RandomSeedBeforeReAdd);

	TArray<FTransform> StackTransformsBeforeSwap;
	for (const FTowelVisualLayerRecord& Record : Stack->LayerRecords)
	{
		StackTransformsBeforeSwap.Add(Record.LocalTransform);
	}
	Stack->SetTargetPresentation(ETowelState::Used, 3, 4, false);
	TestEqual(TEXT("State conversion preserves stack count"), Stack->DisplayedCount, 3);
	for (int32 Index = 0; Index < Stack->LayerRecords.Num(); ++Index)
	{
		TestTrue(TEXT("State conversion preserves each layout transform"),
			Stack->LayerRecords[Index].LocalTransform.Equals(StackTransformsBeforeSwap[Index]));
		TestTrue(TEXT("State conversion replaces only the mesh profile"),
			Stack->LayerRecords[Index].Mesh == Cylinder);
	}
	TestEqual(TEXT("State conversion collapses to one unique mesh bucket"), Stack->MeshBuckets.Num(), 1);
	for (const TPair<TObjectPtr<UStaticMesh>, TObjectPtr<UInstancedStaticMeshComponent>>& Pair : Stack->MeshBuckets)
	{
		TestEqual(TEXT("ISM bucket has no collision"), Pair.Value->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
		TestFalse(TEXT("ISM bucket does not generate overlap events"), Pair.Value->GetGenerateOverlapEvents());
		TestFalse(TEXT("ISM bucket cannot affect navigation"), Pair.Value->CanEverAffectNavigation());
		TestFalse(TEXT("ISM bucket tick is disabled"), Pair.Value->PrimaryComponentTick.bCanEverTick);
	}
	Stack->SetTargetPresentation(ETowelState::None, 0, 5, false);
	TestEqual(TEXT("Removing every layer clears all mesh buckets"), Stack->MeshBuckets.Num(), 0);

	AActor* PileOwnerA = CreatePresentationOwner(*World, TEXT("PileRootA"));
	AActor* PileOwnerB = CreatePresentationOwner(*World, TEXT("PileRootB"));
	UTowelPileVisualComponent* PileA = AddVisualComponent<UTowelPileVisualComponent>(*PileOwnerA, TEXT("PileA"));
	UTowelPileVisualComponent* PileB = AddVisualComponent<UTowelPileVisualComponent>(*PileOwnerB, TEXT("PileB"));
	for (UTowelPileVisualComponent* Pile : {PileA, PileB})
	{
		Pile->MeshProfile = Profile;
		Pile->RandomSeed = 81;
		Pile->RandomStream.Initialize(Pile->RandomSeed);
		Pile->PileHalfExtent = FVector(20.0f, 30.0f, 40.0f);
		Pile->BaseLocalOffset = FVector(1.0f, 2.0f, 3.0f);
		Pile->ItemsPerLayer = 2;
		Pile->LayerSpacing = 10.0f;
		Pile->MaxZJitter = 0.0f;
		Pile->SetTargetPresentation(ETowelState::Clean, 6, 0, false);
	}
	for (int32 Index = 0; Index < PileA->LayerRecords.Num(); ++Index)
	{
		const FVector Relative = PileA->LayerRecords[Index].LocalTransform.GetLocation() - PileA->BaseLocalOffset;
		TestTrue(TEXT("Pile X stays inside authored extent"), FMath::Abs(Relative.X) <= PileA->PileHalfExtent.X);
		TestTrue(TEXT("Pile Y stays inside authored extent"), FMath::Abs(Relative.Y) <= PileA->PileHalfExtent.Y);
		TestTrue(TEXT("Pile Z stays inside authored lower-first extent"),
			Relative.Z >= 0.0f && Relative.Z <= PileA->PileHalfExtent.Z);
		TestEqual(TEXT("Pile fills lower layers first"), Relative.Z,
			static_cast<double>(Index / PileA->ItemsPerLayer) * PileA->LayerSpacing);
		TestTrue(TEXT("Equal seed reproduces each pile transform"),
			PileA->LayerRecords[Index].LocalTransform.Equals(PileB->LayerRecords[Index].LocalTransform));
	}

	AActor* SlotOwner = CreatePresentationOwner(*World, TEXT("SlotRoot"));
	USceneComponent* SlotOne = NewObject<USceneComponent>(SlotOwner, TEXT("SlotOne"));
	USceneComponent* SlotTwo = NewObject<USceneComponent>(SlotOwner, TEXT("SlotTwo"));
	for (USceneComponent* Slot : {SlotOne, SlotTwo})
	{
		SlotOwner->AddInstanceComponent(Slot);
		Slot->SetupAttachment(SlotOwner->GetRootComponent());
		Slot->RegisterComponent();
	}
	SlotOne->SetRelativeLocation(FVector(10.0f, 0.0f, 0.0f));
	SlotTwo->SetRelativeLocation(FVector(20.0f, 5.0f, 0.0f));
	AActor* ForeignOwner = CreatePresentationOwner(*World, TEXT("ForeignSlotRoot"));
	USceneComponent* ForeignSlot = NewObject<USceneComponent>(ForeignOwner, TEXT("ForeignSlot"));
	ForeignOwner->AddInstanceComponent(ForeignSlot);
	ForeignSlot->SetupAttachment(ForeignOwner->GetRootComponent());
	ForeignSlot->RegisterComponent();
	UTowelSlotVisualComponent* SlotVisual = AddVisualComponent<UTowelSlotVisualComponent>(
		*SlotOwner, TEXT("SlotVisual"));
	SlotVisual->MeshProfile = Profile;
	FComponentReference SlotTwoReference;
	SlotTwoReference.OverrideComponent = SlotTwo;
	FComponentReference DuplicateReference = SlotTwoReference;
	FComponentReference ForeignReference;
	ForeignReference.OtherActor = ForeignOwner;
	ForeignReference.OverrideComponent = ForeignSlot;
	FComponentReference UnresolvedReference;
	FComponentReference SlotOneReference;
	SlotOneReference.OverrideComponent = SlotOne;
	SlotVisual->SlotReferences = {
		SlotTwoReference,
		DuplicateReference,
		ForeignReference,
		UnresolvedReference,
		SlotOneReference};
	SlotVisual->ResolveSlots();
	SlotVisual->SetTargetPresentation(ETowelState::Clean, 5, 0, false);
	TestEqual(TEXT("Slot validation retains only ordered same-owner unique references"),
		SlotVisual->ResolvedSlots.Num(), 2);
	TestEqual(TEXT("Slot capacity clamps presentation without changing requested target"),
		SlotVisual->DisplayedCount, 2);
	TestEqual(TEXT("Slot keeps the authoritative target count for diagnostics"), SlotVisual->TargetCount, 5);
	TestTrue(TEXT("First valid authored slot stays first"),
		SlotVisual->LayerRecords[0].LocalTransform.Equals(
			SlotTwo->GetComponentTransform().GetRelativeTransform(SlotVisual->GetComponentTransform())));
	TestTrue(TEXT("Later valid authored slot keeps its relative order"),
		SlotVisual->LayerRecords[1].LocalTransform.Equals(
			SlotOne->GetComponentTransform().GetRelativeTransform(SlotVisual->GetComponentTransform())));
	SlotVisual->PreviewState = ETowelState::Clean;
	SlotVisual->PreviewCount = 0;
	const int64 GameGuardRevisionBefore = SlotVisual->PreviewRevision;
	SlotVisual->RebuildPreview();
	TestEqual(TEXT("Game-world preview rebuild does not mutate the runtime slot presentation"),
		SlotVisual->DisplayedCount, 2);
	TestEqual(TEXT("Game-world preview rebuild preserves the runtime target"), SlotVisual->TargetCount, 5);
	TestEqual(TEXT("Game-world preview rebuild does not advance preview revision"),
		SlotVisual->PreviewRevision, GameGuardRevisionBefore);

	World->WorldType = EWorldType::EditorPreview;
	Stack->PreviewState = ETowelState::Clean;
	Stack->PreviewCount = 3;
	Stack->RebuildPreview();
	TArray<FTowelVisualLayerRecord> FirstStackPreview = Stack->LayerRecords;
	Stack->ClearPreview();
	TestEqual(TEXT("Stack preview clear removes transient records"), Stack->LayerRecords.Num(), 0);
	TestEqual(TEXT("Stack preview clear removes transient buckets"), Stack->MeshBuckets.Num(), 0);
	Stack->RebuildPreview();
	TestEqual(TEXT("Stack preview rebuild restores the requested count"), Stack->DisplayedCount, 3);
	for (int32 Index = 0; Index < Stack->LayerRecords.Num(); ++Index)
	{
		TestTrue(TEXT("Same-seed stack preview reproduces its transform"),
			Stack->LayerRecords[Index].LocalTransform.Equals(FirstStackPreview[Index].LocalTransform));
		TestEqual(TEXT("Same-seed stack preview reproduces its mesh"),
			Stack->LayerRecords[Index].Mesh, FirstStackPreview[Index].Mesh);
	}

	PileA->PreviewState = ETowelState::Clean;
	PileA->PreviewCount = 6;
	PileA->RebuildPreview();
	TArray<FTowelVisualLayerRecord> FirstPilePreview = PileA->LayerRecords;
	PileA->ClearPreview();
	TestEqual(TEXT("Pile preview clear removes transient records"), PileA->LayerRecords.Num(), 0);
	PileA->RebuildPreview();
	TestEqual(TEXT("Pile preview rebuild restores the requested count"), PileA->DisplayedCount, 6);
	for (int32 Index = 0; Index < PileA->LayerRecords.Num(); ++Index)
	{
		TestTrue(TEXT("Same-seed pile preview reproduces its transform"),
			PileA->LayerRecords[Index].LocalTransform.Equals(FirstPilePreview[Index].LocalTransform));
		TestEqual(TEXT("Same-seed pile preview reproduces its mesh"),
			PileA->LayerRecords[Index].Mesh, FirstPilePreview[Index].Mesh);
	}

	SlotVisual->PreviewCount = 2;
	SlotVisual->RebuildPreview();
	TestEqual(TEXT("CallInEditor preview builds the requested valid slots"), SlotVisual->DisplayedCount, 2);
	World->WorldType = EWorldType::Game;
	SlotVisual->PreviewCount = 0;
	SlotVisual->RebuildPreview();
	SlotVisual->ClearPreview();
	TestEqual(TEXT("Game-world preview rebuild and clear leave the existing preview untouched"),
		SlotVisual->DisplayedCount, 2);
	World->WorldType = EWorldType::EditorPreview;
	SlotVisual->ClearPreview();
	TestEqual(TEXT("Slot preview clear removes transient preview records"), SlotVisual->LayerRecords.Num(), 0);
	TestEqual(TEXT("Slot preview clear removes transient preview buckets"), SlotVisual->MeshBuckets.Num(), 0);
	World->WorldType = EWorldType::Game;

	AActor* BindingOwner = CreatePresentationOwner(*World, TEXT("BindingRoot"));
	UTowelInventoryComponent* Inventory = NewObject<UTowelInventoryComponent>(BindingOwner, TEXT("BindingInventory"));
	Inventory->ConfigureDefaults(ETowelState::Clean, 2, 10);
	BindingOwner->AddInstanceComponent(Inventory);
	Inventory->RegisterComponent();
	UTowelStackVisualComponent* BindingVisual = AddVisualComponent<UTowelStackVisualComponent>(
		*BindingOwner, TEXT("BindingVisual"));
	BindingVisual->MeshProfile = Profile;
	World->WorldType = EWorldType::EditorPreview;
	BindingVisual->PreviewState = ETowelState::Clean;
	BindingVisual->PreviewCount = 4;
	BindingVisual->RebuildPreview();
	TestEqual(TEXT("Binding visual creates an unbound editor preview"), BindingVisual->DisplayedCount, 4);
	TestNull(TEXT("Editor preview does not bind an inventory source"), BindingVisual->GetBoundInventorySource());
	World->WorldType = EWorldType::Game;
	BeginActorForPresentationTest(BindingOwner);
	TestEqual(TEXT("BeginPlay removes the stale unbound editor preview"), BindingVisual->DisplayedCount, 0);
	BindingVisual->BindInventorySource(Inventory);
	TestEqual(TEXT("Runtime bind removes stale preview records before applying authority"),
		BindingVisual->LayerRecords.Num(), 2);
	TestEqual(TEXT("Bind applies the current inventory snapshot immediately"), BindingVisual->DisplayedCount, 2);
	CommitInventoryForPresentation(*Inventory, ETowelState::Clean, 5, 1001);
	TestEqual(TEXT("Inventory commit updates the latest target"), BindingVisual->TargetCount, 5);
	BindingVisual->SetTargetPresentation(ETowelState::Clean, 1, 0, false);
	TestEqual(TEXT("A stale presentation revision is ignored"), BindingVisual->TargetCount, 5);
	BindingVisual->SetTargetPresentation(ETowelState::Clean, 1, 1, false);
	TestEqual(TEXT("Equal-revision divergent input resynchronizes to the authoritative target"),
		BindingVisual->TargetCount, 5);
	BindingVisual->SetTargetPresentation(ETowelState::Clean, 8, 2, true);
	BindingVisual->AdvanceOneStep();
	BindingVisual->AdvanceOneStep();
	BindingVisual->SetTargetPresentation(ETowelState::Clean, 1, 3, true);
	BindingVisual->SynchronizeImmediately();
	TestEqual(TEXT("Rapid target reversal converges to the newest count"), BindingVisual->DisplayedCount, 1);
	BindingVisual->UnregisterComponent();
	TestNull(TEXT("Unregister suspends the inventory source"), BindingVisual->GetBoundInventorySource());
	BindingVisual->RegisterComponent();
	TestEqual(TEXT("Reregister restores the explicit inventory source"),
		BindingVisual->GetBoundInventorySource(), Inventory);
	TestEqual(TEXT("Reregister resynchronizes the current authoritative snapshot"),
		BindingVisual->DisplayedCount, Inventory->GetSnapshot().Count);
	BindingVisual->UnbindInventorySource();
	TestNull(TEXT("Unbind removes the explicit source"), BindingVisual->GetBoundInventorySource());
	TestEqual(TEXT("Unbind clears transient presentation"), BindingVisual->DisplayedCount, 0);
	CommitInventoryForPresentation(*Inventory, ETowelState::Clean, 6, 1002);
	TestEqual(TEXT("Unbound inventory commits no longer update the visual target"), BindingVisual->TargetCount, 0);

	ACleanTowelStackActor* CleanStack = World->SpawnActor<ACleanTowelStackActor>();
	AUsedTowelBinActor* UsedBin = World->SpawnActor<AUsedTowelBinActor>();
	ATowelBasketActor* Basket = World->SpawnActor<ATowelBasketActor>();
	ATowelProcessingMachineActor* Machine = World->SpawnActor<ATowelProcessingMachineActor>();
	BeginActorForPresentationTest(CleanStack);
	BeginActorForPresentationTest(UsedBin);
	BeginActorForPresentationTest(Basket);
	BeginActorForPresentationTest(Machine);
	UTowelStackVisualComponent* CleanStackVisual = CleanStack->FindComponentByClass<UTowelStackVisualComponent>();
	UTowelStackVisualComponent* UsedBinVisual = UsedBin->FindComponentByClass<UTowelStackVisualComponent>();
	UTowelStackVisualComponent* BasketVisual = Basket->FindComponentByClass<UTowelStackVisualComponent>();
	UTowelPileVisualComponent* MachineVisual = Machine->FindComponentByClass<UTowelPileVisualComponent>();
	TestNotNull(TEXT("Clean stack owns TowelPresentationVisual"), CleanStackVisual);
	TestNotNull(TEXT("Used bin owns TowelPresentationVisual"), UsedBinVisual);
	TestNotNull(TEXT("Basket owns TowelPresentationVisual"), BasketVisual);
	TestNotNull(TEXT("Processing machine owns TowelPresentationVisual"), MachineVisual);
	TestEqual(TEXT("Clean stack explicitly binds its own inventory"),
		CleanStackVisual->GetBoundInventorySource(), CleanStack->GetInventory());
	TestEqual(TEXT("Used bin explicitly binds its own inventory"),
		UsedBinVisual->GetBoundInventorySource(), UsedBin->GetInventory());
	TestEqual(TEXT("Basket explicitly binds its own inventory"),
		BasketVisual->GetBoundInventorySource(), Basket->GetInventory());
	TestEqual(TEXT("Machine explicitly binds its own inventory"),
		MachineVisual->GetBoundInventorySource(), Machine->GetInventory());
	TestEqual(TEXT("Clean stack initial snapshot is immediately displayed"),
		CleanStackVisual->GetDisplayedCount(), CleanStack->GetInventory()->GetSnapshot().Count);
	TestEqual(TEXT("Stack default subobject name remains stable"), CleanStackVisual->GetFName(),
		FName(TEXT("TowelPresentationVisual")));
	TestEqual(TEXT("Pile default subobject name remains stable"), MachineVisual->GetFName(),
		FName(TEXT("TowelPresentationVisual")));

	MachineVisual->MeshProfile = Profile;
	CommitInventoryForPresentation(*Machine->GetInventory(), ETowelState::Used, 3, 2001);
	MachineVisual->SynchronizeImmediately();
	TArray<FTransform> MachinePileTransforms;
	for (const FTowelVisualLayerRecord& Record : MachineVisual->LayerRecords)
	{
		MachinePileTransforms.Add(Record.LocalTransform);
		TestTrue(TEXT("Machine input revision uses the Used mesh profile"), Record.Mesh == Cylinder);
	}
	CommitInventoryForPresentation(*Machine->GetInventory(), ETowelState::Wet, 3, 2002);
	MachineVisual->SynchronizeImmediately();
	TestEqual(TEXT("Machine state conversion preserves visible count"), MachineVisual->DisplayedCount, 3);
	for (int32 Index = 0; Index < MachineVisual->LayerRecords.Num(); ++Index)
	{
		TestTrue(TEXT("Machine state conversion preserves each pile transform"),
			MachineVisual->LayerRecords[Index].LocalTransform.Equals(MachinePileTransforms[Index]));
		TestTrue(TEXT("Machine output revision replaces only the Wet mesh profile"),
			MachineVisual->LayerRecords[Index].Mesh == Sphere);
	}

	CleanStack->EndPlay(EEndPlayReason::EndPlayInEditor);
	UsedBin->EndPlay(EEndPlayReason::EndPlayInEditor);
	Basket->EndPlay(EEndPlayReason::EndPlayInEditor);
	Machine->EndPlay(EEndPlayReason::EndPlayInEditor);
	TestNull(TEXT("Clean stack EndPlay unbinds TowelPresentationVisual"), CleanStackVisual->GetBoundInventorySource());
	TestNull(TEXT("Used bin EndPlay unbinds TowelPresentationVisual"), UsedBinVisual->GetBoundInventorySource());
	TestNull(TEXT("Basket EndPlay unbinds TowelPresentationVisual"), BasketVisual->GetBoundInventorySource());
	TestNull(TEXT("Machine EndPlay unbinds TowelPresentationVisual"), MachineVisual->GetBoundInventorySource());

	return true;
}

#endif
