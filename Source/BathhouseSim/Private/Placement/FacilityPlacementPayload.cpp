#include "Placement/FacilityPlacementPayload.h"

#include "Components/ActorComponent.h"
#include "GameFramework/Actor.h"
#include "Placement/FacilityPlacementDefinition.h"
#include "Placement/PlaceableFacilityItemActor.h"
#include "UObject/ScriptInterface.h"
#include "UObject/UnrealType.h"

#define LOCTEXT_NAMESPACE "FacilityPlacementPayload"

namespace
{
bool RejectRuntimeReference(FText& OutFailureReason)
{
	OutFailureReason = LOCTEXT(
		"RuntimeReferenceInPayload",
		"설비 변환 데이터에는 Actor 또는 Component 참조를 저장할 수 없습니다.");
	return false;
}

bool ValidateReflectedValue(
	const FProperty& Property,
	const void* Value,
	FText& OutFailureReason)
{
	if (const FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(&Property))
	{
		const UClass* PropertyClass = ObjectProperty->PropertyClass;
		if (Property.IsA<FSoftObjectProperty>() && PropertyClass
			&& (PropertyClass->IsChildOf(AActor::StaticClass())
				|| PropertyClass->IsChildOf(UActorComponent::StaticClass())))
		{
			return RejectRuntimeReference(OutFailureReason);
		}
		const UObject* ReferencedObject = ObjectProperty->GetObjectPropertyValue(Value);
		return ReferencedObject
			&& (ReferencedObject->IsA<AActor>() || ReferencedObject->IsA<UActorComponent>())
			? RejectRuntimeReference(OutFailureReason)
			: true;
	}
	if (const FInterfaceProperty* InterfaceProperty = CastField<FInterfaceProperty>(&Property))
	{
		(void)InterfaceProperty;
		const FScriptInterface* InterfaceValue = static_cast<const FScriptInterface*>(Value);
		const UObject* ReferencedObject = InterfaceValue ? InterfaceValue->GetObject() : nullptr;
		return ReferencedObject
			&& (ReferencedObject->IsA<AActor>() || ReferencedObject->IsA<UActorComponent>())
			? RejectRuntimeReference(OutFailureReason)
			: true;
	}
	if (const FStructProperty* StructProperty = CastField<FStructProperty>(&Property))
	{
		for (TFieldIterator<FProperty> It(StructProperty->Struct, EFieldIteratorFlags::IncludeSuper); It; ++It)
		{
			for (int32 Index = 0; Index < It->ArrayDim; ++Index)
			{
				const void* NestedValue = It->ContainerPtrToValuePtr<void>(Value, Index);
				if (!ValidateReflectedValue(**It, NestedValue, OutFailureReason))
				{
					return false;
				}
			}
		}
		return true;
	}
	if (const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(&Property))
	{
		FScriptArrayHelper Helper(ArrayProperty, Value);
		for (int32 Index = 0; Index < Helper.Num(); ++Index)
		{
			if (!ValidateReflectedValue(*ArrayProperty->Inner, Helper.GetRawPtr(Index), OutFailureReason))
			{
				return false;
			}
		}
		return true;
	}
	if (const FSetProperty* SetProperty = CastField<FSetProperty>(&Property))
	{
		FScriptSetHelper Helper(SetProperty, Value);
		for (int32 Index = 0; Index < Helper.GetMaxIndex(); ++Index)
		{
			if (Helper.IsValidIndex(Index)
				&& !ValidateReflectedValue(*SetProperty->ElementProp, Helper.GetElementPtr(Index), OutFailureReason))
			{
				return false;
			}
		}
		return true;
	}
	if (const FMapProperty* MapProperty = CastField<FMapProperty>(&Property))
	{
		FScriptMapHelper Helper(MapProperty, Value);
		for (int32 Index = 0; Index < Helper.GetMaxIndex(); ++Index)
		{
			if (!Helper.IsValidIndex(Index))
			{
				continue;
			}
			if (!ValidateReflectedValue(*MapProperty->KeyProp, Helper.GetKeyPtr(Index), OutFailureReason)
				|| !ValidateReflectedValue(*MapProperty->ValueProp, Helper.GetValuePtr(Index), OutFailureReason))
			{
				return false;
			}
		}
		return true;
	}
	if (Property.IsA<FDelegateProperty>() || Property.IsA<FMulticastDelegateProperty>())
	{
		OutFailureReason = LOCTEXT(
			"DelegateInPayload",
			"설비 변환 데이터에는 delegate를 저장할 수 없습니다.");
		return false;
	}
	return true;
}
}

bool FFacilityPlacementPayload::Validate(
	const APlaceableFacilityItemActor& ExpectedOuter,
	FText& OutFailureReason) const
{
	if (!IsValid(Definition.Get()) || !IsValid(InstanceData.Get())
		|| InstanceData->GetOuter() != &ExpectedOuter)
	{
		OutFailureReason = LOCTEXT("InvalidPayloadOwnership", "설비 변환 데이터의 소유 관계가 올바르지 않습니다.");
		return false;
	}

	for (TFieldIterator<FProperty> It(InstanceData->GetClass(), EFieldIteratorFlags::IncludeSuper); It; ++It)
	{
		for (int32 Index = 0; Index < It->ArrayDim; ++Index)
		{
			const void* Value = It->ContainerPtrToValuePtr<void>(InstanceData, Index);
			if (!ValidateReflectedValue(**It, Value, OutFailureReason))
			{
				return false;
			}
		}
	}
	return true;
}

void FFacilityPlacementPayload::Reset()
{
	Definition = nullptr;
	InstanceData = nullptr;
}

#undef LOCTEXT_NAMESPACE
