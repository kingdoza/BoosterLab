#include "Towel/TowelTransferPortComponent.h"

#include "Interaction/PlayerCarryComponent.h"
#include "Towel/TowelBasketActor.h"
#include "Towel/TowelInventoryComponent.h"
#include "Towel/TowelProcessingMachineActor.h"
#include "Towel/TowelTransferSubsystem.h"

#define LOCTEXT_NAMESPACE "TowelTransferPortComponent"

UTowelTransferPortComponent::UTowelTransferPortComponent()
{
	InitBoxExtent(FVector(30.0f));
	SetCollisionProfileName(TEXT("BlockAllDynamic"));
}

FPlayerInteractionQuery UTowelTransferPortComponent::QueryInteraction(const FPlayerInteractionContext& Context) const
{
	FPlayerInteractionQuery Query;
	Query.bVisible = true;
	Query.TargetName = LOCTEXT("MachinePort", "수건 투입구");
	Query.ActionName = LOCTEXT("TransferOne", "수건 한 장 옮기기");
	Query.bSecondaryVisible = true;
	Query.SecondaryActionName = LOCTEXT("TransferMax", "가능한 만큼 옮기기");
	const ATowelProcessingMachineActor* Machine = Cast<ATowelProcessingMachineActor>(GetOwner());
	const ATowelBasketActor* Basket = Context.CarryComponent
		? Cast<ATowelBasketActor>(Context.CarryComponent->GetHeldObject())
		: nullptr;
	bool bCanTransfer = Machine && Basket && Machine->GetMachineState() != ETowelMachineState::Processing;
	if (bCanTransfer && Machine->GetMachineState() == ETowelMachineState::Waiting)
	{
		const FTowelInventorySnapshot BasketSnapshot = Basket->GetInventory()->GetSnapshot();
		bCanTransfer = BasketSnapshot.Count > 0 && BasketSnapshot.State == Machine->GetInputState()
			&& Machine->GetInventory()->CanAccept(Machine->GetInputState());
	}
	else if (bCanTransfer && Machine->GetMachineState() == ETowelMachineState::Complete)
	{
		bCanTransfer = Machine->GetInventory()->GetSnapshot().Count > 0
			&& Basket->GetInventory()->CanAccept(Machine->GetOutputState());
	}
	Query.bCanInteract = bCanTransfer;
	Query.bCanSecondaryInteract = bCanTransfer;
	if (!bCanTransfer)
	{
		Query.FailureReason = LOCTEXT("PortUnavailable", "현재 수건을 옮길 수 없습니다.");
		Query.SecondaryFailureReason = Query.FailureReason;
	}
	return Query;
}

FPlayerInteractionResult UTowelTransferPortComponent::ExecuteInteraction(const FPlayerInteractionContext& Context)
{
	return Transfer(Context, 1, EPlayerInteractionIntent::Primary);
}

FPlayerInteractionResult UTowelTransferPortComponent::ExecuteSecondaryInteraction(const FPlayerInteractionContext& Context)
{
	return Transfer(Context, MAX_int32, EPlayerInteractionIntent::Secondary);
}

FPlayerInteractionResult UTowelTransferPortComponent::Transfer(
	const FPlayerInteractionContext& Context,
	const int32 RequestedCount,
	const EPlayerInteractionIntent Intent)
{
	ATowelProcessingMachineActor* Machine = Cast<ATowelProcessingMachineActor>(GetOwner());
	ATowelBasketActor* Basket = Context.CarryComponent
		? Cast<ATowelBasketActor>(Context.CarryComponent->GetHeldObject())
		: nullptr;
	UTowelTransferSubsystem* TransferSubsystem = GetWorld()->GetSubsystem<UTowelTransferSubsystem>();
	if (!Machine || !Basket || !TransferSubsystem || Machine->GetMachineState() == ETowelMachineState::Processing)
	{
		return FPlayerInteractionResult::Failed(LOCTEXT("PortUnavailable", "현재 수건을 옮길 수 없습니다."), Intent);
	}
	FTowelTransferRequest Request;
	if (Machine->GetMachineState() == ETowelMachineState::Waiting)
	{
		Request.Source = Basket->GetInventory();
		Request.Destination = Machine->GetInventory();
	}
	else
	{
		Request.Source = Machine->GetInventory();
		Request.Destination = Basket->GetInventory();
	}
	Request.RequestedCount = RequestedCount;
	Request.ExpectedSourceRevision = Request.Source->GetSnapshot().Revision;
	Request.ExpectedDestinationRevision = Request.Destination->GetSnapshot().Revision;
	const FTowelTransferResult Result = TransferSubsystem->TryTransfer(Request);
	if (Result.bSucceeded)
	{
		return FPlayerInteractionResult::Succeeded(Intent);
	}
	return FPlayerInteractionResult::Failed(LOCTEXT("TransferFailed", "수건을 옮길 수 없습니다."), Intent);
}

#undef LOCTEXT_NAMESPACE
