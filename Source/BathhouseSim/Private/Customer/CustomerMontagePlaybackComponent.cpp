#include "Customer/CustomerMontagePlaybackComponent.h"

#include "AlphaBlend.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"

UCustomerMontagePlaybackComponent::UCustomerMontagePlaybackComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UCustomerMontagePlaybackComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	InterruptCurrentPlayback(0.0f);
	UnbindCurrentMontageDelegate();
	CurrentAnimInstance.Reset();
	CurrentMontage = nullptr;
	CurrentPlaybackToken = 0;
	PreviousPlaybackToken = 0;
	CurrentResult = ECustomerMontagePlaybackResult::Invalid;
	PreviousResult = ECustomerMontagePlaybackResult::Invalid;
	Super::EndPlay(EndPlayReason);
}

bool UCustomerMontagePlaybackComponent::PlayMontage(
	UAnimMontage* Montage,
	const float PlayRate,
	const float BlendInTime,
	const FName StartSection,
	const FName LoopSection,
	uint64& OutPlaybackToken)
{
	OutPlaybackToken = 0;
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	USkeletalMeshComponent* Mesh = Character ? Character->GetMesh() : nullptr;
	UAnimInstance* AnimInstance = Mesh ? Mesh->GetAnimInstance() : nullptr;
	if (!Montage || !AnimInstance || PlayRate <= 0.0f
		|| (!StartSection.IsNone() && !Montage->IsValidSectionName(StartSection))
		|| (!LoopSection.IsNone() && !Montage->IsValidSectionName(LoopSection)))
	{
		return false;
	}

	InterruptCurrentPlayback(0.0f);
	ArchiveCurrentPlayback();
	const float PlaybackLength = AnimInstance->Montage_PlayWithBlendIn(
		Montage,
		FAlphaBlendArgs(FMath::Max(0.0f, BlendInTime)),
		PlayRate,
		EMontagePlayReturnType::MontageLength,
		0.0f,
		false);
	if (PlaybackLength <= 0.0f)
	{
		return false;
	}

	CurrentPlaybackToken = AllocatePlaybackToken();
	CurrentMontage = Montage;
	CurrentAnimInstance = AnimInstance;
	CurrentResult = ECustomerMontagePlaybackResult::Playing;
	FOnMontageEnded EndDelegate;
	EndDelegate.BindUObject(
		this,
		&UCustomerMontagePlaybackComponent::HandleMontageEnded,
		CurrentPlaybackToken);
	AnimInstance->Montage_SetEndDelegate(EndDelegate, Montage);

	if (!StartSection.IsNone())
	{
		AnimInstance->Montage_JumpToSection(StartSection, Montage);
	}
	if (!LoopSection.IsNone())
	{
		AnimInstance->Montage_SetNextSection(LoopSection, LoopSection, Montage);
	}

	OutPlaybackToken = CurrentPlaybackToken;
	return true;
}

bool UCustomerMontagePlaybackComponent::StopPlayback(const uint64 PlaybackToken, const float BlendOutTime)
{
	if (!IsPlaybackTokenCurrent(PlaybackToken))
	{
		return false;
	}
	InterruptCurrentPlayback(BlendOutTime);
	return true;
}

ECustomerMontagePlaybackResult UCustomerMontagePlaybackComponent::GetPlaybackResult(const uint64 PlaybackToken) const
{
	if (PlaybackToken != 0 && PlaybackToken == CurrentPlaybackToken)
	{
		return CurrentResult;
	}
	if (PlaybackToken != 0 && PlaybackToken == PreviousPlaybackToken)
	{
		return PreviousResult;
	}
	return ECustomerMontagePlaybackResult::Invalid;
}

bool UCustomerMontagePlaybackComponent::IsPlaybackTokenCurrent(const uint64 PlaybackToken) const
{
	return PlaybackToken != 0
		&& PlaybackToken == CurrentPlaybackToken
		&& CurrentResult == ECustomerMontagePlaybackResult::Playing;
}

void UCustomerMontagePlaybackComponent::InterruptActivePlayback(const float BlendOutTime)
{
	InterruptCurrentPlayback(BlendOutTime);
}

uint64 UCustomerMontagePlaybackComponent::AllocatePlaybackToken()
{
	uint64 Token = NextPlaybackToken++;
	if (Token == 0)
	{
		Token = NextPlaybackToken++;
	}
	return Token;
}

void UCustomerMontagePlaybackComponent::ArchiveCurrentPlayback()
{
	if (CurrentPlaybackToken != 0)
	{
		PreviousPlaybackToken = CurrentPlaybackToken;
		PreviousResult = CurrentResult;
	}
	UnbindCurrentMontageDelegate();
	CurrentAnimInstance.Reset();
	CurrentMontage = nullptr;
	CurrentPlaybackToken = 0;
	CurrentResult = ECustomerMontagePlaybackResult::Invalid;
}

void UCustomerMontagePlaybackComponent::InterruptCurrentPlayback(const float BlendOutTime)
{
	if (CurrentPlaybackToken == 0 || CurrentResult != ECustomerMontagePlaybackResult::Playing)
	{
		return;
	}

	UAnimInstance* AnimInstance = CurrentAnimInstance.Get();
	UAnimMontage* Montage = CurrentMontage.Get();
	UnbindCurrentMontageDelegate();
	CurrentResult = ECustomerMontagePlaybackResult::Interrupted;
	CurrentAnimInstance.Reset();
	CurrentMontage = nullptr;
	if (AnimInstance && Montage)
	{
		AnimInstance->Montage_Stop(FMath::Max(0.0f, BlendOutTime), Montage);
	}
}

void UCustomerMontagePlaybackComponent::UnbindCurrentMontageDelegate()
{
	if (UAnimInstance* AnimInstance = CurrentAnimInstance.Get())
	{
		if (FOnMontageEnded* EndDelegate = AnimInstance->Montage_GetEndedDelegate(CurrentMontage.Get()))
		{
			EndDelegate->Unbind();
		}
	}
}

void UCustomerMontagePlaybackComponent::HandleMontageEnded(
	UAnimMontage* Montage,
	const bool bInterrupted,
	const uint64 PlaybackToken)
{
	if (PlaybackToken != CurrentPlaybackToken || Montage != CurrentMontage)
	{
		return;
	}
	CurrentResult = bInterrupted
		? ECustomerMontagePlaybackResult::Interrupted
		: ECustomerMontagePlaybackResult::Succeeded;
	CurrentAnimInstance.Reset();
	CurrentMontage = nullptr;
}
