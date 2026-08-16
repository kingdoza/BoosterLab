#if WITH_DEV_AUTOMATION_TESTS

#include "Tests/BathhouseCleaningTowelTestProbe.h"

#include "Blueprint/UserWidget.h"
#include "Camera/CameraComponent.h"
#include "Character/FirstPersonCharacter.h"
#include "Character/FirstPersonMovementComponent.h"
#include "Cleaning/WaterStainActor.h"
#include "Cleaning/WetMopActor.h"
#include "Components/Button.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextBlock.h"
#include "Components/WidgetComponent.h"
#include "Components/WidgetInteractionComponent.h"
#include "Computer/BathhouseComputerActor.h"
#include "Computer/PlayerComputerUseComponent.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Interaction/BathhouseKeyActor.h"
#include "Interaction/PlayerCarryComponent.h"
#include "Interaction/PlayerInteractionComponent.h"
#include "Misc/AutomationTest.h"
#include "SceneUtils.h"
#include "Towel/TowelBasketActor.h"
#include "UI/ComputerSampleScreenWidget.h"

namespace
{
class FScopedComputerAutomationWorld
{
public:
	explicit FScopedComputerAutomationWorld(const TCHAR* BaseName)
	{
		if (!GEngine)
		{
			return;
		}
		const FName WorldName = MakeUniqueObjectName(nullptr, UWorld::StaticClass(), BaseName);
		FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
		World = UWorld::CreateWorld(EWorldType::Game, false, WorldName, GetTransientPackage());
		if (!World)
		{
			GEngine->DestroyWorldContext(World);
			return;
		}
		World->AddToRoot();
		WorldContext.SetCurrentWorld(World);
		World->InitializeActorsForPlay(FURL());
	}

	~FScopedComputerAutomationWorld()
	{
		if (World && GEngine)
		{
			World->DestroyWorld(false);
			GEngine->DestroyWorldContext(World);
			World->RemoveFromRoot();
		}
	}

