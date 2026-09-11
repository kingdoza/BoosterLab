#include "Placement/FacilityPlacementPreviewSource.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Misc/ScopeExit.h"

namespace
{
struct FDefaultSceneTemplate
{
	const USceneComponent* Component = nullptr;
	FName StableName = NAME_None;
	FName ParentName = NAME_None;
	FTransform RelativeToRoot = FTransform::Identity;
	bool bNative = false;
};

bool IsExcludedPreviewMesh(const UStaticMeshComponent& Component)
{
	const FString Name = Component.GetName().ToLower();
	return Component.IsA<UInstancedStaticMeshComponent>()
		|| Component.IsEditorOnly() || !Component.IsVisible() || Component.bHiddenInGame
		|| Name.Contains(TEXT("footprint")) || Name.Contains(TEXT("packagephysicalroot"))
		|| Name.Contains(TEXT("interaction")) || Name.Contains(TEXT("helper"))
		|| Name.Contains(TEXT("slot")) || Name.Contains(TEXT("pile"))
		|| Name.Contains(TEXT("water")) || Name.Contains(TEXT("contents"));
}

bool ResolveRootRelativeTransforms(
	const AActor& PlacedCDO,
	TArray<FDefaultSceneTemplate>& Templates,
	const TMap<FName, int32>& TemplateByName,
	FText& OutFailureReason)
{
	TArray<TOptional<FTransform>> ResolvedTransforms;
	ResolvedTransforms.SetNum(Templates.Num());
	TFunction<bool(int32, TSet<int32>&)> ResolveTransform =
		[&](const int32 Index, TSet<int32>& Visiting)
		{
			if (!Templates.IsValidIndex(Index) || !Templates[Index].Component)
			{
				return false;
			}
			if (ResolvedTransforms[Index].IsSet())
			{
				return true;
			}
			if (Visiting.Contains(Index))
			{
				return false;
			}
			Visiting.Add(Index);
			const FDefaultSceneTemplate& Entry = Templates[Index];
			FTransform RelativeToRoot;
			if (Entry.Component == PlacedCDO.GetRootComponent())
			{
				RelativeToRoot = FTransform::Identity;
			}
			else
			{
				RelativeToRoot = Entry.Component->GetRelativeTransform();
				if (!Entry.ParentName.IsNone()
					&& Entry.ParentName != PlacedCDO.GetRootComponent()->GetFName())
				{
					const int32* ParentIndex = TemplateByName.Find(Entry.ParentName);
					if (!ParentIndex || !ResolveTransform(*ParentIndex, Visiting))
					{
						return false;
					}
					RelativeToRoot = RelativeToRoot * ResolvedTransforms[*ParentIndex].GetValue();
				}
			}
			Visiting.Remove(Index);
			ResolvedTransforms[Index] = RelativeToRoot;
			return true;
		};

	for (int32 Index = 0; Index < Templates.Num(); ++Index)
	{
		TSet<int32> Visiting;
		if (!ResolveTransform(Index, Visiting))
		{
			OutFailureReason = FText::Format(
				NSLOCTEXT("FacilityPlacementPreview", "InvalidDefaultHierarchy", "배치 설비 component '{0}'의 기본 부모 transform을 확인할 수 없습니다."),
				FText::FromName(Templates[Index].StableName));
			return false;
		}
		Templates[Index].RelativeToRoot = ResolvedTransforms[Index].GetValue();
	}
	return true;
}
}

