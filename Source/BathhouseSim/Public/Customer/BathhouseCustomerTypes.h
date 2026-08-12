#pragma once

#include "CoreMinimal.h"
#include "NativeGameplayTags.h"
#include "BathhouseCustomerTypes.generated.h"

UENUM(BlueprintType)
enum class EBathhouseCustomerActivity : uint8
{
	None,
	StoreShoes,
	Undress,
	PreShower,
	BathDwell,
	MainShower,
	Drying,
	ReturnTowel,
	Dress,
	WearShoes
};

UENUM(BlueprintType)
enum class EBathhouseCustomerPresentationState : uint8
{
	Entering,
	QueueingCheckIn,
	WaitingForKey,
	UsingFacility,
	Bathing,
	QueueingCheckout,
	OfferingPayment,
	Leaving
};

UENUM(BlueprintType)
enum class EBathhouseCustomerDepartureReason : uint8
{
	None,
	Completed,
	CheckInTimedOut,
	TechnicalAbort
};

UENUM(BlueprintType)
enum class ECustomerFacilitySnapTarget : uint8
{
	ActionPoint,
	ApproachPoint
};

UENUM()
enum class ECustomerMontagePlaybackResult : uint8
{
	Invalid,
	Playing,
	Succeeded,
	Interrupted
};

UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Customer_Event_KeyReceived);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Customer_Event_CheckInTimedOut);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Customer_Event_FacilityAvailable);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Customer_Event_BathStayExpired);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Customer_Event_CashClaimed);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Customer_Event_QueueChanged);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Customer_Event_TowelAvailable);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Customer_Event_TowelWaitExpired);
