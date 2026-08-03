#include "UI/BathhouseHUD.h"

#include "Blueprint/UserWidget.h"
#include "GameFramework/PlayerController.h"
#include "Interaction/PlayerInteractionComponent.h"
#include "UI/InteractionPromptWidget.h"

void ABathhouseHUD::BeginPlay()
{
	Super::BeginPlay();

	APlayerController* PlayerController = GetOwningPlayerController();
	if (!PlayerController || !PlayerController->IsLocalController())
	{
		return;
	}

	PlayerController->OnPossessedPawnChanged.AddDynamic(this, &ABathhouseHUD::HandlePossessedPawnChanged);
	if (InteractionPromptWidgetClass)
	{
		InteractionPromptWidget = CreateWidget<UInteractionPromptWidget>(PlayerController, InteractionPromptWidgetClass);
		if (InteractionPromptWidget)
		{
			InteractionPromptWidget->AddToViewport();
		}
	}
	RebindPawn(PlayerController->GetPawn());
}

void ABathhouseHUD::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (APlayerController* PlayerController = GetOwningPlayerController())
	{
		PlayerController->OnPossessedPawnChanged.RemoveDynamic(this, &ABathhouseHUD::HandlePossessedPawnChanged);
	}
	if (InteractionPromptWidget)
	{
		InteractionPromptWidget->SetInteractionComponent(nullptr);
		InteractionPromptWidget->RemoveFromParent();
		InteractionPromptWidget = nullptr;
	}
	Super::EndPlay(EndPlayReason);
}

void ABathhouseHUD::HandlePossessedPawnChanged(APawn* OldPawn, APawn* NewPawn)
{
	RebindPawn(NewPawn);
}

void ABathhouseHUD::RebindPawn(APawn* Pawn)
{
	if (InteractionPromptWidget)
	{
		InteractionPromptWidget->SetInteractionComponent(Pawn ? Pawn->FindComponentByClass<UPlayerInteractionComponent>() : nullptr);
	}
}
