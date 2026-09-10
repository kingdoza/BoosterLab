#include "Placement/FacilityActorConversionTransaction.h"

#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "Interaction/PlayerCarryComponent.h"
#include "Placement/FacilityPlacementCollisionUtils.h"
#include "Placement/FacilityPlacementComponent.h"
#include "Placement/FacilityPlacementDefinition.h"
#include "Placement/FacilityPlacementZoneActor.h"
#include "Placement/PlaceableFacility.h"
#include "Placement/PlaceableFacilityItemActor.h"

#define LOCTEXT_NAMESPACE "FacilityActorConversionTransaction"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
	FFacilityActorConversionTransaction::ETestFault GFacilityConversionTestFault =
		FFacilityActorConversionTransaction::ETestFault::None;
}

void FFacilityActorConversionTransaction::SetTestFault(const ETestFault Fault)
{
	GFacilityConversionTestFault = Fault;
}

void FFacilityActorConversionTransaction::ClearTestFault()
{
	GFacilityConversionTestFault = ETestFault::None;
}

bool FFacilityActorConversionTransaction::ConsumeTestFault(const ETestFault Fault)
{
	if (GFacilityConversionTestFault != Fault)
	{
		return false;
	}
	GFacilityConversionTestFault = ETestFault::None;
	return true;
}
#endif

bool FFacilityActorConversionTransaction::ValidateRecoveryCandidate(
	AActor& FacilityActor,
	FTransform& OutItemTransform,
	FText& OutFailureReason)
{
	IPlaceableFacility* Facility = Cast<IPlaceableFacility>(&FacilityActor);
	UFacilityPlacementComponent* Placement = Facility
		? Facility->GetFacilityPlacementComponent()
		: nullptr;
	UFacilityPlacementDefinition* Definition = Placement ? Placement->GetDefinition() : nullptr;
	UWorld* World = FacilityActor.GetWorld();
	if (!Facility || !Facility->SupportsFacilityActorConversion()
		|| !Placement || !Definition || !World
		|| Placement->IsStagedPlacement()
		|| !Definition->ValidateRuntime(OutFailureReason)
		|| !Placement->GetRecoveryDropTransform(OutItemTransform, OutFailureReason))
	{
		if (OutFailureReason.IsEmpty())
		{
			OutFailureReason = LOCTEXT("InvalidRecoverySource", "회수할 설비의 변환 설정이 올바르지 않습니다.");
		}
		return false;
	}

	const APlaceableFacilityItemActor* ItemCDO = Definition->RecoveryItemClass
		? Definition->RecoveryItemClass->GetDefaultObject<APlaceableFacilityItemActor>()
		: nullptr;
	if (!ItemCDO || !ItemCDO->GetItemRoot())
	{
		OutFailureReason = LOCTEXT("MissingRecoveryItemCDO", "회수 아이템 기본 설정을 찾을 수 없습니다.");
		return false;
	}
	OutItemTransform.SetScale3D(ItemCDO->GetActorScale3D());

	FVector QueryLocation;
	FQuat QueryRotation;
	FCollisionShape QueryShape;
	const UPrimitiveComponent* CollisionTemplate = nullptr;
	if (!APlaceableFacilityItemActor::BuildDefinitionCollisionQuery(
		*Definition,
		OutItemTransform,
		QueryLocation,
		QueryRotation,
		QueryShape,
		CollisionTemplate,
		OutFailureReason))
	{
		return false;
	}
	FCollisionQueryParams Params(SCENE_QUERY_STAT(FacilityRecoveryItemPreviewOverlap), false, &FacilityActor);
	if (FacilityPlacementCollision::HasBlockingOverlap(
		*World,
		QueryLocation,
		QueryRotation,
		QueryShape,
		*CollisionTemplate,
		Params))
	{
		OutFailureReason = LOCTEXT("RecoveryItemBlocked", "설비 회수 아이템을 생성할 공간이 막혀 있습니다.");
		return false;
	}
	return true;
}

