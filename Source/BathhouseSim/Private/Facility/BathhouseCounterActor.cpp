#include "Facility/BathhouseCounterActor.h"

#include "Components/SceneComponent.h"
#include "Facility/CustomerQueueOverflowWanderVolume.h"

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
	ReturnedKeyDropPoint = CreateDefaultSubobject<USceneComponent>(TEXT("ReturnedKeyDropPoint"));
	ReturnedKeyDropPoint->SetupAttachment(SceneRoot);
}

void ABathhouseCounterActor::BeginPlay()
{
	Super::BeginPlay();
	ResolveConfiguredPoints();
}

void ABathhouseCounterActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CheckInQueue.Reset();
	CheckoutQueue.Reset();
	ResolvedCheckInQueuePoints.Reset();
	ResolvedCheckoutQueuePoints.Reset();
	OnQueueChangedNative.Clear();
	OnReturnedKeySlotsChangedNative.Clear();
	Super::EndPlay(EndPlayReason);
}

bool ABathhouseCounterActor::EnqueueActor(const EBathhouseCounterLane Lane, AActor* Actor)
{
	bool bCheckInChanged = CompactInvalidEntries(EBathhouseCounterLane::CheckIn);
	bool bCheckoutChanged = CompactInvalidEntries(EBathhouseCounterLane::Checkout);
	auto CommitCompaction = [this, &bCheckInChanged, &bCheckoutChanged]()
	{
		if (bCheckInChanged)
		{
			AdvanceQueueRevision(EBathhouseCounterLane::CheckIn);
			BroadcastQueueChanged(EBathhouseCounterLane::CheckIn);
		}
		if (bCheckoutChanged)
		{
			AdvanceQueueRevision(EBathhouseCounterLane::Checkout);
			BroadcastQueueChanged(EBathhouseCounterLane::Checkout);
		}
	};

	if (!IsValid(Actor) || Lane == EBathhouseCounterLane::None)
	{
		CommitCompaction();
		return false;
	}

	if (GetQueue(EBathhouseCounterLane::CheckIn).ContainsByPredicate([Actor](const FBathhouseQueueEntry& Entry) { return Entry.Actor.Get() == Actor; })
		|| GetQueue(EBathhouseCounterLane::Checkout).ContainsByPredicate([Actor](const FBathhouseQueueEntry& Entry) { return Entry.Actor.Get() == Actor; }))
	{
		const bool bAlreadyInLane = GetQueue(Lane).ContainsByPredicate(
			[Actor](const FBathhouseQueueEntry& Entry) { return Entry.Actor.Get() == Actor; });
		CommitCompaction();
		return bAlreadyInLane;
	}

	FBathhouseQueueEntry& Entry = GetMutableQueue(Lane).AddDefaulted_GetRef();
	Entry.Actor = Actor;
	Entry.Sequence = NextQueueSequence++;
	if (Lane == EBathhouseCounterLane::CheckIn)
	{
		bCheckInChanged = true;
	}
	else
	{
		bCheckoutChanged = true;
	}
	CommitCompaction();
	return true;
}

bool ABathhouseCounterActor::DequeueActor(const EBathhouseCounterLane Lane, AActor* Actor)
{
	if (Lane == EBathhouseCounterLane::None)
	{
		return false;
	}
	TArray<FBathhouseQueueEntry>& Queue = GetMutableQueue(Lane);
	const bool bCompacted = CompactInvalidEntries(Lane);
	const int32 Removed = Queue.RemoveAll([Actor](const FBathhouseQueueEntry& Entry) { return Entry.Actor.Get() == Actor; });
	if (bCompacted || Removed > 0)
	{
		AdvanceQueueRevision(Lane);
		BroadcastQueueChanged(Lane);
	}
	return Removed > 0;
}

bool ABathhouseCounterActor::IsFront(const EBathhouseCounterLane Lane, const AActor* Actor) const
{
	FBathhouseQueueAssignment Assignment;
	return ResolveQueueAssignment(Lane, Actor, Assignment)
		&& Assignment.Type == EBathhouseQueueAssignmentType::ServicePoint;
}

