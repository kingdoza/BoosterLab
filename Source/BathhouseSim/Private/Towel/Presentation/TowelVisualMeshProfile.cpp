#include "Towel/Presentation/TowelVisualMeshProfile.h"

#include "Engine/StaticMesh.h"
#include "Misc/DataValidation.h"

#define LOCTEXT_NAMESPACE "TowelVisualMeshProfile"

namespace
{
const TArray<TObjectPtr<UStaticMesh>> EmptyTowelMeshVariants;
}

UStaticMesh* UTowelVisualMeshProfile::SelectMesh(
	const ETowelState State,
	FRandomStream& RandomStream) const
{
	const TArray<TObjectPtr<UStaticMesh>>& Variants = GetValidVariants(State);
	if (Variants.IsEmpty())
	{
		return nullptr;
	}
	if (Variants.Num() == 1)
	{
		return Variants[0];
	}
	return Variants[RandomStream.RandRange(0, Variants.Num() - 1)];
}

const TArray<TObjectPtr<UStaticMesh>>& UTowelVisualMeshProfile::GetValidVariants(
	const ETowelState State) const
{
	if (!bCacheValid)
	{
		RebuildCache();
	}
	if (const TArray<TObjectPtr<UStaticMesh>>* Variants = CachedVariants.Find(State))
	{
		return *Variants;
	}
	return EmptyTowelMeshVariants;
}

bool UTowelVisualMeshProfile::ValidateProfile(
	TArray<FText>& OutErrors,
	TArray<FText>& OutWarnings) const
{
	OutErrors.Reset();
	OutWarnings.Reset();
	RebuildCache(&OutErrors, &OutWarnings);
	return OutErrors.IsEmpty();
}

void UTowelVisualMeshProfile::PostLoad()
{
	Super::PostLoad();
	InvalidateCache();
}

EDataValidationResult UTowelVisualMeshProfile::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	TArray<FText> Errors;
	TArray<FText> Warnings;
	ValidateProfile(Errors, Warnings);
	for (const FText& Error : Errors)
	{
		Context.AddError(Error);
		Result = EDataValidationResult::Invalid;
	}
	for (const FText& Warning : Warnings)
	{
		Context.AddWarning(Warning);
	}
	return Result == EDataValidationResult::NotValidated
		? EDataValidationResult::Valid
		: Result;
}

#if WITH_EDITOR
void UTowelVisualMeshProfile::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	InvalidateCache();
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

void UTowelVisualMeshProfile::InvalidateCache()
{
	bCacheValid = false;
	CachedVariants.Reset();
}

void UTowelVisualMeshProfile::RebuildCache(
	TArray<FText>* OutErrors,
	TArray<FText>* OutWarnings) const
{
	CachedVariants.Reset();
	TSet<ETowelState> SeenStates;
	for (const FTowelStateMeshVariants& Entry : StateVariants)
	{
		if (Entry.State == ETowelState::None)
		{
			if (OutErrors)
			{
				OutErrors->Add(LOCTEXT("NoneStateEntry", "Towel mesh profile cannot contain a None state entry."));
			}
			continue;
		}
		if (SeenStates.Contains(Entry.State))
		{
			if (OutErrors)
			{
				OutErrors->Add(FText::Format(
					LOCTEXT("DuplicateStateEntry", "Towel mesh profile contains duplicate state entry {0}."),
					FText::AsNumber(static_cast<uint8>(Entry.State))));
			}
			continue;
		}
		SeenStates.Add(Entry.State);

		TArray<TObjectPtr<UStaticMesh>>& ValidVariants = CachedVariants.Add(Entry.State);
		for (UStaticMesh* Mesh : Entry.MeshVariants)
		{
			if (Mesh)
			{
				ValidVariants.Add(Mesh);
			}
		}
		if (ValidVariants.IsEmpty() && OutWarnings)
		{
			OutWarnings->Add(FText::Format(
				LOCTEXT("NoValidVariants", "Towel mesh profile state {0} has no valid mesh variants and will render nothing."),
				FText::AsNumber(static_cast<uint8>(Entry.State))));
		}
	}
	bCacheValid = true;
}

#undef LOCTEXT_NAMESPACE
