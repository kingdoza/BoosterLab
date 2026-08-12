#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Towel/TowelTypes.h"
#include "TowelProcessingMachineActor.generated.h"

class USceneComponent;
class UTowelInventoryComponent;
class UTowelMachineControlComponent;
class UTowelPileVisualComponent;
class UTowelTransferPortComponent;
class UTowelTransferSubsystem;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOnTowelMachineStateChanged,
	ETowelMachineState,
	PreviousState,
	ETowelMachineState,
	NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTowelMachineProgressChanged, float, Progress);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOnTowelMachineContentsChanged,
	const FTowelInventorySnapshot&,
	Snapshot);

UCLASS(Blueprintable)
class BATHHOUSESIM_API ATowelProcessingMachineActor : public AActor
{
	GENERATED_BODY()

public:
	ATowelProcessingMachineActor();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaTime) override;

	UFUNCTION(BlueprintPure, Category = "Towel Machine")
	ETowelMachineState GetMachineState() const { return MachineState; }

	UFUNCTION(BlueprintPure, Category = "Towel Machine")
	ETowelMachineKind GetMachineKind() const { return MachineKind; }

	UFUNCTION(BlueprintPure, Category = "Towel Machine")
	float GetProcessingProgress() const;

	UFUNCTION(BlueprintPure, Category = "Towel Machine")
	UTowelInventoryComponent* GetInventory() const { return Inventory; }

	ETowelState GetInputState() const;
	ETowelState GetOutputState() const;
	bool CanStartProcessing(FText& OutFailureReason) const;
	bool StartProcessing(FText& OutFailureReason);

	UPROPERTY(BlueprintAssignable, Category = "Towel Machine|Presentation")
	FOnTowelMachineStateChanged OnMachineStateChanged;

	UPROPERTY(BlueprintAssignable, Category = "Towel Machine|Presentation")
	FOnTowelMachineProgressChanged OnMachineProgressChanged;

	UPROPERTY(BlueprintAssignable, Category = "Towel Machine|Presentation")
	FOnTowelMachineContentsChanged OnMachineContentsChanged;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Towel Machine")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Towel Machine")
	TObjectPtr<UTowelInventoryComponent> Inventory;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Towel Machine")
	TObjectPtr<UTowelTransferPortComponent> TransferPort;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Towel Machine")
	TObjectPtr<UTowelMachineControlComponent> MachineControl;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Towel Machine|Presentation")
	TObjectPtr<UTowelPileVisualComponent> TowelPresentationVisual;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Towel Machine")
	ETowelMachineKind MachineKind = ETowelMachineKind::Washer;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Towel Machine", meta = (ClampMin = "0.1"))
	float ProcessingDurationSeconds = 10.0f;

private:
	friend class FBathhouseTowelTransferTest;
	friend class UTowelTransferSubsystem;

	bool AllowsInventoryTransfer(
		const UTowelInventoryComponent* Source,
		const UTowelInventoryComponent* Destination,
		const FTowelInventorySnapshot& SourceSnapshot,
		const FTowelInventorySnapshot& DestinationSnapshot) const;
	void HandleCommittedInventoryTransfer(const UTowelInventoryComponent* Source);

	UFUNCTION()
	void HandleInventoryChanged(
		const FTowelInventorySnapshot& Previous,
		const FTowelInventorySnapshot& Current,
		int64 TransactionId);

	void CompleteProcessing();
	void CommitMachineState(ETowelMachineState NewState);

	ETowelMachineState MachineState = ETowelMachineState::Waiting;
	double ProcessingEndTime = 0.0;
	FTimerHandle ProcessingTimerHandle;
};