APlaceableFacilityItemActor* FFacilityActorConversionTransaction::RecoverFacilityToItem(
	AActor& FacilityActor,
	FText& OutFailureReason)
{
	IPlaceableFacility* Facility = Cast<IPlaceableFacility>(&FacilityActor);
	UFacilityPlacementComponent* Placement = Facility
		? Facility->GetFacilityPlacementComponent()
		: nullptr;
	FTransform ItemTransform;
	const FFacilityPlacementTransactionResult RecoveryQuery = Facility
		? Facility->QueryFacilityRecovery()
		: FFacilityPlacementTransactionResult::Failed(EFacilityPlacementFailureCode::InvalidActor, FText::GetEmpty());
	if (!Facility || !Facility->SupportsFacilityActorConversion()
		|| !Placement || !RecoveryQuery.bSucceeded
		|| !ValidateRecoveryCandidate(FacilityActor, ItemTransform, OutFailureReason)
		|| !Placement->BeginTransition(OutFailureReason))
	{
		if (OutFailureReason.IsEmpty())
		{
			OutFailureReason = RecoveryQuery.FailureReason.IsEmpty()
				? LOCTEXT("RecoveryRevalidationFailed", "설비 회수 조건이 변경되었습니다.")
				: RecoveryQuery.FailureReason;
		}
		return nullptr;
	}

	UFacilityPlacementDefinition* Definition = Placement->GetDefinition();
	UWorld* World = FacilityActor.GetWorld();
#if WITH_DEV_AUTOMATION_TESTS
	if (ConsumeTestFault(ETestFault::RecoverySpawn))
	{
		Placement->EndTransition();
		OutFailureReason = LOCTEXT("RecoveryItemSpawnInjectedFailure", "설비 회수 아이템 생성 실패가 주입되었습니다.");
		return nullptr;
	}
#endif
	APlaceableFacilityItemActor* Item = World->SpawnActorDeferred<APlaceableFacilityItemActor>(
		Definition->RecoveryItemClass,
		ItemTransform,
		nullptr,
		nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Item)
	{
		Placement->EndTransition();
		OutFailureReason = LOCTEXT("RecoveryItemSpawnFailed", "설비 회수 아이템 생성을 시작할 수 없습니다.");
		return nullptr;
	}

	auto Fail = [&]() -> APlaceableFacilityItemActor*
	{
		if (IsValid(Item))
		{
			Item->Destroy();
		}
		if (IsValid(&FacilityActor))
		{
			Placement->EndTransition();
		}
		return nullptr;
	};

	FFacilityPlacementPayload Payload;
	if (
#if WITH_DEV_AUTOMATION_TESTS
		ConsumeTestFault(ETestFault::RecoveryPayload) ||
#endif
		!Item->InitializeStaged(*Definition, OutFailureReason)
		|| !Facility->ExportPlacementPayload(*Item, Payload, OutFailureReason)
		|| !Item->SetPlacementPayload(Payload, OutFailureReason))
	{
		return Fail();
	}
	Item->FinishSpawning(ItemTransform);
	if (!IsValid(Item) || !Item->ValidatePlacementPayload(OutFailureReason))
	{
		return Fail();
	}

	UPrimitiveComponent* ItemRoot = Item->GetItemRoot();
	FVector CommitQueryLocation;
	FQuat CommitQueryRotation;
	FCollisionShape CommitQueryShape;
	const UPrimitiveComponent* CommitCollisionTemplate = nullptr;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(FacilityRecoveryItemCommitOverlap), false, &FacilityActor);
	Params.AddIgnoredActor(Item);
	if (!ItemRoot || !APlaceableFacilityItemActor::BuildDefinitionCollisionQuery(
		*Definition,
		Item->GetActorTransform(),
		CommitQueryLocation,
		CommitQueryRotation,
		CommitQueryShape,
		CommitCollisionTemplate,
		OutFailureReason)
#if WITH_DEV_AUTOMATION_TESTS
		|| ConsumeTestFault(ETestFault::RecoveryCommitCollision)
#endif
		|| FacilityPlacementCollision::HasBlockingOverlap(
		*World,
		CommitQueryLocation,
		CommitQueryRotation,
		CommitQueryShape,
		*CommitCollisionTemplate,
		Params))
	{
		OutFailureReason = LOCTEXT("RecoveryItemCommitBlocked", "회수 아이템의 실제 충돌 공간이 막혀 있습니다.");
		return Fail();
	}

	FFacilityPlacementPublication Publication;
	if (
#if WITH_DEV_AUTOMATION_TESTS
		ConsumeTestFault(ETestFault::RecoveryDomainUnregistration) ||
#endif
		!Facility->StagePlacedDomainUnregistration(Publication, OutFailureReason))
	{
		return Fail();
	}
	bool bSourceDomainStaged = true;
	auto RollbackSource = [&]()
	{
		if (bSourceDomainStaged && IsValid(&FacilityActor))
		{
			FText RollbackFailure;
			ensureMsgf(
				Facility->RollbackPlacedDomainUnregistration(RollbackFailure),
				TEXT("Failed to restore placed facility domain after recovery conversion failure: %s"),
				*RollbackFailure.ToString());
			Placement->EndTransition();
			bSourceDomainStaged = false;
		}
	};

	if (
#if WITH_DEV_AUTOMATION_TESTS
		ConsumeTestFault(ETestFault::RecoveryActivation) ||
#endif
		!Item->ActivateFreeWorld(ItemTransform, OutFailureReason))
	{
		RollbackSource();
		return Fail();
	}
	if (
#if WITH_DEV_AUTOMATION_TESTS
		ConsumeTestFault(ETestFault::RecoverySourceDestroy) ||
#endif
		!FacilityActor.Destroy())
	{
		OutFailureReason = LOCTEXT("RecoverySourceDestroyFailed", "원본 설비를 제거할 수 없습니다.");
		RollbackSource();
		return Fail();
	}
	bSourceDomainStaged = false;
	Publication.Publish();
	// Source Destroy callbacks may independently remove the already-committed item.
	// That is a post-commit external deletion, not a reason to report that the
	// source-to-item transaction rolled back.
	return Item;
}

