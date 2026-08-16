#include "Towel/Presentation/TowelStackVisualComponent.h"

#include "Engine/World.h"

void UTowelStackVisualComponent::RebuildPreview()
{
	const UWorld* World = GetWorld();
	if (!World || (World->WorldType != EWorldType::Editor && World->WorldType != EWorldType::EditorPreview))
	{
		RebuildEditorPreview(PreviewState, PreviewCount, PreviewRevision);
		return;
	}
	RebuildEditorPreview(PreviewState, PreviewCount, ++PreviewRevision);
}

void UTowelStackVisualComponent::ClearPreview()
{
	ClearEditorPreview();
}

FTransform UTowelStackVisualComponent::BuildLocalTransform(const int32 VisualIndex)
{
	const FVector Location = BaseLocalOffset
		+ FVector::UpVector * FMath::Max(0.0f, ZSpacing) * FMath::Max(0, VisualIndex);
	return FTransform(FQuat::Identity, Location);
}
