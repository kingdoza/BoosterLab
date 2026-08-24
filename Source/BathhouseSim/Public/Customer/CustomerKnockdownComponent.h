#pragma once

#include "CoreMinimal.h"
#include "Combat/CombatTypes.h"
#include "Components/ActorComponent.h"
#include "Engine/EngineTypes.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "CustomerKnockdownComponent.generated.h"

class UHealthComponent;
class UCapsuleComponent;
class USkeletalMeshComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnCustomerKnockdownPresentationEvent);

UCLASS(ClassGroup = (Customer), meta = (BlueprintSpawnableComponent))
class BATHHOUSESIM_API UCustomerKnockdownComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCustomerKnockdownComponent();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION(BlueprintPure, Category = "Customer|Recovery")
	bool IsKnockedDown() const { return bKnockedDown; }

	UPROPERTY(BlueprintAssignable, Category = "Customer|Recovery|Presentation")
	FOnCustomerKnockdownPresentationEvent OnCustomerKnockdownStarted;

	UPROPERTY(BlueprintAssignable, Category = "Customer|Recovery|Presentation")
	FOnCustomerKnockdownPresentationEvent OnCustomerRecovered;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Customer|Recovery", meta = (ClampMin = "0.1"))
	float KnockdownDurationSeconds = 4.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Customer|Recovery", meta = (ClampMin = "0.01", ClampMax = "1.0"))
	float RecoveryHealthRatio = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Customer|Recovery")
	FName RootBoneName = TEXT("root");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Customer|Recovery")
	FName RagdollCollisionProfileName = TEXT("Ragdoll");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Customer|Recovery")
	TEnumAsByte<ECollisionChannel> FloorTraceChannel = ECC_Visibility;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Customer|Recovery", meta = (ClampMin = "1.0"))
	float FloorTraceDistance = 300.0f;

private:
	friend class FBathhouseCustomerKnockdownTest;

	UFUNCTION()
	void HandleHealthDepleted(const FCombatDamageContext& DamageContext);

	void RecoverCustomer();
	void RestoreCollisionState(USkeletalMeshComponent& Mesh, UCapsuleComponent& Capsule);
	bool HasConfiguredRootBody(FString* OutError = nullptr) const;

	UPROPERTY(Transient)
	TObjectPtr<UHealthComponent> Health = nullptr;

	FTransform SavedActorTransform;
	FTransform SavedMeshRelativeTransform;
	FTransform LastValidRootWorldTransform;
	FName SavedCapsuleCollisionProfile;
	FName SavedMeshCollisionProfile;
	TEnumAsByte<ECollisionEnabled::Type> SavedCapsuleCollisionEnabled = ECollisionEnabled::QueryAndPhysics;
	TEnumAsByte<ECollisionEnabled::Type> SavedMeshCollisionEnabled = ECollisionEnabled::QueryOnly;
	TEnumAsByte<ECollisionChannel> SavedMeshCollisionObjectType = ECC_Pawn;
	FCollisionResponseContainer SavedMeshCollisionResponses;
	TEnumAsByte<EMovementMode> SavedMovementMode = MOVE_Walking;
	uint8 SavedCustomMovementMode = 0;
	FTimerHandle RecoveryTimerHandle;
	bool bKnockedDown = false;
};