	UWorld* Get() const { return World; }

private:
	UWorld* World = nullptr;
};

void BeginActorForComputerTest(AActor* Actor)
{
	if (Actor && !Actor->HasActorBegunPlay())
	{
		Actor->DispatchBeginPlay();
	}
}

void ConfigureCarryMesh(AActor* Actor, UStaticMesh* Mesh)
{
	if (UStaticMeshComponent* Primitive = Actor ? Actor->FindComponentByClass<UStaticMeshComponent>() : nullptr)
	{
		Primitive->SetStaticMesh(Mesh);
		Primitive->SetWorldScale3D(FVector(0.2f));
		Primitive->UpdateBounds();
	}
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBathhouseComputerSessionTest,
	"BathhouseSim.Computer.FocusSessionSuppressionAndSampleScreen",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBathhouseComputerSessionTest::RunTest(const FString& Parameters)
{
	FScopedComputerAutomationWorld TestWorld(TEXT("ComputerAutomationWorld"));
	UWorld* World = TestWorld.Get();
	if (!World)
	{
		AddError(TEXT("Failed to create the computer automation world."));
		return false;
	}

	UStaticMesh* CubeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	TestNotNull(TEXT("The engine cube mesh is available for interaction traces"), CubeMesh);

	APlayerController* PlayerController = World->SpawnActor<APlayerController>();
	AFirstPersonCharacter* Character = World->SpawnActor<AFirstPersonCharacter>();
	BeginActorForComputerTest(PlayerController);
	BeginActorForComputerTest(Character);
	ULocalPlayer* LocalPlayer = NewObject<ULocalPlayer>(GEngine, TEXT("ComputerTestLocalPlayer"));
	PlayerController->SetPlayer(LocalPlayer);
	PlayerController->Possess(Character);
	PlayerController->SetViewTarget(Character);
	TestTrue(TEXT("The automation character is locally controlled"), Character->IsLocallyControlled());

	ABathhouseComputerActor* Computer = World->SpawnActor<ABathhouseComputerActor>();
	Computer->ComputerMesh->SetStaticMesh(CubeMesh);
	Computer->ComputerMesh->SetWorldScale3D(FVector(0.5f));
	Computer->FocusBlendInSeconds = 0.0f;
	Computer->FocusBlendOutSeconds = 0.0f;
	UUserWidget* ScreenInstance = NewObject<UComputerSampleScreenWidget>(PlayerController, TEXT("ComputerScreenInstance"));
	Computer->ScreenWidget->SetWidget(ScreenInstance);
	BeginActorForComputerTest(Computer);

	TestEqual(TEXT("ComputerMesh keeps its reflected subobject name"), Computer->ComputerMesh->GetFName(), FName(TEXT("ComputerMesh")));
	TestEqual(TEXT("ScreenWidget keeps its reflected subobject name"), Computer->ScreenWidget->GetFName(), FName(TEXT("ScreenWidget")));
	TestEqual(TEXT("FocusCamera keeps its reflected subobject name"), Computer->FocusCamera->GetFName(), FName(TEXT("FocusCamera")));
	TestTrue(TEXT("A real screen widget makes the computer ready"), Computer->IsScreenReady());

	FPlayerInteractionContext Context;
	Context.Interactor = Character;
	Context.CarryComponent = Character->GetPlayerCarry();
	FPlayerInteractionContext MissingCarryContext = Context;
	MissingCarryContext.CarryComponent = nullptr;
	const FPlayerInteractionQuery MissingCarryQuery = Computer->QueryInteraction(MissingCarryContext);
	TestFalse(TEXT("A missing carry context rejects computer use"), MissingCarryQuery.bCanInteract);
	TestEqual(TEXT("Missing carry reports the carry-specific failure"),
		MissingCarryQuery.FailureReason.ToString(), FString(TEXT("소지 상태를 확인할 수 없습니다.")));

	AActor* MissingUseOwner = World->SpawnActor<AActor>();
	UPlayerCarryComponent* MissingUseCarry = NewObject<UPlayerCarryComponent>(MissingUseOwner, TEXT("MissingUseCarry"));
	MissingUseOwner->AddInstanceComponent(MissingUseCarry);
	MissingUseCarry->RegisterComponent();
	FPlayerInteractionContext MissingUseContext;
	MissingUseContext.Interactor = MissingUseOwner;
	MissingUseContext.CarryComponent = MissingUseCarry;
	TestFalse(TEXT("A missing player-computer component rejects computer use"),
		Computer->QueryInteraction(MissingUseContext).bCanInteract);

	ABathhouseKeyActor* Key = World->SpawnActor<ABathhouseKeyActor>();
	BeginActorForComputerTest(Key);
	TestTrue(TEXT("The key can occupy the character hand for the gate test"), Character->GetPlayerCarry()->CommitTakeKey(Key));
	const FPlayerInteractionQuery KeyHeldQuery = Computer->QueryInteraction(Context);
	TestFalse(TEXT("A held key rejects computer use"), KeyHeldQuery.bCanInteract);
	TestEqual(TEXT("Every held object uses the exact empty-hand failure"),
		KeyHeldQuery.FailureReason.ToString(), FString(TEXT("손에 든 물건을 내려놓아야 합니다")));
	Character->GetPlayerCarry()->CommitReleaseKey(Key);

	AWetMopActor* Mop = World->SpawnActor<AWetMopActor>();
	ATowelBasketActor* Basket = World->SpawnActor<ATowelBasketActor>();
	BeginActorForComputerTest(Mop);
	BeginActorForComputerTest(Basket);
	ConfigureCarryMesh(Mop, CubeMesh);
	ConfigureCarryMesh(Basket, CubeMesh);
	FText CarryFailure;
	TestTrue(TEXT("The wet mop can occupy the hand for the gate test"),
		Character->GetPlayerCarry()->TryTakePhysicalObject(Mop, CarryFailure));
	TestFalse(TEXT("A held wet mop rejects computer use"), Computer->QueryInteraction(Context).bCanInteract);
	Character->GetPlayerCarry()->CommitReleasePhysicalObject(Mop);
	TestTrue(TEXT("The towel basket can occupy the hand for the gate test"),
		Character->GetPlayerCarry()->TryTakePhysicalObject(Basket, CarryFailure));
	TestFalse(TEXT("A held towel basket rejects computer use"), Computer->QueryInteraction(Context).bCanInteract);
	Character->GetPlayerCarry()->CommitReleasePhysicalObject(Basket);
	Key->SetActorEnableCollision(false);
	Mop->SetActorEnableCollision(false);
	Basket->SetActorEnableCollision(false);
	Computer->ComputerMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Computer->ComputerMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	Computer->ComputerMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	Computer->SetActorLocation(
		Character->GetFirstPersonCamera()->GetComponentLocation()
		+ Character->GetFirstPersonCamera()->GetForwardVector() * 150.0f);
	Computer->ComputerMesh->UpdateBounds();
	Computer->ComputerMesh->RecreatePhysicsState();
	World->UpdateWorldComponents(true, false);

	ABathhouseComputerActor* MissingScreenComputer = World->SpawnActor<ABathhouseComputerActor>();
	BeginActorForComputerTest(MissingScreenComputer);
	TestFalse(TEXT("A computer without a user widget is unavailable"),
		MissingScreenComputer->QueryInteraction(Context).bCanInteract);
	MissingScreenComputer->SetActorEnableCollision(false);

	UPlayerComputerUseComponent* OtherUser = NewObject<UPlayerComputerUseComponent>(MissingUseOwner, TEXT("OtherComputerUser"));
	MissingUseOwner->AddInstanceComponent(OtherUser);
	OtherUser->RegisterComponent();
	TestTrue(TEXT("The computer accepts the first reservation"), Computer->TryReserveFor(OtherUser));
	TestFalse(TEXT("An occupied computer rejects another player"), Computer->QueryInteraction(Context).bCanInteract);
	Computer->ReleaseReservation(Character->GetPlayerComputerUse());
	TestTrue(TEXT("A different user's release cannot clear the reservation"), Computer->IsReservedBy(OtherUser));
	Computer->ReleaseReservation(OtherUser);
	Computer->ReleaseReservation(OtherUser);
	TestFalse(TEXT("Expected release is idempotent"), Computer->IsReservedBy(OtherUser));

	AActor* InvalidBeginOwner = World->SpawnActor<AActor>();
	UPlayerCarryComponent* InvalidBeginCarry = NewObject<UPlayerCarryComponent>(InvalidBeginOwner, TEXT("InvalidBeginCarry"));
	UPlayerComputerUseComponent* InvalidOwnerUse = NewObject<UPlayerComputerUseComponent>(InvalidBeginOwner, TEXT("InvalidOwnerUse"));
	InvalidBeginOwner->AddInstanceComponent(InvalidBeginCarry);
	InvalidBeginOwner->AddInstanceComponent(InvalidOwnerUse);
	InvalidBeginCarry->RegisterComponent();
	InvalidOwnerUse->RegisterComponent();
	FPlayerInteractionContext InvalidBeginContext;
	InvalidBeginContext.Interactor = InvalidBeginOwner;
	InvalidBeginContext.CarryComponent = InvalidBeginCarry;
	TestFalse(TEXT("Session start fails for a non-pawn owner"), Computer->ExecuteInteraction(InvalidBeginContext).bSucceeded);
	TestFalse(TEXT("Failed session start rolls its reservation back"), Computer->IsReservedBy(InvalidOwnerUse));

	UPlayerInteractionComponent* Interaction = Character->GetPlayerInteraction();
	UPlayerComputerUseComponent* ComputerUse = Character->GetPlayerComputerUse();
	UFirstPersonMovementComponent* Movement = Character->GetFirstPersonMovement();
	const EMovementMode MovementModeBeforeFocus = Movement->MovementMode;
	TestTrue(TEXT("An empty hand can query the available computer"), Computer->QueryInteraction(Context).bCanInteract);
	FHitResult DirectTraceHit;
	FCollisionQueryParams DirectTraceParams(SCENE_QUERY_STAT(ComputerAutomationTrace), true, Character);
	const FVector DirectTraceStart = Character->GetFirstPersonCamera()->GetComponentLocation();
	const bool bDirectTraceHit = World->LineTraceSingleByChannel(
		DirectTraceHit,
		DirectTraceStart,
		DirectTraceStart + Character->GetFirstPersonCamera()->GetForwardVector() * 300.0f,
		ECC_Visibility,
		DirectTraceParams);
	TestTrue(TEXT("The fixture visibility trace reaches the computer mesh"),
		bDirectTraceHit && DirectTraceHit.GetActor() == Computer);
	Interaction->RefreshInteractionQuery();
	TestTrue(TEXT("The first-person trace sees the computer"), Interaction->GetCurrentInteractionQuery().bCanInteract);

	int32 AttemptBroadcastCount = 0;
	IConsoleVariable* AntiAliasingMethod = IConsoleManager::Get().FindConsoleVariable(TEXT("r.AntiAliasingMethod"));
	TestNotNull(TEXT("The renderer exposes its anti-aliasing method CVar"), AntiAliasingMethod);
	const int32 AntiAliasingMethodBeforeFocus = AntiAliasingMethod ? AntiAliasingMethod->GetInt() : INDEX_NONE;
	Interaction->OnInteractionAttemptFinishedNative.AddLambda(
		[&AttemptBroadcastCount](const FPlayerInteractionResult&) { ++AttemptBroadcastCount; });
	Character->InteractStartInput();
	TestTrue(TEXT("Entry E Started owns the press after opening the computer"), Character->bComputerOwnsInteractPress);
	TestTrue(TEXT("Zero blend enters Active immediately"), ComputerUse->IsActive());
	TestTrue(TEXT("The active session suppresses world interaction"), Interaction->IsInteractionSuppressed());
	TestEqual(TEXT("The active session disables movement"), Movement->MovementMode.GetValue(), MOVE_None);
	TestTrue(TEXT("The controller view target becomes the computer"), PlayerController->GetViewTarget() == Computer);
	TestTrue(TEXT("The active session enables widget hit testing"), Character->ComputerWidgetInteraction->bEnableHitTesting);
	TestTrue(TEXT("The active session displays the mouse cursor"), PlayerController->bShowMouseCursor);
	if (AntiAliasingMethod)
	{
		TestEqual(TEXT("The active computer focus uses FXAA"),
			AntiAliasingMethod->GetInt(), static_cast<int32>(AAM_FXAA));
	}
	TestEqual(TEXT("The screen widget instance is retained on focus-in"),
		Computer->ScreenWidget->GetUserWidgetObject(), ScreenInstance);
	TestFalse(TEXT("A duplicate begin request is rejected"), Computer->ExecuteInteraction(Context).bSucceeded);
	TestTrue(TEXT("A rejected duplicate begin keeps the existing reservation"),
		Computer->IsReservedBy(ComputerUse));

	Character->InteractEndInput();
	TestFalse(TEXT("Entry E Completed is consumed without closing the computer"), Character->bComputerOwnsInteractPress);
	TestTrue(TEXT("Entry release leaves the session active"), ComputerUse->IsActive());

	const FRotator ControlRotationBefore = PlayerController->GetControlRotation();
	Character->DoMove(1.0f, 1.0f);
	Character->DoLook(5.0f, 5.0f);
	Character->DoJumpStart();
	Character->SprintStartInput();
	Character->SecondaryInteractInput();
	Character->DropCarryInput();
	TestTrue(TEXT("Capture blocks movement input"), Character->GetPendingMovementInputVector().IsNearlyZero());
	TestTrue(TEXT("Capture blocks look input"), PlayerController->GetControlRotation().Equals(ControlRotationBefore));
	TestFalse(TEXT("Capture blocks jump start"), Character->IsJumpProvidingForce());
	TestFalse(TEXT("Capture blocks sprint start"), Movement->IsSprinting());
	TestEqual(TEXT("Capture blocks F/G before attempt broadcasting"), AttemptBroadcastCount, 1);

	TestTrue(TEXT("Active pointer press begins exactly once"), ComputerUse->PressPointer());
	TestFalse(TEXT("A repeated pointer press is rejected"), ComputerUse->PressPointer());
	TestTrue(TEXT("Pointer state records the owned press"), ComputerUse->bPointerDown);
	Character->InteractStartInput();
	TestTrue(TEXT("Exit E Started owns its press"), Character->bComputerOwnsInteractPress);
	TestFalse(TEXT("Zero blend exits immediately"), ComputerUse->IsCapturingInput());
	TestFalse(TEXT("Focus-out forces a pending pointer release"), ComputerUse->bPointerDown);
	TestFalse(TEXT("Focus-out disables widget hit testing"), Character->ComputerWidgetInteraction->bEnableHitTesting);
	TestTrue(TEXT("Focus-out restores the previous view target"), PlayerController->GetViewTarget() == Character);
	TestEqual(TEXT("Focus-out restores the movement mode"),
		Movement->MovementMode.GetValue(), MovementModeBeforeFocus);
	TestFalse(TEXT("Focus-out restores interaction availability"), Interaction->IsInteractionSuppressed());
	if (AntiAliasingMethod)
	{
		TestEqual(TEXT("Focus-out restores the previous anti-aliasing method"),
			AntiAliasingMethod->GetInt(), AntiAliasingMethodBeforeFocus);
	}
	TestTrue(TEXT("Focus-out releases the reservation"), !Computer->IsReservedBy(ComputerUse));
	TestEqual(TEXT("Focus-out does not recreate the screen widget"),
		Computer->ScreenWidget->GetUserWidgetObject(), ScreenInstance);
	Character->InteractEndInput();
	TestFalse(TEXT("Exit E Completed is consumed after the session has ended"), Character->bComputerOwnsInteractPress);

	Computer->FocusBlendInSeconds = 0.2f;
	Computer->FocusBlendOutSeconds = 0.2f;
	Interaction->RefreshInteractionQuery();
	Character->InteractStartInput();
	TestEqual(TEXT("A non-zero entry blend stays in FocusingIn"),
		ComputerUse->GetPhase(), EPlayerComputerUsePhase::FocusingIn);
	TestFalse(TEXT("Pointer input is rejected during FocusingIn"), ComputerUse->PressPointer());
	Character->InteractEndInput();
	Character->InteractStartInput();
	TestEqual(TEXT("E can reverse a session while FocusingIn"),
		ComputerUse->GetPhase(), EPlayerComputerUsePhase::FocusingOut);
	ComputerUse->RequestEndComputerUse();
	TestEqual(TEXT("Repeated end during FocusingOut is idempotent"),
		ComputerUse->GetPhase(), EPlayerComputerUsePhase::FocusingOut);
	ComputerUse->CompleteFocusOut();
	TestFalse(TEXT("Manual blend completion returns to Inactive"), ComputerUse->IsCapturingInput());
	Character->InteractEndInput();

	Computer->FocusBlendInSeconds = 0.0f;
	Computer->FocusBlendOutSeconds = 0.0f;
	Interaction->RefreshInteractionQuery();
	Character->InteractStartInput();
	TestTrue(TEXT("The session can be re-entered after cleanup"), ComputerUse->IsActive());
	PlayerController->UnPossess();
	ComputerUse->TickComponent(0.0f, LEVELTICK_All, nullptr);
	TestFalse(TEXT("Controller loss cleans the player session"), ComputerUse->IsCapturingInput());
	TestFalse(TEXT("Controller loss releases the reservation"), Computer->IsReservedBy(ComputerUse));
	TestFalse(TEXT("Controller loss restores interaction"), Interaction->IsInteractionSuppressed());
	Character->InteractEndInput();
	PlayerController->Possess(Character);
	PlayerController->SetViewTarget(Character);
	Interaction->RefreshInteractionQuery();
	Character->InteractStartInput();
	TestTrue(TEXT("The session can re-enter after controller recovery"), ComputerUse->IsActive());
	Computer->Destroy();
	TestFalse(TEXT("Computer EndPlay cleans the player session"), ComputerUse->IsCapturingInput());
	TestFalse(TEXT("Computer EndPlay restores interaction"), Interaction->IsInteractionSuppressed());
	TestTrue(TEXT("Computer EndPlay restores the player view"), PlayerController->GetViewTarget() == Character);

	UComputerSampleScreenWidget* SampleWidget = NewObject<UComputerSampleScreenWidget>();
	SampleWidget->TestButton = NewObject<UButton>(SampleWidget, TEXT("TestButton"));
	SampleWidget->ClickResultText = NewObject<UTextBlock>(SampleWidget, TEXT("ClickResultText"));
	SampleWidget->NativeConstruct();
	TestEqual(TEXT("The native sample screen starts with its prompt"),
		SampleWidget->ClickResultText->GetText().ToString(), FString(TEXT("버튼을 클릭하세요")));
	SampleWidget->TestButton->OnClicked.Broadcast();
	TestEqual(TEXT("The native button click updates its result"),
		SampleWidget->ClickResultText->GetText().ToString(), FString(TEXT("클릭 확인")));
	SampleWidget->NativeDestruct();
	SampleWidget->ClickResultText->SetText(FText::GetEmpty());
	SampleWidget->NativeConstruct();
	TestEqual(TEXT("Reconstructing the same widget instance preserves click state"),
		SampleWidget->ClickResultText->GetText().ToString(), FString(TEXT("클릭 확인")));
	SampleWidget->NativeDestruct();

	APawn* SuppressionPawn = World->SpawnActor<APawn>();
	UCameraComponent* SuppressionCamera = NewObject<UCameraComponent>(SuppressionPawn, TEXT("SuppressionCamera"));
	UPlayerCarryComponent* SuppressionCarry = NewObject<UPlayerCarryComponent>(SuppressionPawn, TEXT("SuppressionCarry"));
	UPlayerInteractionComponent* SuppressionInteraction = NewObject<UPlayerInteractionComponent>(SuppressionPawn, TEXT("SuppressionInteraction"));
	SuppressionPawn->SetRootComponent(SuppressionCamera);
	SuppressionPawn->AddInstanceComponent(SuppressionCamera);
	SuppressionPawn->AddInstanceComponent(SuppressionCarry);
	SuppressionPawn->AddInstanceComponent(SuppressionInteraction);
	SuppressionCamera->RegisterComponent();
	SuppressionCarry->RegisterComponent();
	SuppressionInteraction->RegisterComponent();
	SuppressionCarry->ConfigureHeldAnchor(SuppressionCamera);
	SuppressionInteraction->Configure(SuppressionCamera, SuppressionCarry);
	AWetMopActor* SuppressionMop = World->SpawnActor<AWetMopActor>();
	AWaterStainActor* SuppressionStain = World->SpawnActor<AWaterStainActor>();
	BeginActorForComputerTest(SuppressionMop);
	BeginActorForComputerTest(SuppressionStain);
	ConfigureCarryMesh(SuppressionMop, CubeMesh);
	TestTrue(TEXT("Suppression setup takes a wet mop"),
		SuppressionCarry->TryTakePhysicalObject(SuppressionMop, CarryFailure));
	FPlayerInteractionContext SuppressionContext;
	SuppressionContext.Interactor = SuppressionPawn;
	SuppressionContext.CarryComponent = SuppressionCarry;
	FText HoldFailure;
	TestTrue(TEXT("Suppression setup begins a cleaning hold"),
		SuppressionStain->BeginHoldInteraction(SuppressionContext, HoldFailure));
	SuppressionStain->UpdateHoldInteraction(SuppressionContext, 0.5f);
	SuppressionInteraction->ActiveHoldTarget = SuppressionStain;
	SuppressionInteraction->ActiveHoldContext = SuppressionContext;
	SuppressionInteraction->ActiveHoldProgress = SuppressionStain->GetCleaningProgress();
	SuppressionInteraction->bPrimaryInputHeld = true;
	SuppressionInteraction->CurrentTarget = SuppressionStain;
	SuppressionInteraction->CurrentQuery = SuppressionStain->QueryInteraction(SuppressionContext);
	UBathhouseCleaningCancelProbe* CancelProbe = NewObject<UBathhouseCleaningCancelProbe>();
	CancelProbe->Bind(SuppressionStain);
	UBathhouseInteractionQueryProbe* QueryProbe = NewObject<UBathhouseInteractionQueryProbe>();
	QueryProbe->Bind(SuppressionInteraction);
	int32 SuppressedAttemptCount = 0;
	SuppressionInteraction->OnInteractionAttemptFinishedNative.AddLambda(
		[&SuppressedAttemptCount](const FPlayerInteractionResult&) { ++SuppressedAttemptCount; });
	SuppressionInteraction->SetInteractionSuppressed(true);
	SuppressionInteraction->SetInteractionSuppressed(true);
	TestEqual(TEXT("Suppression cancels the active hold exactly once"), CancelProbe->CancelCount, 1);
	TestFalse(TEXT("Suppression clears the active hold"), SuppressionInteraction->IsPrimaryHoldActive());
	TestFalse(TEXT("Suppression commits one empty query"), SuppressionInteraction->GetCurrentInteractionQuery().bVisible);
	TestEqual(TEXT("Suppression broadcasts the empty query exactly once"), QueryProbe->BroadcastCount, 1);
	TestFalse(TEXT("The suppression query broadcast is empty"), QueryProbe->LastQuery.bVisible);
	SuppressionInteraction->BeginPrimaryInteraction();
	SuppressionInteraction->TryInteract();
	SuppressionInteraction->TrySecondaryInteract();
	SuppressionInteraction->TryDropCarry(FVector::ZeroVector, FVector::ForwardVector);
	SuppressionInteraction->EndPrimaryInteraction();
	TestEqual(TEXT("Suppressed direct attempts do not broadcast results"), SuppressedAttemptCount, 0);
	TestTrue(TEXT("Suppressed direct attempts do not mutate the held mop"),
		SuppressionCarry->GetHeldObject() == SuppressionMop);
	SuppressionInteraction->SetInteractionSuppressed(false);
	const int32 QueryBroadcastCountAfterRefresh = QueryProbe->BroadcastCount;
	SuppressionInteraction->SetInteractionSuppressed(false);
	TestFalse(TEXT("Suppression release is idempotent"), SuppressionInteraction->IsInteractionSuppressed());
	TestEqual(TEXT("Repeated suppression release does not refresh again"),
		QueryProbe->BroadcastCount, QueryBroadcastCountAfterRefresh);
	QueryProbe->Unbind();
	CancelProbe->Unbind();

	return true;
}

#endif
