#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ComputerSampleScreenWidget.generated.h"

class UButton;
class UTextBlock;

UCLASS()
class BATHHOUSESIM_API UComputerSampleScreenWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UFUNCTION()
	void HandleTestButtonClicked();

	void ApplyClickState();

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> TestButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> ClickResultText;

private:
	friend class FBathhouseComputerSessionTest;

	bool bWasClicked = false;
};
