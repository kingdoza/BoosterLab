#include "Towel/CleanTowelStackActor.h"

#include "Interaction/PlayerCarryComponent.h"
#include "Towel/TowelBasketActor.h"
#include "Towel/TowelInventoryComponent.h"
#include "Towel/Presentation/TowelStackVisualComponent.h"
#include "Towel/TowelTransferSubsystem.h"

#define LOCTEXT_NAMESPACE "CleanTowelStackActor"

ACleanTowelStackActor::ACleanTowelStackActor()
{
	FacilityType = EBathhouseFacilityType::TowelShelf;
	Inventory = CreateDefaultSubobject<UTowelInventoryComponent>(TEXT("TowelInventory"));
	Inventory->ConfigureDefaults(ETowelState::Clean, 20, 30);
	TowelPresentationVisual = CreateDefaultSubobject<UTowelStackVisualComponent>(TEXT("TowelPresentationVisual"));
	TowelPresentationVisual->SetupAttachment(GetRootComponent());
}

void ACleanTowelStackActor::BeginPlay()
{
	Super::BeginPlay();
	TowelPresentationVisual->BindInventorySource(Inventory);
}

void ACleanTowelStackActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	TowelPresentationVisual->UnbindInventorySource();
	Super::EndPlay(EndPlayReason);
}

FPlayerInteractionQuery ACleanTowelStackActor::QueryInteraction(const FPlayerInteractionContext& Context) const
{
	FPlayerInteractionQuery Query;
	Query.bVisible = true;
	Query.TargetName = LOCTEXT("CleanStack", "깨끗한 수건 선반");
	Query.ActionName = LOCTEXT("DepositOne", "수건 한 장 놓기");
	Query.bSecondaryVisible = true;
	Query.SecondaryActionName = LOCTEXT("DepositMax", "가능한 만큼 놓기");
	const ATowelBasketActor* Basket = Context.CarryComponent
		? Cast<ATowelBasketActor>(Context.CarryComponent->GetHeldObject())
		: nullptr;
	if (!Basket)
	{
		Query.FailureReason = LOCTEXT("CleanBasketRequired", "깨끗한 수건 바구니가 필요합니다.");
		Query.SecondaryFailureReason = Query.FailureReason;
		return Query;
	}
	const FTowelInventorySnapshot BasketSnapshot = Basket->GetInventory()->GetSnapshot();
	const FTowelInventorySnapshot StackSnapshot = Inventory->GetSnapshot();
	Query.bCanInteract = BasketSnapshot.State == ETowelState::Clean && BasketSnapshot.Count > 0
		&& StackSnapshot.Count < StackSnapshot.Capacity;
	Query.bCanSecondaryInteract = Query.bCanInteract;
	if (!Query.bCanInteract)
	{
		Query.FailureReason = BasketSnapshot.Count <= 0
			? LOCTEXT("BasketEmpty", "바구니가 비어 있습니다.")
			: LOCTEXT("CleanStateOrCapacity", "깨끗한 수건만 놓을 수 있거나 선반이 가득 찼습니다.");
		Query.SecondaryFailureReason = Query.FailureReason;
	}
	return Query;
}

FPlayerInteractionResult ACleanTowelStackActor::ExecuteInteraction(const FPlayerInteractionContext& Context)
{
	return TransferFromHeldBasket(Context, 1, EPlayerInteractionIntent::Primary);
}

FPlayerInteractionResult ACleanTowelStackActor::ExecuteSecondaryInteraction(const FPlayerInteractionContext& Context)
{
	return TransferFromHeldBasket(Context, MAX_int32, EPlayerInteractionIntent::Secondary);
}

FPlayerInteractionResult ACleanTowelStackActor::TransferFromHeldBasket(
	const FPlayerInteractionContext& Context,
	const int32 RequestedCount,
	const EPlayerInteractionIntent Intent)
{
	ATowelBasketActor* Basket = Context.CarryComponent
		? Cast<ATowelBasketActor>(Context.CarryComponent->GetHeldObject())
		: nullptr;
	UTowelTransferSubsystem* Transfer = GetWorld()->GetSubsystem<UTowelTransferSubsystem>();
	if (!Basket || !Transfer)
	{
		return FPlayerInteractionResult::Failed(LOCTEXT("CleanBasketRequired", "깨끗한 수건 바구니가 필요합니다."), Intent);
	}
	FTowelTransferRequest Request;
	Request.Source = Basket->GetInventory();
	Request.Destination = Inventory;
	Request.RequestedCount = RequestedCount;
	Request.ExpectedSourceRevision = Request.Source->GetSnapshot().Revision;
	Request.ExpectedDestinationRevision = Request.Destination->GetSnapshot().Revision;
	const FTowelTransferResult Result = Transfer->TryTransfer(Request);
	return Result.bSucceeded
		? FPlayerInteractionResult::Succeeded(Intent)
		: FPlayerInteractionResult::Failed(LOCTEXT("DepositFailed", "수건을 선반으로 옮길 수 없습니다."), Intent);
}

#undef LOCTEXT_NAMESPACE
