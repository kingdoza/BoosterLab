#include "Towel/WorldUsedTowelActor.h"

#include "Components/StaticMeshComponent.h"
#include "Interaction/PlayerCarryComponent.h"
#include "Towel/TowelBasketActor.h"
#include "Towel/TowelCirculationSubsystem.h"
#include "Towel/TowelInventoryComponent.h"
#include "Towel/TowelTransferSubsystem.h"

#define LOCTEXT_NAMESPACE "WorldUsedTowelActor"

AWorldUsedTowelActor::AWorldUsedTowelActor()
{
	PrimaryActorTick.bCanEverTick = false;
	WorldMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WorldMesh"));
	SetRootComponent(WorldMesh);
	WorldMesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	Inventory = CreateDefaultSubobject<UTowelInventoryComponent>(TEXT("TowelInventory"));
	Inventory->ConfigureDefaults(ETowelState::Used, 1, 1);
	Inventory->SetRecoverContentsOnEndPlay(false);
}

void AWorldUsedTowelActor::BeginPlay()
{
	Super::BeginPlay();
	WorldMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (UTowelCirculationSubsystem* Subsystem = GetWorld()->GetSubsystem<UTowelCirculationSubsystem>())
	{
		Subsystem->RegisterWorldTowel(this);
	}
}

void AWorldUsedTowelActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		if (UTowelCirculationSubsystem* Subsystem = World->GetSubsystem<UTowelCirculationSubsystem>())
		{
			Subsystem->UnregisterWorldTowel(this);
			if (bTokenCommitted && !bConsumed && !bRecoveryCommitted && Inventory->GetSnapshot().Count > 0)
			{
				Subsystem->RecoverInventory(Inventory);
				bRecoveryCommitted = true;
			}
		}
	}
	Super::EndPlay(EndPlayReason);
}

FPlayerInteractionQuery AWorldUsedTowelActor::QueryInteraction(const FPlayerInteractionContext& Context) const
{
	FPlayerInteractionQuery Query;
	if (!bTokenCommitted || bConsumed)
	{
		return Query;
	}
	Query.bVisible = true;
	Query.TargetName = LOCTEXT("UsedTowel", "사용한 수건");
	Query.ActionName = LOCTEXT("CollectUsedTowel", "수건 줍기");
	const ATowelBasketActor* Basket = Context.CarryComponent
		? Cast<ATowelBasketActor>(Context.CarryComponent->GetHeldObject())
		: nullptr;
	Query.bCanInteract = Basket && Basket->GetInventory()->CanAccept(ETowelState::Used);
	if (!Query.bCanInteract)
	{
		Query.FailureReason = LOCTEXT("UsedBasketRequired", "비어 있거나 사용한 수건용 바구니가 필요합니다.");
	}
	return Query;
}

void AWorldUsedTowelActor::CommitStagedToken()
{
	if (bTokenCommitted || bConsumed)
	{
		return;
	}
	bTokenCommitted = true;
	Inventory->SetRecoverContentsOnEndPlay(true);
	WorldMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
}

FPlayerInteractionResult AWorldUsedTowelActor::ExecuteInteraction(const FPlayerInteractionContext& Context)
{
	ATowelBasketActor* Basket = Context.CarryComponent
		? Cast<ATowelBasketActor>(Context.CarryComponent->GetHeldObject())
		: nullptr;
	UTowelTransferSubsystem* Transfer = GetWorld()->GetSubsystem<UTowelTransferSubsystem>();
	if (!Basket || !Transfer || !bTokenCommitted || bConsumed)
	{
		return FPlayerInteractionResult::Failed(LOCTEXT("CollectFailed", "수건을 주울 수 없습니다."));
	}
	FTowelTransferRequest Request;
	Request.Source = Inventory;
	Request.Destination = Basket->GetInventory();
	Request.RequestedCount = 1;
	Request.ExpectedSourceRevision = Inventory->GetSnapshot().Revision;
	Request.ExpectedDestinationRevision = Basket->GetInventory()->GetSnapshot().Revision;
	const FTowelTransferResult Result = Transfer->TryTransfer(Request);
	if (!Result.bSucceeded)
	{
		return FPlayerInteractionResult::Failed(LOCTEXT("CollectFailed", "수건을 주울 수 없습니다."));
	}
	bConsumed = true;
	WorldMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetLifeSpan(0.05f);
	return FPlayerInteractionResult::Succeeded();
}

#undef LOCTEXT_NAMESPACE
