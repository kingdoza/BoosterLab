#include "Facility/BathhouseExpansionAuthority.h"

#include "Facility/BathhouseExpansionDefinition.h"
#include "Facility/BathhouseFacilitySubsystem.h"

#define LOCTEXT_NAMESPACE "BathhouseExpansionAuthority"

ABathhouseExpansionAuthority::ABathhouseExpansionAuthority()
{
	PrimaryActorTick.bCanEverTick = false;
}

void ABathhouseExpansionAuthority::BeginPlay()
{
	Super::BeginPlay();
	CurrentTierIndex = ExpansionDefinition && ExpansionDefinition->GetTier(InitialTierIndex)
		? InitialTierIndex
		: INDEX_NONE;
	FText FailureReason;
	if (CurrentTierIndex == INDEX_NONE)
	{
		FailureReason = LOCTEXT("InvalidInitialTier", "확장 Definition 또는 initial tier가 올바르지 않습니다.");
	}
	else if (UBathhouseFacilitySubsystem* Subsystem = GetWorld()->GetSubsystem<UBathhouseFacilitySubsystem>())
	{
		bRegistered = Subsystem->RegisterExpansionAuthority(this, FailureReason);
	}
	if (!bRegistered)
	{
		UE_LOG(LogTemp, Error, TEXT("Expansion authority %s was disabled: %s"), *GetName(), *FailureReason.ToString());
	}
}

void ABathhouseExpansionAuthority::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (bRegistered)
	{
		if (UBathhouseFacilitySubsystem* Subsystem = GetWorld() ? GetWorld()->GetSubsystem<UBathhouseFacilitySubsystem>() : nullptr)
		{
			Subsystem->UnregisterExpansionAuthority(this);
		}
	}
	bRegistered = false;
	OnExpansionTierChanged.Clear();
	Super::EndPlay(EndPlayReason);
}

int32 ABathhouseExpansionAuthority::GetCurrentKeyPoolSize() const
{
	const FBathhouseExpansionTier* Tier = ExpansionDefinition ? ExpansionDefinition->GetTier(CurrentTierIndex) : nullptr;
	return Tier ? Tier->KeyPoolSize : 0;
}

int32 ABathhouseExpansionAuthority::GetCurrentMaxInstalledLockerSlots() const
{
	const FBathhouseExpansionTier* Tier = ExpansionDefinition ? ExpansionDefinition->GetTier(CurrentTierIndex) : nullptr;
	return Tier ? Tier->MaxInstalledLockerSlots : 0;
}

bool ABathhouseExpansionAuthority::TryAdvanceToTier(const int32 NewTierIndex, FText& OutFailureReason)
{
	if (!bRegistered || !ExpansionDefinition || !ExpansionDefinition->GetTier(NewTierIndex))
	{
		OutFailureReason = LOCTEXT("InvalidTier", "요청한 확장 단계를 사용할 수 없습니다.");
		return false;
	}
	if (NewTierIndex <= CurrentTierIndex)
	{
		OutFailureReason = LOCTEXT("NotAnUpgrade", "확장 단계는 현재 단계보다 높은 단계로만 변경할 수 있습니다.");
		return false;
	}
	CurrentTierIndex = NewTierIndex;
	OnExpansionTierChanged.Broadcast(CurrentTierIndex);
	if (UBathhouseFacilitySubsystem* Subsystem = GetWorld()->GetSubsystem<UBathhouseFacilitySubsystem>())
	{
		Subsystem->OnExpansionAuthorityChanged.Broadcast(this);
	}
	return true;
}

#undef LOCTEXT_NAMESPACE
