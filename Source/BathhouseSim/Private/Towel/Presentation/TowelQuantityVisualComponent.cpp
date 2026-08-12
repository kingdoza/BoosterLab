#include "Towel/Presentation/TowelQuantityVisualComponent.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "TimerManager.h"
#include "Towel/Presentation/TowelVisualMeshProfile.h"
#include "Towel/TowelInventoryComponent.h"

UTowelQuantityVisualComponent::UTowelQuantityVisualComponent()
	: RandomStream(RandomSeed)
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UTowelQuantityVisualComponent::OnRegister()
{
	Super::OnRegister();
	RandomStream.Initialize(RandomSeed);
	PrepareLayout();
	if (UTowelInventoryComponent* InventorySource = PendingReregisterInventory.Get())
	{
		PendingReregisterInventory.Reset();
		BindInventorySource(InventorySource);
	}
}

void UTowelQuantityVisualComponent::OnUnregister()
{
	UTowelInventoryComponent* InventoryToRestore = BoundInventory.Get();
	bCleaningUp = true;
	UnbindInventorySource();
	ClearPresentation();
	bCleaningUp = false;
	PendingReregisterInventory = InventoryToRestore;
	Super::OnUnregister();
}

void UTowelQuantityVisualComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	bCleaningUp = true;
	UnbindInventorySource();
	ClearPresentation();
	bCleaningUp = false;
	Super::EndPlay(EndPlayReason);
}

void UTowelQuantityVisualComponent::BindInventorySource(UTowelInventoryComponent* InventorySource)
{
	if (BoundInventory.Get() == InventorySource)
	{
		if (InventorySource)
		{
			const FTowelInventorySnapshot Snapshot = InventorySource->GetSnapshot();
			SetTargetPresentation(Snapshot.State, Snapshot.Count, Snapshot.Revision, false);
		}
		return;
	}

	UnbindInventorySource();
	ResetForNewSource();
	if (!IsValid(InventorySource))
	{
		return;
	}

	BoundInventory = InventorySource;
	InventorySource->OnInventoryChanged.AddDynamic(
		this,
		&UTowelQuantityVisualComponent::HandleInventoryChanged);
	const FTowelInventorySnapshot Snapshot = InventorySource->GetSnapshot();
	SetTargetPresentation(Snapshot.State, Snapshot.Count, Snapshot.Revision, false);
}

void UTowelQuantityVisualComponent::UnbindInventorySource()
{
	PendingReregisterInventory.Reset();
	if (UTowelInventoryComponent* InventorySource = BoundInventory.Get())
	{
		InventorySource->OnInventoryChanged.RemoveDynamic(
			this,
			&UTowelQuantityVisualComponent::HandleInventoryChanged);
	}
	BoundInventory.Reset();
	StopStepTimer();
	if (!bCleaningUp)
	{
		ClearPresentation();
		TargetState = ETowelState::None;
		DisplayedState = ETowelState::None;
		TargetCount = 0;
		DisplayedCount = 0;
		AppliedRevision = -1;
	}
}

void UTowelQuantityVisualComponent::SetTargetPresentation(
	ETowelState State,
	const int32 Count,
	const int64 Revision,
	const bool bAnimate)
{
	const int32 SafeCount = FMath::Max(0, Count);
	if (SafeCount == 0)
	{
		State = ETowelState::None;
	}
	else if (State == ETowelState::None)
	{
		ensureMsgf(false, TEXT("A positive towel presentation count requires a non-None state."));
		return;
	}

	if (Revision < AppliedRevision)
	{
		return;
	}
	if (Revision == AppliedRevision)
	{
		if (State == TargetState && SafeCount == TargetCount)
		{
			if (!bAnimate)
			{
				SynchronizeImmediately();
			}
			return;
		}

		UE_LOG(
			LogTemp,
			Warning,
			TEXT("Towel presentation %s received divergent payloads for revision %lld; retaining the authoritative source target."),
			*GetPathName(),
			Revision);
		if (UTowelInventoryComponent* InventorySource = BoundInventory.Get())
		{
			const FTowelInventorySnapshot Authoritative = InventorySource->GetSnapshot();
			if (Authoritative.Revision > AppliedRevision)
			{
				SetTargetPresentation(
					Authoritative.State,
					Authoritative.Count,
					Authoritative.Revision,
					false);
			}
		}
		return;
	}

	AppliedRevision = Revision;
	TargetState = State;
	TargetCount = SafeCount;
	PrepareLayout();
	ApplyTargetStateToVisibleLayers();

	if (!bAnimate || CountStepInterval <= 0.0f || !GetWorld() || !IsRegistered())
	{
		SynchronizeImmediately();
		return;
	}
	EnsureStepTimer();
}

