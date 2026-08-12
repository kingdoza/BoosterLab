#include "Towel/TowelMachineControlComponent.h"

#include "Towel/TowelProcessingMachineActor.h"

#define LOCTEXT_NAMESPACE "TowelMachineControlComponent"

UTowelMachineControlComponent::UTowelMachineControlComponent()
{
	InitBoxExtent(FVector(20.0f));
	SetCollisionProfileName(TEXT("BlockAllDynamic"));
}

FPlayerInteractionQuery UTowelMachineControlComponent::QueryInteraction(const FPlayerInteractionContext& Context) const
{
	FPlayerInteractionQuery Query;
	Query.bVisible = true;
	Query.TargetName = LOCTEXT("MachineControl", "수건 처리기 조작부");
	Query.ActionName = LOCTEXT("StartMachine", "작동 시작");
	const ATowelProcessingMachineActor* Machine = Cast<ATowelProcessingMachineActor>(GetOwner());
	FText FailureReason;
	Query.bCanInteract = Machine && Machine->CanStartProcessing(FailureReason);
	Query.FailureReason = Query.bCanInteract ? FText::GetEmpty() : FailureReason;
	return Query;
}

FPlayerInteractionResult UTowelMachineControlComponent::ExecuteInteraction(const FPlayerInteractionContext& Context)
{
	ATowelProcessingMachineActor* Machine = Cast<ATowelProcessingMachineActor>(GetOwner());
	FText FailureReason;
	return Machine && Machine->StartProcessing(FailureReason)
		? FPlayerInteractionResult::Succeeded()
		: FPlayerInteractionResult::Failed(FailureReason);
}

#undef LOCTEXT_NAMESPACE
