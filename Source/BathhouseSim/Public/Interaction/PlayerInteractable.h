#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Interaction/InteractionTypes.h"
#include "PlayerInteractable.generated.h"

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UPlayerInteractable : public UInterface
{
	GENERATED_BODY()
};

class BATHHOUSESIM_API IPlayerInteractable
{
	GENERATED_BODY()

public:
	virtual FPlayerInteractionQuery QueryInteraction(const FPlayerInteractionContext& Context) const = 0;
	virtual FPlayerInteractionResult ExecuteInteraction(const FPlayerInteractionContext& Context) = 0;
	virtual FPlayerInteractionResult ExecuteSecondaryInteraction(const FPlayerInteractionContext& Context)
	{
		return FPlayerInteractionResult::Failed(
			NSLOCTEXT("BathhouseInteraction", "SecondaryUnavailable", "보조 상호작용을 사용할 수 없습니다."),
			EPlayerInteractionIntent::Secondary);
	}
	virtual bool BeginHoldInteraction(const FPlayerInteractionContext& Context, FText& OutFailureReason)
	{
		OutFailureReason = NSLOCTEXT("BathhouseInteraction", "HoldUnavailable", "길게 상호작용할 수 없습니다.");
		return false;
	}
	virtual FPlayerHoldInteractionUpdate UpdateHoldInteraction(
		const FPlayerInteractionContext& Context,
		float DeltaTime)
	{
		FPlayerHoldInteractionUpdate Update;
		Update.FailureReason = NSLOCTEXT("BathhouseInteraction", "HoldUnavailable", "길게 상호작용할 수 없습니다.");
		return Update;
	}
	virtual void CancelHoldInteraction(const FPlayerInteractionContext& Context) {}
};
