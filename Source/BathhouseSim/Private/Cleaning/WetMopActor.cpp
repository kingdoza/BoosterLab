#include "Cleaning/WetMopActor.h"

#include "Cleaning/WaterStainActor.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Interaction/PlayerCarryComponent.h"
#include "Interaction/HeldEquipmentMotionComponent.h"
#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#define LOCTEXT_NAMESPACE "WetMopActor"

AWetMopActor::AWetMopActor()
{
	PrimaryActorTick.bCanEverTick = false;
	WorldMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WorldMesh"));
	SetRootComponent(WorldMesh);
	WorldMesh->SetCollisionProfileName(TEXT("PhysicsActor"));
}

void AWetMopActor::BeginPlay()
{
	Super::BeginPlay();
	InitialTransform = GetActorTransform();
	LastSafeTransform = InitialTransform;
}

void AWetMopActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	FHeldEquipmentUseContext UseContext;
	UseContext.User = Carrier ? Carrier->GetOwner() : nullptr;
	StopMopping(UseContext);
	if (Carrier)
	{
		Carrier->NotifyHeldActorEnding(this);
	}
	Carrier = nullptr;
	OnHeldPresentationChanged.Clear();
	OnMoppingStateChanged.Clear();
	Super::EndPlay(EndPlayReason);
}

void AWetMopActor::FellOutOfWorld(const UDamageType& DamageType)
{
	RecoverPhysicalCarryable(Carrier);
}

FPlayerInteractionQuery AWetMopActor::QueryInteraction(const FPlayerInteractionContext& Context) const
{
	FPlayerInteractionQuery Query;
	if (Carrier)
	{
		return Query;
	}
	Query.bVisible = true;
	Query.TargetName = LOCTEXT("MopTarget", "물걸레");
	Query.ActionName = LOCTEXT("TakeMop", "물걸레 들기");
	Query.bCanInteract = Context.CarryComponent && Context.CarryComponent->IsHandEmpty();
	if (!Query.bCanInteract)
	{
		Query.FailureReason = LOCTEXT("HandOccupied", "이미 다른 물건을 들고 있습니다.");
	}
	return Query;
}

FPlayerInteractionResult AWetMopActor::ExecuteInteraction(const FPlayerInteractionContext& Context)
{
	FText FailureReason;
	return Context.CarryComponent && Context.CarryComponent->TryTakePhysicalObject(this, FailureReason)
		? FPlayerInteractionResult::Succeeded()
		: FPlayerInteractionResult::Failed(FailureReason);
}

FText AWetMopActor::GetPhysicalCarryDisplayName() const
{
	return LOCTEXT("MopTarget", "물걸레");
}

FTransform AWetMopActor::GetHeldTransform() const
{
	FTransform Result = HeldTransform;
	Result.SetScale3D(FVector::OneVector);
	return Result;
}

bool AWetMopActor::CanBeTakenBy(const UPlayerCarryComponent& Carry, FText& OutFailureReason) const
{
	if (Carrier || !Carry.IsHandEmpty())
	{
		OutFailureReason = LOCTEXT("HandOccupied", "이미 다른 물건을 들고 있습니다.");
		return false;
	}
	return true;
}