AActor* FFacilityActorConversionTransaction::PlaceItemAsFacility(
	APlaceableFacilityItemActor& ItemActor,
	const FTransform& CandidateTransform,
	const AFacilityPlacementZoneActor& Zone,
	UPlayerCarryComponent& Carry,
	FText& OutFailureReason)
{
	UFacilityPlacementDefinition* Definition = ItemActor.GetDefinition();
	UWorld* World = ItemActor.GetWorld();
	if (!Definition || !World || !ItemActor.IsHeldForPlacement()
		|| Carry.GetHeldObject() != &ItemActor
		|| !ItemActor.ValidatePlacementPayload(OutFailureReason))
	{
		if (OutFailureReason.IsEmpty())
		{
			OutFailureReason = LOCTEXT("InvalidPlacementItem", "배치할 설비 아이템 상태가 올바르지 않습니다.");
		}
		return nullptr;
	}

	AActor* PlacedCDO = Definition->PlacedFacilityClass
		? Definition->PlacedFacilityClass->GetDefaultObject<AActor>()
		: nullptr;
	IPlaceableFacility* PlacedCDOFacility = Cast<IPlaceableFacility>(PlacedCDO);
	if (!PlacedCDO || !PlacedCDOFacility
		|| !PlacedCDOFacility->SupportsFacilityActorConversion())
	{
		OutFailureReason = LOCTEXT("MissingPlacedFacilityCDO", "배치 설비 기본 설정을 찾을 수 없습니다.");
		return nullptr;
	}
	UFacilityPlacementComponent* PlacedCDOPlacement =
		PlacedCDOFacility->GetFacilityPlacementComponent();
	FTransform SpawnTransform;
	if (!PlacedCDOPlacement
		|| !PlacedCDOPlacement->BuildPlacedActorTransform(
			CandidateTransform,
			SpawnTransform,
			OutFailureReason))
	{
		return nullptr;
	}
#if WITH_DEV_AUTOMATION_TESTS
	if (ConsumeTestFault(ETestFault::PlacementSpawn))
	{
		OutFailureReason = LOCTEXT("PlacedFacilitySpawnInjectedFailure", "배치 설비 생성 실패가 주입되었습니다.");
		return nullptr;
	}
#endif
	AActor* NewFacilityActor = World->SpawnActorDeferred<AActor>(
		Definition->PlacedFacilityClass,
		SpawnTransform,
		nullptr,
		nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn,
		ESpawnActorScaleMethod::OverrideRootScale);
	IPlaceableFacility* NewFacility = Cast<IPlaceableFacility>(NewFacilityActor);
	UFacilityPlacementComponent* NewPlacement = NewFacility
		? NewFacility->GetFacilityPlacementComponent()
		: nullptr;
	if (!NewFacilityActor || !NewFacility || !NewPlacement
		|| !NewFacility->SupportsFacilityActorConversion())
	{
		if (IsValid(NewFacilityActor))
		{
			NewFacilityActor->Destroy();
		}
		OutFailureReason = LOCTEXT("PlacedFacilitySpawnFailed", "배치 설비 생성을 시작할 수 없습니다.");
		return nullptr;
	}

	auto DestroyStaged = [&]()
	{
		if (IsValid(NewFacilityActor))
		{
			NewFacilityActor->Destroy();
		}
	};
	NewPlacement->PrepareForStagedPlacement(*Definition);
	if (
#if WITH_DEV_AUTOMATION_TESTS
		ConsumeTestFault(ETestFault::PlacementImport) ||
#endif
		!NewFacility->ImportPlacementPayload(ItemActor, ItemActor.GetPlacementPayload(), OutFailureReason))
	{
		DestroyStaged();
		return nullptr;
	}
	NewFacilityActor->FinishSpawning(
		SpawnTransform,
		false,
		nullptr,
		ESpawnActorScaleMethod::OverrideRootScale);
	const FFacilityPlacementTransactionResult PlacementQuery = IsValid(NewFacilityActor)
		? NewFacility->QueryFacilityPlacement(SpawnTransform, Zone)
		: FFacilityPlacementTransactionResult::Failed(EFacilityPlacementFailureCode::InvalidActor, FText::GetEmpty());
	if (!IsValid(NewFacilityActor) || NewFacilityActor->GetClass() != Definition->PlacedFacilityClass
		|| !NewPlacement->IsStagedPlacement() || !PlacementQuery.bSucceeded
		|| !NewPlacement->BeginTransition(OutFailureReason))
	{
		if (OutFailureReason.IsEmpty())
		{
			OutFailureReason = PlacementQuery.FailureReason.IsEmpty()
				? LOCTEXT("PlacedFacilityValidationFailed", "새 배치 설비의 구성 또는 domain 조건이 올바르지 않습니다.")
				: PlacementQuery.FailureReason;
		}
		DestroyStaged();
		return nullptr;
	}
	if (
#if WITH_DEV_AUTOMATION_TESTS
		ConsumeTestFault(ETestFault::PlacementDomainRegistration) ||
#endif
		!NewFacility->StagePlacedDomainRegistration(OutFailureReason))
	{
		NewPlacement->EndTransition();
		DestroyStaged();
		return nullptr;
	}

	if (!ItemActor.BeginPlacementConsumption(Carry, OutFailureReason))
	{
		NewFacility->RollbackPlacedDomainRegistration();
		NewPlacement->EndTransition();
		DestroyStaged();
		return nullptr;
	}
	const bool bCarryCommitted = Carry.CommitReleasePhysicalObjectForPlacement(
		&ItemActor,
		[&ItemActor]()
		{
#if WITH_DEV_AUTOMATION_TESTS
			if (ConsumeTestFault(ETestFault::PlacementCarryCommit))
			{
				return false;
			}
#endif
			return ItemActor.DestroyForPlacementConsumption();
		});
	if (!bCarryCommitted)
	{
		ItemActor.RollbackPlacementConsumption(Carry);
		NewFacility->RollbackPlacedDomainRegistration();
		NewPlacement->EndTransition();
		DestroyStaged();
		OutFailureReason = LOCTEXT("PlacementItemConsumeFailed", "설비 아이템 소지 상태를 확정할 수 없습니다.");
		return nullptr;
	}

	TWeakObjectPtr<AActor> NewFacilityWeak(NewFacilityActor);
	TWeakObjectPtr<UFacilityPlacementComponent> NewPlacementWeak(NewPlacement);
	if (NewFacilityWeak.IsValid() && NewPlacementWeak.IsValid())
	{
		NewFacility->PublishPlacedDomainRegistration();
		if (NewFacilityWeak.IsValid() && NewPlacementWeak.IsValid())
		{
			NewPlacementWeak->EndTransition();
		}
	}
	// Item Destroy callbacks may independently remove the committed facility.
	// Avoid dereferencing it after the callback and retain successful commit
	// semantics for the carry transaction that already completed.
	return NewFacilityActor;
}

#undef LOCTEXT_NAMESPACE
