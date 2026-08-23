#include "Customer/CustomerKnockdownComponent.h"

#include "AIController.h"
#include "Combat/HealthComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Customer/BathhouseCustomerCharacter.h"
#include "Customer/CustomerMontagePlaybackComponent.h"
#include "Customer/CustomerRoutineInterruptionComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "PhysicsEngine/BodyInstance.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "TimerManager.h"
#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogBathhouseCustomerKnockdown, Log, All);

UCustomerKnockdownComponent::UCustomerKnockdownComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UCustomerKnockdownComponent::BeginPlay()
{
	Super::BeginPlay();
	Health = GetOwner() ? GetOwner()->FindComponentByClass<UHealthComponent>() : nullptr;
	if (Health)
	{
		Health->OnHealthDepleted.AddDynamic(this, &UCustomerKnockdownComponent::HandleHealthDepleted);
	}
}

void UCustomerKnockdownComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (Health)
	{
		Health->OnHealthDepleted.RemoveDynamic(this, &UCustomerKnockdownComponent::HandleHealthDepleted);
	}
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RecoveryTimerHandle);
	}
	OnCustomerKnockdownStarted.Clear();
	OnCustomerRecovered.Clear();
	Health = nullptr;
	bKnockedDown = false;
	Super::EndPlay(EndPlayReason);
}

void UCustomerKnockdownComponent::HandleHealthDepleted(const FCombatDamageContext& DamageContext)
{
	if (bKnockedDown)
	{
		return;
	}
	ABathhouseCustomerCharacter* Customer = Cast<ABathhouseCustomerCharacter>(GetOwner());
	USkeletalMeshComponent* Mesh = Customer ? Customer->GetMesh() : nullptr;
	UCapsuleComponent* Capsule = Customer ? Customer->GetCapsuleComponent() : nullptr;
	UCharacterMovementComponent* Movement = Customer ? Customer->GetCharacterMovement() : nullptr;
	if (!Customer || !Mesh || !Capsule || !Movement)
	{
		UE_LOG(LogBathhouseCustomerKnockdown, Error, TEXT("Customer knockdown is missing character physics components."));
		return;
	}
	FString RootError;
	if (!HasConfiguredRootBody(&RootError))
	{
		UE_LOG(LogBathhouseCustomerKnockdown, Error, TEXT("%s"), *RootError);
		return;
	}

	bKnockedDown = true;
	SavedActorTransform = Customer->GetActorTransform();
	SavedMeshRelativeTransform = Mesh->GetRelativeTransform();
	LastValidRootWorldTransform = Mesh->GetSocketTransform(RootBoneName, RTS_World);
	SavedCapsuleCollisionProfile = Capsule->GetCollisionProfileName();
	SavedMeshCollisionProfile = Mesh->GetCollisionProfileName();
	SavedCapsuleCollisionEnabled = Capsule->GetCollisionEnabled();
	SavedMeshCollisionEnabled = Mesh->GetCollisionEnabled();
	SavedMovementMode = Movement->MovementMode;
	SavedCustomMovementMode = Movement->CustomMovementMode;

	if (UCustomerRoutineInterruptionComponent* Interruption = Customer->GetCustomerRoutineInterruption())
	{
		Interruption->BeginSoftInterruption();
	}
	if (AAIController* Controller = Cast<AAIController>(Customer->GetController()))
	{
		Controller->StopMovement();
	}
	if (UCustomerMontagePlaybackComponent* Montage = Customer->GetCustomerMontagePlayback())
	{
		Montage->InterruptActivePlayback(0.0f);
	}
	Movement->StopMovementImmediately();
	Movement->DisableMovement();
	Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCollisionProfileName(RagdollCollisionProfileName);
	Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Mesh->SetAllBodiesSimulatePhysics(true);
	Mesh->SetSimulatePhysics(true);
	Mesh->WakeAllRigidBodies();

	if (FBodyInstance* RootBody = Mesh->GetBodyInstance(RootBoneName))
	{
		const FVector Direction = DamageContext.CameraDirection.GetSafeNormal();
		RootBody->AddImpulse(
			Direction * FMath::Max(0.0f, DamageContext.ImpulseStrength)
				+ FVector::UpVector * DamageContext.VerticalImpulse,
			true);
	}
	OnCustomerKnockdownStarted.Broadcast();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			RecoveryTimerHandle,
			this,
			&UCustomerKnockdownComponent::RecoverCustomer,
			FMath::Max(0.1f, KnockdownDurationSeconds),
			false);
	}
}