void UTowelQuantityVisualComponent::SynchronizeImmediately()
{
	StopStepTimer();
	PrepareLayout();
	ApplyTargetStateToVisibleLayers();
	const int32 EffectiveTargetCount = GetEffectiveTargetCount();
	while (DisplayedCount < EffectiveTargetCount)
	{
		AddVisualLayer();
	}
	while (DisplayedCount > EffectiveTargetCount)
	{
		RemoveLastVisualLayer();
	}
	if (DisplayedCount == 0)
	{
		DisplayedState = TargetState;
	}
}

FTransform UTowelQuantityVisualComponent::BuildLocalTransform(const int32 VisualIndex)
{
	(void)VisualIndex;
	return FTransform::Identity;
}

int32 UTowelQuantityVisualComponent::GetVisualCapacity() const
{
	return MAX_int32;
}

void UTowelQuantityVisualComponent::PrepareLayout()
{
}

void UTowelQuantityVisualComponent::RebuildVisibleMeshesPreservingTransforms()
{
	TArray<FTransform> PreservedTransforms;
	PreservedTransforms.Reserve(LayerRecords.Num());
	for (const FTowelVisualLayerRecord& Record : LayerRecords)
	{
		PreservedTransforms.Add(Record.LocalTransform);
	}

	DestroyAllBuckets();
	LayerRecords.Reset();
	DisplayedCount = 0;
	DisplayedState = TargetState;
	for (const FTransform& Transform : PreservedTransforms)
	{
		FTowelVisualLayerRecord& Record = LayerRecords.AddDefaulted_GetRef();
		Record.LocalTransform = Transform;
		Record.Mesh = MeshProfile ? MeshProfile->SelectMesh(DisplayedState, RandomStream) : nullptr;
		if (Record.Mesh)
		{
			Record.Bucket = FindOrCreateBucket(Record.Mesh);
			Record.InstanceIndex = Record.Bucket
				? Record.Bucket->AddInstance(Record.LocalTransform, false)
				: INDEX_NONE;
		}
		++DisplayedCount;
	}
}

void UTowelQuantityVisualComponent::ClearPresentation()
{
	StopStepTimer();
	DestroyAllBuckets();
	LayerRecords.Reset();
	DisplayedCount = 0;
}

void UTowelQuantityVisualComponent::HandleInventoryChanged(
	const FTowelInventorySnapshot& Previous,
	const FTowelInventorySnapshot& Current,
	const int64 TransactionId)
{
	(void)Previous;
	(void)TransactionId;
	SetTargetPresentation(
		Current.State,
		Current.Count,
		Current.Revision,
		bAnimateInventoryChanges);
}

void UTowelQuantityVisualComponent::ResetForNewSource()
{
	ClearPresentation();
	RandomStream.Initialize(RandomSeed);
	TargetState = ETowelState::None;
	DisplayedState = ETowelState::None;
	TargetCount = 0;
	DisplayedCount = 0;
	AppliedRevision = -1;
}

void UTowelQuantityVisualComponent::EnsureStepTimer()
{
	if (DisplayedCount == GetEffectiveTargetCount() && DisplayedState == TargetState)
	{
		StopStepTimer();
		return;
	}
	if (!GetWorld() || GetWorld()->GetTimerManager().IsTimerActive(StepTimerHandle))
	{
		return;
	}
	GetWorld()->GetTimerManager().SetTimer(
		StepTimerHandle,
		this,
		&UTowelQuantityVisualComponent::AdvanceOneStep,
		FMath::Max(0.001f, CountStepInterval),
		true);
}

void UTowelQuantityVisualComponent::StopStepTimer()
{
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(StepTimerHandle);
	}
}

void UTowelQuantityVisualComponent::AdvanceOneStep()
{
	ApplyTargetStateToVisibleLayers();
	const int32 EffectiveTargetCount = GetEffectiveTargetCount();
	if (DisplayedCount < EffectiveTargetCount)
	{
		AddVisualLayer();
	}
	else if (DisplayedCount > EffectiveTargetCount)
	{
		RemoveLastVisualLayer();
	}

	if (DisplayedCount == EffectiveTargetCount)
	{
		if (DisplayedCount == 0)
		{
			DisplayedState = TargetState;
		}
		StopStepTimer();
	}
}

