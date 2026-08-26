// Copyright Epic Games, Inc. All Rights Reserved.

#include "Character/FirstPersonCharacter.h"

#include "Camera/CameraComponent.h"
#include "Camera/FirstPersonCameraShakeComponent.h"
#include "Character/FirstPersonMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/WidgetInteractionComponent.h"
#include "Computer/PlayerComputerUseComponent.h"
#include "EnhancedInputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "InputActionValue.h"
#include "Interaction/PlayerCarryComponent.h"
#include "Interaction/HeldEquipmentMotionComponent.h"
#include "Interaction/PlayerEquipmentUseComponent.h"
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
	HeldEquipmentMotion = CreateDefaultSubobject<UHeldEquipmentMotionComponent>(TEXT("HeldEquipmentMotion"));
	PlayerEquipmentUse = CreateDefaultSubobject<UPlayerEquipmentUseComponent>(TEXT("PlayerEquipmentUse"));
	PlayerEquipmentUse->Configure(FirstPersonCamera, PlayerCarry, PlayerInteraction, HeldEquipmentMotion);
	PlayerInteraction->ConfigureEquipmentUse(PlayerEquipmentUse);
	PlayerCarry->ConfigureEquipmentUse(PlayerEquipmentUse);
	ComputerWidgetInteraction = CreateDefaultSubobject<UWidgetInteractionComponent>(TEXT("ComputerWidgetInteraction"));
	ComputerWidgetInteraction->SetupAttachment(FirstPersonCamera);
	ComputerWidgetInteraction->InteractionSource = EWidgetInteractionSource::Mouse;
	ComputerWidgetInteraction->InteractionDistance = 500.0f;
	ComputerWidgetInteraction->bEnableHitTesting = false;
	ComputerWidgetInteraction->bShowDebug = false;
	PlayerComputerUse = CreateDefaultSubobject<UPlayerComputerUseComponent>(TEXT("PlayerComputerUse"));
	PlayerComputerUse->Configure(
		FirstPersonCamera,
		FirstPersonMovement,
		PlayerInteraction,
		PlayerCarry,
		ComputerWidgetInteraction);

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
		EnhancedInputComponent->BindAction(InteractAction, ETriggerEvent::Started, this, &AFirstPersonCharacter::InteractStartInput);
		EnhancedInputComponent->BindAction(InteractAction, ETriggerEvent::Completed, this, &AFirstPersonCharacter::InteractEndInput);
		EnhancedInputComponent->BindAction(InteractAction, ETriggerEvent::Canceled, this, &AFirstPersonCharacter::InteractEndInput);
	}

	if (SecondaryInteractAction)
	{
		EnhancedInputComponent->BindAction(
			SecondaryInteractAction,
			ETriggerEvent::Started,
			this,
			&AFirstPersonCharacter::SecondaryInteractInput);
	}

	if (DropCarryAction)
	{
		EnhancedInputComponent->BindAction(
			DropCarryAction,
			ETriggerEvent::Started,
			this,
			&AFirstPersonCharacter::DropCarryInput);
	}

	UInputAction* BoundPrimaryUseAction = PrimaryUseAction ? PrimaryUseAction.Get() : ComputerClickAction.Get();
	if (BoundPrimaryUseAction)
	{
		EnhancedInputComponent->BindAction(
			BoundPrimaryUseAction,
			ETriggerEvent::Started,
			this,
			&AFirstPersonCharacter::PrimaryUseStartInput);
		EnhancedInputComponent->BindAction(
			BoundPrimaryUseAction,
			ETriggerEvent::Triggered,
			this,
			&AFirstPersonCharacter::PrimaryUseTriggeredInput);
		EnhancedInputComponent->BindAction(
			BoundPrimaryUseAction,
			ETriggerEvent::Completed,
			this,
			&AFirstPersonCharacter::PrimaryUseEndInput);
		EnhancedInputComponent->BindAction(
			BoundPrimaryUseAction,
			ETriggerEvent::Canceled,
			this,
			&AFirstPersonCharacter::PrimaryUseEndInput);
	}
}

void AFirstPersonCharacter::MoveInput(const FInputActionValue& Value)
{
	if (PlayerComputerUse && PlayerComputerUse->IsCapturingInput())
	{
		return;
	}
	const FVector2D MovementValue = Value.Get<FVector2D>() * MoveSpeedScale;
	DoMove(MovementValue.X, MovementValue.Y);
}

void AFirstPersonCharacter::LookInput(const FInputActionValue& Value)
{
	if (PlayerComputerUse && PlayerComputerUse->IsCapturingInput())
	{
		return;
	}
	const FVector2D LookAxisValue = Value.Get<FVector2D>() * LookSpeedScale;
	DoLook(LookAxisValue.X, LookAxisValue.Y);
}

