#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Customer/BathhouseCustomerTypes.h"
#include "CustomerMontagePlaybackComponent.generated.h"

class UAnimInstance;
class UAnimMontage;

UCLASS(ClassGroup = (Customer), meta = (BlueprintSpawnableComponent))
class BATHHOUSESIM_API UCustomerMontagePlaybackComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCustomerMontagePlaybackComponent();

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	bool PlayMontage(
		UAnimMontage* Montage,
		float PlayRate,
		float BlendInTime,
		FName StartSection,
		FName LoopSection,
		uint64& OutPlaybackToken);
	bool StopPlayback(uint64 PlaybackToken, float BlendOutTime);
	ECustomerMontagePlaybackResult GetPlaybackResult(uint64 PlaybackToken) const;
	bool IsPlaybackTokenCurrent(uint64 PlaybackToken) const;
	void InterruptActivePlayback(float BlendOutTime);

private:
	friend class FBathhouseCustomerMontageTest;

	uint64 AllocatePlaybackToken();
	void ArchiveCurrentPlayback();
	void InterruptCurrentPlayback(float BlendOutTime);
	void UnbindCurrentMontageDelegate();
	void HandleMontageEnded(UAnimMontage* Montage, bool bInterrupted, uint64 PlaybackToken);

	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> CurrentMontage = nullptr;

	TWeakObjectPtr<UAnimInstance> CurrentAnimInstance;
	uint64 NextPlaybackToken = 1;
	uint64 CurrentPlaybackToken = 0;
	uint64 PreviousPlaybackToken = 0;
	ECustomerMontagePlaybackResult CurrentResult = ECustomerMontagePlaybackResult::Invalid;
	ECustomerMontagePlaybackResult PreviousResult = ECustomerMontagePlaybackResult::Invalid;
};