bool AWetMopActor::HandleTakenBy(UPlayerCarryComponent& Carry, USceneComponent* HeldAnchor)
{
	if (Carrier)
	{
		return false;
	}
	LastSafeTransform = GetActorTransform();
	Carrier = &Carry;
	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	SetWorldPhysics(false);
	if (HeldAnchor)
	{
		AttachToComponent(HeldAnchor, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		ApplyHeldTransform();
	}
	OnHeldPresentationChanged.Broadcast(true);
	return true;
}

UPrimitiveComponent* AWetMopActor::GetPhysicalCarryPrimitive() const
{
	return WorldMesh;
}

void AWetMopActor::NotifyPhysicalDropCommitted(UPlayerCarryComponent& Carry)
{
	ensureMsgf(Carrier == &Carry, TEXT("Wet mop drop committed by a carry component that does not own it."));
	FHeldEquipmentUseContext UseContext;
	UseContext.User = Carry.GetOwner();
	StopMopping(UseContext);
	Carrier = nullptr;
	LastSafeTransform = GetActorTransform();
	OnHeldPresentationChanged.Broadcast(false);
}

FHeldEquipmentUseQuery AWetMopActor::QueryEquipmentUse(const FHeldEquipmentUseContext& Context) const
{
	FHeldEquipmentUseQuery Query;
	Query.DisplayName = LOCTEXT("MopTarget", "물걸레");
	Query.ActionName = LOCTEXT("MopUse", "물걸레질");
	Query.ActivationMode = EPlayerInteractionActivationMode::Hold;
	Query.bCanUse = Carrier == Context.CarryComponent
		&& Context.CarryComponent
		&& Context.CarryComponent->GetHeldObject() == this;
	if (!Query.bCanUse)
	{
		Query.FailureReason = LOCTEXT("MopNotHeld", "물걸레를 들고 있어야 합니다.");
		return Query;
	}
	if (const AWaterStainActor* Stain = ResolveFocusedStain(Context))
	{
		FText TargetFailure;
		float TargetProgress = 0.0f;
		Stain->QueryMopCleaning(Context.User, TargetFailure, TargetProgress);
		Query.Progress = TargetProgress;
		Query.FailureReason = TargetFailure;
	}
	else if (IsValid(ActiveStain))
	{
		Query.Progress = ActiveStain->GetCleaningProgress();
	}
	return Query;
}

FHeldEquipmentUseResult AWetMopActor::BeginEquipmentUse(const FHeldEquipmentUseContext& Context)
{
	const FHeldEquipmentUseQuery Query = QueryEquipmentUse(Context);
	if (!Query.bCanUse || !IsValid(Context.User))
	{
		return FHeldEquipmentUseResult::Failed(Query.FailureReason);
	}
	if (!bIsMopping)
	{
		bIsMopping = true;
		OnMoppingStateChanged.Broadcast(true);
	}
	if (Context.MotionComponent && GetRootComponent())
	{
		Context.MotionComponent->StartLoop(
			GetRootComponent(),
			MoppingPositionCurve,
			MoppingRotationCurve,
			FMath::Max(0.05f, MoppingMotionPeriodSeconds));
	}
	ChangeActiveStain(ResolveFocusedStain(Context), Context.User);
	return FHeldEquipmentUseResult::Succeeded();
}

FHeldEquipmentUseUpdate AWetMopActor::UpdateEquipmentUse(
	const FHeldEquipmentUseContext& Context,
	const float DeltaTime)
{
	FHeldEquipmentUseUpdate Update;
	if (!bIsMopping || Carrier != Context.CarryComponent
		|| !Context.CarryComponent || Context.CarryComponent->GetHeldObject() != this)
	{
		StopMopping(Context);
		Update.FailureReason = LOCTEXT("MoppingInvalidated", "물걸레 사용 상태가 유지되지 않았습니다.");
		return Update;
	}

	AWaterStainActor* FocusedStain = ResolveFocusedStain(Context);
	if (FocusedStain != ActiveStain)
	{
		ChangeActiveStain(FocusedStain, Context.User);
	}
	if (IsValid(ActiveStain))
	{
		Update = ActiveStain->UpdateMopCleaning(Context.User, DeltaTime);
		if (Update.State == EPlayerHoldInteractionState::Succeeded)
		{
			ActiveStain = nullptr;
			Update.State = EPlayerHoldInteractionState::Running;
			Update.Progress = 0.0f;
		}
		else if (Update.State == EPlayerHoldInteractionState::Failed)
		{
			ActiveStain = nullptr;
			Update.State = EPlayerHoldInteractionState::Running;
		}
		return Update;
	}
	Update.State = EPlayerHoldInteractionState::Running;
	Update.Progress = 0.0f;
	return Update;
}

FHeldEquipmentUseResult AWetMopActor::EndEquipmentUse(const FHeldEquipmentUseContext& Context)
{
	StopMopping(Context);
	return FHeldEquipmentUseResult::Succeeded();
}

void AWetMopActor::CancelEquipmentUse(const FHeldEquipmentUseContext& Context)
{
	StopMopping(Context);
}

void AWetMopActor::RecoverPhysicalCarryable(UPlayerCarryComponent* PreviousCarry)
{
	if (Carrier && PreviousCarry && Carrier != PreviousCarry)
	{
		return;
	}
	const bool bWasHeld = Carrier != nullptr;
	FHeldEquipmentUseContext UseContext;
	UseContext.User = Carrier ? Carrier->GetOwner() : nullptr;
	StopMopping(UseContext);
	if (Carrier)
	{
		Carrier->CommitReleasePhysicalObject(this);
	}
	Carrier = nullptr;
	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	SetWorldPhysics(false);
	SetActorTransform(LastSafeTransform.Equals(FTransform::Identity) ? InitialTransform : LastSafeTransform);
	SetWorldPhysics(true);
	if (bWasHeld)
	{
		OnHeldPresentationChanged.Broadcast(false);
	}
}

AWaterStainActor* AWetMopActor::ResolveFocusedStain(const FHeldEquipmentUseContext& Context) const
{
	return Cast<AWaterStainActor>(Context.FocusHit.GetActor());
}

void AWetMopActor::ChangeActiveStain(AWaterStainActor* NewStain, AActor* Cleaner)
{
	if (ActiveStain == NewStain)
	{
		return;
	}
	if (IsValid(ActiveStain))
	{
		ActiveStain->CancelMopCleaning(Cleaner);
	}
	ActiveStain = nullptr;
	if (IsValid(NewStain))
	{
		FText FailureReason;
		if (NewStain->BeginMopCleaning(Cleaner, FailureReason))
		{
			ActiveStain = NewStain;
		}
	}
}

void AWetMopActor::StopMopping(const FHeldEquipmentUseContext& Context)
{
	if (IsValid(ActiveStain))
	{
		ActiveStain->CancelMopCleaning(Context.User);
	}
	ActiveStain = nullptr;
	if (Context.MotionComponent)
	{
		Context.MotionComponent->StopMotion();
	}
	if (bIsMopping)
	{
		bIsMopping = false;
		OnMoppingStateChanged.Broadcast(false);
	}
}

void AWetMopActor::SetWorldPhysics(const bool bEnabled)
{
	if (WorldMesh)
	{
		if (bEnabled)
		{
			WorldMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			WorldMesh->SetSimulatePhysics(true);
		}
		else
		{
			WorldMesh->SetSimulatePhysics(false);
			WorldMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
	}
}

void AWetMopActor::ApplyHeldTransform()
{
	const FTransform LocalHeldTransform = GetHeldTransform();
	if (USceneComponent* Root = GetRootComponent())
	{
		Root->SetRelativeLocationAndRotation(
			LocalHeldTransform.GetLocation(),
			LocalHeldTransform.GetRotation());
	}
}

#if WITH_EDITOR
EDataValidationResult AWetMopActor::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	if (!HeldTransform.GetScale3D().Equals(FVector::OneVector))
	{
		Context.AddWarning(LOCTEXT(
			"HeldTransformScaleIgnored",
			"HeldTransform scale is ignored at runtime. Author a unit scale and use location/rotation only."));
	}
	return Result == EDataValidationResult::NotValidated ? EDataValidationResult::Valid : Result;
}
#endif

#undef LOCTEXT_NAMESPACE
