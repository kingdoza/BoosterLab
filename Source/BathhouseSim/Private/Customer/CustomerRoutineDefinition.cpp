#include "Customer/CustomerRoutineDefinition.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

float UCustomerRoutineDefinition::GetActivityDuration(const EBathhouseCustomerActivity Activity) const
{
	switch (Activity)
	{
	case EBathhouseCustomerActivity::StoreShoes:
		return StoreShoesSeconds;
	case EBathhouseCustomerActivity::Undress:
		return UndressSeconds;
	case EBathhouseCustomerActivity::PreShower:
		return PreShowerSeconds;
	case EBathhouseCustomerActivity::BathDwell:
		return FMath::FRandRange(
			FMath::Min(BathDwellMinSeconds, BathDwellMaxSeconds),
			FMath::Max(BathDwellMinSeconds, BathDwellMaxSeconds));
	case EBathhouseCustomerActivity::MainShower:
		return MainShowerSeconds;
	case EBathhouseCustomerActivity::Drying:
		return DryingSeconds;
	case EBathhouseCustomerActivity::ReturnTowel:
		return ReturnTowelSeconds;
	case EBathhouseCustomerActivity::Dress:
		return DressSeconds;
	case EBathhouseCustomerActivity::WearShoes:
		return WearShoesSeconds;
	default:
		return 0.0f;
	}
}

#if WITH_EDITOR
EDataValidationResult UCustomerRoutineDefinition::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	auto RequirePositive = [&Context, &Result](const float Value, const TCHAR* Name)
	{
		if (!FMath::IsFinite(Value) || Value <= 0.0f)
		{
			Context.AddError(FText::Format(
				NSLOCTEXT("CustomerRoutineDefinition", "QueuePositive", "{0} must be finite and greater than zero."),
				FText::FromString(Name)));
			Result = EDataValidationResult::Invalid;
		}
	};
	RequirePositive(QueueAcceptanceRadius, TEXT("QueueAcceptanceRadius"));
	RequirePositive(QueueFacingRotationSpeedDegrees, TEXT("QueueFacingRotationSpeedDegrees"));
	RequirePositive(QueueFacingToleranceDegrees, TEXT("QueueFacingToleranceDegrees"));
	RequirePositive(OverflowWanderAcceptanceRadius, TEXT("OverflowWanderAcceptanceRadius"));
	if (!FMath::IsFinite(OverflowPauseMinSeconds) || !FMath::IsFinite(OverflowPauseMaxSeconds)
		|| OverflowPauseMinSeconds < 0.0f || OverflowPauseMaxSeconds < 0.0f)
	{
		Context.AddError(NSLOCTEXT(
			"CustomerRoutineDefinition",
			"OverflowPauseNonNegative",
			"Overflow wander pause bounds must be finite and non-negative."));
		Result = EDataValidationResult::Invalid;
	}
	else if (OverflowPauseMinSeconds > OverflowPauseMaxSeconds)
	{
		Context.AddError(NSLOCTEXT(
			"CustomerRoutineDefinition",
			"OverflowPauseOrder",
			"OverflowPauseMinSeconds cannot exceed OverflowPauseMaxSeconds."));
		Result = EDataValidationResult::Invalid;
	}
	return Result;
}
#endif
