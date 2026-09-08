#include "Facility/BathhouseExpansionDefinition.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#if WITH_EDITOR
EDataValidationResult UBathhouseExpansionDefinition::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	if (Tiers.IsEmpty())
	{
		Context.AddError(NSLOCTEXT("BathhouseExpansion", "MissingTiers", "At least one expansion tier is required."));
		return EDataValidationResult::Invalid;
	}
	int32 PreviousKeys = -1;
	int32 PreviousSlots = -1;
	for (int32 Index = 0; Index < Tiers.Num(); ++Index)
	{
		const FBathhouseExpansionTier& Tier = Tiers[Index];
		if (Tier.KeyPoolSize < Tier.MaxInstalledLockerSlots || Tier.KeyPoolSize < PreviousKeys || Tier.MaxInstalledLockerSlots < PreviousSlots)
		{
			Context.AddError(FText::Format(
				NSLOCTEXT("BathhouseExpansion", "InvalidTier", "Expansion tier {0} must be monotonic and its key pool must cover all locker slots."),
				Index));
			Result = EDataValidationResult::Invalid;
		}
		PreviousKeys = Tier.KeyPoolSize;
		PreviousSlots = Tier.MaxInstalledLockerSlots;
	}
	return Result;
}
#endif
