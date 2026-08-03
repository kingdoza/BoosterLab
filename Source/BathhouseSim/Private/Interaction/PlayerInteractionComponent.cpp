#include "Interaction/PlayerInteractionComponent.h"

#include "Camera/CameraComponent.h"
#include "GameFramework/Pawn.h"
#include "Interaction/PlayerCarryComponent.h"
#include "Interaction/PlayerInteractable.h"

UPlayerInteractionComponent::UPlayerInteractionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
}

void UPlayerInteractionComponent::BeginPlay()
{
	Super::BeginPlay();
	RefreshInteractionQuery();
}

void UPlayerInteractionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearInteractionQuery();
	OnInteractionQueryChanged.Clear();
	OnInteractionAttemptFinishedNative.Clear();
	Camera = nullptr;
	CarryComponent = nullptr;
	Super::EndPlay(EndPlayReason);
}

void UPlayerInteractionComponent::TickComponent(
	const float DeltaTime,
	const ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (OwnerPawn && OwnerPawn->IsLocallyControlled())
	{
		RefreshInteractionQuery();
	}
	else
	{
		ClearInteractionQuery();
	}
}

void UPlayerInteractionComponent::Configure(UCameraComponent* InCamera, UPlayerCarryComponent* InCarryComponent)
{
	Camera = InCamera;
	CarryComponent = InCarryComponent;
	RefreshInteractionQuery();
}

FPlayerInteractionResult UPlayerInteractionComponent::TryInteract()
{
	FPlayerInteractionContext Context;
	IPlayerInteractable* Interactable = nullptr;
	UObject* TargetObject = nullptr;
	if (!BuildInteraction(Context, Interactable, TargetObject) || !Interactable)
	{
		RefreshInteractionQuery();
		return FinishInteractionAttempt(
			FPlayerInteractionResult::Failed(NSLOCTEXT("BathhouseInteraction", "NoTarget", "상호작용 대상이 없습니다.")));
	}

	const FPlayerInteractionQuery Query = Interactable->QueryInteraction(Context);
	CommitQuery(TargetObject, Query);
	if (!Query.bVisible || !Query.bCanInteract)
	{
		return FinishInteractionAttempt(FPlayerInteractionResult::Failed(Query.FailureReason));
	}

	const FPlayerInteractionResult Result = Interactable->ExecuteInteraction(Context);
	RefreshInteractionQuery();
	return FinishInteractionAttempt(Result);
}

void UPlayerInteractionComponent::RefreshInteractionQuery()
{
	FPlayerInteractionContext Context;
	IPlayerInteractable* Interactable = nullptr;
	UObject* TargetObject = nullptr;
	if (!BuildInteraction(Context, Interactable, TargetObject) || !Interactable)
	{
		ClearInteractionQuery();
		return;
	}

	CommitQuery(TargetObject, Interactable->QueryInteraction(Context));
}

void UPlayerInteractionComponent::ClearInteractionQuery()
{
	CommitQuery(nullptr, FPlayerInteractionQuery());
}

bool UPlayerInteractionComponent::BuildInteraction(
	FPlayerInteractionContext& OutContext,
	IPlayerInteractable*& OutInteractable,
	UObject*& OutTargetObject) const
{
	OutInteractable = nullptr;
	OutTargetObject = nullptr;
	if (!Camera || !GetWorld() || !GetOwner())
	{
		return false;
	}

	const FVector Start = Camera->GetComponentLocation();
	const FVector End = Start + Camera->GetForwardVector() * TraceDistance;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(PlayerInteraction), true, GetOwner());
	FHitResult Hit;
	if (!GetWorld()->LineTraceSingleByChannel(Hit, Start, End, TraceChannel, QueryParams))
	{
		return false;
	}

	UObject* Candidate = Hit.GetComponent();
	if (!Candidate || !Candidate->GetClass()->ImplementsInterface(UPlayerInteractable::StaticClass()))
	{
		Candidate = Hit.GetActor();
	}
	if (!Candidate || !Candidate->GetClass()->ImplementsInterface(UPlayerInteractable::StaticClass()))
	{
		return false;
	}

	OutInteractable = Cast<IPlayerInteractable>(Candidate);
	OutTargetObject = Candidate;
	OutContext.Interactor = GetOwner();
	OutContext.CarryComponent = CarryComponent;
	OutContext.HitActor = Hit.GetActor();
	OutContext.HitComponent = Hit.GetComponent();
	OutContext.HitResult = Hit;
	return OutInteractable != nullptr;
}

FPlayerInteractionResult UPlayerInteractionComponent::FinishInteractionAttempt(const FPlayerInteractionResult& Result)
{
	OnInteractionAttemptFinishedNative.Broadcast(Result);
	return Result;
}

void UPlayerInteractionComponent::CommitQuery(UObject* TargetObject, const FPlayerInteractionQuery& NewQuery)
{
	if (CurrentTarget == TargetObject && CurrentQuery.Equals(NewQuery))
	{
		return;
	}

	CurrentTarget = TargetObject;
	CurrentQuery = NewQuery;
	OnInteractionQueryChanged.Broadcast(CurrentQuery);
}
