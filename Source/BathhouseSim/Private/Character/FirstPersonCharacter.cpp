// Copyright Epic Games, Inc. All Rights Reserved.

#include "Character/FirstPersonCharacter.h"

#include "Camera/CameraComponent.h"
#include "Camera/FirstPersonCameraShakeComponent.h"
#include "Character/FirstPersonMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "InputActionValue.h"
#include "Interaction/PlayerCarryComponent.h"
#include "Interaction/PlayerInteractionComponent.h"

AFirstPersonCharacter::AFirstPersonCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UFirstPersonMovementComponent>(ACharacter::CharacterMovementComponentName))
{
	FirstPersonMovement = Cast<UFirstPersonMovementComponent>(GetCharacterMovement());
	FirstPersonCameraShake = CreateDefaultSubobject<UFirstPersonCameraShakeComponent>(TEXT("FirstPersonCameraShake"));

	FirstPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	FirstPersonCamera->SetupAttachment(GetCapsuleComponent());
	FirstPersonCamera->SetRelativeLocation(FVector(-10.0f, 0.0f, 70.0f));
	FirstPersonCamera->bUsePawnControlRotation = true;

	HeldKeyAnchor = CreateDefaultSubobject<USceneComponent>(TEXT("HeldKeyAnchor"));
	HeldKeyAnchor->SetupAttachment(FirstPersonCamera);
	HeldKeyAnchor->SetRelativeLocation(FVector(45.0f, 18.0f, -18.0f));

	PlayerCarry = CreateDefaultSubobject<UPlayerCarryComponent>(TEXT("PlayerCarry"));
	PlayerCarry->ConfigureHeldAnchor(HeldKeyAnchor);
	PlayerInteraction = CreateDefaultSubobject<UPlayerInteractionComponent>(TEXT("PlayerInteraction"));
	PlayerInteraction->Configure(FirstPersonCamera, PlayerCarry);

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = true;
	bUseControllerRotationRoll = false;

	GetCapsuleComponent()->SetCapsuleSize(34.0f, 96.0f);

	if (UCharacterMovementComponent* MovementComponent = GetCharacterMovement())
	{
		MovementComponent->bOrientRotationToMovement = false;
		MovementComponent->BrakingDecelerationFalling = 1500.0f;
		MovementComponent->AirControl = 0.5f;
	}

	if (USkeletalMeshComponent* CharacterMesh = GetMesh())
	{
		CharacterMesh->SetOwnerNoSee(true);
		CharacterMesh->CastShadow = false;
		CharacterMesh->bHiddenInGame = true;
	}
}

void AFirstPersonCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!EnhancedInputComponent)
	{
		return;
	}

	if (MoveAction)
	{
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AFirstPersonCharacter::MoveInput);
	}

	if (LookAction)
	{
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &AFirstPersonCharacter::LookInput);
	}

	if (JumpAction)
	{
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &AFirstPersonCharacter::DoJumpStart);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &AFirstPersonCharacter::DoJumpEnd);
	}

	if (SprintAction)
	{
		EnhancedInputComponent->BindAction(SprintAction, ETriggerEvent::Started, this, &AFirstPersonCharacter::SprintStartInput);
		EnhancedInputComponent->BindAction(SprintAction, ETriggerEvent::Completed, this, &AFirstPersonCharacter::SprintReleaseInput);
	}

	if (InteractAction)
	{
		EnhancedInputComponent->BindAction(InteractAction, ETriggerEvent::Started, this, &AFirstPersonCharacter::InteractInput);
	}
}

void AFirstPersonCharacter::MoveInput(const FInputActionValue& Value)
{
	const FVector2D MovementValue = Value.Get<FVector2D>() * MoveSpeedScale;
	DoMove(MovementValue.X, MovementValue.Y);
}

void AFirstPersonCharacter::LookInput(const FInputActionValue& Value)
{
	const FVector2D LookAxisValue = Value.Get<FVector2D>() * LookSpeedScale;
	DoLook(LookAxisValue.X, LookAxisValue.Y);
}

void AFirstPersonCharacter::SprintStartInput()
{
	if (!FirstPersonMovement)
	{
		return;
	}

	if (bSprintToggle)
	{
		FirstPersonMovement->SwitchSprinting();
		return;
	}

	FirstPersonMovement->StartSprinting();
}

void AFirstPersonCharacter::SprintReleaseInput()
{
	if (!FirstPersonMovement || bSprintToggle)
	{
		return;
	}

	FirstPersonMovement->StopSprinting();
}

void AFirstPersonCharacter::InteractInput()
{
	if (PlayerInteraction)
	{
		PlayerInteraction->TryInteract();
	}
}

void AFirstPersonCharacter::DoMove(float Right, float Forward)
{
	if (!Controller)
	{
		return;
	}

	AddMovementInput(GetActorRightVector(), Right);
	AddMovementInput(GetActorForwardVector(), Forward);
}

void AFirstPersonCharacter::DoLook(float Yaw, float Pitch)
{
	if (!Controller)
	{
		return;
	}

	AddControllerYawInput(Yaw);
	AddControllerPitchInput(Pitch);
}

void AFirstPersonCharacter::DoJumpStart()
{
	Jump();
}

void AFirstPersonCharacter::DoJumpEnd()
{
	StopJumping();
}
