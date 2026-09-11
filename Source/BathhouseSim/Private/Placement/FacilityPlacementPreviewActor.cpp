#include "Placement/FacilityPlacementPreviewActor.h"

#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "MaterialShared.h"
#include "Placement/FacilityPlacementComponent.h"
#include "Placement/FacilityPlacementPreviewSource.h"
#include "Placement/FacilityPlacementSettings.h"
#include "Placement/PlaceableFacility.h"

AFacilityPlacementPreviewActor::AFacilityPlacementPreviewActor()
{
	PrimaryActorTick.bCanEverTick = false;
	SetActorEnableCollision(false);
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
	SceneRoot->SetCanEverAffectNavigation(false);
}

bool AFacilityPlacementPreviewActor::InitializeFromPlacedClass(
	const TSubclassOf<AActor> PlacedClass,
	FText& OutFailureReason)
{
	PreviewMeshes.Reset();
	SourcePlacedClass = nullptr;
	const AActor* PlacedCDO = PlacedClass ? PlacedClass->GetDefaultObject<AActor>() : nullptr;
	const IPlaceableFacility* Placeable = Cast<IPlaceableFacility>(PlacedCDO);
	const UFacilityPlacementComponent* Placement = Placeable
		? Placeable->GetFacilityPlacementComponent() : nullptr;
	const USceneComponent* PlacedRoot = PlacedCDO ? PlacedCDO->GetRootComponent() : nullptr;
	FTransform FootprintRelative;
	FIntPoint FootprintCells;
	if (!PlacedCDO || !PlacedRoot || !Placement
		|| !Placement->GetFootprintRelativeToRoot(FootprintRelative, OutFailureReason)
		|| !Placement->DeriveFootprintCells(FootprintCells, OutFailureReason))
	{
		if (OutFailureReason.IsEmpty())
		{
			OutFailureReason = NSLOCTEXT(
				"FacilityPlacementPreview", "InvalidGeometry",
				"미리보기의 배치 설비 geometry가 올바르지 않습니다.");
		}
		return false;
	}
	SourcePlacedClass = PlacedClass;
	SourceFootprintRelative = FootprintRelative;
	SourceFootprintExtent = Placement->GetPlacementFootprint()->GetUnscaledBoxExtent();
	SourceRootScale = PlacedRoot->GetRelativeScale3D();

	const UFacilityPlacementSettings* Settings = GetDefault<UFacilityPlacementSettings>();
	ValidMaterial = Settings->LoadValidPreviewMaterial();
	InvalidMaterial = Settings->LoadInvalidPreviewMaterial();
	if (!ValidMaterial || !InvalidMaterial
		|| !IsTranslucentBlendMode(ValidMaterial->GetBlendMode())
		|| !IsTranslucentBlendMode(InvalidMaterial->GetBlendMode()))
	{
		OutFailureReason = NSLOCTEXT(
			"FacilityPlacementPreview", "InvalidMaterials",
			"설비 배치 미리보기의 유효/무효 반투명 머티리얼 설정이 올바르지 않습니다.");
		return false;
	}

	TArray<FFacilityPlacementPreviewMeshSource> Sources;
	if (!FacilityPlacementPreviewSource::Collect(
		PlacedClass, *this, Sources, OutFailureReason))
	{
		return false;
	}
	if (Sources.IsEmpty())
	{
		OutFailureReason = NSLOCTEXT(
			"FacilityPlacementPreview", "NoEligibleMeshes",
			"배치 설비 클래스에서 미리보기에 사용할 Static Mesh를 찾을 수 없습니다.");
		return false;
	}

	for (const FFacilityPlacementPreviewMeshSource& Source : Sources)
	{
		UStaticMeshComponent* Mesh = NewObject<UStaticMeshComponent>(
			this,
			MakeUniqueObjectName(this, UStaticMeshComponent::StaticClass(), Source.StableName),
			RF_Transient);
		if (!Mesh)
		{
			OutFailureReason = NSLOCTEXT(
				"FacilityPlacementPreview", "MeshCreationFailed",
				"설비 배치 미리보기 Mesh Component를 만들 수 없습니다.");
			return false;
		}
		AddInstanceComponent(Mesh);
		Mesh->SetupAttachment(SceneRoot);
		Mesh->SetStaticMesh(Source.StaticMesh);
		Mesh->SetRelativeTransform(Source.RelativeToRoot);
		Mesh->SetVisibility(Source.bVisible);
		Mesh->SetHiddenInGame(Source.bHiddenInGame);
		Mesh->SetCastShadow(Source.bCastShadow);
		Mesh->SetReceivesDecals(Source.bReceivesDecals);
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh->SetGenerateOverlapEvents(false);
		Mesh->SetSimulatePhysics(false);
		Mesh->SetCanEverAffectNavigation(false);
		Mesh->PrimaryComponentTick.bCanEverTick = false;
		Mesh->RegisterComponent();
		PreviewMeshes.Add(Mesh);
	}
	return ApplyPreviewMaterial(InvalidMaterial, OutFailureReason);
}

