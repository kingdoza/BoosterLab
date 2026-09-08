#include "Placement/FacilityPlacementDefinition.h"
#include "Placement/FacilityPlacementPreviewActor.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

FPrimaryAssetId UFacilityPlacementDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(TEXT("FacilityPlacementDefinition"), StableId.IsNone() ? GetFName() : StableId);
}

#if WITH_EDITOR
EDataValidationResult UFacilityPlacementDefinition::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	auto Invalidate = [&](const FText& Message)
	{
		Context.AddError(Message);
		Result = EDataValidationResult::Invalid;
	};
	if (StableId.IsNone())
	{
		Invalidate(NSLOCTEXT("FacilityPlacementDefinition", "MissingStableId", "Stable Id is required."));
	}
	if (FacilityTags.IsEmpty())
	{
		Invalidate(NSLOCTEXT("FacilityPlacementDefinition", "MissingTags", "At least one facility tag is required."));
	}
	if (!PreviewActorClass)
	{
		Invalidate(NSLOCTEXT("FacilityPlacementDefinition", "MissingPreviewClass", "A placement preview actor class is required."));
	}
	if (FootprintCellsX < 1 || FootprintCellsY < 1)
	{
		Invalidate(NSLOCTEXT("FacilityPlacementDefinition", "InvalidFootprint", "Footprint cell counts must be at least one."));
	}
	if (LockerSlotCount < 0)
	{
		Invalidate(NSLOCTEXT("FacilityPlacementDefinition", "InvalidLockerCount", "Locker slot count cannot be negative."));
	}
	return Result;
}
#endif
