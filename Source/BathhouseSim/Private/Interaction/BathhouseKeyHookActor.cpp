#include "Interaction/BathhouseKeyHookActor.h"

#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Facility/BathhouseFacilitySubsystem.h"
#include "Interaction/BathhouseKeyActor.h"
#include "Interaction/PlayerCarryComponent.h"

#define LOCTEXT_NAMESPACE "BathhouseKeyHookActor"

ABathhouseKeyHookActor::ABathhouseKeyHookActor()
{
	PrimaryActorTick.bCanEverTick = false;
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
	InteractionCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractionCollision"));
	InteractionCollision->SetupAttachment(SceneRoot);
	InteractionCollision->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	KeyAnchor = CreateDefaultSubobject<USceneComponent>(TEXT("KeyAnchor"));
	KeyAnchor->SetupAttachment(SceneRoot);
}

void ABathhouseKeyHookActor::BeginPlay()
{
	Super::BeginPlay();
	if (UBathhouseFacilitySubsystem* Subsystem = GetWorld()->GetSubsystem<UBathhouseFacilitySubsystem>())
	{
		Subsystem->RegisterKeyHook(this, KeyNumber);
	}
	if (KeyActor)
	{
		KeyActor->InitializeAtHook(this);
	}
}

void ABathhouseKeyHookActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UBathhouseFacilitySubsystem* Subsystem = GetWorld() ? GetWorld()->GetSubsystem<UBathhouseFacilitySubsystem>() : nullptr)
	{
		Subsystem->UnregisterKeyHook(this, KeyNumber);
	}
	Super::EndPlay(EndPlayReason);
}

FPlayerInteractionQuery ABathhouseKeyHookActor::QueryInteraction(const FPlayerInteractionContext& Context) const
{
	FPlayerInteractionQuery Query;
	Query.bVisible = true;
	Query.TargetName = FText::Format(LOCTEXT("HookTarget", "{0}번 키 걸이"), FText::AsNumber(KeyNumber));

	FText TopologyFailure;
	if (!IsNumberTopologyValid(&TopologyFailure))
	{
		Query.FailureReason = TopologyFailure;
		return Query;
	}
	if (!KeyActor || !Context.CarryComponent)
	{
		Query.FailureReason = LOCTEXT("MissingKey", "연결된 키가 없습니다.");
		return Query;
	}

	ABathhouseKeyActor* HeldKey = Context.CarryComponent->GetHeldKey();
	if (HeldKey)
	{
		Query.ActionName = LOCTEXT("ReturnKey", "키 반환하기");
		Query.bCanInteract = HeldKey == KeyActor && KeyActor->GetKeyState() == EBathhouseKeyState::HeldByPlayer;
		if (!Query.bCanInteract)
		{
			Query.FailureReason = LOCTEXT("WrongKey", "이 걸이에 반환할 키가 아닙니다.");
		}
		return Query;
	}

	Query.ActionName = LOCTEXT("TakeKey", "키 가져가기");
	Query.bCanInteract = KeyActor->GetKeyState() == EBathhouseKeyState::AtHook;
	if (!Query.bCanInteract)
	{
		Query.FailureReason = LOCTEXT("KeyUnavailable", "키가 현재 걸이에 없습니다.");
	}
	return Query;
}

FPlayerInteractionResult ABathhouseKeyHookActor::ExecuteInteraction(const FPlayerInteractionContext& Context)
{
	const FPlayerInteractionQuery Query = QueryInteraction(Context);
	if (!Query.bCanInteract || !KeyActor || !Context.CarryComponent)
	{
		return FPlayerInteractionResult::Failed(Query.FailureReason);
	}

	const bool bSucceeded = Context.CarryComponent->GetHeldKey()
		? KeyActor->TryReturnToHook(*Context.CarryComponent, *this)
		: KeyActor->TryTakeFromHook(*Context.CarryComponent, *this);
	return bSucceeded
		? FPlayerInteractionResult::Succeeded()
		: FPlayerInteractionResult::Failed(LOCTEXT("TransactionFailed", "키 상태가 변경되어 상호작용에 실패했습니다."));
}

bool ABathhouseKeyHookActor::IsNumberTopologyValid(FText* OutFailureReason) const
{
	if (!KeyActor || KeyActor->GetKeyNumber() != KeyNumber)
	{
		if (OutFailureReason)
		{
			*OutFailureReason = LOCTEXT("InvalidKeyLink", "키 걸이와 키 번호 연결이 올바르지 않습니다.");
		}
		return false;
	}

	const UBathhouseFacilitySubsystem* Subsystem = GetWorld() ? GetWorld()->GetSubsystem<UBathhouseFacilitySubsystem>() : nullptr;
	if (!Subsystem)
	{
		if (OutFailureReason)
		{
			*OutFailureReason = LOCTEXT("MissingSubsystem", "시설 정보를 확인할 수 없습니다.");
		}
		return false;
	}
	return Subsystem->ValidateKeyNumber(KeyNumber, this, OutFailureReason);
}

#undef LOCTEXT_NAMESPACE