bool ABathhouseCounterActor::ResolveQueueAssignment(
	const EBathhouseCounterLane Lane,
	const AActor* Actor,
	FBathhouseQueueAssignment& OutAssignment) const
{
	OutAssignment = FBathhouseQueueAssignment();
	OutAssignment.LaneRevision = GetQueueRevision(Lane);
	if (Lane == EBathhouseCounterLane::None || !IsValid(Actor))
	{
		return false;
	}

	const TArray<FBathhouseQueueEntry>& Queue = GetQueue(Lane);
	int32 LogicalIndex = 0;
	bool bFound = false;
	for (const FBathhouseQueueEntry& Entry : Queue)
	{
		if (!Entry.Actor.IsValid())
		{
			continue;
		}
		if (Entry.Actor.Get() == Actor)
		{
			bFound = true;
			break;
		}
		++LogicalIndex;
	}
	if (!bFound)
	{
		return false;
	}

	OutAssignment.LogicalIndex = LogicalIndex;
	if (LogicalIndex == 0)
	{
		if (const USceneComponent* ServicePoint = GetServicePoint(Lane))
		{
			OutAssignment.Type = EBathhouseQueueAssignmentType::ServicePoint;
			OutAssignment.TargetTransform = ServicePoint->GetComponentTransform();
			return true;
		}
		return false;
	}

	const TArray<TObjectPtr<USceneComponent>>& Points = GetQueuePoints(Lane);
	const int32 PointIndex = LogicalIndex - 1;
	if (Points.IsValidIndex(PointIndex) && Points[PointIndex])
	{
		OutAssignment.Type = EBathhouseQueueAssignmentType::QueuePoint;
		OutAssignment.QueuePointIndex = PointIndex;
		OutAssignment.TargetTransform = Points[PointIndex]->GetComponentTransform();
		return true;
	}
	if (Lane == EBathhouseCounterLane::Checkout && PointIndex >= Points.Num())
	{
		OutAssignment.Type = EBathhouseQueueAssignmentType::OverflowWander;
		return true;
	}
	return false;
}

int64 ABathhouseCounterActor::GetQueueRevision(const EBathhouseCounterLane Lane) const
{
	if (Lane == EBathhouseCounterLane::None)
	{
		return 0;
	}
	return Lane == EBathhouseCounterLane::Checkout ? CheckoutQueueRevision : CheckInQueueRevision;
}

bool ABathhouseCounterActor::GetQueueTargetTransform(
	const EBathhouseCounterLane Lane,
	const AActor* Actor,
	FTransform& OutTransform) const
{
	FBathhouseQueueAssignment Assignment;
	if (!ResolveQueueAssignment(Lane, Actor, Assignment) || !Assignment.IsVisibleAssignment())
	{
		return false;
	}
	OutTransform = Assignment.TargetTransform;
	return true;
}

bool ABathhouseCounterActor::TrySampleCheckoutOverflowPoint(const AActor& Requestor, FVector& OutPoint) const
{
	if (CheckoutOverflowVolumes.IsEmpty())
	{
		return false;
	}
	const int32 StartIndex = FMath::RandHelper(CheckoutOverflowVolumes.Num());
	for (int32 Offset = 0; Offset < CheckoutOverflowVolumes.Num(); ++Offset)
	{
		const int32 Index = (StartIndex + Offset) % CheckoutOverflowVolumes.Num();
		const ACustomerQueueOverflowWanderVolume* Volume = CheckoutOverflowVolumes[Index];
		if (IsValid(Volume) && Volume->TrySampleReachablePoint(Requestor, OutPoint))
		{
			return true;
		}
	}
	return false;
}

bool ABathhouseCounterActor::TryReserveReturnedObjectSlot(AActor* Requestor, int32& OutSlotIndex, FTransform& OutTransform)
{
	OutSlotIndex = INDEX_NONE;
	return false;
}

bool ABathhouseCounterActor::PlaceReturnedObject(AActor* Requestor, const int32 SlotIndex, AActor* ReturnedObject)
{
	return false;
}

bool ABathhouseCounterActor::TakeReturnedObject(AActor* ReturnedObject)
{
	return false;
}

bool ABathhouseCounterActor::ReleaseReturnedObjectReservation(AActor* Requestor, const int32 SlotIndex)
{
	return false;
}

USceneComponent* ABathhouseCounterActor::GetReturnSlotComponent(const int32 SlotIndex) const
{
	return nullptr;
}

void ABathhouseCounterActor::NotifyReturnedKeyDropped(AActor* ReturnedKey)
{
	if (IsValid(ReturnedKey))
	{
		OnReturnedKeyDropped(ReturnedKey);
	}
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
		if (Point == CheckInServicePoint || Point == CheckoutServicePoint)
		{
			UE_LOG(
				LogBathhouseCounter,
				Error,
				TEXT("Counter queue point %s[%d] on %s uses native service point %s; service points are reserved and cannot be queue-point references."),
				RoleName,
				Index,
				*GetPathName(),
				*Point->GetName());
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

bool ABathhouseCounterActor::CompactInvalidEntries(const EBathhouseCounterLane Lane)
{
	if (Lane == EBathhouseCounterLane::None)
	{
		return false;
	}
	return GetMutableQueue(Lane).RemoveAll(
		[](const FBathhouseQueueEntry& Entry) { return !Entry.Actor.IsValid(); }) > 0;
}

void ABathhouseCounterActor::AdvanceQueueRevision(const EBathhouseCounterLane Lane)
{
	int64& Revision = Lane == EBathhouseCounterLane::Checkout
		? CheckoutQueueRevision
		: CheckInQueueRevision;
	if (Revision == MAX_int64)
	{
		Revision = 1;
	}
	else
	{
		++Revision;
		if (Revision == 0)
		{
			Revision = 1;
		}
	}
}
