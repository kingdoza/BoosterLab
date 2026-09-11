#pragma once

#include "CoreMinimal.h"

class AActor;
class UStaticMesh;

struct FFacilityPlacementPreviewMeshSource
{
	TObjectPtr<UStaticMesh> StaticMesh = nullptr;
	FName StableName = NAME_None;
	FTransform RelativeToRoot = FTransform::Identity;
	bool bVisible = true;
	bool bHiddenInGame = false;
	bool bCastShadow = true;
	bool bReceivesDecals = true;
};

namespace FacilityPlacementPreviewSource
{
	bool Collect(
		TSubclassOf<AActor> PlacedClass,
		AActor& ScratchOwner,
		TArray<FFacilityPlacementPreviewMeshSource>& OutSources,
		FText& OutFailureReason);
}