bool FacilityPlacementPreviewSource::Collect(
	const TSubclassOf<AActor> PlacedClass,
	AActor& ScratchOwner,
	TArray<FFacilityPlacementPreviewMeshSource>& OutSources,
	FText& OutFailureReason)
{
	OutSources.Reset();
	const AActor* PlacedCDO = PlacedClass ? PlacedClass->GetDefaultObject<AActor>() : nullptr;
	if (!PlacedCDO || !PlacedCDO->GetRootComponent())
	{
		OutFailureReason = NSLOCTEXT(
			"FacilityPlacementPreview", "MissingDefaultHierarchy",
			"배치 설비 클래스의 기본 component hierarchy를 확인할 수 없습니다.");
		return false;
	}

	TArray<TObjectPtr<UActorComponent>> ScratchComponents;
	ON_SCOPE_EXIT
	{
		for (UActorComponent* Scratch : ScratchComponents)
		{
			if (IsValid(Scratch))
			{
				Scratch->DestroyComponent();
			}
		}
	};

	TArray<FDefaultSceneTemplate> Templates;
	TMap<FName, int32> TemplateByName;
	TInlineComponentArray<USceneComponent*> NativeComponents(PlacedCDO);
	for (const USceneComponent* Component : NativeComponents)
	{
		if (!Component)
		{
			continue;
		}
		const USceneComponent* Parent = Component->GetAttachParent();
		const int32 Index = Templates.Add(
			{ Component, Component->GetFName(), Parent ? Parent->GetFName() : NAME_None,
				FTransform::Identity, true });
		TemplateByName.Add(Component->GetFName(), Index);
	}

	UBlueprintGeneratedClass* ActualClass = Cast<UBlueprintGeneratedClass>(PlacedClass.Get());
	if (ActualClass)
	{
		TMap<const USCS_Node*, FName> TreeParents;
		UBlueprintGeneratedClass::ForEachGeneratedClassInHierarchy(
			PlacedClass,
			[&](const UBlueprintGeneratedClass* CurrentClass)
			{
				const USimpleConstructionScript* SCS = CurrentClass
					? CurrentClass->SimpleConstructionScript.Get() : nullptr;
				if (!SCS)
				{
					return true;
				}
				for (const USCS_Node* ParentNode : SCS->GetAllNodes())
				{
					if (!ParentNode)
					{
						continue;
					}
					for (const USCS_Node* ChildNode : ParentNode->GetChildNodes())
					{
						if (ChildNode)
						{
							TreeParents.Add(ChildNode, ParentNode->GetVariableName());
						}
					}
				}
				return true;
			});

		UBlueprintGeneratedClass::ForEachGeneratedClassInHierarchy(
			PlacedClass,
			[&](const UBlueprintGeneratedClass* CurrentClass)
			{
				const USimpleConstructionScript* SCS = CurrentClass
					? CurrentClass->SimpleConstructionScript.Get() : nullptr;
				if (!SCS)
				{
					return true;
				}
				for (const USCS_Node* Node : SCS->GetAllNodes())
				{
					UActorComponent* ActualComponent = Node
						? Node->GetActualComponentTemplate(ActualClass) : nullptr;
					const FBlueprintCookedComponentInstancingData* CookedData =
						Node && ActualClass->UseFastPathComponentInstancing()
							? Node->GetActualComponentTemplateData(ActualClass) : nullptr;
					if (CookedData && CookedData->bHasValidCookedData
						&& CookedData->ComponentTemplateClass)
					{
						ActualComponent = ScratchOwner.CreateComponentFromTemplateData(
							CookedData,
							MakeUniqueObjectName(
								&ScratchOwner,
								CookedData->ComponentTemplateClass,
								*FString::Printf(TEXT("PreviewSource_%s"), *Node->GetVariableName().ToString())));
						if (ActualComponent)
						{
							ScratchComponents.Add(ActualComponent);
						}
					}
					const USceneComponent* Component = Cast<USceneComponent>(ActualComponent);
					if (!Component)
					{
						continue;
					}
					const FName StableName = Node->GetVariableName().IsNone()
						? Component->GetFName() : Node->GetVariableName();
					FName ParentName = Node->ParentComponentOrVariableName;
					if (ParentName.IsNone())
					{
						if (const FName* TreeParent = TreeParents.Find(Node))
						{
							ParentName = *TreeParent;
						}
						else
						{
							ParentName = PlacedCDO->GetRootComponent()->GetFName();
						}
					}
					const int32 Index = Templates.Add(
						{ Component, StableName, ParentName, FTransform::Identity, false });
					TemplateByName.Add(StableName, Index);
					TemplateByName.Add(Component->GetFName(), Index);
				}
				return true;
			});
	}

	if (!ResolveRootRelativeTransforms(
		*PlacedCDO, Templates, TemplateByName, OutFailureReason))
	{
		return false;
	}
	for (const FDefaultSceneTemplate& Template : Templates)
	{
		const UStaticMeshComponent* Mesh = Cast<UStaticMeshComponent>(Template.Component);
		if (!Mesh || !Mesh->GetStaticMesh() || IsExcludedPreviewMesh(*Mesh))
		{
			continue;
		}
		OutSources.Add({
			Mesh->GetStaticMesh(),
			Template.StableName,
			Template.RelativeToRoot,
			Mesh->IsVisible(),
			Mesh->bHiddenInGame != 0,
			Mesh->CastShadow != 0,
			Mesh->bReceivesDecals != 0 });
	}
	OutSources.Sort([](
		const FFacilityPlacementPreviewMeshSource& Left,
		const FFacilityPlacementPreviewMeshSource& Right)
	{
		return Left.StableName.LexicalLess(Right.StableName);
	});
	return true;
}