void UCustomerKnockdownComponent::RecoverCustomer()
{
	if (!bKnockedDown)
	{
		return;
	}
	ABathhouseCustomerCharacter* Customer = Cast<ABathhouseCustomerCharacter>(GetOwner());
	USkeletalMeshComponent* Mesh = Customer ? Customer->GetMesh() : nullptr;
	UCapsuleComponent* Capsule = Customer ? Customer->GetCapsuleComponent() : nullptr;
	UCharacterMovementComponent* Movement = Customer ? Customer->GetCharacterMovement() : nullptr;
	if (!Customer || !Mesh || !Capsule || !Movement)
	{
		return;
	}
	FTransform FinalRootTransform = Mesh->GetSocketTransform(RootBoneName, RTS_World);
	if (!FinalRootTransform.GetLocation().ContainsNaN())
	{
		LastValidRootWorldTransform = FinalRootTransform;
	}
	const FVector RootLocation = LastValidRootWorldTransform.GetLocation();
	FVector RecoveryLocation(RootLocation.X, RootLocation.Y, SavedActorTransform.GetLocation().Z);
	if (UWorld* World = GetWorld())
	{
		FHitResult FloorHit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(CustomerRecoveryFloor), false, Customer);
		const FVector Start = RootLocation + FVector::UpVector * FloorTraceDistance;
		const FVector End = RootLocation - FVector::UpVector * FloorTraceDistance;
		if (World->LineTraceSingleByChannel(FloorHit, Start, End, FloorTraceChannel, Params))
		{
			RecoveryLocation.Z = FloorHit.ImpactPoint.Z + Capsule->GetScaledCapsuleHalfHeight();
		}
	}

	Mesh->SetSimulatePhysics(false);
	Mesh->SetAllBodiesSimulatePhysics(false);
	Mesh->SetCollisionProfileName(SavedMeshCollisionProfile);
	Mesh->SetCollisionEnabled(SavedMeshCollisionEnabled);
	Mesh->SetRelativeTransform(SavedMeshRelativeTransform);
	Capsule->SetCollisionProfileName(SavedCapsuleCollisionProfile);
	Capsule->SetCollisionEnabled(SavedCapsuleCollisionEnabled);
	Customer->SetActorLocationAndRotation(
		RecoveryLocation,
		SavedActorTransform.Rotator(),
		false,
		nullptr,
		ETeleportType::TeleportPhysics);
	const EMovementMode RestoredMode = SavedMovementMode != MOVE_None ? SavedMovementMode.GetValue() : MOVE_Walking;
	Movement->SetMovementMode(RestoredMode, RestoredMode == MOVE_Custom ? SavedCustomMovementMode : 0);
	if (Health)
	{
		Health->RestoreHealthToRatio(FMath::Clamp(RecoveryHealthRatio, 0.01f, 1.0f));
	}
	bKnockedDown = false;
	if (UCustomerRoutineInterruptionComponent* Interruption = Customer->GetCustomerRoutineInterruption())
	{
		Interruption->EndSoftInterruption();
	}
	OnCustomerRecovered.Broadcast();
}

bool UCustomerKnockdownComponent::HasConfiguredRootBody(FString* OutError) const
{
	const ABathhouseCustomerCharacter* Customer = Cast<ABathhouseCustomerCharacter>(GetOwner());
	const USkeletalMeshComponent* Mesh = Customer ? Customer->GetMesh() : nullptr;
	const UPhysicsAsset* PhysicsAsset = Mesh ? Mesh->GetPhysicsAsset() : nullptr;
	const bool bValid = Mesh
		&& !RootBoneName.IsNone()
		&& Mesh->GetBoneIndex(RootBoneName) != INDEX_NONE
		&& PhysicsAsset
		&& PhysicsAsset->FindBodyIndex(RootBoneName) != INDEX_NONE;
	if (!bValid && OutError)
	{
		*OutError = FString::Printf(
			TEXT("Customer %s requires PhysicsAsset body '%s' for knockdown; no fallback body is used."),
			*GetNameSafe(GetOwner()),
			*RootBoneName.ToString());
	}
	return bValid;
}

#if WITH_EDITOR
EDataValidationResult UCustomerKnockdownComponent::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	FString Error;
	if (GetOwner() && !HasConfiguredRootBody(&Error))
	{
		Context.AddError(FText::FromString(Error));
		return EDataValidationResult::Invalid;
	}
	return Result == EDataValidationResult::NotValidated ? EDataValidationResult::Valid : Result;
}
#endif
