#include "Towel/Presentation/TowelPileVisualComponent.h"

#include "Engine/World.h"

void UTowelPileVisualComponent::RebuildPreview()
{
	const UWorld* World = GetWorld();
	if (!World || (World->WorldType != EWorldType::Editor && World->WorldType != EWorldType::EditorPreview))
	{
		RebuildEditorPreview(PreviewState, PreviewCount, PreviewRevision);
		return;
	}
	RebuildEditorPreview(PreviewState, PreviewCount, ++PreviewRevision);
}

void UTowelPileVisualComponent::ClearPreview()
{
	ClearEditorPreview();
}

FTransform UTowelPileVisualComponent::BuildLocalTransform(const int32 VisualIndex)
{
	FRandomStream& Random = GetVisualRandomStream();
	const FVector SafeExtent(
		FMath::Max(0.0f, PileHalfExtent.X),
		FMath::Max(0.0f, PileHalfExtent.Y),
		FMath::Max(0.0f, PileHalfExtent.Z));
	const int32 LayerIndex = FMath::Max(0, VisualIndex) / FMath::Max(1, ItemsPerLayer);
	const float RelativeZ = FMath::Clamp(
		LayerIndex * FMath::Max(0.0f, LayerSpacing)
			+ Random.FRandRange(-FMath::Max(0.0f, MaxZJitter), FMath::Max(0.0f, MaxZJitter)),
		0.0f,
		SafeExtent.Z);
	const FVector Location = BaseLocalOffset + FVector(
		Random.FRandRange(-SafeExtent.X, SafeExtent.X),
		Random.FRandRange(-SafeExtent.Y, SafeExtent.Y),
		RelativeZ);
	const FRotator Rotation(
		Random.FRandRange(
			FMath::Min(MinRandomRotation.Pitch, MaxRandomRotation.Pitch),
			FMath::Max(MinRandomRotation.Pitch, MaxRandomRotation.Pitch)),
		Random.FRandRange(
			FMath::Min(MinRandomRotation.Yaw, MaxRandomRotation.Yaw),
			FMath::Max(MinRandomRotation.Yaw, MaxRandomRotation.Yaw)),
		Random.FRandRange(
			FMath::Min(MinRandomRotation.Roll, MaxRandomRotation.Roll),
			FMath::Max(MinRandomRotation.Roll, MaxRandomRotation.Roll)));
	return FTransform(Rotation, Location);
}
