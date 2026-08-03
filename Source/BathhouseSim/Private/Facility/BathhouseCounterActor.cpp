#include "Facility/BathhouseCounterActor.h"

#include "Components/SceneComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogBathhouseCounter, Log, All);

ABathhouseCounterActor::ABathhouseCounterActor()
{
	PrimaryActorTick.bCanEverTick = false;
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	CheckInServicePoint = CreateDefaultSubobject<USceneComponent>(TEXT("CheckInServicePoint"));
	CheckInServicePoint->SetupAttachment(SceneRoot);
	CheckoutServicePoint = CreateDefaultSubobject<USceneComponent>(TEXT("CheckoutServicePoint"));
	CheckoutServicePoint->SetupAttachment(SceneRoot);
	CashOfferPoint = CreateDefaultSubobject<USceneComponent>(TEXT("CashOfferPoint"));
	CashOfferPoint->SetupAttachment(SceneRoot);
}

void ABathhouseCounterActor::BeginPlay()
{
	Super::BeginPlay();
	ResolveConfiguredPoints();
	RuntimeReturnedSlots.Reset(ResolvedReturnedKeyPoints.Num());
	for (USceneComponent* Point : ResolvedReturnedKeyPoints)
	{
		FBathhouseReturnedObjectSlot& Slot = RuntimeReturnedSlots.AddDefaulted_GetRef();
		Slot.Point = Point;
	}
}

void ABathhouseCounterActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CheckInQueue.Reset();
	CheckoutQueue.Reset();
	ResolvedCheckInQueuePoints.Reset();
	ResolvedCheckoutQueuePoints.Reset();
	ResolvedReturnedKeyPoints.Reset();
	RuntimeReturnedSlots.Reset();
	OnQueueChangedNative.Clear();
	OnReturnedKeySlotsChangedNative.Clear();
	Super::EndPlay(EndPlayReason);
}

bool ABathhouseCounterActor::EnqueueActor(const EBathhouseCounterLane Lane, AActor* Actor)
{
	if (!IsValid(Actor) || Lane == EBathhouseCounterLane::None)
	{
		return false;
	}

	if (GetQueue(EBathhouseCounterLane::CheckIn).ContainsByPredicate([Actor](const FBathhouseQueueEntry& Entry) { return Entry.Actor == Actor; })
		|| GetQueue(EBathhouseCounterLane::Checkout).ContainsByPredicate([Actor](const FBathhouseQueueEntry& Entry) { return Entry.Actor == Actor; }))
	{
		return GetQueue(Lane).ContainsByPredicate([Actor](const FBathhouseQueueEntry& Entry) { return Entry.Actor == Actor; });
	}

	FBathhouseQueueEntry& Entry = GetMutableQueue(Lane).AddDefaulted_GetRef();
	Entry.Actor = Actor;
	Entry.Sequence = NextQueueSequence++;
	BroadcastQueueChanged(Lane);
	return true;
}

bool ABathhouseCounterActor::DequeueActor(const EBathhouseCounterLane Lane, AActor* Actor)
{
	TArray<FBathhouseQueueEntry>& Queue = GetMutableQueue(Lane);
	const int32 Removed = Queue.RemoveAll([Actor](const FBathhouseQueueEntry& Entry) { return Entry.Actor == Actor; });
	if (Removed > 0)
	{
		BroadcastQueueChanged(Lane);
	}
	return Removed > 0;
}

bool ABathhouseCounterActor::IsFront(const EBathhouseCounterLane Lane, const AActor* Actor) const
{
	const TArray<FBathhouseQueueEntry>& Queue = GetQueue(Lane);
	return Queue.Num() > 0 && Queue[0].Actor == Actor;
}

