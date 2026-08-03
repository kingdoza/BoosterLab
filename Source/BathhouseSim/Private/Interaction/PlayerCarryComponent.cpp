#include "Interaction/PlayerCarryComponent.h"

#include "Components/SceneComponent.h"
#include "Interaction/BathhouseKeyActor.h"

UPlayerCarryComponent::UPlayerCarryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UPlayerCarryComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (HeldKey)
	{
		HeldKey->RecoverToHook(this);
	}
	HeldKey = nullptr;
	HeldAnchor = nullptr;
	OnHeldKeyChanged.Clear();
	Super::EndPlay(EndPlayReason);
}

void UPlayerCarryComponent::ConfigureHeldAnchor(USceneComponent* InHeldAnchor)
{
	HeldAnchor = InHeldAnchor;
}

bool UPlayerCarryComponent::CommitTakeKey(ABathhouseKeyActor* Key)
{
	if (!IsValid(Key) || HeldKey)
	{
		return false;
	}
	HeldKey = Key;
	OnHeldKeyChanged.Broadcast(HeldKey);
	return true;
}

bool UPlayerCarryComponent::CommitReleaseKey(ABathhouseKeyActor* Key)
{
	// Pointer identity is sufficient for release. In particular, a key actor can
	// already be ending play when it asks its carry owner to drop the reference.
	if (!Key || HeldKey != Key)
	{
		return false;
	}
	HeldKey = nullptr;
	OnHeldKeyChanged.Broadcast(nullptr);
	return true;
}
