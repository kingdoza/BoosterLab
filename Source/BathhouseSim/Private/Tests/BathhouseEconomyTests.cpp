#include "Tests/BathhouseEconomyTestProbe.h"

#include "Economy/BathhouseCashPaymentActor.h"
#include "Economy/BathhousePlayerState.h"
#include "Economy/PlayerWalletComponent.h"
#include "GameFramework/Pawn.h"

void UBathhouseCashReentryTestProbe::Initialize(
	ABathhouseCashPaymentActor* InCashActor,
	const FPlayerInteractionContext& InContext)
{
	CashActor = InCashActor;
	InteractionContext = InContext;
}

void UBathhouseCashReentryTestProbe::BindToWallet(UPlayerWalletComponent* InWallet)
{
	UnbindFromWallet();
	Wallet = InWallet;
	if (Wallet)
	{
		Wallet->OnMoneyChanged.AddDynamic(this, &UBathhouseCashReentryTestProbe::HandleMoneyChanged);
	}
}

void UBathhouseCashReentryTestProbe::UnbindFromWallet()
{
	if (Wallet)
	{
		Wallet->OnMoneyChanged.RemoveDynamic(this, &UBathhouseCashReentryTestProbe::HandleMoneyChanged);
	}
	Wallet = nullptr;
}

void UBathhouseCashReentryTestProbe::HandleMoneyChanged(const int32 PreviousMoney, const int32 CurrentMoney)
{
	if (bDidAttemptReentry || !CashActor)
	{
		return;
	}
	bDidAttemptReentry = true;
	++ReentryAttemptCount;
	ReentryResult = CashActor->ExecuteInteraction(InteractionContext);
}

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBathhouseWalletIdempotenceTest,
	"BathhouseSim.Economy.WalletValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBathhouseWalletIdempotenceTest::RunTest(const FString& Parameters)
{
	UPlayerWalletComponent* Wallet = NewObject<UPlayerWalletComponent>();
	TestNotNull(TEXT("Wallet component is created"), Wallet);
	if (!Wallet)
	{
		return false;
	}

	TestFalse(TEXT("Zero is rejected"), Wallet->TryAddMoney(0));
	TestFalse(TEXT("Negative values are rejected"), Wallet->TryAddMoney(-1));
	TestTrue(TEXT("Positive payment succeeds"), Wallet->TryAddMoney(10000));
	TestEqual(TEXT("Payment is applied exactly once per successful call"), Wallet->GetCurrentMoney(), 10000);
	TestFalse(TEXT("Overflow is rejected"), Wallet->TryAddMoney(MAX_int32));
	TestEqual(TEXT("Rejected overflow leaves the balance unchanged"), Wallet->GetCurrentMoney(), 10000);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBathhouseCashReentrancyTest,
	"BathhouseSim.Economy.CashReentrancy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBathhouseCashReentrancyTest::RunTest(const FString& Parameters)
{
	ABathhousePlayerState* PlayerState = NewObject<ABathhousePlayerState>();
	APawn* Pawn = NewObject<APawn>();
	ABathhouseCashPaymentActor* CashActor = NewObject<ABathhouseCashPaymentActor>();
	UBathhouseCashReentryTestProbe* Probe = NewObject<UBathhouseCashReentryTestProbe>();
	TestNotNull(TEXT("Player state is created"), PlayerState);
	TestNotNull(TEXT("Pawn is created"), Pawn);
	TestNotNull(TEXT("Cash actor is created"), CashActor);
	TestNotNull(TEXT("Reentry probe is created"), Probe);
	if (!PlayerState || !Pawn || !CashActor || !Probe)
	{
		return false;
	}

	Pawn->SetPlayerState(PlayerState);
	UPlayerWalletComponent* Wallet = PlayerState->GetWallet();
	TestNotNull(TEXT("Player wallet is available"), Wallet);
	if (!Wallet)
	{
		return false;
	}

	FPlayerInteractionContext Context;
	Context.Interactor = Pawn;
	Context.HitActor = CashActor;
	Probe->Initialize(CashActor, Context);
	Probe->BindToWallet(Wallet);

	const FPlayerInteractionResult Result = CashActor->ExecuteInteraction(Context);
	TestTrue(TEXT("The original cash claim succeeds"), Result.bSucceeded);
	TestEqual(TEXT("The money-change listener attempts exactly one reentry"), Probe->GetReentryAttemptCount(), 1);
	TestFalse(TEXT("The reentrant cash claim is rejected"), Probe->GetReentryResult().bSucceeded);
	TestEqual(TEXT("One cash actor adds exactly one payment"), Wallet->GetCurrentMoney(), 10000);
	TestTrue(TEXT("The cash actor remains committed as claimed"), CashActor->IsClaimed());

	Probe->UnbindFromWallet();
	return true;
}

#endif
