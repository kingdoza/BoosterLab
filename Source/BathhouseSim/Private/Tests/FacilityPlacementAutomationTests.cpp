#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Customer/BathhouseCustomerTypes.h"
#include "Camera/CameraComponent.h"
#include "Character/FirstPersonCharacter.h"
#include "Components/BoxComponent.h"
#include "Components/ActorComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/DamageType.h"
#include "Facility/BathWaterStateComponent.h"
#include "Facility/BathhouseExpansionAuthority.h"
#include "Facility/BathhouseExpansionDefinition.h"
#include "Facility/BathhouseFacilityActor.h"
#include "Facility/BathhouseFacilityPlacementInstanceData.h"
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
#include "Materials/Material.h"
#include "PhysicsEngine/AggregateGeom.h"
#include "PhysicsEngine/BodySetup.h"
#include "Placement/FacilityPlacementSettings.h"
#include "Placement/FacilityActorConversionTransaction.h"
#include "Placement/FacilityPlacementComponent.h"
#include "Placement/FacilityPlacementDefinition.h"
#include "Placement/FacilityPlacementPreviewActor.h"
#include "Placement/FacilityPlacementZoneActor.h"
#include "Placement/PlaceableFacilityItemActor.h"
#include "Placement/PlayerFacilityPlacementComponent.h"
#include "Towel/TowelInventoryComponent.h"
#include "Towel/CleanTowelStackActor.h"
#include "Towel/TowelMachinePlacementInstanceData.h"
#include "Towel/TowelProcessingMachineActor.h"
#include "Towel/UsedTowelBinActor.h"
#include "Tests/FacilityPlacementAutomationTestProbe.h"

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
	Settings->FacilityItemHeldTransform = FTransform(
		FRotator(10.0f, 20.0f, 30.0f), FVector(1.0f, 2.0f, 3.0f), FVector(7.0f));
	TestTrue(TEXT("Global facility held transform preserves location and rotation while normalizing scale"),
		Settings->GetFacilityItemHeldTransform().GetLocation().Equals(FVector(1.0f, 2.0f, 3.0f))
		&& Settings->GetFacilityItemHeldTransform().Rotator().Equals(FRotator(10.0f, 20.0f, 30.0f))
		&& Settings->GetFacilityItemHeldTransform().GetScale3D().Equals(FVector::OneVector));

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

	const FGameplayTag PlacementTag = FGameplayTag::RequestGameplayTag(TEXT("Facility.Placeable"));
	UFacilityPlacementDefinition* ValidDefinition = NewObject<UFacilityPlacementDefinition>();
	ValidDefinition->StableId = TEXT("DefinitionValidation");
	ValidDefinition->FacilityTags.AddTag(PlacementTag);
	ValidDefinition->PlacedFacilityClass = AFacilityPlacementAutomationActor::StaticClass();
	ValidDefinition->RecoveryItemClass = APlaceableFacilityItemActor::StaticClass();
	UFacilityPlacementSettings* GlobalSettings = GetMutableDefault<UFacilityPlacementSettings>();
	const float SavedGridSize = GlobalSettings->GridSizeCm;
	FIntPoint DerivedCells;
	FText FootprintFailure;
	GlobalSettings->GridSizeCm = 5.0f;
	TestTrue(TEXT("Footprint full size derives two-by-two cells after a grid change"),
		ValidDefinition->DeriveFootprintCells(DerivedCells, FootprintFailure)
		&& DerivedCells == FIntPoint(2, 2));
	GlobalSettings->GridSizeCm = 4.0f;
	TestFalse(TEXT("Non-integral footprint-to-grid ratio fails closed"),
		ValidDefinition->DeriveFootprintCells(DerivedCells, FootprintFailure));
	GlobalSettings->GridSizeCm = SavedGridSize;
	AFacilityPlacementAutomationActor* HeightFixture = NewObject<AFacilityPlacementAutomationActor>();
	for (const float HalfHeight : { 38.0f, 40.0f, 50.0f })
	{
		HeightFixture->GetFacilityPlacementComponent()->GetPlacementFootprint()->SetBoxExtent(
			FVector(5.0f, 5.0f, HalfHeight));
		HeightFixture->GetFacilityPlacementComponent()->GetPlacementFootprint()->SetRelativeLocation(
			FVector(0.0f, 0.0f, HalfHeight));
		FTransform FinalTransform;
		TestTrue(FString::Printf(TEXT("Half-height %.0f uses the common floor exactly once"), HalfHeight),
			HeightFixture->GetFacilityPlacementComponent()->BuildPlacedActorTransform(
				FTransform(FRotator::ZeroRotator, FVector(20.0f, 30.0f, 123.0f)),
				FinalTransform,
				FootprintFailure)
			&& FMath::IsNearlyEqual(FinalTransform.GetLocation().Z, 123.0f));
	}
	FDataValidationContext DefinitionValidation;
	TestEqual(TEXT("A complete facility definition normalizes base NotValidated to Valid"),
		ValidDefinition->IsDataValid(DefinitionValidation), EDataValidationResult::Valid);
	ValidDefinition->RecoveryItemClass = AFacilityPlacementItemAutomationActor::StaticClass();
	FText DerivedItemFailure;
	TestTrue(TEXT("A derived common facility item class passes runtime validation"),
		ValidDefinition->ValidateRuntime(DerivedItemFailure));
	FDataValidationContext DerivedItemValidation;
	TestEqual(TEXT("A derived common facility item class passes Editor data validation"),
		ValidDefinition->IsDataValid(DerivedItemValidation), EDataValidationResult::Valid);
	ValidDefinition->RecoveryItemClass = APlaceableFacilityItemActor::StaticClass();

	UFacilityPlacementDefinition* ExcludedStackDefinition = DuplicateObject<UFacilityPlacementDefinition>(
		ValidDefinition, GetTransientPackage());
	ExcludedStackDefinition->PlacedFacilityClass = ACleanTowelStackActor::StaticClass();
	FText FailureReason;
	TestFalse(TEXT("Clean towel stacks are excluded from Actor placement and recovery"),
		ExcludedStackDefinition->ValidateRuntime(FailureReason));
	ExcludedStackDefinition->PlacedFacilityClass = AUsedTowelBinActor::StaticClass();
	TestFalse(TEXT("Used towel bins are excluded from Actor placement and recovery"),
		ExcludedStackDefinition->ValidateRuntime(FailureReason));

	UStaticMesh* OffsetMesh = NewObject<UStaticMesh>();
	OffsetMesh->SetExtendedBounds(FBoxSphereBounds(
		FVector(25.0f, 0.0f, 0.0f),
		FVector(50.0f),
		FVector(50.0f).Length()));
	UBodySetup* OffsetBody = NewObject<UBodySetup>(OffsetMesh);
	OffsetMesh->SetBodySetup(OffsetBody);
	FKBoxElem OffsetBox;
	OffsetBox.Center = FVector(25.0f, 0.0f, 0.0f);
	OffsetBox.X = 100.0f;
	OffsetBox.Y = 100.0f;
	OffsetBox.Z = 100.0f;
	OffsetBody->AggGeom.BoxElems.Add(OffsetBox);
	TestFalse(TEXT("Matching non-zero mesh and box offsets still violate the offset-free contract"),
		APlaceableFacilityItemActor::ValidateRecoveryMesh(*OffsetMesh, FailureReason));

	APlaceableFacilityItemActor* PayloadOwner = NewObject<APlaceableFacilityItemActor>();
	UFacilityPlacementReferenceTestData* ReferenceData =
		NewObject<UFacilityPlacementReferenceTestData>(PayloadOwner);
	FFacilityPlacementPayload ReferencePayload;
	ReferencePayload.Definition = ValidDefinition;
	ReferencePayload.InstanceData = ReferenceData;
	ReferenceData->DirectActor = NewObject<AActor>();
	TestFalse(TEXT("Payload validation rejects a direct Actor reference"),
		ReferencePayload.Validate(*PayloadOwner, FailureReason));
	ReferenceData->DirectActor = nullptr;
	ReferenceData->Nested.Actor = NewObject<AActor>();
	TestFalse(TEXT("Payload validation rejects a nested-struct Actor reference"),
		ReferencePayload.Validate(*PayloadOwner, FailureReason));
	ReferenceData->Nested.Actor = nullptr;
	ReferenceData->ActorArray.Add(NewObject<AActor>());
	TestFalse(TEXT("Payload validation rejects an array Actor reference"),
		ReferencePayload.Validate(*PayloadOwner, FailureReason));
	ReferenceData->ActorArray.Reset();
	ReferenceData->ActorSet.Add(NewObject<AActor>());
	TestFalse(TEXT("Payload validation rejects a set Actor reference"),
		ReferencePayload.Validate(*PayloadOwner, FailureReason));
	ReferenceData->ActorSet.Reset();
	ReferenceData->ComponentMap.Add(TEXT("Component"), NewObject<USceneComponent>());
	TestFalse(TEXT("Payload validation rejects a map Component reference"),
		ReferencePayload.Validate(*PayloadOwner, FailureReason));
	ReferenceData->ComponentMap.Reset();
	UBathhouseFacilityPlacementInstanceData* ScalarData =
		NewObject<UBathhouseFacilityPlacementInstanceData>(PayloadOwner);
	ReferencePayload.InstanceData = ScalarData;
	TestTrue(TEXT("Scalar-only facility payload remains valid"),
		ReferencePayload.Validate(*PayloadOwner, FailureReason));

	TestEqual(TEXT("Existing physical carry ordinals remain stable"), static_cast<uint8>(EPhysicalCarryKind::MonkeyWrench), static_cast<uint8>(4));
	TestEqual(TEXT("Facility carry kind is appended"), static_cast<uint8>(EPhysicalCarryKind::Facility), static_cast<uint8>(5));
	TestEqual(TEXT("Existing interaction intent ordinals remain stable"), static_cast<uint8>(EPlayerInteractionIntent::EquipmentUse), static_cast<uint8>(3));
	TestEqual(TEXT("Placement intent is appended"), static_cast<uint8>(EPlayerInteractionIntent::PlacementConfirm), static_cast<uint8>(4));
	TestEqual(TEXT("Recovery intent is appended"), static_cast<uint8>(EPlayerInteractionIntent::FacilityRecovery), static_cast<uint8>(5));
	TestEqual(TEXT("ShoeLocker compatibility ordinal remains stable"), static_cast<uint8>(EBathhouseFacilityType::ShoeLocker), static_cast<uint8>(0));
	TestEqual(TEXT("StoreShoes compatibility ordinal remains stable"), static_cast<uint8>(EBathhouseCustomerActivity::StoreShoes), static_cast<uint8>(1));
	TestEqual(TEXT("WearShoes compatibility ordinal remains stable"), static_cast<uint8>(EBathhouseCustomerActivity::WearShoes), static_cast<uint8>(9));

	UPlayerFacilityPlacementComponent* Placement = NewObject<UPlayerFacilityPlacementComponent>();
	Placement->PreviewFacility = NewObject<APlaceableFacilityItemActor>();
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
	"BathhouseSim.Placement.ActorReplacementTransaction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBathhouseFacilityPlacementRuntimeTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	UFacilityPlacementSettings* RuntimeSettings = GetMutableDefault<UFacilityPlacementSettings>();
	const TSoftObjectPtr<UMaterialInterface> SavedValidPreviewMaterial = RuntimeSettings->ValidPreviewMaterial;
	const TSoftObjectPtr<UMaterialInterface> SavedInvalidPreviewMaterial = RuntimeSettings->InvalidPreviewMaterial;
	UMaterial* ValidPreviewMaterial = NewObject<UMaterial>();
	UMaterial* InvalidPreviewMaterial = NewObject<UMaterial>();
	ValidPreviewMaterial->BlendMode = BLEND_Translucent;
	InvalidPreviewMaterial->BlendMode = BLEND_Translucent;
	RuntimeSettings->ValidPreviewMaterial = ValidPreviewMaterial;
	RuntimeSettings->InvalidPreviewMaterial = InvalidPreviewMaterial;
	if (!GEngine)
	{
		AddError(TEXT("GEngine is required for the placement runtime test."));
		return false;
	}
	const FName WorldName = MakeUniqueObjectName(nullptr, UWorld::StaticClass(), TEXT("FacilityReplacementAutomationWorld"));
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

	AFirstPersonCharacter* RuntimeCharacter = World->SpawnActor<AFirstPersonCharacter>(
		AFirstPersonCharacter::StaticClass(),
		FVector(-5000.0f, 0.0f, 200.0f),
		FRotator::ZeroRotator);
	TestNotNull(TEXT("Runtime character exists for recovery wiring validation"), RuntimeCharacter);
	if (RuntimeCharacter)
	{
		if (!RuntimeCharacter->HasActorBegunPlay())
		{
			RuntimeCharacter->DispatchBeginPlay();
		}
		TestTrue(TEXT("BeginPlay reconnects the placement progress provider to Interaction"),
			RuntimeCharacter->GetPlayerInteraction()
			&& RuntimeCharacter->GetPlayerInteraction()->SupplementalIntentSource
				== RuntimeCharacter->GetPlayerFacilityPlacement());
		TestTrue(TEXT("BeginPlay reconnects Placement to the runtime Interaction instance"),
			RuntimeCharacter->GetPlayerFacilityPlacement()
			&& RuntimeCharacter->GetPlayerFacilityPlacement()->Interaction
				== RuntimeCharacter->GetPlayerInteraction());
	}

	const FGameplayTag PlacementTag = FGameplayTag::RequestGameplayTag(TEXT("Facility.Placeable"));
	auto ConfigureDefinition = [&](UFacilityPlacementDefinition& Definition, const FName StableId,
		const TSubclassOf<AActor> PlacedClass, const int32 LockerSlotCount = 0)
	{
		Definition.StableId = StableId;
		Definition.FacilityTags.AddTag(PlacementTag);
		Definition.PlacedFacilityClass = PlacedClass;
		Definition.RecoveryItemClass = APlaceableFacilityItemActor::StaticClass();
		Definition.RecoveryItemMesh = nullptr;
		Definition.LockerSlotCount = LockerSlotCount;
	};

	UBathhouseExpansionDefinition* Expansion = NewObject<UBathhouseExpansionDefinition>();
	Expansion->Tiers = { { 4, 4 }, { 8, 8 } };
	ABathhouseExpansionAuthority* Authority = World->SpawnActorDeferred<ABathhouseExpansionAuthority>(
		ABathhouseExpansionAuthority::StaticClass(), FTransform(FVector(1000.0f, 0.0f, 0.0f)));
	Authority->ExpansionDefinition = Expansion;
	Authority->InitialTierIndex = 0;
	Authority->FinishSpawning(FTransform(FVector(1000.0f, 0.0f, 0.0f)));
	if (!Authority->HasActorBegunPlay()) Authority->DispatchBeginPlay();

	ABathhouseKeyRackActor* Rack = World->SpawnActorDeferred<ABathhouseKeyRackActor>(
		ABathhouseKeyRackActor::StaticClass(), FTransform(FVector(2000.0f, 0.0f, 0.0f)));
	for (int32 Index = 0; Index < 8; ++Index)
	{
		Rack->PairTransforms.Add(FTransform(FVector(0.0f, Index * 30.0f, 100.0f)));
	}
	Rack->FinishSpawning(FTransform(FVector(2000.0f, 0.0f, 0.0f)));
	if (!Rack->HasActorBegunPlay()) Rack->DispatchBeginPlay();

	UFacilityPlacementDefinition* LockerDefinition = NewObject<UFacilityPlacementDefinition>();
	ConfigureDefinition(*LockerDefinition, TEXT("AutomationLocker4"), ABathhouseFacilityActor::StaticClass(), 4);
	FText FailureReason;
	TestTrue(TEXT("Complete Definition accepts the native Cube fallback"),
		LockerDefinition->ValidateRuntime(FailureReason));
	LockerDefinition->RecoveryItemClass = nullptr;
	TestFalse(TEXT("Definition requires a common recovery item base or derived class"),
		LockerDefinition->ValidateRuntime(FailureReason));
	LockerDefinition->RecoveryItemClass = APlaceableFacilityItemActor::StaticClass();

	ABathhouseFacilityActor* Locker = World->SpawnActorDeferred<ABathhouseFacilityActor>(
		ABathhouseFacilityActor::StaticClass(), FTransform(FVector(4000.0f, 0.0f, 100.0f)));
	Locker->FacilityType = EBathhouseFacilityType::ClothesLocker;
	Locker->FacilityNumber = 23;
	Locker->SelectionWeight = 2.5f;
	Locker->bEnabled = false;
	Locker->FacilityPlacement->Definition = LockerDefinition;
	TArray<ULockerActionSlotComponent*> AuthoredSlots;
	for (int32 Index = 0; Index < 4; ++Index)
	{
		ULockerActionSlotComponent* Slot = NewObject<ULockerActionSlotComponent>(
			Locker, *FString::Printf(TEXT("LockerActionSlot_%d"), Index));
		Locker->AddInstanceComponent(Slot);
		Slot->SetupAttachment(Locker->SceneRoot);
		Slot->LockerSlotId = *FString::Printf(TEXT("LockerSlot_%d"), Index);
		Slot->RegisterComponent();
		AuthoredSlots.Add(Slot);
	}
	Locker->FinishSpawning(FTransform(FVector(4000.0f, 0.0f, 100.0f)));
	if (!Locker->HasActorBegunPlay()) Locker->DispatchBeginPlay();
	ULockerCapacitySubsystem* Lockers = World->GetSubsystem<ULockerCapacitySubsystem>();
	if (!Lockers->IsLockerBankRegistered(Locker))
	{
		TestTrue(TEXT("Transient locker bank registers its authored action slots"),
			Lockers->RegisterLockerBank(Locker, AuthoredSlots, 4, FailureReason));
		Locker->bPlacedDomainRegistered = true;
		Locker->FacilityPlacement->SetPlacedDomainActive(true);
	}
	Locker->PackagePhysicalRoot->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Locker->PackagePhysicalRoot->SetCollisionResponseToAllChannels(ECR_Ignore);
	Locker->PackagePhysicalRoot->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	TestEqual(TEXT("Initial expansion owns four key pairs"), Rack->GetMaterializedPairCount(), 4);
	TestEqual(TEXT("Placed locker contributes four slots"), Lockers->GetInstalledLockerCapacity(), 4);

	TArray<FLockerCapacityLeaseHandle> Leases;
	for (int32 Index = 0; Index < 2; ++Index)
	{
		FLockerCapacityLeaseHandle Handle;
		TestTrue(TEXT("Locker admits a provisional customer"),
			Lockers->TryAcquireProvisionalLease(NewObject<AActor>(), Handle, FailureReason));
		TestTrue(TEXT("Provisional locker lease commits"), Lockers->CommitLease(Handle));
		Leases.Add(Handle);
	}
	TestFalse(TEXT("Active leases block locker recovery"), Locker->QueryFacilityRecovery().bSucceeded);
	for (FLockerCapacityLeaseHandle& Handle : Leases)
	{
		Lockers->ReleaseLease(Handle);
	}
	ULockerActionSlotComponent* ReservedSlot = AuthoredSlots[0];
	AActor* SlotUser = NewObject<AActor>();
	TestTrue(TEXT("Locker slot reserves for an activity"), ReservedSlot->TryReserve(SlotUser));
	TestFalse(TEXT("Reserved slot blocks locker recovery"), Locker->QueryFacilityRecovery().bSucceeded);
	ReservedSlot->Release(SlotUser);

	FTransform ExpectedLockerDrop;
	TestTrue(TEXT("Recovery drop resolves from the footprint"),
		Locker->FacilityPlacement->GetRecoveryDropTransform(ExpectedLockerDrop, FailureReason));
	AActor* RecoveryBlocker = World->SpawnActor<AActor>();
	UBoxComponent* RecoveryBlockerBox = NewObject<UBoxComponent>(RecoveryBlocker, TEXT("RecoveryBlocker"));
	RecoveryBlocker->AddInstanceComponent(RecoveryBlockerBox);
	RecoveryBlocker->SetRootComponent(RecoveryBlockerBox);
	RecoveryBlockerBox->SetBoxExtent(FVector(2.0f));
	RecoveryBlockerBox->SetCollisionObjectType(ECC_WorldDynamic);
	RecoveryBlockerBox->SetCollisionResponseToAllChannels(ECR_Overlap);
	RecoveryBlockerBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	RecoveryBlockerBox->RegisterComponent();
	RecoveryBlocker->SetActorLocation(ExpectedLockerDrop.GetLocation());
	TestTrue(TEXT("Non-blocking overlap permits recovery"), Locker->QueryFacilityRecovery().bSucceeded);
	RecoveryBlockerBox->SetCollisionObjectType(ECC_PhysicsBody);
	RecoveryBlockerBox->SetCollisionResponseToAllChannels(ECR_Block);
	TestFalse(TEXT("Blocking PhysicsBody rejects recovery before mutation"), Locker->QueryFacilityRecovery().bSucceeded);
	RecoveryBlocker->Destroy();

	UFacilityPlacementDefinition* ShowerDefinition = NewObject<UFacilityPlacementDefinition>();
	ConfigureDefinition(*ShowerDefinition, TEXT("AutomationShower"), AFacilityPlacementAutomationActor::StaticClass());
	ShowerDefinition->RecoveryItemClass = AFacilityPlacementItemAutomationActor::StaticClass();
	ABathhouseFacilityActor* ShowerSource = World->SpawnActorDeferred<ABathhouseFacilityActor>(
		ABathhouseFacilityActor::StaticClass(), FTransform(FVector(6000.0f, 0.0f, 100.0f)));
	ShowerSource->FacilityType = EBathhouseFacilityType::Shower;
	ShowerSource->FacilityNumber = 17;
	ShowerSource->SelectionWeight = 1.75f;
	ShowerSource->bEnabled = false;
	ShowerSource->FacilityPlacement->Definition = ShowerDefinition;
	ShowerSource->FinishSpawning(FTransform(FVector(6000.0f, 0.0f, 100.0f)));
	if (!ShowerSource->HasActorBegunPlay()) ShowerSource->DispatchBeginPlay();
	TWeakObjectPtr<ABathhouseFacilityActor> ShowerSourceWeak(ShowerSource);
	APlaceableFacilityItemActor* ShowerItem = FFacilityActorConversionTransaction::RecoverFacilityToItem(
		*ShowerSource, FailureReason);
	TestNotNull(TEXT("Recovery creates a dedicated facility item"), ShowerItem);
	TestFalse(TEXT("Recovery removes the original facility actor"), ShowerSourceWeak.IsValid());
	if (!ShowerItem)
	{
		World->DestroyWorld(false);
		GEngine->DestroyWorldContext(World);
		World->RemoveFromRoot();
		return false;
	}
	TestTrue(TEXT("Recovery spawns the configured derived facility item class"),
		ShowerItem->IsA(AFacilityPlacementItemAutomationActor::StaticClass()));
	TestTrue(TEXT("Recovery uses the derived facility item CDO scale"),
		ShowerItem->GetActorScale3D().Equals(FVector(0.25f)));
	const UBathhouseFacilityPlacementInstanceData* ShowerPayload =
		Cast<UBathhouseFacilityPlacementInstanceData>(ShowerItem->GetPlacementPayload().InstanceData);
	TestTrue(TEXT("Facility payload preserves authoritative values"), ShowerPayload
		&& ShowerPayload->FacilityType == EBathhouseFacilityType::Shower
		&& ShowerPayload->FacilityNumber == 17
		&& FMath::IsNearlyEqual(ShowerPayload->SelectionWeight, 1.75f)
		&& !ShowerPayload->bEnabled);
	TestTrue(TEXT("Recovered item enables physics, CCD and Pawn ignore"),
		ShowerItem->GetItemRoot()->IsSimulatingPhysics()
		&& ShowerItem->GetItemRoot()->BodyInstance.bUseCCD
		&& ShowerItem->GetItemRoot()->GetCollisionResponseToChannel(ECC_Pawn) == ECR_Ignore);
	TestTrue(TEXT("Recovery adds no launch velocity"),
		ShowerItem->GetItemRoot()->GetPhysicsLinearVelocity().IsNearlyZero()
		&& ShowerItem->GetItemRoot()->GetPhysicsAngularVelocityInDegrees().IsNearlyZero());

	AActor* Player = World->SpawnActor<AActor>(AActor::StaticClass(), FVector(3900.0f, 0.0f, 100.0f), FRotator::ZeroRotator);
	if (!Player->HasActorBegunPlay()) Player->DispatchBeginPlay();
	UCameraComponent* Camera = NewObject<UCameraComponent>(Player, TEXT("PlacementCamera"));
	Player->AddInstanceComponent(Camera);
	Player->SetRootComponent(Camera);
	Camera->RegisterComponent();
	UPlayerCarryComponent* Carry = NewObject<UPlayerCarryComponent>(Player);
	Player->AddInstanceComponent(Carry);
	Carry->ConfigureHeldAnchor(Camera);
	Carry->RegisterComponent();
	const FVector ItemScale(0.5f, 0.75f, 1.25f);
	ShowerItem->SetActorScale3D(ItemScale);
	UFacilityPlacementSettings* MutablePlacementSettings = GetMutableDefault<UFacilityPlacementSettings>();
	const FTransform SavedGlobalHeldTransform = MutablePlacementSettings->FacilityItemHeldTransform;
	MutablePlacementSettings->FacilityItemHeldTransform = FTransform(
		FRotator(0.0f, 20.0f, 0.0f), FVector(12.0f, 3.0f, -8.0f), FVector(4.0f));
	TestTrue(TEXT("Authored HeldTransform scale is ignored"), ShowerItem->GetHeldTransform().GetScale3D().Equals(FVector::OneVector));
	MutablePlacementSettings->FacilityItemHeldTransform = SavedGlobalHeldTransform;
	TestTrue(TEXT("Facility item supports FreeDrop only"),
		ShowerItem->GetPhysicalCarryCapabilities() == EPhysicalCarryCapability::FreeDrop);
	FPlayerInteractionContext TakeContext;
	TakeContext.Interactor = Player;
	TakeContext.CarryComponent = Carry;
	TakeContext.HitActor = ShowerItem;
	TestTrue(TEXT("E interaction picks up the facility item"), ShowerItem->ExecuteInteraction(TakeContext).bSucceeded);
	TestTrue(TEXT("Pickup preserves item Root scale"), ShowerItem->GetActorScale3D().Equals(ItemScale));
	TestTrue(TEXT("G commits free-drop through the common carry transaction"),
		Carry->TryFreeDropHeldObject(FVector::ForwardVector).bSucceeded);
	TestTrue(TEXT("Free-drop preserves item Root scale"), ShowerItem->GetActorScale3D().Equals(ItemScale));
	const FTransform ExpectedFallRecoveryTransform = ShowerItem->GetActorTransform();
	TestTrue(TEXT("Dropped item can be picked up again with E"), ShowerItem->ExecuteInteraction(TakeContext).bSucceeded);
	UFacilityPlacementEventAutomationProbe* CarryEventProbe =
		NewObject<UFacilityPlacementEventAutomationProbe>();
	CarryEventProbe->Bind(Carry, nullptr);
	CarryEventProbe->ResetCounts();
	ShowerItem->FellOutOfWorld(*GetDefault<UDamageType>());
	TestTrue(TEXT("Held facility-item fall clears the authoritative carry owner"), Carry->IsHandEmpty());
	TestTrue(TEXT("Held facility-item fall recovers the same item at its last-safe transform"),
		IsValid(ShowerItem)
		&& ShowerItem->GetActorTransform().Equals(ExpectedFallRecoveryTransform)
		&& ShowerItem->GetItemRoot()->IsSimulatingPhysics());
	TestEqual(TEXT("Held facility-item fall publishes one held change"),
		CarryEventProbe->HeldChangeCount, 1);
	CarryEventProbe->Unbind();
	TestTrue(TEXT("Fall-recovered facility item can be picked up again"),
		ShowerItem->ExecuteInteraction(TakeContext).bSucceeded);

	UPlayerInteractionComponent* FocusedInteraction = NewObject<UPlayerInteractionComponent>(Player);
	Player->AddInstanceComponent(FocusedInteraction);
	FocusedInteraction->RegisterComponent();
	FocusedInteraction->Configure(Camera, nullptr);
	UPlayerFacilityPlacementComponent* PlacementInput = NewObject<UPlayerFacilityPlacementComponent>(Player);
	Player->AddInstanceComponent(PlacementInput);
	PlacementInput->RegisterComponent();
	PlacementInput->Configure(Camera, Carry, FocusedInteraction);
	FocusedInteraction->ConfigureSupplementalIntentSource(PlacementInput);
	Player->SetActorLocationAndRotation(FVector(3900.0f, 0.0f, 100.0f), FRotator::ZeroRotator);
	Camera->SetWorldLocationAndRotation(FVector(3900.0f, 0.0f, 100.0f), FRotator::ZeroRotator);
	FocusedInteraction->RefreshInteractionQuery();
	TestTrue(FString::Printf(TEXT("Recovery trace resolves locker, got %s"),
		*GetNameSafe(PlacementInput->TraceRecoveryTarget())),
		PlacementInput->TraceRecoveryTarget() == Locker);
	const float RequiredHold = GetDefault<UFacilityPlacementSettings>()->GetRecoveryHoldSeconds();
	FocusedInteraction->ConfigureSupplementalIntentSource(nullptr);
	TestTrue(TEXT("Q Started fixes the recovery target"), PlacementInput->BeginRecoveryHold());
	TestTrue(TEXT("Q Started reasserts the live placement progress provider"),
		FocusedInteraction->SupplementalIntentSource == PlacementInput);
	PlacementInput->TickComponent(RequiredHold * 0.5f, LEVELTICK_All, nullptr);
	TestTrue(TEXT("Recovery progress reaches the prompt"),
		FMath::IsNearlyEqual(FocusedInteraction->GetCurrentInteractionQuery().RecoveryProgress, 0.5f));
	PlacementInput->CancelRecoveryHold();
	TestTrue(TEXT("Early Q release leaves source intact"), IsValid(Locker));
	TestTrue(TEXT("Recovery restarts after cancellation"), PlacementInput->BeginRecoveryHold());
	Player->SetActorRotation(FRotator(0.0f, 180.0f, 0.0f));
	PlacementInput->TickComponent(RequiredHold, LEVELTICK_All, nullptr);
	TestTrue(TEXT("Active hold cancels on gaze loss before automatic completion"), IsValid(Locker));
	Player->SetActorRotation(FRotator::ZeroRotator);
	Camera->SetWorldRotation(FRotator::ZeroRotator);
	TestTrue(TEXT("Recovery starts while locker gates are valid"), PlacementInput->BeginRecoveryHold());
	TestTrue(TEXT("Locker state can change during hold"), ReservedSlot->TryReserve(SlotUser));
	PlacementInput->TickComponent(RequiredHold, LEVELTICK_All, nullptr);
	TestTrue(TEXT("Active hold cancels when a locker slot becomes reserved"), IsValid(Locker));
	ReservedSlot->Release(SlotUser);

	UBathhouseFacilitySubsystem* Facilities = World->GetSubsystem<UBathhouseFacilitySubsystem>();
	int32 LockerNotifications = 0;
	bool bHeldUnchangedAtPublish = false;
	const FDelegateHandle LockerHandle = Facilities->OnFacilityAvailabilityChanged.AddLambda(
		[&](const EBathhouseFacilityType FacilityType)
		{
			if (FacilityType == EBathhouseFacilityType::ClothesLocker)
			{
				++LockerNotifications;
				bHeldUnchangedAtPublish = Carry->GetHeldObject() == ShowerItem;
			}
		});
	TWeakObjectPtr<ABathhouseFacilityActor> LockerWeak(Locker);
	TestTrue(TEXT("Final locker hold starts"), PlacementInput->BeginRecoveryHold());
	PlacementInput->TickComponent(RequiredHold, LEVELTICK_All, nullptr);
	TestFalse(TEXT("Hold duration automatically commits recovery before Q release"),
		PlacementInput->IsRecoveryActive());
	PlacementInput->CancelRecoveryHold();
	Facilities->OnFacilityAvailabilityChanged.Remove(LockerHandle);
	TestFalse(TEXT("Successful recovery leaves no original locker"), LockerWeak.IsValid());
	TestEqual(TEXT("Recovery publishes one facility event"), LockerNotifications, 1);
	TestTrue(TEXT("Recovery observer sees existing held item unchanged"), bHeldUnchangedAtPublish);
	TestTrue(TEXT("Recovery leaves existing held object unchanged"), Carry->GetHeldObject() == ShowerItem);
	TestEqual(TEXT("Locker recovery removes installed capacity"), Lockers->GetInstalledLockerCapacity(), 0);
	TestEqual(TEXT("Locker recovery leaves key pool invariant"), Rack->GetMaterializedPairCount(), 4);
	APlaceableFacilityItemActor* LockerItem = nullptr;
	for (TActorIterator<APlaceableFacilityItemActor> It(World); It; ++It)
	{
		if (It->GetDefinition() == LockerDefinition)
		{
			LockerItem = *It;
			break;
		}
	}
	TestNotNull(TEXT("Locker recovery creates the definition-owned item"), LockerItem);
	if (LockerItem)
	{
		const UBathhouseFacilityPlacementInstanceData* LockerPayload =
			Cast<UBathhouseFacilityPlacementInstanceData>(LockerItem->GetPlacementPayload().InstanceData);
		TestTrue(TEXT("Locker payload preserves typed values"), LockerPayload
			&& LockerPayload->FacilityType == EBathhouseFacilityType::ClothesLocker
			&& LockerPayload->FacilityNumber == 23
			&& FMath::IsNearlyEqual(LockerPayload->SelectionWeight, 2.5f)
			&& !LockerPayload->bEnabled);
		TestTrue(TEXT("Recovered locker item uses the requested drop location"),
			LockerItem->GetActorLocation().Equals(ExpectedLockerDrop.GetLocation(), 0.1f));
	}
	TestTrue(TEXT("Expansion tier can advance"), Authority->TryAdvanceToTier(1, FailureReason));
	TestEqual(TEXT("Tier advance appends expansion-owned keys"), Rack->GetMaterializedPairCount(), 8);

	AFacilityPlacementZoneActor* PlacementZone = World->SpawnActor<AFacilityPlacementZoneActor>(
		AFacilityPlacementZoneActor::StaticClass(), FVector(5000.0f, 1000.0f, 0.0f), FRotator::ZeroRotator);
	PlacementZone->GetZoneBounds()->SetBoxExtent(FVector(100.0f, 100.0f, 5.0f));
	AActor* PlacementFloor = World->SpawnActor<AActor>();
	UBoxComponent* FloorBox = NewObject<UBoxComponent>(PlacementFloor, TEXT("PlacementFloor"));
	PlacementFloor->AddInstanceComponent(FloorBox);
	PlacementFloor->SetRootComponent(FloorBox);
	FloorBox->SetBoxExtent(FVector(100.0f, 100.0f, 5.0f));
	FloorBox->SetCollisionObjectType(ECC_WorldStatic);
	FloorBox->SetCollisionResponseToAllChannels(ECR_Block);
	FloorBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	FloorBox->RegisterComponent();
	PlacementFloor->SetActorLocation(FVector(5000.0f, 1000.0f, -5.0f));
	Player->SetActorLocationAndRotation(FVector(5000.0f, 1000.0f, 150.0f), FRotator(-90.0f, 0.0f, 0.0f));
	Camera->SetWorldLocationAndRotation(FVector(5000.0f, 1000.0f, 150.0f), FRotator(-90.0f, 0.0f, 0.0f));
	PlacementInput->Configure(Camera, Carry, nullptr);
	PlacementInput->RefreshPreview();
	TestTrue(FString::Printf(TEXT("Held item produces valid placement preview: %s"),
		*PlacementInput->CurrentPlacementQuery.FailureReason.ToString()),
		PlacementInput->CurrentPlacementQuery.bSucceeded);
	if (AFacilityPlacementPreviewActor* LivePreview = PlacementInput->PreviewActor.Get())
	{
		const TArray<TObjectPtr<UStaticMeshComponent>> InitialMeshes = LivePreview->GetPreviewMeshes();
		TestEqual(TEXT("Native preview clones both eligible Static Mesh components"), InitialMeshes.Num(), 2);
		const UStaticMeshComponent* NestedPreviewMesh = nullptr;
		for (const UStaticMeshComponent* Mesh : InitialMeshes)
		{
			if (Mesh && Mesh->GetName().Contains(TEXT("AutomationPreviewBodySecondary")))
			{
				NestedPreviewMesh = Mesh;
				break;
			}
		}
		TestTrue(FString::Printf(TEXT("Nested preview mesh preserves its Actor-root-relative transform; actual=%s"),
			*(NestedPreviewMesh ? NestedPreviewMesh->GetRelativeLocation().ToString() : FString(TEXT("missing")))),
			NestedPreviewMesh
			&& NestedPreviewMesh->GetRelativeLocation().Equals(FVector(20.0f, 0.0f, 0.0f), 0.01f));
		LivePreview->SetPlacementValidity(false, FText::GetEmpty());
		TestTrue(TEXT("Preview invalidity swaps every slot without recreating components"),
			LivePreview->GetPreviewMeshes() == InitialMeshes
			&& !LivePreview->GetPreviewMeshes().ContainsByPredicate([InvalidPreviewMaterial](const UStaticMeshComponent* Mesh)
			{
				if (!Mesh || Mesh->GetNumMaterials() <= 0)
				{
					return true;
				}
				for (int32 Slot = 0; Slot < Mesh->GetNumMaterials(); ++Slot)
				{
					if (Mesh->GetMaterial(Slot) != InvalidPreviewMaterial)
					{
						return true;
					}
				}
				return false;
			}));
		LivePreview->SetPlacementValidity(true, FText::GetEmpty());

		AFacilityPlacementAutomationActor* PreviewCDO =
			AFacilityPlacementAutomationActor::StaticClass()
				->GetDefaultObject<AFacilityPlacementAutomationActor>();
		UBoxComponent* PreviewFootprint = PreviewCDO
			? PreviewCDO->GetFacilityPlacementComponent()->GetPlacementFootprint() : nullptr;
		if (PreviewFootprint)
		{
			const FVector SavedFootprintExtent = PreviewFootprint->GetUnscaledBoxExtent();
			PreviewFootprint->SetBoxExtent(SavedFootprintExtent + FVector(1.0f, 0.0f, 0.0f));
			FText GeometryFailure;
			TestFalse(TEXT("Preview geometry snapshot rejects a changed authoritative CDO footprint"),
				LivePreview->ValidateSourceGeometry(
					AFacilityPlacementAutomationActor::StaticClass(), GeometryFailure));
			PreviewFootprint->SetBoxExtent(SavedFootprintExtent);
			GeometryFailure = FText::GetEmpty();
			TestTrue(TEXT("Preview geometry snapshot accepts the restored authoritative CDO footprint"),
				LivePreview->ValidateSourceGeometry(
					AFacilityPlacementAutomationActor::StaticClass(), GeometryFailure));
		}
	}

	const TCHAR* BlueprintPreviewClasses[] =
	{
		TEXT("/Game/Bathhouse/Blueprints/Facility/BP_Bath.BP_Bath_C"),
		TEXT("/Game/Bathhouse/Blueprints/Facility/BP_Shower.BP_Shower_C"),
		TEXT("/Game/Bathhouse/Blueprints/Facility/BP_ClothesLocker.BP_ClothesLocker_C"),
		TEXT("/Game/Bathhouse/Blueprints/Facility/BP_ClothesLocker_4.BP_ClothesLocker_4_C"),
		TEXT("/Game/Bathhouse/Blueprints/Facility/BP_ClothesLocker_8.BP_ClothesLocker_8_C"),
		TEXT("/Game/Bathhouse/Blueprints/Towel/BP_Washer.BP_Washer_C"),
		TEXT("/Game/Bathhouse/Blueprints/Towel/BP_Dryer.BP_Dryer_C")
	};
	for (const TCHAR* ClassPath : BlueprintPreviewClasses)
	{
		UClass* BlueprintClass = LoadObject<UClass>(nullptr, ClassPath);
		TestNotNull(FString::Printf(TEXT("Blueprint preview class loads through the cooked class-default path: %s"), ClassPath),
			BlueprintClass);
		if (!BlueprintClass)
		{
			continue;
		}
		AFacilityPlacementPreviewActor* BlueprintPreview =
			World->SpawnActor<AFacilityPlacementPreviewActor>();
		FText BlueprintPreviewFailure;
		const bool bInitialized = BlueprintPreview
			&& BlueprintPreview->InitializeFromPlacedClass(BlueprintClass, BlueprintPreviewFailure);
		TestTrue(FString::Printf(TEXT("Blueprint SCS preview initializes without spawning its gameplay Actor: %s (%s)"),
			ClassPath, *BlueprintPreviewFailure.ToString()), bInitialized);
		if (bInitialized)
		{
			TestTrue(TEXT("Blueprint SCS preview contains at least one eligible body mesh"),
				!BlueprintPreview->GetPreviewMeshes().IsEmpty());
			for (const UStaticMeshComponent* Mesh : BlueprintPreview->GetPreviewMeshes())
			{
				const FString MeshName = Mesh ? Mesh->GetName().ToLower() : FString();
				const bool bExcludedHelper = MeshName.Contains(TEXT("footprint"))
					|| MeshName.Contains(TEXT("helper")) || MeshName.Contains(TEXT("slot"))
					|| MeshName.Contains(TEXT("pile")) || MeshName.Contains(TEXT("water"))
					|| MeshName.Contains(TEXT("contents"));
				bool bAllSlotsReplaced = Mesh && Mesh->GetNumMaterials() > 0;
				for (int32 Slot = 0; Mesh && Slot < Mesh->GetNumMaterials(); ++Slot)
				{
					bAllSlotsReplaced &= Mesh->GetMaterial(Slot) == InvalidPreviewMaterial;
				}
				TestFalse(TEXT("Blueprint SCS preview excludes helper presentation meshes"), bExcludedHelper);
				TestTrue(TEXT("Blueprint SCS preview replaces every material slot"), bAllSlotsReplaced);
			}
		}
		if (BlueprintPreview)
		{
			BlueprintPreview->Destroy();
		}
	}

	const TWeakObjectPtr<AFacilityPlacementPreviewActor> PreviewBeforeFailure = PlacementInput->PreviewActor;
	const FTransform ItemTransformBeforeFailure = ShowerItem->GetActorTransform();
	USceneComponent* ItemParentBeforeFailure = ShowerItem->GetRootComponent()->GetAttachParent();
	const ECollisionEnabled::Type CollisionBeforeFailure = ShowerItem->GetItemRoot()->GetCollisionEnabled();
	ShowerDefinition->PlacedFacilityClass = ATowelProcessingMachineActor::StaticClass();
	PlacementInput->RefreshPreview();
	TestFalse(TEXT("Typed payload/class mismatch rejects placement"), PlacementInput->ConfirmPlacement().bSucceeded);
	TestTrue(TEXT("Failed placement preserves held identity and snapshot"),
		Carry->GetHeldObject() == ShowerItem
		&& ShowerItem->GetActorTransform().Equals(ItemTransformBeforeFailure)
		&& ShowerItem->GetRootComponent()->GetAttachParent() == ItemParentBeforeFailure
		&& ShowerItem->GetItemRoot()->GetCollisionEnabled() == CollisionBeforeFailure
		&& !ShowerItem->GetItemRoot()->IsSimulatingPhysics());
	TestTrue(TEXT("Failed placement preserves preview"),
		PreviewBeforeFailure.IsValid() && PlacementInput->PreviewActor == PreviewBeforeFailure);
	ShowerDefinition->PlacedFacilityClass = AFacilityPlacementAutomationActor::StaticClass();
	PlacementInput->RefreshPreview();

	int32 PlacementNotifications = 0;
	bool bObserverSawEmptyHand = false;
	bool bObserverSawRegistry = false;
	bool bReentrantRecoveryRejected = false;
	const FDelegateHandle PlacementHandle = Facilities->OnFacilityAvailabilityChanged.AddLambda(
		[&](const EBathhouseFacilityType FacilityType)
		{
			if (FacilityType != EBathhouseFacilityType::Shower) return;
			++PlacementNotifications;
			bObserverSawEmptyHand = Carry->IsHandEmpty();
			ABathhouseFacilityActor* NewShower = nullptr;
			for (TActorIterator<ABathhouseFacilityActor> It(World); It; ++It)
			{
				if (It->FacilityPlacement->GetDefinition() == ShowerDefinition)
				{
					NewShower = *It;
					break;
				}
			}
			bObserverSawRegistry = NewShower && Facilities->IsFacilityRegistered(NewShower);
			FText ReentrantFailure;
			bReentrantRecoveryRejected = NewShower
				&& FFacilityActorConversionTransaction::RecoverFacilityToItem(*NewShower, ReentrantFailure) == nullptr;
		});
	TWeakObjectPtr<APlaceableFacilityItemActor> ShowerItemWeak(ShowerItem);
	const FPlayerInteractionResult PlacementResult = PlacementInput->ConfirmPlacement();
	Facilities->OnFacilityAvailabilityChanged.Remove(PlacementHandle);
	TestTrue(FString::Printf(TEXT("LMB replaces item with placed facility: %s"),
		*PlacementResult.FailureReason.ToString()), PlacementResult.bSucceeded);
	TestFalse(TEXT("Successful placement removes original item"), ShowerItemWeak.IsValid());
	TestTrue(TEXT("Successful placement clears hand"), Carry->IsHandEmpty());
	TestEqual(TEXT("Successful placement publishes once"), PlacementNotifications, 1);
	TestTrue(TEXT("Observer sees empty hand and final registry"), bObserverSawEmptyHand && bObserverSawRegistry);
	TestTrue(TEXT("Reentrant recovery is rejected"), bReentrantRecoveryRejected);
	ABathhouseFacilityActor* PlacedShower = nullptr;
	for (TActorIterator<ABathhouseFacilityActor> It(World); It; ++It)
	{
		if (It->FacilityPlacement->GetDefinition() == ShowerDefinition)
		{
			PlacedShower = *It;
			break;
		}
	}
	TestNotNull(TEXT("Placement produces one new placed facility"), PlacedShower);
	TestTrue(TEXT("Payload survives the item boundary"), PlacedShower
		&& PlacedShower->FacilityNumber == 17
		&& FMath::IsNearlyEqual(PlacedShower->SelectionWeight, 1.75f)
		&& !PlacedShower->bEnabled);
	TestTrue(TEXT("Item scale does not leak into placed actor"), PlacedShower
		&& PlacedShower->GetActorScale3D().Equals(
			ABathhouseFacilityActor::StaticClass()->GetDefaultObject<AActor>()->GetActorScale3D()));
	TestTrue(TEXT("Placed domain is active and legacy carry fails closed"), PlacedShower
		&& PlacedShower->FacilityPlacement->IsPlacedDomainActive()
		&& !PlacedShower->FacilityPlacement->IsStagedPlacement()
		&& PlacedShower->GetPhysicalCarryCapabilities() == EPhysicalCarryCapability::None
		&& PlacedShower->GetPhysicalCarryPrimitive() == nullptr
		&& !PlacedShower->CommitPlaceableFacilityMode(EPlaceableFacilityMode::Packaged, FailureReason));

	UFacilityPlacementDefinition* BathDefinition = NewObject<UFacilityPlacementDefinition>();
	ConfigureDefinition(*BathDefinition, TEXT("RecoveryGateBath"), ABathhouseFacilityActor::StaticClass());
	ABathhouseFacilityActor* Bath = World->SpawnActorDeferred<ABathhouseFacilityActor>(
		ABathhouseFacilityActor::StaticClass(), FTransform(FVector(8000.0f, 0.0f, 100.0f)));
	Bath->FacilityType = EBathhouseFacilityType::Bath;
	Bath->FacilityPlacement->Definition = BathDefinition;
	Bath->FinishSpawning(FTransform(FVector(8000.0f, 0.0f, 100.0f)));
	if (!Bath->HasActorBegunPlay()) Bath->DispatchBeginPlay();
	TestTrue(TEXT("Empty bath passes recovery gate"), Bath->QueryFacilityRecovery().bSucceeded);
	Bath->BathWaterState->SetWaterState(EBathWaterState::Filled);
	TestFalse(TEXT("Bath water blocks recovery"), Bath->QueryFacilityRecovery().bSucceeded);

	UFacilityPlacementDefinition* MachineDefinition = NewObject<UFacilityPlacementDefinition>();
	ConfigureDefinition(*MachineDefinition, TEXT("RecoveryGateMachine"), ATowelProcessingMachineActor::StaticClass());
	ATowelProcessingMachineActor* Machine = World->SpawnActorDeferred<ATowelProcessingMachineActor>(
		ATowelProcessingMachineActor::StaticClass(), FTransform(FVector(9000.0f, 0.0f, 100.0f)));
	Machine->MachineKind = ETowelMachineKind::Dryer;
	Machine->ProcessingDurationSeconds = 13.0f;
	Machine->FacilityPlacement->Definition = MachineDefinition;
	Machine->FinishSpawning(FTransform(FVector(9000.0f, 0.0f, 100.0f)));
	if (!Machine->HasActorBegunPlay()) Machine->DispatchBeginPlay();
	const FFacilityPlacementTransactionResult MachineRecoveryQuery = Machine->QueryFacilityRecovery();
	TestTrue(FString::Printf(TEXT("Empty waiting machine passes recovery gate: %s"),
		*MachineRecoveryQuery.FailureReason.ToString()), MachineRecoveryQuery.bSucceeded);
	Machine->Inventory->Count = 1;
	TestFalse(TEXT("Machine inventory blocks recovery"), Machine->QueryFacilityRecovery().bSucceeded);
	Machine->Inventory->Count = 0;
	Machine->MachineState = ETowelMachineState::Processing;
	TestFalse(TEXT("Processing machine blocks recovery"), Machine->QueryFacilityRecovery().bSucceeded);
	Machine->MachineState = ETowelMachineState::Waiting;
	TWeakObjectPtr<ATowelProcessingMachineActor> MachineWeak(Machine);
	APlaceableFacilityItemActor* MachineItem = FFacilityActorConversionTransaction::RecoverFacilityToItem(*Machine, FailureReason);
	TestNotNull(TEXT("Machine recovery creates common facility item"), MachineItem);
	TestFalse(TEXT("Machine recovery removes source actor"), MachineWeak.IsValid());
	if (MachineItem)
	{
		const UTowelMachinePlacementInstanceData* MachinePayload =
			Cast<UTowelMachinePlacementInstanceData>(MachineItem->GetPlacementPayload().InstanceData);
		TestTrue(TEXT("Machine payload preserves typed values"), MachinePayload
			&& MachinePayload->MachineKind == ETowelMachineKind::Dryer
			&& FMath::IsNearlyEqual(MachinePayload->ProcessingDurationSeconds, 13.0f));
	}

	World->DestroyWorld(false);
	GEngine->DestroyWorldContext(World);
	World->RemoveFromRoot();
	RuntimeSettings->ValidPreviewMaterial = SavedValidPreviewMaterial;
	RuntimeSettings->InvalidPreviewMaterial = SavedInvalidPreviewMaterial;
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBathhouseFacilityConversionSafetyTest,
	"BathhouseSim.Placement.ActorReplacementFailureAtomicity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBathhouseFacilityConversionSafetyTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	if (!GEngine)
	{
		AddError(TEXT("GEngine is required for the facility conversion safety test."));
		return false;
	}

	FFacilityActorConversionTransaction::ClearTestFault();
	const FName WorldName = MakeUniqueObjectName(
		nullptr,
		UWorld::StaticClass(),
		TEXT("FacilityConversionSafetyWorld"));
	FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, WorldName, GetTransientPackage());
	if (!World)
	{
		GEngine->DestroyWorldContext(World);
		AddError(TEXT("Failed to create the facility conversion safety world."));
		return false;
	}
	World->AddToRoot();
	WorldContext.SetCurrentWorld(World);
	World->InitializeActorsForPlay(FURL());
	World->BeginPlay();

	const FGameplayTag PlacementTag = FGameplayTag::RequestGameplayTag(TEXT("Facility.Placeable"));
	auto ConfigureDefinition = [&](UFacilityPlacementDefinition& Definition,
		const FName StableId,
		const TSubclassOf<AActor> PlacedClass,
		const int32 CellsX = 1,
		const int32 CellsY = 1,
		const int32 LockerSlots = 0)
	{
		Definition.StableId = StableId;
		Definition.FacilityTags.AddTag(PlacementTag);
		Definition.PlacedFacilityClass = PlacedClass;
		Definition.RecoveryItemClass = APlaceableFacilityItemActor::StaticClass();
		(void)CellsX;
		(void)CellsY;
		Definition.LockerSlotCount = LockerSlots;
	};

	auto SpawnFacility = [&](UFacilityPlacementDefinition& Definition,
		const FVector& Location,
		const EBathhouseFacilityType Type = EBathhouseFacilityType::Shower)
	{
		AFacilityPlacementAutomationActor* Actor =
			World->SpawnActorDeferred<AFacilityPlacementAutomationActor>(
				Definition.PlacedFacilityClass,
				FTransform(Location));
		Actor->ConfigureForTest(Definition, Type);
		Actor->FinishSpawning(FTransform(Location));
		if (!Actor->HasActorBegunPlay()) Actor->DispatchBeginPlay();
		return Actor;
	};

	auto CountFacilityItems = [&]()
	{
		int32 Count = 0;
		for (TActorIterator<APlaceableFacilityItemActor> It(World); It; ++It)
		{
			Count += IsValid(*It) ? 1 : 0;
		}
		return Count;
	};
	auto CountPlacedActorsForDefinition = [&](const UFacilityPlacementDefinition* ExpectedDefinition)
	{
		int32 Count = 0;
		for (TActorIterator<AFacilityPlacementAutomationActor> It(World); It; ++It)
		{
			Count += IsValid(*It)
				&& It->GetFacilityPlacementComponent()->GetDefinition() == ExpectedDefinition ? 1 : 0;
		}
		return Count;
	};

	UFacilityPlacementDefinition* Definition = NewObject<UFacilityPlacementDefinition>();
	ConfigureDefinition(
		*Definition,
		TEXT("ConversionFailureAutomation"),
		AFacilityPlacementAutomationActor::StaticClass());
	UBathhouseFacilitySubsystem* Facilities = World->GetSubsystem<UBathhouseFacilitySubsystem>();
	FText FailureReason;

	struct FRecoveryFaultCase
	{
		FFacilityActorConversionTransaction::ETestFault Fault;
		const TCHAR* Name;
	};
	const FRecoveryFaultCase RecoveryFaults[] =
	{
		{ FFacilityActorConversionTransaction::ETestFault::RecoverySpawn, TEXT("spawn") },
		{ FFacilityActorConversionTransaction::ETestFault::RecoveryPayload, TEXT("payload") },
		{ FFacilityActorConversionTransaction::ETestFault::RecoveryCommitCollision, TEXT("commit collision") },
		{ FFacilityActorConversionTransaction::ETestFault::RecoveryDomainUnregistration, TEXT("silent unregister") },
		{ FFacilityActorConversionTransaction::ETestFault::RecoveryActivation, TEXT("physics activation") },
		{ FFacilityActorConversionTransaction::ETestFault::RecoverySourceDestroy, TEXT("source destroy") }
	};
	int32 FaultIndex = 0;
	for (const FRecoveryFaultCase& FaultCase : RecoveryFaults)
	{
		AFacilityPlacementAutomationActor* Source = SpawnFacility(
			*Definition,
			FVector(1000.0f + FaultIndex * 1000.0f, 0.0f, 100.0f));
		const int32 ItemCountBefore = CountFacilityItems();
		FFacilityActorConversionTransaction::SetTestFault(FaultCase.Fault);
		APlaceableFacilityItemActor* Result =
			FFacilityActorConversionTransaction::RecoverFacilityToItem(*Source, FailureReason);
		TestNull(FString::Printf(TEXT("Injected recovery %s failure rejects conversion"), FaultCase.Name), Result);
		TestTrue(FString::Printf(TEXT("Injected recovery %s failure preserves source identity"), FaultCase.Name),
			IsValid(Source) && Facilities->IsFacilityRegistered(Source)
			&& Source->GetFacilityPlacementComponent()->IsPlacedDomainActive());
		TestEqual(FString::Printf(TEXT("Injected recovery %s failure leaves no staged item"), FaultCase.Name),
			CountFacilityItems(), ItemCountBefore);
		Source->Destroy();
		++FaultIndex;
	}
	FFacilityActorConversionTransaction::ClearTestFault();

	AFacilityPlacementZoneAutomationActor* Zone =
		World->SpawnActor<AFacilityPlacementZoneAutomationActor>(
			AFacilityPlacementZoneAutomationActor::StaticClass(),
			FTransform(FVector(10000.0f, 0.0f, 0.0f)));
	Zone->AddAllowedTag(PlacementTag);
	Zone->GetZoneBounds()->SetBoxExtent(FVector(10000.0f, 10000.0f, 500.0f));

	AActor* CarryOwner = World->SpawnActor<AActor>();
	USceneComponent* HeldAnchor = NewObject<USceneComponent>(CarryOwner, TEXT("SafetyHeldAnchor"));
	CarryOwner->AddInstanceComponent(HeldAnchor);
	CarryOwner->SetRootComponent(HeldAnchor);
	HeldAnchor->RegisterComponent();
	UPlayerCarryComponent* Carry = NewObject<UPlayerCarryComponent>(CarryOwner);
	CarryOwner->AddInstanceComponent(Carry);
	Carry->ConfigureHeldAnchor(HeldAnchor);
	Carry->RegisterComponent();

	AFacilityPlacementAutomationActor* PlacementSource =
		SpawnFacility(*Definition, FVector(8000.0f, 0.0f, 100.0f));
	APlaceableFacilityItemActor* PlacementItem =
		FFacilityActorConversionTransaction::RecoverFacilityToItem(*PlacementSource, FailureReason);
	TestNotNull(TEXT("Placement fault fixture recovers a facility item"), PlacementItem);
	if (!PlacementItem || !Carry->TryTakePhysicalObject(PlacementItem, FailureReason))
	{
		FFacilityActorConversionTransaction::ClearTestFault();
		World->DestroyWorld(false);
		GEngine->DestroyWorldContext(World);
		World->RemoveFromRoot();
		AddError(TEXT("Placement fault fixture could not hold the facility item."));
		return false;
	}

	const FTransform HeldTransformBefore = PlacementItem->GetActorTransform();
	USceneComponent* HeldParentBefore = PlacementItem->GetRootComponent()->GetAttachParent();
	const FName HeldSocketBefore = PlacementItem->GetRootComponent()->GetAttachSocketName();
	const ECollisionEnabled::Type HeldCollisionBefore = PlacementItem->GetItemRoot()->GetCollisionEnabled();
	const bool bHeldPhysicsBefore = PlacementItem->GetItemRoot()->IsSimulatingPhysics();
	const bool bHeldCCDBefore = PlacementItem->GetItemRoot()->BodyInstance.bUseCCD;
	const FTransform PlacementCandidate(FRotator::ZeroRotator, FVector(10000.0f, 0.0f, 100.0f));

	struct FPlacementFaultCase
	{
		FFacilityActorConversionTransaction::ETestFault Fault;
		const TCHAR* Name;
	};
	const FPlacementFaultCase PlacementFaults[] =
	{
		{ FFacilityActorConversionTransaction::ETestFault::PlacementSpawn, TEXT("spawn") },
		{ FFacilityActorConversionTransaction::ETestFault::PlacementCollisionSnapshotDuplicate, TEXT("duplicate collision snapshot") },
		{ FFacilityActorConversionTransaction::ETestFault::PlacementCollisionSnapshotMissing, TEXT("missing collision snapshot") },
		{ FFacilityActorConversionTransaction::ETestFault::PlacementImport, TEXT("import") },
		{ FFacilityActorConversionTransaction::ETestFault::PlacementDomainRegistration, TEXT("domain registration") },
		{ FFacilityActorConversionTransaction::ETestFault::PlacementCarryCommit, TEXT("carry commit") }
	};
	for (const FPlacementFaultCase& FaultCase : PlacementFaults)
	{
		FFacilityActorConversionTransaction::SetTestFault(FaultCase.Fault);
		AActor* Result = FFacilityActorConversionTransaction::PlaceItemAsFacility(
			*PlacementItem,
			PlacementCandidate,
			*Zone,
			*Carry,
			FailureReason);
		TestNull(FString::Printf(TEXT("Injected placement %s failure rejects conversion"), FaultCase.Name), Result);
		TestTrue(FString::Printf(TEXT("Injected placement %s failure preserves exact held identity"), FaultCase.Name),
			IsValid(PlacementItem) && Carry->GetHeldObject() == PlacementItem
			&& PlacementItem->IsHeldForPlacement());
		TestTrue(FString::Printf(TEXT("Injected placement %s failure preserves the physical snapshot"), FaultCase.Name),
			PlacementItem->GetActorTransform().Equals(HeldTransformBefore)
			&& PlacementItem->GetRootComponent()->GetAttachParent() == HeldParentBefore
			&& PlacementItem->GetRootComponent()->GetAttachSocketName() == HeldSocketBefore
			&& PlacementItem->GetItemRoot()->GetCollisionEnabled() == HeldCollisionBefore
			&& PlacementItem->GetItemRoot()->IsSimulatingPhysics() == bHeldPhysicsBefore
			&& PlacementItem->GetItemRoot()->BodyInstance.bUseCCD == bHeldCCDBefore);
		TestEqual(FString::Printf(TEXT("Injected placement %s failure leaves no staged facility"), FaultCase.Name),
			CountPlacedActorsForDefinition(Definition), 0);
	}
	FFacilityActorConversionTransaction::ClearTestFault();
	TestTrue(TEXT("Fault fixture can recover its held item through the carry owner"),
		Carry->RecoverHeldPhysicalObject(PlacementItem));
	PlacementItem->Destroy();

	UFacilityPlacementDefinition* ConstructionCollisionDefinition =
		NewObject<UFacilityPlacementDefinition>();
	ConfigureDefinition(
		*ConstructionCollisionDefinition,
		TEXT("ConstructionCollisionAutomation"),
		AFacilityPlacementConstructionCollisionAutomationActor::StaticClass());
	AFacilityPlacementAutomationActor* ConstructionSource = SpawnFacility(
		*ConstructionCollisionDefinition,
		FVector(11000.0f, 0.0f, 100.0f));
	TestTrue(TEXT("Construction collision fixture enables Actor collision after FinishSpawning"),
		ConstructionSource && ConstructionSource->GetActorEnableCollision());
	FailureReason = FText::GetEmpty();
	APlaceableFacilityItemActor* ConstructionItem = ConstructionSource
		? FFacilityActorConversionTransaction::RecoverFacilityToItem(*ConstructionSource, FailureReason)
		: nullptr;
	TestNotNull(FString::Printf(TEXT("Construction collision fixture recovers an item: %s"),
		*FailureReason.ToString()), ConstructionItem);
	if (ConstructionItem && Carry->TryTakePhysicalObject(ConstructionItem, FailureReason))
	{
		AActor* RebuiltConstructionActor = FFacilityActorConversionTransaction::PlaceItemAsFacility(
			*ConstructionItem,
			FTransform(FRotator::ZeroRotator, FVector(11500.0f, 0.0f, 100.0f)),
			*Zone,
			*Carry,
			FailureReason);
		TestTrue(FString::Printf(TEXT("Placement restores the post-Construction authored collision value: %s"),
			*FailureReason.ToString()),
			RebuiltConstructionActor && RebuiltConstructionActor->GetActorEnableCollision());
		if (RebuiltConstructionActor)
		{
			TestFalse(TEXT("Successful placement consumes the collision snapshot"),
				CastChecked<IPlaceableFacility>(RebuiltConstructionActor)
					->GetFacilityPlacementComponent()->HasActorCollisionSnapshot());
			RebuiltConstructionActor->Destroy();
		}
	}
	else
	{
		AddError(TEXT("Construction collision fixture could not hold its recovered item."));
	}

	FTransform ScaledSourceTransform(FRotator::ZeroRotator, FVector(12000.0f, 0.0f, 100.0f));
	ScaledSourceTransform.SetScale3D(FVector(3.0f, 0.5f, 2.0f));
	AFacilityPlacementAutomationActor* ScaledSource =
		World->SpawnActorDeferred<AFacilityPlacementAutomationActor>(
			AFacilityPlacementAutomationActor::StaticClass(),
			ScaledSourceTransform);
	ScaledSource->ConfigureForTest(*Definition);
	ScaledSource->SetInstanceScaleKeepingUnitFootprint(FVector(3.0f, 0.5f, 2.0f));
	ScaledSource->FinishSpawning(ScaledSourceTransform);
	if (!ScaledSource->HasActorBegunPlay()) ScaledSource->DispatchBeginPlay();
	FailureReason = FText::GetEmpty();
	APlaceableFacilityItemActor* ScaleIsolatedItem =
		FFacilityActorConversionTransaction::RecoverFacilityToItem(*ScaledSource, FailureReason);
	TestTrue(FString::Printf(TEXT("Source instance scale does not leak into the recovery item: %s"),
		*FailureReason.ToString()),
		ScaleIsolatedItem
		&& ScaleIsolatedItem->GetActorScale3D().Equals(
			APlaceableFacilityItemActor::StaticClass()->GetDefaultObject<AActor>()->GetActorScale3D()));
	if (ScaleIsolatedItem) ScaleIsolatedItem->Destroy();

	UFacilityPlacementDefinition* ScaleDefinition = NewObject<UFacilityPlacementDefinition>();
	ConfigureDefinition(
		*ScaleDefinition,
		TEXT("PlacedCDOScaleAutomation"),
		AFacilityPlacementScaleAutomationActor::StaticClass(),
		2,
		2);
	const FVector ScaleActorDefaultScale =
		AFacilityPlacementScaleAutomationActor::StaticClass()->GetDefaultObject<AActor>()
			->GetRootComponent()->GetRelativeScale3D();
	FTransform ScaleSourceTransform(FRotator::ZeroRotator, FVector(14000.0f, 0.0f, 100.0f));
	ScaleSourceTransform.SetScale3D(ScaleActorDefaultScale);
	AFacilityPlacementScaleAutomationActor* ScaleSource =
		World->SpawnActorDeferred<AFacilityPlacementScaleAutomationActor>(
			AFacilityPlacementScaleAutomationActor::StaticClass(),
			ScaleSourceTransform,
			nullptr,
			nullptr,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn,
			ESpawnActorScaleMethod::OverrideRootScale);
	ScaleSource->ConfigureForTest(*ScaleDefinition);
	ScaleSource->FinishSpawning(
		ScaleSourceTransform,
		false,
		nullptr,
		ESpawnActorScaleMethod::OverrideRootScale);
	if (!ScaleSource->HasActorBegunPlay()) ScaleSource->DispatchBeginPlay();
	FailureReason = FText::GetEmpty();
	APlaceableFacilityItemActor* ScaleItem =
		FFacilityActorConversionTransaction::RecoverFacilityToItem(*ScaleSource, FailureReason);
	TestTrue(FString::Printf(TEXT("Non-unit placed CDO fixture recovers to the common item scale: %s"),
		*FailureReason.ToString()),
		ScaleItem && ScaleItem->GetActorScale3D().Equals(FVector::OneVector));
	TestTrue(TEXT("Carry is empty before the non-unit CDO placement fixture"), Carry->IsHandEmpty());
	if (ScaleItem && Carry->TryTakePhysicalObject(ScaleItem, FailureReason))
	{
		const FTransform RequestedPlacementTransform(
			FRotator::ZeroRotator,
			FVector(15000.0f, 0.0f, 100.0f),
			FVector(9.0f));
		FTransform CDOPlacementTransform;
		UFacilityPlacementComponent* ScaleCDOPlacement =
			AFacilityPlacementScaleAutomationActor::StaticClass()
				->GetDefaultObject<AFacilityPlacementScaleAutomationActor>()
				->GetFacilityPlacementComponent();
		TestTrue(TEXT("Non-unit CDO builds the authoritative placement candidate"),
			ScaleCDOPlacement
			&& ScaleCDOPlacement->BuildPlacedActorTransform(
				RequestedPlacementTransform,
				CDOPlacementTransform,
				FailureReason));
		TestTrue(TEXT("CDO-derived candidate uses the Root default scale"),
			CDOPlacementTransform.GetScale3D().Equals(ScaleActorDefaultScale));
		AActor* PlacedScaleActor = FFacilityActorConversionTransaction::PlaceItemAsFacility(
			*ScaleItem,
			CDOPlacementTransform,
			*Zone,
			*Carry,
			FailureReason);
		AFacilityPlacementScaleAutomationActor* TypedPlacedScale =
			Cast<AFacilityPlacementScaleAutomationActor>(PlacedScaleActor);
		const FVector PlacedFootprintExtent = TypedPlacedScale
			? TypedPlacedScale->GetFacilityPlacementComponent()->GetPlacementFootprint()->GetScaledBoxExtent()
			: FVector::ZeroVector;
		TestNotNull(FString::Printf(TEXT("Non-unit target placement succeeds: %s"),
			*FailureReason.ToString()), TypedPlacedScale);
		TestTrue(FString::Printf(TEXT("Placement uses target CDO scale; actual=%s expected=%s"),
			*(TypedPlacedScale ? TypedPlacedScale->GetActorScale3D().ToString() : FString(TEXT("invalid"))),
			*ScaleActorDefaultScale.ToString()),
			TypedPlacedScale && TypedPlacedScale->GetActorScale3D().Equals(ScaleActorDefaultScale));
		TestTrue(TEXT("CDO-derived candidate matches the deferred-spawn Actor transform"),
			TypedPlacedScale
			&& TypedPlacedScale->GetActorTransform().Equals(CDOPlacementTransform));
		const UBoxComponent* PlacedFootprint = TypedPlacedScale
			? TypedPlacedScale->GetFacilityPlacementComponent()->GetPlacementFootprint()
			: nullptr;
		const FVector PlacedBottomCenter = PlacedFootprint
			? PlacedFootprint->GetComponentTransform().TransformPosition(
				FVector(0.0f, 0.0f, -PlacedFootprint->GetUnscaledBoxExtent().Z))
			: FVector::ZeroVector;
		TestTrue(FString::Printf(
			TEXT("Deferred-spawn footprint keeps the final candidate floor height; actual=%.3f expected=%.3f"),
			PlacedBottomCenter.Z,
			RequestedPlacementTransform.GetLocation().Z),
			PlacedFootprint
			&& FMath::IsNearlyEqual(
				PlacedBottomCenter.Z,
				RequestedPlacementTransform.GetLocation().Z,
				0.1f));
		TestTrue(FString::Printf(TEXT("Placement uses target CDO footprint; actual=%s"),
			*PlacedFootprintExtent.ToString()),
			TypedPlacedScale
			&& FMath::IsNearlyEqual(PlacedFootprintExtent.X, 10.0f)
			&& FMath::IsNearlyEqual(PlacedFootprintExtent.Y, 10.0f));
		if (TypedPlacedScale) TypedPlacedScale->Destroy();
	}
	else
	{
		AddError(TEXT("Non-unit placed CDO fixture could not hold its recovered item."));
	}

	UBathhouseExpansionDefinition* Expansion = NewObject<UBathhouseExpansionDefinition>();
	Expansion->Tiers = { { 4, 4 } };
	AFacilityPlacementExpansionAutomationAuthority* Authority =
		World->SpawnActorDeferred<AFacilityPlacementExpansionAutomationAuthority>(
			AFacilityPlacementExpansionAutomationAuthority::StaticClass(),
			FTransform(FVector(16000.0f, 0.0f, 0.0f)));
	Authority->ConfigureForTest(*Expansion);
	Authority->FinishSpawning(FTransform(FVector(16000.0f, 0.0f, 0.0f)));
	if (!Authority->HasActorBegunPlay()) Authority->DispatchBeginPlay();

	UFacilityPlacementDefinition* LockerDefinition = NewObject<UFacilityPlacementDefinition>();
	ConfigureDefinition(
		*LockerDefinition,
		TEXT("CallbackDestructionLocker"),
		AFacilityPlacementLockerAutomationActor::StaticClass(),
		1,
		1,
		1);
	AFacilityPlacementLockerAutomationActor* LockerSource =
		World->SpawnActorDeferred<AFacilityPlacementLockerAutomationActor>(
			AFacilityPlacementLockerAutomationActor::StaticClass(),
			FTransform(FVector(17000.0f, 0.0f, 100.0f)));
	LockerSource->ConfigureForTest(*LockerDefinition, EBathhouseFacilityType::ClothesLocker);
	LockerSource->FinishSpawning(FTransform(FVector(17000.0f, 0.0f, 100.0f)));
	if (!LockerSource->HasActorBegunPlay()) LockerSource->DispatchBeginPlay();
	ULockerCapacitySubsystem* Lockers = World->GetSubsystem<ULockerCapacitySubsystem>();
	APlaceableFacilityItemActor* LockerItem =
		FFacilityActorConversionTransaction::RecoverFacilityToItem(*LockerSource, FailureReason);
	TestTrue(TEXT("Callback-destruction fixture recovers one locker item"),
		LockerItem && Lockers->GetInstalledLockerCapacity() == 0);
	if (LockerItem && Carry->TryTakePhysicalObject(LockerItem, FailureReason))
	{
		UFacilityPlacementEventAutomationProbe* EventProbe =
			NewObject<UFacilityPlacementEventAutomationProbe>();
		EventProbe->Bind(Carry, Lockers);
		EventProbe->ResetCounts();
		int32 LockerFacilityEvents = 0;
		bool bDestroyedPlacementTarget = false;
		TWeakObjectPtr<ABathhouseFacilityActor> DestroyedFromPlacementCallback;
		const FDelegateHandle DestroyHandle = Facilities->OnFacilityAvailabilityChanged.AddLambda(
			[&](const EBathhouseFacilityType Type)
			{
				if (Type != EBathhouseFacilityType::ClothesLocker)
				{
					return;
				}
				++LockerFacilityEvents;
				for (TActorIterator<AFacilityPlacementLockerAutomationActor> It(World); It; ++It)
				{
					ABathhouseFacilityActor* Candidate = *It;
					if (Candidate->GetFacilityPlacementComponent()->GetDefinition() == LockerDefinition)
					{
						DestroyedFromPlacementCallback = Candidate;
						bDestroyedPlacementTarget = Candidate->Destroy();
						break;
					}
				}
			});
		const int64 CapacityRevisionBefore = Lockers->GetRevision();
		AActor* PlacementResult = FFacilityActorConversionTransaction::PlaceItemAsFacility(
			*LockerItem,
			FTransform(FRotator::ZeroRotator, FVector(18000.0f, 0.0f, 100.0f)),
			*Zone,
			*Carry,
			FailureReason);
		World->Tick(LEVELTICK_All, 0.0f);
		Facilities->OnFacilityAvailabilityChanged.Remove(DestroyHandle);
		TestTrue(TEXT("Facility availability callback destroys the exact placed locker"),
			bDestroyedPlacementTarget);
		TestTrue(TEXT("Synchronous placement observer destruction remains a committed external deletion"),
			PlacementResult != nullptr && !DestroyedFromPlacementCallback.IsValid() && Carry->IsHandEmpty());
		TestEqual(TEXT("Placement followed by callback destruction publishes facility add/remove only"),
			LockerFacilityEvents, 2);
		TestEqual(TEXT("Callback destruction emits one final locker-capacity mutation without a stale third publish"),
			EventProbe->CapacityChangeCount, 1);
		TestEqual(TEXT("Callback destruction advances locker capacity revision exactly once"),
			Lockers->GetRevision(), CapacityRevisionBefore + 1);
		TestEqual(TEXT("Placement consumption publishes held empty exactly once"),
			EventProbe->HeldChangeCount, 1);
		TestEqual(TEXT("Callback-destroyed locker leaves no installed capacity"),
			Lockers->GetInstalledLockerCapacity(), 0);
		EventProbe->Unbind();
	}
	else
	{
		AddError(TEXT("Callback-destruction fixture could not hold its locker item."));
	}

	FFacilityActorConversionTransaction::ClearTestFault();
	World->DestroyWorld(false);
	GEngine->DestroyWorldContext(World);
	World->RemoveFromRoot();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBathhouseFacilityStartupReconciliationTest,
	"BathhouseSim.Placement.StartupLockerReconciliation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBathhouseFacilityStartupReconciliationTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	AddExpectedError(
		TEXT("was rejected permanently"),
		EAutomationExpectedErrorFlags::Contains,
		4);
	const FGameplayTag PlacementTag = FGameplayTag::RequestGameplayTag(TEXT("Facility.Placeable"));
	auto MakeDefinition = [&](const TCHAR* Name, const int32 Slots)
	{
		UFacilityPlacementDefinition* Definition = NewObject<UFacilityPlacementDefinition>();
		Definition->StableId = Name;
		Definition->FacilityTags.AddTag(PlacementTag);
		Definition->PlacedFacilityClass = AFacilityPlacementLockerAutomationActor::StaticClass();
		Definition->RecoveryItemClass = APlaceableFacilityItemActor::StaticClass();
		Definition->LockerSlotCount = Slots;
		return Definition;
	};
	auto CreateWorld = [&](const TCHAR* BaseName, FWorldContext*& OutContext)
	{
		const FName WorldName = MakeUniqueObjectName(nullptr, UWorld::StaticClass(), BaseName);
		OutContext = &GEngine->CreateNewWorldContext(EWorldType::Game);
		UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, WorldName, GetTransientPackage());
		World->AddToRoot();
		OutContext->SetCurrentWorld(World);
		return World;
	};
	auto SpawnStartupLocker = [&](UWorld& World, UFacilityPlacementDefinition& Definition,
		const FGuid& Id, const int32 Slots, const FVector& Location)
	{
		AFacilityPlacementLockerAutomationActor* Locker =
			World.SpawnActorDeferred<AFacilityPlacementLockerAutomationActor>(
				AFacilityPlacementLockerAutomationActor::StaticClass(), FTransform(Location));
		Locker->ConfigureStartupForTest(Definition, Id, Slots);
		Locker->FinishSpawning(FTransform(Location));
		return Locker;
	};
	auto SpawnAuthority = [&](UWorld& World, UBathhouseExpansionDefinition& Expansion)
	{
		AFacilityPlacementExpansionAutomationAuthority* Authority =
			World.SpawnActorDeferred<AFacilityPlacementExpansionAutomationAuthority>(
				AFacilityPlacementExpansionAutomationAuthority::StaticClass(), FTransform::Identity);
		Authority->ConfigureForTest(Expansion);
		Authority->FinishSpawning(FTransform::Identity);
		return Authority;
	};
	auto CleanupWorld = [](UWorld* World, FWorldContext* Context)
	{
		World->DestroyWorld(false);
		GEngine->DestroyWorldContext(World);
		World->RemoveFromRoot();
		(void)Context;
	};

	FWorldContext* Context = nullptr;
	UWorld* World = CreateWorld(TEXT("StartupLockerOrderingWorld"), Context);
	UBathhouseExpansionDefinition* Expansion = NewObject<UBathhouseExpansionDefinition>();
	Expansion->Tiers = { { 5, 5 } };
	AFacilityPlacementExpansionAutomationAuthority* StartupAuthority = SpawnAuthority(*World, *Expansion);
	UFacilityPlacementDefinition* FourSlotDefinition = MakeDefinition(TEXT("StartupFour"), 4);
	UFacilityPlacementDefinition* EightSlotDefinition = MakeDefinition(TEXT("StartupEight"), 8);
	UFacilityPlacementDefinition* OneSlotDefinition = MakeDefinition(TEXT("StartupOne"), 1);
	#if WITH_EDITOR
	AFacilityPlacementLockerAutomationActor* DuplicateModeFixture =
		NewObject<AFacilityPlacementLockerAutomationActor>();
	const FGuid SerializedId(0, 0, 9, 1);
	DuplicateModeFixture->ConfigureStartupForTest(*OneSlotDefinition, SerializedId, 1);
	DuplicateModeFixture->PostDuplicate(EDuplicateMode::PIE);
	TestEqual(TEXT("PIE duplication preserves the serialized locker RegistrationId"),
		DuplicateModeFixture->GetRegistrationId(), SerializedId);
	DuplicateModeFixture->PostDuplicate(EDuplicateMode::Normal);
	const FGuid NormalDuplicateId = DuplicateModeFixture->GetRegistrationId();
	TestTrue(TEXT("Normal Editor duplication generates a distinct locker RegistrationId"),
		NormalDuplicateId.IsValid() && NormalDuplicateId != SerializedId);
	DuplicateModeFixture->PostEditImport();
	TestTrue(TEXT("Editor import generates another distinct locker RegistrationId"),
		DuplicateModeFixture->GetRegistrationId().IsValid()
		&& DuplicateModeFixture->GetRegistrationId() != NormalDuplicateId);
	#endif
	AFacilityPlacementLockerAutomationActor* FourSlot = SpawnStartupLocker(
		*World, *FourSlotDefinition, FGuid(0, 0, 0, 1), 4, FVector(100.0f, 0.0f, 0.0f));
	AFacilityPlacementLockerAutomationActor* EightSlot = SpawnStartupLocker(
		*World, *EightSlotDefinition, FGuid(0, 0, 0, 2), 8, FVector(200.0f, 0.0f, 0.0f));
	AFacilityPlacementLockerAutomationActor* OneSlot = SpawnStartupLocker(
		*World, *OneSlotDefinition, FGuid(0, 0, 0, 3), 1, FVector(300.0f, 0.0f, 0.0f));
	UFacilityPlacementDefinition* DuplicateDefinitionA = MakeDefinition(TEXT("StartupDuplicateA"), 1);
	UFacilityPlacementDefinition* DuplicateDefinitionB = MakeDefinition(TEXT("StartupDuplicateB"), 1);
	AFacilityPlacementLockerAutomationActor* DuplicateA = SpawnStartupLocker(
		*World, *DuplicateDefinitionA, FGuid(0, 0, 0, 4), 1, FVector(400.0f, 0.0f, 0.0f));
	AFacilityPlacementLockerAutomationActor* DuplicateB = SpawnStartupLocker(
		*World, *DuplicateDefinitionB, FGuid(0, 0, 0, 4), 1, FVector(500.0f, 0.0f, 0.0f));
	ULockerCapacitySubsystem* Lockers = World->GetSubsystem<ULockerCapacitySubsystem>();
	UBathhouseFacilitySubsystem* Facilities = World->GetSubsystem<UBathhouseFacilitySubsystem>();
	UFacilityPlacementEventAutomationProbe* Probe = NewObject<UFacilityPlacementEventAutomationProbe>();
	Probe->Bind(nullptr, Lockers);
	int32 FacilityPublications = 0;
	const FDelegateHandle FacilityHandle = Facilities->OnFacilityAvailabilityChanged.AddLambda(
		[&](const EBathhouseFacilityType Type)
		{
			FacilityPublications += Type == EBathhouseFacilityType::ClothesLocker ? 1 : 0;
		});
	Facilities->SubmitStartupLocker(FourSlot);
	Facilities->SubmitStartupLocker(FourSlot);
	World->InitializeActorsForPlay(FURL());
	for (AActor* StartupActor : { static_cast<AActor*>(StartupAuthority), static_cast<AActor*>(FourSlot),
		static_cast<AActor*>(EightSlot), static_cast<AActor*>(OneSlot),
		static_cast<AActor*>(DuplicateA), static_cast<AActor*>(DuplicateB) })
	{
		if (StartupActor && !StartupActor->HasActorBegunPlay())
		{
			StartupActor->DispatchBeginPlay();
		}
	}
	World->BeginPlay();
	TestEqual(TEXT("ID-sorted 4/8/1 banks skip the oversized bank and accept fitting banks"),
		Lockers->GetInstalledLockerCapacity(), 5);
	TestTrue(TEXT("Accepted startup lockers restore authored collision and activate their domain"),
		FourSlot->GetActorEnableCollision() && OneSlot->GetActorEnableCollision()
		&& FourSlot->GetFacilityPlacementComponent()->IsPlacedDomainActive()
		&& OneSlot->GetFacilityPlacementComponent()->IsPlacedDomainActive());
	TestTrue(TEXT("Oversized and duplicate-ID startup lockers remain fail-closed"),
		!EightSlot->GetActorEnableCollision() && !DuplicateA->GetActorEnableCollision()
		&& !DuplicateB->GetActorEnableCollision()
		&& !EightSlot->GetFacilityPlacementComponent()->IsPlacedDomainActive());
	TestEqual(TEXT("Startup reconciliation publishes facility availability once"), FacilityPublications, 1);
	TestEqual(TEXT("Startup reconciliation publishes capacity once"), Probe->CapacityChangeCount, 1);
	Facilities->SubmitStartupLocker(FourSlot);
	TestEqual(TEXT("Accepted startup locker resubmission is a complete capacity no-op"),
		Lockers->GetInstalledLockerCapacity(), 5);
	TestEqual(TEXT("Accepted startup locker resubmission does not publish a second batch"),
		FacilityPublications, 1);
	AFacilityPlacementLockerAutomationActor* AcceptedIdDuplicate = SpawnStartupLocker(
		*World, *OneSlotDefinition, FourSlot->GetRegistrationId(), 1, FVector(600.0f, 0.0f, 0.0f));
	if (!AcceptedIdDuplicate->HasActorBegunPlay())
	{
		AcceptedIdDuplicate->DispatchBeginPlay();
	}
	TestFalse(TEXT("A different startup locker cannot reuse an accepted RegistrationId"),
		AcceptedIdDuplicate->GetActorEnableCollision()
		|| AcceptedIdDuplicate->GetFacilityPlacementComponent()->IsPlacedDomainActive());
	TestEqual(TEXT("Accepted-ID duplicate leaves installed capacity unchanged"),
		Lockers->GetInstalledLockerCapacity(), 5);
	Facilities->OnFacilityAvailabilityChanged.Remove(FacilityHandle);
	Probe->Unbind();
	CleanupWorld(World, Context);

	FWorldContext* LateContext = nullptr;
	UWorld* LateWorld = CreateWorld(TEXT("StartupLockerLateAuthorityWorld"), LateContext);
	UFacilityPlacementDefinition* LateDefinition = MakeDefinition(TEXT("StartupLateAuthority"), 1);
	AFacilityPlacementLockerAutomationActor* LateLocker = SpawnStartupLocker(
		*LateWorld, *LateDefinition, FGuid(0, 0, 1, 1), 1, FVector::ZeroVector);
	ULockerCapacitySubsystem* LateLockers = LateWorld->GetSubsystem<ULockerCapacitySubsystem>();
	LateWorld->InitializeActorsForPlay(FURL());
	if (!LateLocker->HasActorBegunPlay())
	{
		LateLocker->DispatchBeginPlay();
	}
	LateWorld->BeginPlay();
	TestEqual(TEXT("Authority-not-ready keeps the startup locker pending without capacity"),
		LateLockers->GetInstalledLockerCapacity(), 0);
	TestFalse(TEXT("Pending startup locker keeps collision disabled"), LateLocker->GetActorEnableCollision());
	UBathhouseExpansionDefinition* LateExpansion = NewObject<UBathhouseExpansionDefinition>();
	LateExpansion->Tiers = { { 1, 1 } };
	AFacilityPlacementExpansionAutomationAuthority* LateAuthority = SpawnAuthority(*LateWorld, *LateExpansion);
	if (!LateAuthority->HasActorBegunPlay())
	{
		LateAuthority->DispatchBeginPlay();
	}
	TestEqual(TEXT("Late authority reconciles the pending locker exactly once"),
		LateLockers->GetInstalledLockerCapacity(), 1);
	TestTrue(TEXT("Late-authority success restores collision and placed domain"),
		LateLocker->GetActorEnableCollision()
		&& LateLocker->GetFacilityPlacementComponent()->IsPlacedDomainActive());
	FText DuplicateFailure;
	TestEqual(TEXT("Typed registration reports AlreadyRegistered without double counting"),
		LateLockers->RegisterLockerBankTyped(
			LateLocker,
			TArray<ULockerActionSlotComponent*>(),
			1,
			DuplicateFailure,
			false),
		ELockerBankRegistrationResult::AlreadyRegistered);
	TestEqual(TEXT("AlreadyRegistered leaves capacity unchanged"), LateLockers->GetInstalledLockerCapacity(), 1);
	CleanupWorld(LateWorld, LateContext);

	FWorldContext* ReentrantContext = nullptr;
	UWorld* ReentrantWorld = CreateWorld(TEXT("StartupLockerReentrantWorld"), ReentrantContext);
	UBathhouseExpansionDefinition* ReentrantExpansion = NewObject<UBathhouseExpansionDefinition>();
	ReentrantExpansion->Tiers = { { 3, 3 } };
	AFacilityPlacementExpansionAutomationAuthority* ReentrantAuthority =
		SpawnAuthority(*ReentrantWorld, *ReentrantExpansion);
	UFacilityPlacementDefinition* ReentrantDefinition = MakeDefinition(TEXT("StartupReentrant"), 1);
	AFacilityPlacementLockerAutomationActor* FirstReentrantLocker = SpawnStartupLocker(
		*ReentrantWorld, *ReentrantDefinition, FGuid(0, 0, 2, 1), 1, FVector::ZeroVector);
	UBathhouseFacilitySubsystem* ReentrantFacilities =
		ReentrantWorld->GetSubsystem<UBathhouseFacilitySubsystem>();
	ULockerCapacitySubsystem* ReentrantLockers =
		ReentrantWorld->GetSubsystem<ULockerCapacitySubsystem>();
	UFacilityPlacementEventAutomationProbe* ReentrantProbe =
		NewObject<UFacilityPlacementEventAutomationProbe>();
	ReentrantProbe->Bind(nullptr, ReentrantLockers);
	int32 ReentrantFacilityPublications = 0;
	AFacilityPlacementLockerAutomationActor* CallbackLocker = nullptr;
	const FDelegateHandle ReentrantHandle =
		ReentrantFacilities->OnFacilityAvailabilityChanged.AddLambda(
			[&](const EBathhouseFacilityType Type)
			{
				if (Type != EBathhouseFacilityType::ClothesLocker)
				{
					return;
				}
				++ReentrantFacilityPublications;
				if (ReentrantFacilityPublications != 1)
				{
					return;
				}
				ReentrantFacilities->SubmitStartupLocker(FirstReentrantLocker);
				CallbackLocker = SpawnStartupLocker(
					*ReentrantWorld,
					*ReentrantDefinition,
					FGuid(0, 0, 2, 2),
					1,
					FVector(100.0f, 0.0f, 0.0f));
				if (!CallbackLocker->HasActorBegunPlay())
				{
					CallbackLocker->DispatchBeginPlay();
				}
				ReentrantFacilities->UnregisterExpansionAuthority(ReentrantAuthority);
				FText AuthorityFailure;
				TestTrue(TEXT("Authority can be restored during startup publication"),
					ReentrantFacilities->RegisterExpansionAuthority(
						ReentrantAuthority, AuthorityFailure));
			});
	ReentrantWorld->InitializeActorsForPlay(FURL());
	for (AActor* StartupActor : {
		static_cast<AActor*>(ReentrantAuthority),
		static_cast<AActor*>(FirstReentrantLocker) })
	{
		if (StartupActor && !StartupActor->HasActorBegunPlay())
		{
			StartupActor->DispatchBeginPlay();
		}
	}
	ReentrantWorld->BeginPlay();
	TestEqual(TEXT("Publication callback submission is reconciled by an immediate tail pass"),
		ReentrantLockers->GetInstalledLockerCapacity(), 2);
	TestTrue(TEXT("Callback-submitted startup locker reaches its final active collision state"),
		CallbackLocker && CallbackLocker->GetActorEnableCollision()
		&& CallbackLocker->GetFacilityPlacementComponent()->IsPlacedDomainActive());
	TestEqual(TEXT("Reentrant startup work publishes exactly one event per accepted batch"),
		ReentrantFacilityPublications, 2);
	TestEqual(TEXT("Reentrant startup capacity publishes exactly once per accepted batch"),
		ReentrantProbe->CapacityChangeCount, 2);
	ReentrantFacilities->OnFacilityAvailabilityChanged.Remove(ReentrantHandle);
	ReentrantProbe->Unbind();
	CleanupWorld(ReentrantWorld, ReentrantContext);
	return true;
}

#endif