bool ABathhouseCounterActor::GetQueueTargetTransform(
	const EBathhouseCounterLane Lane,
	const AActor* Actor,
	FTransform& OutTransform) const
{
	const TArray<FBathhouseQueueEntry>& Queue = GetQueue(Lane);
	const int32 Index = Queue.IndexOfByPredicate([Actor](const FBathhouseQueueEntry& Entry) { return Entry.Actor == Actor; });
	if (Index == INDEX_NONE)
	{
		return false;
	}

	if (Index == 0)
	{
		if (const USceneComponent* ServicePoint = GetServicePoint(Lane))
		{
			OutTransform = ServicePoint->GetComponentTransform();
			return true;
		}
		return false;
	}

	const TArray<TObjectPtr<USceneComponent>>& Points = GetQueuePoints(Lane);
	if (Points.IsEmpty())
	{
		return false;
	}
	const int32 PointIndex = FMath::Min(Index - 1, Points.Num() - 1);
	if (!Points[PointIndex])
	{
		return false;
	}
	OutTransform = Points[PointIndex]->GetComponentTransform();
	return true;
}

bool ABathhouseCounterActor::TryReserveReturnedObjectSlot(AActor* Requestor, int32& OutSlotIndex, FTransform& OutTransform)
{
	OutSlotIndex = INDEX_NONE;
	if (!IsValid(Requestor))
	{
		return false;
	}

	for (int32 Index = 0; Index < RuntimeReturnedSlots.Num(); ++Index)
	{
		FBathhouseReturnedObjectSlot& Slot = RuntimeReturnedSlots[Index];
		if (Slot.ReservationOwner == Requestor && Slot.ReturnedObject == nullptr && Slot.Point)
		{
			OutSlotIndex = Index;
			OutTransform = Slot.Point->GetComponentTransform();
			return true;
		}
		if (Slot.ReservationOwner == nullptr && Slot.ReturnedObject == nullptr && Slot.Point)
		{
			Slot.ReservationOwner = Requestor;
			OutSlotIndex = Index;
			OutTransform = Slot.Point->GetComponentTransform();
			OnReturnedKeySlotsChanged();
			OnReturnedKeySlotsChangedNative.Broadcast();
			return true;
		}
	}
	return false;
}

bool ABathhouseCounterActor::PlaceReturnedObject(AActor* Requestor, const int32 SlotIndex, AActor* ReturnedObject)
{
	if (!RuntimeReturnedSlots.IsValidIndex(SlotIndex) || !IsValid(Requestor) || !IsValid(ReturnedObject))
	{
		return false;
	}
	FBathhouseReturnedObjectSlot& Slot = RuntimeReturnedSlots[SlotIndex];
	if (Slot.ReservationOwner != Requestor || Slot.ReturnedObject != nullptr || !Slot.Point)
	{
		return false;
	}
	Slot.ReturnedObject = ReturnedObject;
	OnReturnedKeySlotsChanged();
	OnReturnedKeySlotsChangedNative.Broadcast();
	return true;
}

bool ABathhouseCounterActor::TakeReturnedObject(AActor* ReturnedObject)
{
	for (FBathhouseReturnedObjectSlot& Slot : RuntimeReturnedSlots)
	{
		if (Slot.ReturnedObject == ReturnedObject)
		{
			Slot.ReturnedObject = nullptr;
			Slot.ReservationOwner = nullptr;
			OnReturnedKeySlotsChanged();
			OnReturnedKeySlotsChangedNative.Broadcast();
			return true;
		}
	}
	return false;
}

bool ABathhouseCounterActor::ReleaseReturnedObjectReservation(AActor* Requestor, const int32 SlotIndex)
{
	if (!RuntimeReturnedSlots.IsValidIndex(SlotIndex))
	{
		return false;
	}
	FBathhouseReturnedObjectSlot& Slot = RuntimeReturnedSlots[SlotIndex];
	if (Slot.ReservationOwner != Requestor || Slot.ReturnedObject != nullptr)
	{
		return false;
	}
	Slot.ReservationOwner = nullptr;
	OnReturnedKeySlotsChanged();
	OnReturnedKeySlotsChangedNative.Broadcast();
	return true;
}

USceneComponent* ABathhouseCounterActor::GetReturnSlotComponent(const int32 SlotIndex) const
{
	return RuntimeReturnedSlots.IsValidIndex(SlotIndex) ? RuntimeReturnedSlots[SlotIndex].Point.Get() : nullptr;
}

TArray<FBathhouseQueueEntry>& ABathhouseCounterActor::GetMutableQueue(const EBathhouseCounterLane Lane)
{
	return Lane == EBathhouseCounterLane::Checkout ? CheckoutQueue : CheckInQueue;
}

