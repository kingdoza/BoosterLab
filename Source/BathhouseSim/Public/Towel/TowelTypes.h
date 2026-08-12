#pragma once

#include "CoreMinimal.h"
#include "TowelTypes.generated.h"

class ACleanTowelStackActor;
class UTowelInventoryComponent;

UENUM(BlueprintType)
enum class ETowelState : uint8
{
	None,
	Used,
	Wet,
	Clean
};

UENUM(BlueprintType)
enum class ETowelMachineState : uint8
{
	Waiting,
	Processing,
	Complete
};

UENUM(BlueprintType)
enum class ETowelMachineKind : uint8
{
	Washer,
	Dryer
};

UENUM(BlueprintType)
enum class ETowelTransferFailure : uint8
{
	None,
	InvalidEndpoint,
	InvalidCount,
	RevisionMismatch,
	SourceEmpty,
	DestinationFull,
	StateMismatch,
	EndpointBlocked,
	Reentry
};

USTRUCT(BlueprintType)
struct BATHHOUSESIM_API FTowelInventorySnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Towel")
	ETowelState State = ETowelState::None;

	UPROPERTY(BlueprintReadOnly, Category = "Towel")
	int32 Count = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Towel")
	int32 Capacity = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Towel")
	int64 Revision = 0;

	bool IsValid() const
	{
		return Capacity >= 0 && Count >= 0 && Count <= Capacity
			&& ((Count == 0 && State == ETowelState::None)
				|| (Count > 0 && State != ETowelState::None));
	}
};

USTRUCT()
struct BATHHOUSESIM_API FTowelTransferRequest
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<UTowelInventoryComponent> Source = nullptr;

	UPROPERTY()
	TObjectPtr<UTowelInventoryComponent> Destination = nullptr;

	int32 RequestedCount = 1;
	int64 ExpectedSourceRevision = MAX_int64;
	int64 ExpectedDestinationRevision = MAX_int64;
};

USTRUCT(BlueprintType)
struct BATHHOUSESIM_API FTowelTransferResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Towel")
	bool bSucceeded = false;

	UPROPERTY(BlueprintReadOnly, Category = "Towel")
	int32 MovedCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Towel")
	ETowelTransferFailure Failure = ETowelTransferFailure::None;

	UPROPERTY(BlueprintReadOnly, Category = "Towel")
	int64 CommittedSourceRevision = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Towel")
	int64 CommittedDestinationRevision = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Towel")
	int64 TransactionId = 0;
};

USTRUCT(BlueprintType)
struct BATHHOUSESIM_API FTowelUseHandle
{
	GENERATED_BODY()

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Towel")
	FGuid Token;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Towel")
	TObjectPtr<ACleanTowelStackActor> OriginalStack = nullptr;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Towel")
	bool bUsed = false;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Towel")
	bool bTerminal = false;

	bool HasToken() const { return Token.IsValid() && !bTerminal; }
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FOnTowelInventoryChanged,
	const FTowelInventorySnapshot&,
	Previous,
	const FTowelInventorySnapshot&,
	Current,
	int64,
	TransactionId);
