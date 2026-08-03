#include "Customer/CustomerRoutineDefinition.h"

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
