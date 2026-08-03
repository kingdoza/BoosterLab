#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "BathhouseCustomerAIController.generated.h"

class UStateTreeAIComponent;

UCLASS(Blueprintable)
class BATHHOUSESIM_API ABathhouseCustomerAIController : public AAIController
{
	GENERATED_BODY()

public:
	ABathhouseCustomerAIController();

	UFUNCTION(BlueprintPure, Category = "Customer AI")
	UStateTreeAIComponent* GetCustomerStateTree() const { return CustomerStateTree; }

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Customer AI", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStateTreeAIComponent> CustomerStateTree;
};
