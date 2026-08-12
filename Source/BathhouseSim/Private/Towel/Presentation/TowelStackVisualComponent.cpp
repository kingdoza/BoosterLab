#include "Towel/Presentation/TowelStackVisualComponent.h"

FTransform UTowelStackVisualComponent::BuildLocalTransform(const int32 VisualIndex)
{
	const FVector Location = BaseLocalOffset
		+ FVector::UpVector * FMath::Max(0.0f, ZSpacing) * FMath::Max(0, VisualIndex);
	return FTransform(FQuat::Identity, Location);
}
