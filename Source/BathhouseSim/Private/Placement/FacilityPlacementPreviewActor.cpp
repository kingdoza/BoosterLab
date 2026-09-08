#include "Placement/FacilityPlacementPreviewActor.h"

#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"

AFacilityPlacementPreviewActor::AFacilityPlacementPreviewActor()
{
	PrimaryActorTick.bCanEverTick = false;
	SetActorEnableCollision(false);
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
}

void AFacilityPlacementPreviewActor::SetPlacementValidity(const bool bValid, const FText& FailureReason)
{
	SetActorEnableCollision(false);
	TInlineComponentArray<UPrimitiveComponent*> Primitives(this);
	for (UPrimitiveComponent* Primitive : Primitives)
	{
		if (Primitive)
		{
			Primitive->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Primitive->SetCanEverAffectNavigation(false);
		}
	}
	OnPlacementValidityChanged(bValid, FailureReason);
}
