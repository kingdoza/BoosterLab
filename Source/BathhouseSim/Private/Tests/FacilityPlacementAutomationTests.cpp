#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Customer/BathhouseCustomerTypes.h"
#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Facility/BathWaterStateComponent.h"
#include "Facility/BathhouseExpansionAuthority.h"
#include "Facility/BathhouseExpansionDefinition.h"
#include "Facility/BathhouseFacilityActor.h"
#include "Facility/BathhouseFacilitySubsystem.h"
#include "Facility/BathhouseFacilityTypes.h"
#include "Facility/LockerActionSlotComponent.h"
#include "Facility/LockerCapacitySubsystem.h"
#include "Interaction/BathhouseKeyRackActor.h"
#include "Interaction/InteractionTypes.h"
#include "Interaction/PhysicalCarryable.h"
#include "Interaction/PhysicalCarryPlacementTransaction.h"
#include "Interaction/PlayerCarryComponent.h"
#include "Interaction/PlayerInteractionComponent.h"
#include "Misc/DataValidation.h"
#include "Placement/FacilityPlacementSettings.h"
#include "Placement/FacilityPlacementComponent.h"
#include "Placement/FacilityPlacementDefinition.h"
#include "Placement/FacilityPlacementPreviewActor.h"
#include "Placement/FacilityPlacementZoneActor.h"
#include "Placement/PlayerFacilityPlacementComponent.h"
#include "Towel/TowelInventoryComponent.h"
#include "Towel/TowelProcessingMachineActor.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBathhouseFacilityPlacementMathTest,
	"BathhouseSim.Placement.SettingsZoneLeaseAndCompatibility",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBathhouseFacilityPlacementMathTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	UFacilityPlacementSettings* Settings = NewObject<UFacilityPlacementSettings>();
	TestEqual(TEXT("Default placement grid is ten centimetres"), Settings->GridSizeCm, 10.0f);
	TestEqual(TEXT("Default recovery drop height is one metre"), Settings->RecoveryDropZOffsetCm, 100.0f);
	Settings->GridSizeCm = -20.0f;
	Settings->RotationStepDegrees = 1000.0f;
	Settings->RecoveryHoldSeconds = 0.0f;
	Settings->RecoveryDropZOffsetCm = -10.0f;
	TestEqual(TEXT("Runtime grid accessor clamps invalid config"), Settings->GetGridSizeCm(), 1.0f);
	TestEqual(TEXT("Runtime rotation accessor clamps invalid config"), Settings->GetRotationStepDegrees(), 180.0f);
	TestEqual(TEXT("Runtime hold accessor clamps invalid config"), Settings->GetRecoveryHoldSeconds(), 0.1f);
	TestEqual(TEXT("Runtime recovery drop height clamps below the footprint floor"), Settings->GetRecoveryDropZOffsetCm(), 0.0f);

	TestEqual(TEXT("Zone-local positive coordinates snap to the common grid"),
		AFacilityPlacementZoneActor::QuantizeLocalCoordinate(24.0f, 10.0f), 20.0f);
	TestEqual(TEXT("Zone-local negative coordinates snap symmetrically"),
		AFacilityPlacementZoneActor::QuantizeLocalCoordinate(-26.0f, 10.0f), -30.0f);
	TestEqual(TEXT("Accumulated yaw remains normalized"),
		AFacilityPlacementZoneActor::NormalizePlacementYaw(450.0f), 90.0f);

	AFacilityPlacementZoneActor* Zone = NewObject<AFacilityPlacementZoneActor>();
	Zone->GetZoneBounds()->SetBoxExtent(FVector(100.0f, 80.0f, 10.0f));
	TestTrue(TEXT("A rotated footprint fully inside one zone is accepted"),
		Zone->ContainsFootprint(FTransform(FRotator(0.0f, 90.0f, 0.0f), FVector::ZeroVector), FVector(30.0f, 20.0f, 5.0f)));
	TestFalse(TEXT("A footprint crossing the zone boundary is rejected"),
		Zone->ContainsFootprint(FTransform(FRotator::ZeroRotator, FVector(90.0f, 0.0f, 0.0f)), FVector(20.0f, 20.0f, 5.0f)));
	Zone->GetZoneBounds()->SetWorldScale3D(FVector(2.0f, 0.5f, 1.0f));
	TestTrue(TEXT("Scaled zone containment compares corners in one local coordinate space"),
		Zone->ContainsFootprint(FTransform(FVector(150.0f, 0.0f, 0.0f)), FVector(20.0f, 20.0f, 5.0f)));
	TestFalse(TEXT("Scaled zone containment rejects a corner beyond the scaled boundary"),
		Zone->ContainsFootprint(FTransform(FVector(190.0f, 0.0f, 0.0f)), FVector(20.0f, 20.0f, 5.0f)));

	UBathWaterStateComponent* Water = NewObject<UBathWaterStateComponent>();
	TestTrue(TEXT("Bath water begins empty"), Water->IsEmpty());
	TestTrue(TEXT("Bath water state commits natively"), Water->SetWaterState(EBathWaterState::Filling));
	TestFalse(TEXT("Filling water blocks the empty query"), Water->IsEmpty());
	Water->SetNormalizedAmount(2.0f);
	TestEqual(TEXT("Optional water presentation amount is clamped"), Water->GetNormalizedAmount(), 1.0f);

	for (const int32 Capacity : { 1, 4, 8 })
	{
		ULockerCapacitySubsystem* Lockers = NewObject<ULockerCapacitySubsystem>();
		AActor* TestBank = NewObject<AActor>();
		ULockerCapacitySubsystem::FBankRecord BankRecord;
		BankRecord.Bank = TestBank;
		TArray<TObjectPtr<ULockerActionSlotComponent>> TestSlots;
		for (int32 SlotIndex = 0; SlotIndex < Capacity; ++SlotIndex)
		{
			ULockerActionSlotComponent* Slot = NewObject<ULockerActionSlotComponent>(TestBank);
			Slot->LockerSlotId = *FString::Printf(TEXT("MathSlot_%d"), SlotIndex);
			TestSlots.Add(Slot);
			BankRecord.Slots.Add(Slot);
		}
		Lockers->Banks.Add(MoveTemp(BankRecord));
		Lockers->InstalledLockerCapacity = Capacity;
		TArray<FLockerCapacityLeaseHandle> Handles;
		for (int32 Index = 0; Index < Capacity; ++Index)
		{
			FLockerCapacityLeaseHandle Handle;
			FText FailureReason;
			TestTrue(FString::Printf(TEXT("Capacity %d admits lease %d"), Capacity, Index),
				Lockers->TryAcquireProvisionalLease(NewObject<AActor>(), Handle, FailureReason));
			TestTrue(TEXT("Provisional lease commits"), Lockers->CommitLease(Handle));
			Handles.Add(Handle);
		}
		TestEqual(TEXT("Committed lease count matches installed capacity"), Lockers->GetActiveLeaseCount(), Capacity);
		FLockerCapacityLeaseHandle Overflow;
		FText FailureReason;
		TestFalse(TEXT("Concurrent admission cannot exceed installed capacity"),
			Lockers->TryAcquireProvisionalLease(NewObject<AActor>(), Overflow, FailureReason));
		TestTrue(TEXT("Lease release succeeds"), Lockers->ReleaseLease(Handles[0]));
		TestTrue(TEXT("Duplicate lease release is idempotent"), Lockers->ReleaseLease(Handles[0]));
	}

	UBathhouseExpansionDefinition* InvalidExpansion = NewObject<UBathhouseExpansionDefinition>();
	FBathhouseExpansionTier InvalidTier;
	InvalidTier.KeyPoolSize = 3;
	InvalidTier.MaxInstalledLockerSlots = 4;
	InvalidExpansion->Tiers.Add(InvalidTier);
	FDataValidationContext Validation;
	TestEqual(TEXT("Expansion validation rejects a key pool smaller than locker capacity"),
		InvalidExpansion->IsDataValid(Validation), EDataValidationResult::Invalid);

	TestEqual(TEXT("Existing physical carry ordinals remain stable"), static_cast<uint8>(EPhysicalCarryKind::MonkeyWrench), static_cast<uint8>(4));
	TestEqual(TEXT("Facility carry kind is appended"), static_cast<uint8>(EPhysicalCarryKind::Facility), static_cast<uint8>(5));
	TestEqual(TEXT("Existing interaction intent ordinals remain stable"), static_cast<uint8>(EPlayerInteractionIntent::EquipmentUse), static_cast<uint8>(3));
	TestEqual(TEXT("Placement intent is appended"), static_cast<uint8>(EPlayerInteractionIntent::PlacementConfirm), static_cast<uint8>(4));
	TestEqual(TEXT("Recovery intent is appended"), static_cast<uint8>(EPlayerInteractionIntent::FacilityRecovery), static_cast<uint8>(5));
	TestEqual(TEXT("ShoeLocker compatibility ordinal remains stable"), static_cast<uint8>(EBathhouseFacilityType::ShoeLocker), static_cast<uint8>(0));
	TestEqual(TEXT("StoreShoes compatibility ordinal remains stable"), static_cast<uint8>(EBathhouseCustomerActivity::StoreShoes), static_cast<uint8>(1));
	TestEqual(TEXT("WearShoes compatibility ordinal remains stable"), static_cast<uint8>(EBathhouseCustomerActivity::WearShoes), static_cast<uint8>(9));

	UPlayerFacilityPlacementComponent* Placement = NewObject<UPlayerFacilityPlacementComponent>();
	Placement->PreviewFacility = NewObject<AActor>();
	Placement->CurrentPlacementQuery = FFacilityPlacementTransactionResult::Succeeded();
	FPlayerInteractionQuery EquipmentQuery;
	EquipmentQuery.bEquipmentUseVisible = true;
	EquipmentQuery.bCanEquipmentUse = true;
	FPlayerInteractionQuery Merged = Placement->MergeSupplementalInteractionQuery(EquipmentQuery);
	TestTrue(TEXT("A missing preview fails closed while retaining the placement row"), Merged.bPlacementVisible && !Merged.bCanPlace);
	TestFalse(TEXT("Placement LMB ownership hides the equipment row"), Merged.bEquipmentUseVisible);
	Placement->PreviewActor = NewObject<AFacilityPlacementPreviewActor>();
	Merged = Placement->MergeSupplementalInteractionQuery(EquipmentQuery);
	TestTrue(TEXT("A live preview permits a successful placement query"), Merged.bPlacementVisible && Merged.bCanPlace);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBathhouseFacilityPlacementRuntimeTest,
	"BathhouseSim.Placement.LockerRecoveryAndExpansionKeyPool",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBathhouseFacilityPlacementRuntimeTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	if (!GEngine)
	{
		AddError(TEXT("GEngine is required for the placement runtime test."));
		return false;
	}
	const FName WorldName = MakeUniqueObjectName(nullptr, UWorld::StaticClass(), TEXT("FacilityPlacementAutomationWorld"));
	FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, WorldName, GetTransientPackage());
	if (!World)
	{
		GEngine->DestroyWorldContext(World);
		AddError(TEXT("Failed to create the placement runtime world."));
		return false;
	}
	World->AddToRoot();
	WorldContext.SetCurrentWorld(World);
	World->InitializeActorsForPlay(FURL());
	World->BeginPlay();

	UBathhouseExpansionDefinition* Expansion = NewObject<UBathhouseExpansionDefinition>();
	Expansion->Tiers = { { 4, 4 }, { 8, 8 } };
	ABathhouseExpansionAuthority* Authority = World->SpawnActorDeferred<ABathhouseExpansionAuthority>(
		ABathhouseExpansionAuthority::StaticClass(), FTransform(FVector(1000.0f, 0.0f, 0.0f)));
	Authority->ExpansionDefinition = Expansion;
	Authority->InitialTierIndex = 0;
	Authority->FinishSpawning(FTransform(FVector(1000.0f, 0.0f, 0.0f)));
	if (!Authority->HasActorBegunPlay())
	{
		Authority->DispatchBeginPlay();
	}

	ABathhouseKeyRackActor* Rack = World->SpawnActorDeferred<ABathhouseKeyRackActor>(
		ABathhouseKeyRackActor::StaticClass(), FTransform(FVector(2000.0f, 0.0f, 0.0f)));
	for (int32 Index = 0; Index < 8; ++Index)
	{
		Rack->PairTransforms.Add(FTransform(FVector(0.0f, Index * 30.0f, 100.0f)));
	}
	Rack->FinishSpawning(FTransform(FVector(2000.0f, 0.0f, 0.0f)));
	if (!Rack->HasActorBegunPlay())
	{
		Rack->DispatchBeginPlay();
	}

	UFacilityPlacementDefinition* LockerDefinition = NewObject<UFacilityPlacementDefinition>();
	LockerDefinition->StableId = TEXT("AutomationLocker4");
	LockerDefinition->FootprintCellsX = 1;
	LockerDefinition->FootprintCellsY = 1;
	LockerDefinition->LockerSlotCount = 4;
	ABathhouseFacilityActor* Locker = World->SpawnActorDeferred<ABathhouseFacilityActor>(
		ABathhouseFacilityActor::StaticClass(), FTransform(FVector(4000.0f, 0.0f, 100.0f)));
	Locker->FacilityType = EBathhouseFacilityType::ClothesLocker;
	Locker->FacilityPlacement->Definition = LockerDefinition;
	TArray<ULockerActionSlotComponent*> AuthoredSlots;
	for (int32 Index = 0; Index < 4; ++Index)
	{
		ULockerActionSlotComponent* Slot = NewObject<ULockerActionSlotComponent>(
			Locker,
			*FString::Printf(TEXT("LockerActionSlot_%d"), Index));
		Locker->AddInstanceComponent(Slot);
		Slot->SetupAttachment(Locker->SceneRoot);
		Slot->LockerSlotId = *FString::Printf(TEXT("LockerSlot_%d"), Index);
		Slot->RegisterComponent();
		AuthoredSlots.Add(Slot);
	}
	Locker->FinishSpawning(FTransform(FVector(4000.0f, 0.0f, 100.0f)));
	if (!Locker->HasActorBegunPlay())
	{
		Locker->DispatchBeginPlay();
	}

	ULockerCapacitySubsystem* Lockers = World->GetSubsystem<ULockerCapacitySubsystem>();
	FText FailureReason;
	AActor* ValidationBank = World->SpawnActor<AActor>();
	ULockerActionSlotComponent* ValidationSlotA = NewObject<ULockerActionSlotComponent>(ValidationBank);
	ULockerActionSlotComponent* ValidationSlotB = NewObject<ULockerActionSlotComponent>(ValidationBank);
	TArray<ULockerActionSlotComponent*> ValidationSlots { ValidationSlotA, ValidationSlotB };
	const int64 ValidationRevision = Lockers->GetRevision();
	TestFalse(TEXT("A missing explicit locker slot ID fails side-effect-free preparation"),
		Lockers->ValidateLockerBankRegistration(ValidationBank, ValidationSlots, 2, FailureReason));
	TestEqual(TEXT("Rejected locker preparation does not publish a capacity revision"),
		Lockers->GetRevision(), ValidationRevision);
	ValidationSlotA->LockerSlotId = TEXT("DuplicateId");
	ValidationSlotB->LockerSlotId = TEXT("DuplicateId");
	TestFalse(TEXT("Duplicate explicit locker slot IDs fail preparation"),
		Lockers->ValidateLockerBankRegistration(ValidationBank, ValidationSlots, 2, FailureReason));
	ValidationSlotB->LockerSlotId = TEXT("UniqueId");
	TestFalse(TEXT("Locker Definition count mismatch fails preparation"),
		Lockers->ValidateLockerBankRegistration(ValidationBank, ValidationSlots, 1, FailureReason));
	TestFalse(TEXT("A well-formed additional bank is still rejected at the expansion limit"),
		Lockers->ValidateLockerBankRegistration(ValidationBank, ValidationSlots, 2, FailureReason));
	// Dynamically-authored instance components are reconstructed by FinishSpawning in this
	// transient test world. Register the retained authored slots explicitly when that happens;
	// packaged Blueprint actors take the normal BeginPlay registration path.
	if (!Lockers->IsLockerBankRegistered(Locker))
	{
		TestTrue(TEXT("Transient locker bank registers its authored action slots"),
			Lockers->RegisterLockerBank(Locker, AuthoredSlots, 4, FailureReason));
		Locker->bPlacedDomainRegistered = true;
	}
	TestEqual(TEXT("Initial expansion tier materializes only its owned key pool"), Rack->GetMaterializedPairCount(), 4);
	TestEqual(TEXT("Placed four-slot bank contributes four installed slots"), Lockers->GetInstalledLockerCapacity(), 4);
	TestEqual(TEXT("Locker placement does not alter expansion-owned key count"), Rack->GetMaterializedPairCount(), 4);
	TestFalse(TEXT("The current tier rejects additional locker slots at its limit"), Lockers->CanInstallLockerSlots(1, FailureReason));

	TArray<FLockerCapacityLeaseHandle> Leases;
	for (int32 Index = 0; Index < 2; ++Index)
	{
		FLockerCapacityLeaseHandle Handle;
		TestTrue(TEXT("Provisional customer capacity is acquired"),
			Lockers->TryAcquireProvisionalLease(NewObject<AActor>(), Handle, FailureReason));
		TestTrue(TEXT("Customer capacity commits with check-in"), Lockers->CommitLease(Handle));
		Leases.Add(Handle);
	}
	TestFalse(TEXT("Recovery cannot reduce installed capacity below active leases"), Locker->QueryFacilityRecovery().bSucceeded);
	for (FLockerCapacityLeaseHandle& Handle : Leases)
	{
		Lockers->ReleaseLease(Handle);
	}
	ULockerActionSlotComponent* ReservedSlot = AuthoredSlots[0];
	AActor* SlotUser = NewObject<AActor>();
	TestTrue(TEXT("Locker action slot reserves for an activity"), ReservedSlot->TryReserve(SlotUser));
	TestFalse(TEXT("A reserved slot blocks bank recovery"), Locker->QueryFacilityRecovery().bSucceeded);
	ReservedSlot->Release(SlotUser);

	UPrimitiveComponent* RecoveryPrimitive = Locker->GetPhysicalCarryPrimitive();
	RecoveryPrimitive->SetCollisionResponseToAllChannels(ECR_Ignore);
	RecoveryPrimitive->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	RecoveryPrimitive->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
	RecoveryPrimitive->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Block);
	RecoveryPrimitive->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	AActor* RecoveryOverlap = World->SpawnActor<AActor>();
	UBoxComponent* RecoveryOverlapBox = NewObject<UBoxComponent>(RecoveryOverlap, TEXT("RecoveryOverlap"));
	RecoveryOverlap->AddInstanceComponent(RecoveryOverlapBox);
	RecoveryOverlap->SetRootComponent(RecoveryOverlapBox);
	RecoveryOverlapBox->SetBoxExtent(FVector(2.0f));
	RecoveryOverlapBox->SetCollisionObjectType(ECC_WorldDynamic);
	RecoveryOverlapBox->SetCollisionResponseToAllChannels(ECR_Overlap);
	RecoveryOverlapBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	RecoveryOverlapBox->RegisterComponent();
	FTransform ExpectedRecoveryDropTransform;
	TestTrue(TEXT("Recovery drop transform resolves from the authored footprint"),
		Locker->FacilityPlacement->GetRecoveryDropTransform(ExpectedRecoveryDropTransform, FailureReason));
	const float ExpectedFootprintBottomZ = Locker->PlacementFootprint->Bounds.Origin.Z
		- Locker->PlacementFootprint->Bounds.BoxExtent.Z;
	TestEqual(TEXT("Recovery keeps the footprint centre X"),
		ExpectedRecoveryDropTransform.GetLocation().X,
		Locker->PlacementFootprint->GetComponentLocation().X);
	TestEqual(TEXT("Recovery keeps the footprint centre Y"),
		ExpectedRecoveryDropTransform.GetLocation().Y,
		Locker->PlacementFootprint->GetComponentLocation().Y);
	TestEqual(TEXT("Recovery places the package at footprint bottom plus the common Z offset"),
		ExpectedRecoveryDropTransform.GetLocation().Z,
		ExpectedFootprintBottomZ
			+ static_cast<double>(GetDefault<UFacilityPlacementSettings>()->GetRecoveryDropZOffsetCm()));
	RecoveryOverlap->SetActorLocation(ExpectedRecoveryDropTransform.GetLocation());
	TestTrue(TEXT("A non-blocking recovery trigger does not block packaging"),
		Locker->QueryFacilityRecovery().bSucceeded);
	RecoveryOverlapBox->SetCollisionObjectType(ECC_PhysicsBody);
	RecoveryOverlapBox->SetCollisionResponseToAllChannels(ECR_Block);
	TestFalse(TEXT("A blocking PhysicsBody prevents package collision enablement"),
		Locker->QueryFacilityRecovery().bSucceeded);
	const FPlayerInteractionQuery BlockedRecoveryPrompt =
		Locker->MergeSupplementalInteractionQuery(FPlayerInteractionQuery());
	TestTrue(TEXT("A blocked recovery still exposes its focused prompt row"),
		BlockedRecoveryPrompt.bRecoveryVisible);
	TestFalse(TEXT("A blocked recovery prompt carries disabled availability"),
		BlockedRecoveryPrompt.bCanRecover);
	TestFalse(TEXT("A blocked recovery prompt carries its failure reason"),
		BlockedRecoveryPrompt.RecoveryFailureReason.IsEmpty());
	RecoveryOverlap->Destroy();
	TestTrue(TEXT("Empty available locker bank passes recovery query"), Locker->QueryFacilityRecovery().bSucceeded);
	AActor* RecoveryViewer = World->SpawnActor<AActor>(AActor::StaticClass(), FVector(3900.0f, 0.0f, 100.0f), FRotator::ZeroRotator);
	UCameraComponent* RecoveryCamera = NewObject<UCameraComponent>(RecoveryViewer, TEXT("RecoveryCamera"));
	RecoveryViewer->AddInstanceComponent(RecoveryCamera);
	RecoveryViewer->SetRootComponent(RecoveryCamera);
	RecoveryCamera->RegisterComponent();
	RecoveryViewer->SetActorLocationAndRotation(FVector(3900.0f, 0.0f, 100.0f), FRotator::ZeroRotator);
	RecoveryCamera->SetWorldLocationAndRotation(FVector(3900.0f, 0.0f, 100.0f), FRotator::ZeroRotator);
	UPlayerInteractionComponent* FocusedInteraction = NewObject<UPlayerInteractionComponent>(RecoveryViewer);
	RecoveryViewer->AddInstanceComponent(FocusedInteraction);
	FocusedInteraction->RegisterComponent();
	FocusedInteraction->Configure(RecoveryCamera, nullptr);
	FocusedInteraction->RefreshInteractionQuery();
	TestTrue(TEXT("Focused facility query carries the recovery prompt without the player placement bridge"),
		FocusedInteraction->GetCurrentInteractionQuery().bRecoveryVisible);
	TestTrue(TEXT("Focused facility query carries the current recovery availability"),
		FocusedInteraction->GetCurrentInteractionQuery().bCanRecover);
	auto MakeRecoveryComponent = [&]()
	{
		UPlayerFacilityPlacementComponent* Component = NewObject<UPlayerFacilityPlacementComponent>(RecoveryViewer);
		RecoveryViewer->AddInstanceComponent(Component);
		Component->RegisterComponent();
		Component->Configure(RecoveryCamera, nullptr, nullptr);
		return Component;
	};
	const float RequiredHold = GetDefault<UFacilityPlacementSettings>()->GetRecoveryHoldSeconds();
	UPlayerFacilityPlacementComponent* Recovery = MakeRecoveryComponent();
	Recovery->Configure(RecoveryCamera, nullptr, FocusedInteraction);
	FocusedInteraction->ConfigureSupplementalIntentSource(Recovery);
	TestTrue(TEXT("Q Started fixes the visible recovery target"), Recovery->BeginRecoveryHold());
	Recovery->UpdateRecoveryHold(RequiredHold * 0.5f);
	TestTrue(TEXT("Active Q progress is merged onto the focused recovery row"),
		FMath::IsNearlyEqual(FocusedInteraction->GetCurrentInteractionQuery().RecoveryProgress, 0.5f));
	Recovery->CancelRecoveryHold();
	TestEqual(TEXT("Early Q release does not mutate the facility"),
		Locker->FacilityPlacement->GetMode(), EPlaceableFacilityMode::Placed);

	TestTrue(TEXT("A second recovery hold may start after cancellation"), Recovery->BeginRecoveryHold());
	Recovery->UpdateRecoveryHold(RequiredHold);
	RecoveryViewer->SetActorRotation(FRotator(0.0f, 180.0f, 0.0f));
	Recovery->CompleteRecoveryHold();
	TestEqual(TEXT("Completed revalidation rejects gaze loss"),
		Locker->FacilityPlacement->GetMode(), EPlaceableFacilityMode::Placed);
	RecoveryViewer->SetActorRotation(FRotator::ZeroRotator);

	TestTrue(TEXT("Recovery starts while locker conditions are valid"), Recovery->BeginRecoveryHold());
	TestTrue(TEXT("The target condition can change during hold"), ReservedSlot->TryReserve(SlotUser));
	Recovery->UpdateRecoveryHold(RequiredHold);
	Recovery->CompleteRecoveryHold();
	TestEqual(TEXT("Completed revalidation rejects a newly reserved slot"),
		Locker->FacilityPlacement->GetMode(), EPlaceableFacilityMode::Placed);
	ReservedSlot->Release(SlotUser);

	TestTrue(TEXT("Recovery session exists before component teardown"), Recovery->BeginRecoveryHold());
	Recovery->DestroyComponent();
	TestEqual(TEXT("Placement component EndPlay cancels without mutation"),
		Locker->FacilityPlacement->GetMode(), EPlaceableFacilityMode::Placed);

	Recovery = MakeRecoveryComponent();
	TestTrue(TEXT("Recovery may begin before a same-frame suppression change"), Recovery->BeginRecoveryHold());
	Recovery->UpdateRecoveryHold(RequiredHold);
	UPlayerInteractionComponent* RecoverySuppression = NewObject<UPlayerInteractionComponent>(RecoveryViewer);
	RecoverySuppression->SetInteractionSuppressed(true);
	Recovery->Configure(RecoveryCamera, nullptr, RecoverySuppression);
	Recovery->CompleteRecoveryHold();
	TestEqual(TEXT("Suppression immediately before Q completion cannot commit recovery"),
		Locker->FacilityPlacement->GetMode(), EPlaceableFacilityMode::Placed);
	RecoverySuppression->SetInteractionSuppressed(false);
	Recovery->Configure(RecoveryCamera, nullptr, nullptr);
	TestTrue(TEXT("Recovery remains independent of a carry component"), Recovery->BeginRecoveryHold());
	Recovery->UpdateRecoveryHold(RequiredHold);
	TestEqual(TEXT("Triggered only updates progress and does not commit"),
		Locker->FacilityPlacement->GetMode(), EPlaceableFacilityMode::Placed);
	Recovery->CompleteRecoveryHold();
	TestEqual(TEXT("Completed converts the same locker actor to packaged mode"),
		Locker->FacilityPlacement->GetMode(), EPlaceableFacilityMode::Packaged);
	TestTrue(TEXT("Recovered package starts at the footprint bottom plus common offset"),
		Locker->GetActorLocation().Equals(ExpectedRecoveryDropTransform.GetLocation(), 0.1f));
	TestEqual(TEXT("Recovered bank unregisters all installed capacity"), Lockers->GetInstalledLockerCapacity(), 0);
	TestTrue(TEXT("Recovery applies no launch velocity"), Locker->GetPhysicalCarryPrimitive()->GetPhysicsLinearVelocity().IsNearlyZero());
	TestEqual(TEXT("Locker recovery leaves the key pool invariant"), Rack->GetMaterializedPairCount(), 4);

	TestTrue(TEXT("Expansion authority supports tier increases"), Authority->TryAdvanceToTier(1, FailureReason));
	TestEqual(TEXT("Tier increase appends newly owned key-hook pairs"), Rack->GetMaterializedPairCount(), 8);
	TestFalse(TEXT("Runtime expansion downgrade is rejected"), Authority->TryAdvanceToTier(0, FailureReason));

	AFacilityPlacementZoneActor* PlacementZone = World->SpawnActor<AFacilityPlacementZoneActor>(
		AFacilityPlacementZoneActor::StaticClass(),
		FVector(5000.0f, 1000.0f, 0.0f),
		FRotator::ZeroRotator);
	PlacementZone->GetZoneBounds()->SetBoxExtent(FVector(100.0f, 100.0f, 5.0f));
	AActor* PlacementFloor = World->SpawnActor<AActor>();
	UBoxComponent* PlacementFloorBox = NewObject<UBoxComponent>(PlacementFloor, TEXT("PlacementFloor"));
	PlacementFloor->AddInstanceComponent(PlacementFloorBox);
	PlacementFloor->SetRootComponent(PlacementFloorBox);
	PlacementFloorBox->SetBoxExtent(FVector(100.0f, 100.0f, 5.0f));
	PlacementFloorBox->SetCollisionObjectType(ECC_WorldStatic);
	PlacementFloorBox->SetCollisionResponseToAllChannels(ECR_Block);
	PlacementFloorBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	PlacementFloorBox->RegisterComponent();
	PlacementFloor->SetActorLocation(FVector(5000.0f, 1000.0f, -5.0f));
	UFacilityPlacementDefinition* ShowerDefinition = NewObject<UFacilityPlacementDefinition>();
	ShowerDefinition->StableId = TEXT("AutomationShower");
	ShowerDefinition->FootprintCellsX = 1;
	ShowerDefinition->FootprintCellsY = 1;
	ABathhouseFacilityActor* PackagedShower = World->SpawnActorDeferred<ABathhouseFacilityActor>(
		ABathhouseFacilityActor::StaticClass(),
		FTransform(FVector(4800.0f, 1000.0f, 100.0f)));
	PackagedShower->FacilityType = EBathhouseFacilityType::Shower;
	PackagedShower->FacilityPlacement->Definition = ShowerDefinition;
	PackagedShower->FacilityPlacement->Mode = EPlaceableFacilityMode::Packaged;
	PackagedShower->FinishSpawning(FTransform(FVector(4800.0f, 1000.0f, 100.0f)));
	if (!PackagedShower->HasActorBegunPlay())
	{
		PackagedShower->DispatchBeginPlay();
	}

	RecoveryViewer->SetActorLocationAndRotation(FVector(5000.0f, 1000.0f, 150.0f), FRotator(-90.0f, 0.0f, 0.0f));
	RecoveryCamera->SetWorldLocationAndRotation(FVector(5000.0f, 1000.0f, 150.0f), FRotator(-90.0f, 0.0f, 0.0f));
	UPlayerCarryComponent* PlacementCarry = NewObject<UPlayerCarryComponent>(RecoveryViewer);
	RecoveryViewer->AddInstanceComponent(PlacementCarry);
	PlacementCarry->ConfigureHeldAnchor(RecoveryCamera);
	PlacementCarry->RegisterComponent();
	TestTrue(TEXT("A packaged facility enters the existing single-carry contract"),
		PlacementCarry->TryTakePhysicalObject(PackagedShower, FailureReason));
	UPlayerFacilityPlacementComponent* PlacementInput = MakeRecoveryComponent();
	PlacementInput->Configure(RecoveryCamera, PlacementCarry, nullptr);
	PlacementInput->HandleHeldObjectChanged(PackagedShower);
	TestTrue(TEXT("A missing preview class keeps the packaged actor held"),
		PlacementInput->IsPlacementActive() && PlacementCarry->GetHeldObject() == PackagedShower);
	TestFalse(TEXT("A missing preview class fails placement closed"),
		PlacementInput->MergeSupplementalInteractionQuery(FPlayerInteractionQuery()).bCanPlace);
	ShowerDefinition->PreviewActorClass = AFacilityPlacementPreviewActor::StaticClass();
	PlacementInput->HandleHeldObjectChanged(PackagedShower);
	AFacilityPlacementPreviewActor* FirstPreview = PlacementInput->PreviewActor.Get();
	TestNotNull(TEXT("A valid preview class spawns a live preview"), FirstPreview);
	FirstPreview->Destroy();
	TestFalse(TEXT("External preview destruction immediately disables placement"),
		PlacementInput->MergeSupplementalInteractionQuery(FPlayerInteractionQuery()).bCanPlace);
	TestTrue(TEXT("Preview destruction preserves the held gameplay actor"),
		PlacementCarry->GetHeldObject() == PackagedShower);
	PlacementInput->HandleHeldObjectChanged(PackagedShower);
	PlacementInput->RefreshPreview();
	TestTrue(FString::Printf(TEXT("A supported in-zone footprint produces a valid placement query: %s"),
		*PlacementInput->CurrentPlacementQuery.FailureReason.ToString()),
		PlacementInput->CurrentPlacementQuery.bSucceeded);

	AActor* NonBlockingTrigger = World->SpawnActor<AActor>();
	UBoxComponent* TriggerBox = NewObject<UBoxComponent>(NonBlockingTrigger, TEXT("NonBlockingTrigger"));
	NonBlockingTrigger->AddInstanceComponent(TriggerBox);
	NonBlockingTrigger->SetRootComponent(TriggerBox);
	TriggerBox->SetBoxExtent(FVector(4.0f));
	TriggerBox->SetCollisionObjectType(ECC_WorldDynamic);
	TriggerBox->SetCollisionResponseToAllChannels(ECR_Overlap);
	TriggerBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	TriggerBox->RegisterComponent();
	NonBlockingTrigger->SetActorLocation(PlacementInput->CurrentCandidate.GetLocation());
	PlacementInput->RefreshPreview();
	TestTrue(TEXT("A non-blocking overlap volume does not invalidate placement"),
		PlacementInput->CurrentPlacementQuery.bSucceeded);
	TriggerBox->SetCollisionObjectType(ECC_PhysicsBody);
	TriggerBox->SetCollisionResponseToAllChannels(ECR_Block);
	PlacementInput->RefreshPreview();
	TestFalse(TEXT("A blocking PhysicsBody invalidates placement"),
		PlacementInput->CurrentPlacementQuery.bSucceeded);
	NonBlockingTrigger->Destroy();
	PlacementInput->RefreshPreview();

	const FTransform HeldWorldBeforeFailure = PackagedShower->GetActorTransform();
	USceneComponent* HeldParentBeforeFailure = PackagedShower->GetRootComponent()->GetAttachParent();
	const ECollisionEnabled::Type CollisionBeforeFailure = PackagedShower->GetPhysicalCarryPrimitive()->GetCollisionEnabled();
	{
		FPhysicalCarryPlacementTransaction FailedTransaction(
			*PackagedShower,
			*PackagedShower->GetPhysicalCarryPrimitive());
		TestTrue(TEXT("The rollback probe applies the candidate mechanically"),
			FailedTransaction.ApplyPlacedWorld(PlacementInput->CurrentCandidate));
		TestFalse(TEXT("A late domain rejection fails the exact carry release transaction"),
			PlacementCarry->CommitReleasePhysicalObjectForPlacement(PackagedShower, []() { return false; }));
	}
	TestTrue(TEXT("Late placement failure restores exact carry ownership"),
		PlacementCarry->GetHeldObject() == PackagedShower);
	TestTrue(TEXT("Late placement failure restores the prior world transform"),
		PackagedShower->GetActorTransform().Equals(HeldWorldBeforeFailure));
	TestTrue(TEXT("Late placement failure restores attachment and collision"),
		PackagedShower->GetRootComponent()->GetAttachParent() == HeldParentBeforeFailure
		&& PackagedShower->GetPhysicalCarryPrimitive()->GetCollisionEnabled() == CollisionBeforeFailure);

	UPlayerInteractionComponent* SuppressedInteraction = NewObject<UPlayerInteractionComponent>(RecoveryViewer);
	SuppressedInteraction->SetInteractionSuppressed(true);
	PlacementInput->Configure(RecoveryCamera, PlacementCarry, SuppressedInteraction);
	TestFalse(TEXT("Suppression immediately before LMB cannot commit placement"),
		PlacementInput->ConfirmPlacement().bSucceeded);
	TestTrue(TEXT("Suppressed LMB preserves the held Packaged actor"),
		PlacementCarry->GetHeldObject() == PackagedShower
		&& PackagedShower->FacilityPlacement->GetMode() == EPlaceableFacilityMode::Packaged);
	SuppressedInteraction->SetInteractionSuppressed(false);
	PlacementInput->Configure(RecoveryCamera, PlacementCarry, nullptr);
	PlacementInput->HandleHeldObjectChanged(PackagedShower);
	PlacementInput->RefreshPreview();
	UBathhouseFacilitySubsystem* Facilities = World->GetSubsystem<UBathhouseFacilitySubsystem>();
	int32 PlacementAvailabilityNotifications = 0;
	bool bPlacementPublishedAfterHandCleared = false;
	bool bReentrantTransitionRejected = false;
	const FDelegateHandle PlacementNotificationHandle = Facilities->OnFacilityAvailabilityChanged.AddLambda(
		[&](const EBathhouseFacilityType FacilityType)
		{
			if (FacilityType != EBathhouseFacilityType::Shower
				|| !IsValid(PackagedShower)
				|| PackagedShower->FacilityPlacement->GetMode() != EPlaceableFacilityMode::Placed)
			{
				return;
			}
			++PlacementAvailabilityNotifications;
			bPlacementPublishedAfterHandCleared = PlacementCarry->IsHandEmpty();
			FText ReentrantFailure;
			bReentrantTransitionRejected = !PackagedShower->CommitPlaceableFacilityMode(
				EPlaceableFacilityMode::Packaged,
				ReentrantFailure);
		});
	const FPlayerInteractionResult PlacementResult = PlacementInput->ConfirmPlacement();
	Facilities->OnFacilityAvailabilityChanged.Remove(PlacementNotificationHandle);
	TestTrue(FString::Printf(TEXT("LMB placement commits the same actor: %s"), *PlacementResult.FailureReason.ToString()),
		PlacementResult.bSucceeded);
	TestEqual(TEXT("Successful placement publishes one facility availability transition"),
		PlacementAvailabilityNotifications, 1);
	TestTrue(TEXT("Facility observers see the carry hand already committed empty"),
		bPlacementPublishedAfterHandCleared);
	TestTrue(TEXT("Synchronous notification reentrancy is rejected"),
		bReentrantTransitionRejected);
	TestTrue(TEXT("Successful placement clears the held reference last"), PlacementCarry->IsHandEmpty());
	TestEqual(TEXT("Successful placement changes the same actor to placed mode"),
		PackagedShower->FacilityPlacement->GetMode(), EPlaceableFacilityMode::Placed);

	TestTrue(TEXT("Recovered locker can be placed again after the tier increase"),
		Locker->CommitPlaceableFacilityMode(EPlaceableFacilityMode::Placed, FailureReason));
	AActor* PersistentCustomer = World->SpawnActor<AActor>();
	FLockerCapacityLeaseHandle PersistentLease;
	TestTrue(TEXT("A committed customer lease exists before unexpected bank loss"),
		Lockers->TryAcquireProvisionalLease(PersistentCustomer, PersistentLease, FailureReason)
		&& Lockers->CommitLease(PersistentLease));
	Recovery->RecoveryTarget = Locker;
	Locker->OnDestroyed.AddUniqueDynamic(Recovery, &UPlayerFacilityPlacementComponent::HandleRecoveryTargetDestroyed);
	AddExpectedError(TEXT("Unexpected locker-bank EndPlay"), EAutomationExpectedErrorFlags::Contains, 1);
	Locker->Destroy();
	TestFalse(TEXT("Recovery target destruction cancels the session immediately"), Recovery->IsRecoveryActive());
	TestEqual(TEXT("Unexpected bank loss removes effective installed capacity"),
		Lockers->GetInstalledLockerCapacity(), 0);
	TestEqual(TEXT("Unexpected bank loss preserves the live customer lease"),
		Lockers->GetActiveLeaseCount(), 1);
	TestTrue(TEXT("Capacity loss below a preserved lease records an invariant fault"),
		Lockers->HasInvariantFault());
	TestFalse(TEXT("Destroyed facility leaves no stale facility registration"),
		World->GetSubsystem<UBathhouseFacilitySubsystem>()->IsFacilityRegistered(Locker));
	PersistentCustomer->Destroy();
	Lockers->CompactInvalidEntries();
	TestEqual(TEXT("Destroyed customer ownership is compacted from the lease registry"),
		Lockers->GetActiveLeaseCount(), 0);
	TestFalse(TEXT("Customer cleanup clears the capacity invariant fault"),
		Lockers->HasInvariantFault());

	ABathhouseFacilityActor* DestroyedFromNotification = World->SpawnActorDeferred<ABathhouseFacilityActor>(
		ABathhouseFacilityActor::StaticClass(),
		FTransform(FVector(7000.0f, 0.0f, 100.0f)));
	DestroyedFromNotification->FacilityType = EBathhouseFacilityType::Shower;
	DestroyedFromNotification->FacilityPlacement->Definition = ShowerDefinition;
	DestroyedFromNotification->FacilityPlacement->Mode = EPlaceableFacilityMode::Packaged;
	DestroyedFromNotification->FinishSpawning(FTransform(FVector(7000.0f, 0.0f, 100.0f)));
	const FDelegateHandle DestructionHandle = Facilities->OnFacilityAvailabilityChanged.AddLambda(
		[&](const EBathhouseFacilityType FacilityType)
		{
			if (FacilityType == EBathhouseFacilityType::Shower
				&& IsValid(DestroyedFromNotification)
				&& DestroyedFromNotification->FacilityPlacement->GetMode() == EPlaceableFacilityMode::Placed)
			{
				DestroyedFromNotification->Destroy();
			}
		});
	TestTrue(TEXT("A synchronous destruction callback does not corrupt the completed transition"),
		DestroyedFromNotification->CommitPlaceableFacilityMode(EPlaceableFacilityMode::Placed, FailureReason));
	Facilities->OnFacilityAvailabilityChanged.Remove(DestructionHandle);
	TestFalse(TEXT("Destruction during commit notification leaves no stale facility entry"),
		Facilities->IsFacilityRegistered(DestroyedFromNotification));

	APawn* RemotePawn = World->SpawnActor<APawn>();
	UCameraComponent* RemoteCamera = NewObject<UCameraComponent>(RemotePawn);
	RemotePawn->AddInstanceComponent(RemoteCamera);
	RemotePawn->SetRootComponent(RemoteCamera);
	RemoteCamera->RegisterComponent();
	UPlayerFacilityPlacementComponent* RemotePlacement = NewObject<UPlayerFacilityPlacementComponent>(RemotePawn);
	RemotePawn->AddInstanceComponent(RemotePlacement);
	RemotePlacement->RegisterComponent();
	RemotePlacement->Configure(RemoteCamera, nullptr, nullptr);
	AFacilityPlacementZoneActor* IgnoredZone = nullptr;
	FVector IgnoredPoint;
	TestFalse(TEXT("A non-locally-controlled pawn cannot perform placement camera traces"),
		RemotePlacement->TracePlacementZone(IgnoredZone, IgnoredPoint));
	TestFalse(TEXT("A non-locally-controlled pawn cannot perform recovery camera traces"),
		RemotePlacement->TraceRecoveryTarget() != nullptr);
	TestFalse(TEXT("A non-local placement component remains idle"), RemotePlacement->IsComponentTickEnabled());

	UFacilityPlacementDefinition* RecoveryGateDefinition = NewObject<UFacilityPlacementDefinition>();
	RecoveryGateDefinition->StableId = TEXT("RecoveryGateFacility");
	RecoveryGateDefinition->PreviewActorClass = AFacilityPlacementPreviewActor::StaticClass();
	ABathhouseFacilityActor* Bath = World->SpawnActorDeferred<ABathhouseFacilityActor>(
		ABathhouseFacilityActor::StaticClass(),
		FTransform(FVector(8000.0f, 0.0f, 100.0f)));
	Bath->FacilityType = EBathhouseFacilityType::Bath;
	Bath->FacilityPlacement->Definition = RecoveryGateDefinition;
	Bath->FinishSpawning(FTransform(FVector(8000.0f, 0.0f, 100.0f)));
	TestTrue(TEXT("An empty idle bath passes its native recovery gate"), Bath->QueryFacilityRecovery().bSucceeded);
	Bath->BathWaterState->SetWaterState(EBathWaterState::Filled);
	TestFalse(TEXT("Native bath water state blocks facility recovery"), Bath->QueryFacilityRecovery().bSucceeded);

	ATowelProcessingMachineActor* Machine = World->SpawnActorDeferred<ATowelProcessingMachineActor>(
		ATowelProcessingMachineActor::StaticClass(),
		FTransform(FVector(9000.0f, 0.0f, 100.0f)));
	Machine->FacilityPlacement->Definition = RecoveryGateDefinition;
	Machine->FinishSpawning(FTransform(FVector(9000.0f, 0.0f, 100.0f)));
	TestTrue(TEXT("An empty waiting towel machine passes its native recovery gate"),
		Machine->QueryFacilityRecovery().bSucceeded);
	Machine->Inventory->Count = 1;
	TestFalse(TEXT("Authoritative towel inventory blocks machine recovery"),
		Machine->QueryFacilityRecovery().bSucceeded);
	Machine->Inventory->Count = 0;
	Machine->MachineState = ETowelMachineState::Processing;
	TestFalse(TEXT("A non-waiting machine state blocks recovery"),
		Machine->QueryFacilityRecovery().bSucceeded);

	World->DestroyWorld(false);
	GEngine->DestroyWorldContext(World);
	World->RemoveFromRoot();
	return true;
}

#endif
