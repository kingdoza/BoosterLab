#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "BathhouseHUD.generated.h"

class APawn;
class UInteractionPromptWidget;

UCLASS(Blueprintable)
class BATHHOUSESIM_API ABathhouseHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI")
	TSubclassOf<UInteractionPromptWidget> InteractionPromptWidgetClass;

private:
	UFUNCTION()
	void HandlePossessedPawnChanged(APawn* OldPawn, APawn* NewPawn);

	void RebindPawn(APawn* Pawn);

	UPROPERTY(Transient)
	TObjectPtr<UInteractionPromptWidget> InteractionPromptWidget = nullptr;
};
