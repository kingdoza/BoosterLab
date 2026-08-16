#include "UI/ComputerSampleScreenWidget.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"

void UComputerSampleScreenWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (TestButton)
	{
		TestButton->OnClicked.RemoveDynamic(this, &UComputerSampleScreenWidget::HandleTestButtonClicked);
		TestButton->OnClicked.AddDynamic(this, &UComputerSampleScreenWidget::HandleTestButtonClicked);
	}
	ApplyClickState();
}

void UComputerSampleScreenWidget::NativeDestruct()
{
	if (TestButton)
	{
		TestButton->OnClicked.RemoveDynamic(this, &UComputerSampleScreenWidget::HandleTestButtonClicked);
	}

	Super::NativeDestruct();
}

void UComputerSampleScreenWidget::HandleTestButtonClicked()
{
	bWasClicked = true;
	ApplyClickState();
}

void UComputerSampleScreenWidget::ApplyClickState()
{
	if (ClickResultText)
	{
		ClickResultText->SetText(bWasClicked
			? NSLOCTEXT("BathhouseComputer", "SampleClicked", "클릭 확인")
			: NSLOCTEXT("BathhouseComputer", "SampleClickPrompt", "버튼을 클릭하세요"));
	}
}