void AFirstPersonCharacter::SprintStartInput()
{
	if ((PlayerComputerUse && PlayerComputerUse->IsCapturingInput()) || !FirstPersonMovement)
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
	if ((PlayerComputerUse && PlayerComputerUse->IsCapturingInput()) || !FirstPersonMovement || bSprintToggle)
	{
		return;
	}

	FirstPersonMovement->StopSprinting();
}

void AFirstPersonCharacter::InteractStartInput()
{
	if (PlayerComputerUse && PlayerComputerUse->IsCapturingInput())
	{
		bComputerOwnsInteractPress = true;
		PlayerComputerUse->RequestEndComputerUse();
		return;
	}

	bComputerOwnsInteractPress = false;
	if (PlayerInteraction)
	{
		PlayerInteraction->BeginPrimaryInteraction();
	}
	if (PlayerComputerUse && PlayerComputerUse->IsCapturingInput())
	{
		bComputerOwnsInteractPress = true;
	}
}

void AFirstPersonCharacter::InteractEndInput()
{
	if (bComputerOwnsInteractPress)
	{
		bComputerOwnsInteractPress = false;
		return;
	}
	if (PlayerInteraction)
	{
		PlayerInteraction->EndPrimaryInteraction();
	}
}

void AFirstPersonCharacter::SecondaryInteractInput()
{
	if (PlayerComputerUse && PlayerComputerUse->IsCapturingInput())
	{
		return;
	}
	if (PlayerInteraction)
	{
		PlayerInteraction->TrySecondaryInteract();
	}
}

void AFirstPersonCharacter::DropCarryInput()
{
	if (PlayerComputerUse && PlayerComputerUse->IsCapturingInput())
	{
		return;
	}
	if (PlayerInteraction && FirstPersonCamera)
	{
		PlayerInteraction->TryDropCarry(FirstPersonCamera->GetForwardVector());
	}
}

void AFirstPersonCharacter::PrimaryUseStartInput()
{
	if (PrimaryUsePressOwner != EPrimaryUsePressOwner::None)
	{
		return;
	}
	if (PlayerComputerUse && PlayerComputerUse->IsCapturingInput())
	{
		PrimaryUsePressOwner = EPrimaryUsePressOwner::Computer;
		bComputerOwnsPointerPress = PlayerComputerUse->PressPointer();
		return;
	}
	PrimaryUsePressOwner = EPrimaryUsePressOwner::Equipment;
	if (PlayerEquipmentUse)
	{
		PlayerEquipmentUse->BeginEquipmentUse();
	}
}

void AFirstPersonCharacter::PrimaryUseTriggeredInput()
{
	if (PrimaryUsePressOwner == EPrimaryUsePressOwner::Equipment && PlayerEquipmentUse)
	{
		PlayerEquipmentUse->UpdateEquipmentUse(GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.0f);
	}
}

void AFirstPersonCharacter::PrimaryUseEndInput()

{
	const EPrimaryUsePressOwner PreviousOwner = PrimaryUsePressOwner;
	PrimaryUsePressOwner = EPrimaryUsePressOwner::None;
	if (PreviousOwner == EPrimaryUsePressOwner::Computer)
	{
		if (bComputerOwnsPointerPress && PlayerComputerUse)
		{
			PlayerComputerUse->ReleasePointer();
		}
		bComputerOwnsPointerPress = false;
		return;
	}
	if (PreviousOwner == EPrimaryUsePressOwner::Equipment && PlayerEquipmentUse)
	{
		PlayerEquipmentUse->EndEquipmentUse();
	}
}

void AFirstPersonCharacter::ComputerClickStartInput()
{
	PrimaryUseStartInput();
}

void AFirstPersonCharacter::ComputerClickEndInput()
{
	PrimaryUseEndInput();
}

void AFirstPersonCharacter::DoMove(float Right, float Forward)
{
	if ((PlayerComputerUse && PlayerComputerUse->IsCapturingInput()) || !Controller)
	{
		return;
	}

	AddMovementInput(GetActorRightVector(), Right);
	AddMovementInput(GetActorForwardVector(), Forward);
}

void AFirstPersonCharacter::DoLook(float Yaw, float Pitch)
{
	if ((PlayerComputerUse && PlayerComputerUse->IsCapturingInput()) || !Controller)
	{
		return;
	}

	AddControllerYawInput(Yaw);
	AddControllerPitchInput(Pitch);
}

void AFirstPersonCharacter::DoJumpStart()
{
	if (PlayerComputerUse && PlayerComputerUse->IsCapturingInput())
	{
		return;
	}
	Jump();
}

void AFirstPersonCharacter::DoJumpEnd()
{
	StopJumping();
}