void UTowelQuantityVisualComponent::ApplyTargetStateToVisibleLayers()
{
	if (DisplayedState == TargetState)
	{
		return;
	}
	if (DisplayedCount == 0)
	{
		DisplayedState = TargetState;
		return;
	}
	if (TargetState != ETowelState::None)
	{
		RebuildVisibleMeshesPreservingTransforms();
	}
}

int32 UTowelQuantityVisualComponent::GetEffectiveTargetCount() const
{
	return FMath::Clamp(TargetCount, 0, FMath::Max(0, GetVisualCapacity()));
}

void UTowelQuantityVisualComponent::AddVisualLayer()
{
	const int32 VisualIndex = DisplayedCount;
	FTowelVisualLayerRecord& Record = LayerRecords.AddDefaulted_GetRef();
	Record.LocalTransform = BuildLocalTransform(VisualIndex);
	Record.Mesh = MeshProfile ? MeshProfile->SelectMesh(TargetState, RandomStream) : nullptr;
	if (Record.Mesh)
	{
		Record.Bucket = FindOrCreateBucket(Record.Mesh);
		Record.InstanceIndex = Record.Bucket
			? Record.Bucket->AddInstance(Record.LocalTransform, false)
			: INDEX_NONE;
	}
	DisplayedState = TargetState;
	++DisplayedCount;
}

void UTowelQuantityVisualComponent::RemoveLastVisualLayer()
{
	if (LayerRecords.IsEmpty())
	{
		DisplayedCount = 0;
		return;
	}

	const FTowelVisualLayerRecord Record = LayerRecords.Pop(EAllowShrinking::No);
	if (Record.Bucket && Record.InstanceIndex != INDEX_NONE)
	{
		const int32 LastBucketInstance = Record.Bucket->GetInstanceCount() - 1;
		ensureMsgf(
			Record.InstanceIndex == LastBucketInstance,
			TEXT("Global top removal must also remove the last instance in its towel mesh bucket."));
		Record.Bucket->RemoveInstance(Record.InstanceIndex);
		if (Record.Bucket->GetInstanceCount() == 0)
		{
			DestroyBucket(Record.Mesh, Record.Bucket);
		}
	}
	DisplayedCount = FMath::Max(0, DisplayedCount - 1);
}

UInstancedStaticMeshComponent* UTowelQuantityVisualComponent::FindOrCreateBucket(UStaticMesh* Mesh)
{
	if (!Mesh || !GetOwner())
	{
		return nullptr;
	}
	if (TObjectPtr<UInstancedStaticMeshComponent>* Existing = MeshBuckets.Find(Mesh))
	{
		return *Existing;
	}

	UInstancedStaticMeshComponent* Bucket = NewObject<UInstancedStaticMeshComponent>(
		GetOwner(),
		NAME_None,
		RF_Transient);
	if (!Bucket)
	{
		return nullptr;
	}
	Bucket->CreationMethod = EComponentCreationMethod::Instance;
	Bucket->SetStaticMesh(Mesh);
	Bucket->SetupAttachment(this);
	Bucket->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Bucket->SetGenerateOverlapEvents(false);
	Bucket->SetCanEverAffectNavigation(false);
	Bucket->PrimaryComponentTick.bCanEverTick = false;
	GetOwner()->AddInstanceComponent(Bucket);
	Bucket->RegisterComponent();
	MeshBuckets.Add(Mesh, Bucket);
	return Bucket;
}

void UTowelQuantityVisualComponent::DestroyBucket(
	UStaticMesh* Mesh,
	UInstancedStaticMeshComponent* Bucket)
{
	MeshBuckets.Remove(Mesh);
	if (Bucket)
	{
		if (AActor* Owner = GetOwner())
		{
			Owner->RemoveInstanceComponent(Bucket);
		}
		Bucket->DestroyComponent();
	}
}

void UTowelQuantityVisualComponent::DestroyAllBuckets()
{
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> Buckets;
	MeshBuckets.GenerateValueArray(Buckets);
	MeshBuckets.Reset();
	for (UInstancedStaticMeshComponent* Bucket : Buckets)
	{
		if (Bucket)
		{
			if (AActor* Owner = GetOwner())
			{
				Owner->RemoveInstanceComponent(Bucket);
			}
			Bucket->DestroyComponent();
		}
	}
}