const TArray<FBathhouseQueueEntry>& ABathhouseCounterActor::GetQueue(const EBathhouseCounterLane Lane) const
{
	return Lane == EBathhouseCounterLane::Checkout ? CheckoutQueue : CheckInQueue;
}

USceneComponent* ABathhouseCounterActor::GetServicePoint(const EBathhouseCounterLane Lane) const
{
	return Lane == EBathhouseCounterLane::Checkout ? CheckoutServicePoint.Get() : CheckInServicePoint.Get();
}

const TArray<TObjectPtr<USceneComponent>>& ABathhouseCounterActor::GetQueuePoints(const EBathhouseCounterLane Lane) const
{
	return Lane == EBathhouseCounterLane::Checkout ? ResolvedCheckoutQueuePoints : ResolvedCheckInQueuePoints;
}

void ABathhouseCounterActor::ResolveConfiguredPoints()
{
	TSet<USceneComponent*> UsedAcrossRoles;
	ResolvePointReferences(
		TEXT("CheckInQueue"),
		CheckInQueuePointReferences,
		ResolvedCheckInQueuePoints,
		UsedAcrossRoles);
	ResolvePointReferences(
		TEXT("CheckoutQueue"),
		CheckoutQueuePointReferences,
		ResolvedCheckoutQueuePoints,
		UsedAcrossRoles);
	ResolvePointReferences(
		TEXT("ReturnedKey"),
		ReturnedKeyPointReferences,
		ResolvedReturnedKeyPoints,
		UsedAcrossRoles);
}

void ABathhouseCounterActor::ResolvePointReferences(
	const TCHAR* RoleName,
	const TArray<FComponentReference>& References,
	TArray<TObjectPtr<USceneComponent>>& OutPoints,
	TSet<USceneComponent*>& UsedAcrossRoles)
{
	OutPoints.Reset(References.Num());
	TSet<USceneComponent*> UsedInRole;
	for (int32 Index = 0; Index < References.Num(); ++Index)
	{
		const FComponentReference& Reference = References[Index];
		if (Reference.ComponentProperty.IsNone()
			&& Reference.PathToComponent.IsEmpty()
			&& !Reference.OverrideComponent.IsValid())
		{
			UE_LOG(
				LogBathhouseCounter,
				Error,
				TEXT("Counter point reference %s[%d] is not configured on %s."),
				RoleName,
				Index,
				*GetPathName());
			continue;
		}

		UActorComponent* ResolvedComponent = Reference.GetComponent(this);
		USceneComponent* Point = Cast<USceneComponent>(ResolvedComponent);
		if (!Point)
		{
			UE_LOG(
				LogBathhouseCounter,
				Error,
				TEXT("Counter point reference %s[%d] does not resolve to a SceneComponent on %s."),
				RoleName,
				Index,
				*GetPathName());
			continue;
		}
		if (Point->GetOwner() != this)
		{
			UE_LOG(
				LogBathhouseCounter,
				Error,
				TEXT("Counter point reference %s[%d] resolves to component %s owned by another Actor; expected %s."),
				RoleName,
				Index,
				*Point->GetPathName(),
				*GetPathName());
			continue;
		}
		if (UsedInRole.Contains(Point))
		{
			UE_LOG(
				LogBathhouseCounter,
				Error,
				TEXT("Counter point reference %s[%d] duplicates component %s within the same role on %s."),
				RoleName,
				Index,
				*Point->GetName(),
				*GetPathName());
			continue;
		}
		if (UsedAcrossRoles.Contains(Point))
		{
			UE_LOG(
				LogBathhouseCounter,
				Error,
				TEXT("Counter point reference %s[%d] reuses component %s across roles on %s."),
				RoleName,
				Index,
				*Point->GetName(),
				*GetPathName());
			continue;
		}

		UsedInRole.Add(Point);
		UsedAcrossRoles.Add(Point);
		OutPoints.Add(Point);
	}
}

void ABathhouseCounterActor::BroadcastQueueChanged(const EBathhouseCounterLane Lane)
{
	OnQueueChanged(Lane);
	OnQueueChangedNative.Broadcast(Lane);
}
