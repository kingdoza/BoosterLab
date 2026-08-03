#include "Customer/BathhouseCustomerAIController.h"

#include "Components/StateTreeAIComponent.h"

ABathhouseCustomerAIController::ABathhouseCustomerAIController()
{
	CustomerStateTree = CreateDefaultSubobject<UStateTreeAIComponent>(TEXT("CustomerStateTree"));
	BrainComponent = CustomerStateTree;
}
