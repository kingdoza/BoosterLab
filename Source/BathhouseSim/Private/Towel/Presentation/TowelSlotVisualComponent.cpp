#include "Towel/Presentation/TowelSlotVisualComponent.h"

#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

void UTowelSlotVisualComponent::OnRegister()
{
	ResolveSlots();
	Super::OnRegister();
}

void UTowelSlotVisualComponent::RebuildPreview()
{
	const UWorld* World = GetWorld();
	if (!World || (World->WorldType != EWorldType::Editor && World->WorldType != EWorldType::EditorPreview))
	{
		RebuildEditorPreview(PreviewState, PreviewCount, PreviewRevision);
		return;
	}
	ResolveSlots();
	RebuildEditorPreview(PreviewState, PreviewCount, ++PreviewRevision);
}

void UTowelSlotVisualComponent::ClearPreview()
{
	ClearEditorPreview();
}

FTransform UTowelSlotVisualComponent::BuildLocalTransform(const int32 VisualIndex)
{
	if (!ResolvedSlots.IsValidIndex(VisualIndex) || !ResolvedSlots[VisualIndex])
	{
		return FTransform::Identity;
	}
	return ResolvedSlots[VisualIndex]->GetComponentTransform().GetRelativeTransform(GetComponentTransform());
}

int32 UTowelSlotVisualComponent::GetVisualCapacity() const
{
	return ResolvedSlots.Num();
}

void UTowelSlotVisualComponent::PrepareLayout()
{
	if (GetTargetCount() > ResolvedSlots.Num() && LastWarnedOverflowCount != GetTargetCount())
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("Towel slot visual %s clamps presentation count %d to %d valid authored slots."),
			*GetPathName(),
			GetTargetCount(),
			ResolvedSlots.Num());
		LastWarnedOverflowCount = GetTargetCount();
	}
	else if (GetTargetCount() <= ResolvedSlots.Num())
	{
		LastWarnedOverflowCount = INDEX_NONE;
	}
}

void UTowelSlotVisualComponent::ResolveSlots()
{
	ResolvedSlots.Reset();
	LastWarnedOverflowCount = INDEX_NONE;
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	TSet<TObjectPtr<USceneComponent>> SeenSlots;
	for (int32 Index = 0; Index < SlotReferences.Num(); ++Index)
	{
		const FComponentReference& Reference = SlotReferences[Index];
		const bool bEmptyReference = !Reference.OtherActor.IsValid()
			&& Reference.ComponentProperty.IsNone()
			&& Reference.PathToComponent.IsEmpty()
			&& !Reference.OverrideComponent.IsValid();
		if (bEmptyReference || (Reference.OtherActor.IsValid() && Reference.OtherActor.Get() != Owner))
		{
			UE_LOG(LogTemp, Warning, TEXT("Towel slot visual %s rejects unresolved or foreign slot reference %d."),
				*GetPathName(), Index);
			continue;
		}

		USceneComponent* Slot = Cast<USceneComponent>(Reference.GetComponent(Owner));
		if (!Slot || Slot->GetOwner() != Owner)
		{
			UE_LOG(LogTemp, Warning, TEXT("Towel slot visual %s rejects invalid slot reference %d."),
				*GetPathName(), Index);
			continue;
		}
		if (SeenSlots.Contains(Slot))
		{
			UE_LOG(LogTemp, Warning, TEXT("Towel slot visual %s rejects duplicate slot reference %d."),
				*GetPathName(), Index);
			continue;
		}
		SeenSlots.Add(Slot);
		ResolvedSlots.Add(Slot);
	}
}