bool AFacilityPlacementPreviewActor::ValidateSourceGeometry(
	const TSubclassOf<AActor> PlacedClass,
	FText& OutFailureReason) const
{
	const AActor* PlacedCDO = PlacedClass ? PlacedClass->GetDefaultObject<AActor>() : nullptr;
	const IPlaceableFacility* Placeable = Cast<IPlaceableFacility>(PlacedCDO);
	const UFacilityPlacementComponent* Placement = Placeable
		? Placeable->GetFacilityPlacementComponent() : nullptr;
	const USceneComponent* Root = PlacedCDO ? PlacedCDO->GetRootComponent() : nullptr;
	FTransform FootprintRelative;
	if (PlacedClass != SourcePlacedClass || !Placement || !Root
		|| !Placement->GetFootprintRelativeToRoot(FootprintRelative, OutFailureReason)
		|| !Placement->GetPlacementFootprint()
		|| !FootprintRelative.Equals(SourceFootprintRelative, 0.01f)
		|| !Placement->GetPlacementFootprint()->GetUnscaledBoxExtent().Equals(
			SourceFootprintExtent, 0.01f)
		|| !Root->GetRelativeScale3D().Equals(SourceRootScale, 0.0001f))
	{
		if (OutFailureReason.IsEmpty())
		{
			OutFailureReason = NSLOCTEXT(
				"FacilityPlacementPreview", "GeometryChanged",
				"설비 배치 geometry가 미리보기 생성 이후 변경되었습니다.");
		}
		return false;
	}
	return true;
}

bool AFacilityPlacementPreviewActor::ApplyPreviewMaterial(
	UMaterialInterface* Material,
	FText& OutFailureReason)
{
	if (!Material || PreviewMeshes.IsEmpty())
	{
		OutFailureReason = NSLOCTEXT(
			"FacilityPlacementPreview", "MaterialApplicationFailed",
			"설비 배치 미리보기 머티리얼을 적용할 수 없습니다.");
		return false;
	}
	for (UStaticMeshComponent* Mesh : PreviewMeshes)
	{
		if (!IsValid(Mesh) || Mesh->GetNumMaterials() <= 0)
		{
			OutFailureReason = NSLOCTEXT(
				"FacilityPlacementPreview", "InvalidMaterialSlots",
				"설비 배치 미리보기 Mesh의 material slot이 올바르지 않습니다.");
			return false;
		}
		for (int32 Slot = 0; Slot < Mesh->GetNumMaterials(); ++Slot)
		{
			Mesh->SetMaterial(Slot, Material);
		}
	}
	return true;
}

void AFacilityPlacementPreviewActor::SetPlacementValidity(
	const bool bValid,
	const FText& FailureReason)
{
	SetActorEnableCollision(false);
	FText Ignored;
	ApplyPreviewMaterial(bValid ? ValidMaterial : InvalidMaterial, Ignored);
	OnPlacementValidityChanged(bValid, FailureReason);
}
