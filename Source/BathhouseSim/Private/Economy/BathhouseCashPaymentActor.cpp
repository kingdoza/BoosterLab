#include "Economy/BathhouseCashPaymentActor.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Economy/BathhousePlayerState.h"
#include "Economy/PlayerWalletComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"

#define LOCTEXT_NAMESPACE "BathhouseCashPaymentActor"

ABathhouseCashPaymentActor::ABathhouseCashPaymentActor()
{
	PrimaryActorTick.bCanEverTick = false;
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
	WorldMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WorldMesh"));
	WorldMesh->SetupAttachment(SceneRoot);
	WorldMesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));
}

void ABathhouseCashPaymentActor::BeginPlay()
{
	Super::BeginPlay();
	OnCashAvailable();
}

FPlayerInteractionQuery ABathhouseCashPaymentActor::QueryInteraction(const FPlayerInteractionContext& Context) const
{
	FPlayerInteractionQuery Query;
	Query.bVisible = true;
	Query.TargetName = LOCTEXT("CashTarget", "이용 요금");
	Query.ActionName = LOCTEXT("ClaimCash", "현금 받기");

	if (bClaimed)
	{
		Query.FailureReason = LOCTEXT("AlreadyClaimed", "이미 받은 현금입니다.");
		return Query;
	}

	const UPlayerWalletComponent* Wallet = ResolveWallet(Context);
	Query.bCanInteract = Wallet && Wallet->CanAddMoney(PaymentAmount);
	if (!Wallet)
	{
		Query.FailureReason = LOCTEXT("MissingWallet", "플레이어 지갑을 찾을 수 없습니다.");
	}
	else if (!Query.bCanInteract)
	{
		Query.FailureReason = LOCTEXT("WalletOverflow", "지갑에 금액을 추가할 수 없습니다.");
	}
	return Query;
}

FPlayerInteractionResult ABathhouseCashPaymentActor::ExecuteInteraction(const FPlayerInteractionContext& Context)
{
	const FPlayerInteractionQuery Query = QueryInteraction(Context);
	UPlayerWalletComponent* Wallet = ResolveWallet(Context);
	if (!Query.bCanInteract || !Wallet || bClaimed)
	{
		return FPlayerInteractionResult::Failed(Query.FailureReason);
	}

	// Commit the one-shot guard before mutating the wallet. TryAddMoney broadcasts
	// synchronously, so listeners must observe this payment as unavailable while
	// that broadcast is in progress.
	bClaimed = true;
	if (!Wallet->TryAddMoney(PaymentAmount))
	{
		bClaimed = false;
		return FPlayerInteractionResult::Failed(LOCTEXT("PaymentFailed", "현금을 받을 수 없습니다."));
	}

	SetActorEnableCollision(false);
	SetActorHiddenInGame(true);
	OnCashClaimed.Broadcast(this);
	if (GetWorld())
	{
		Destroy();
	}
	return FPlayerInteractionResult::Succeeded();
}

void ABathhouseCashPaymentActor::ConfigurePaymentAmount(const int32 InPaymentAmount)
{
	if (!HasActorBegunPlay() && InPaymentAmount > 0)
	{
		PaymentAmount = InPaymentAmount;
	}
}

UPlayerWalletComponent* ABathhouseCashPaymentActor::ResolveWallet(const FPlayerInteractionContext& Context) const
{
	const APawn* Pawn = Cast<APawn>(Context.Interactor);
	if (!Pawn)
	{
		if (const AController* Controller = Cast<AController>(Context.Interactor))
		{
			Pawn = Controller->GetPawn();
		}
	}
	const ABathhousePlayerState* PlayerState = Pawn ? Pawn->GetPlayerState<ABathhousePlayerState>() : nullptr;
	return PlayerState ? PlayerState->GetWallet() : nullptr;
}

#undef LOCTEXT_NAMESPACE
