# Character System

## Implementation Status

1인칭 이동, sprint, camera, Interaction/Carry 조립과 E primary, F secondary, G equipment release 입력 routing은 현재 Source에 구현되어 있다. InputAction/IMC asset 연결은 Unreal 후속 단계다.

## Responsibilities

Character System은 범용 1인칭 조작 템플릿의 플레이어 조작을 담당한다.

- 1인칭 카메라 구성
- 카메라 셰이크 컴포넌트 조립
- Enhanced Input action binding
- Move/Look/Jump 입력 처리
- Sprint 시작, 종료, toggle 처리
- sprint 상태와 걷기/달리기 속도 관리
- player interaction/carry component와 first-person held key anchor 조립
- InteractAction 입력 라우팅
- SecondaryInteractAction과 DropCarryAction 입력 라우팅

현재 문서화된 Character 책임 밖의 도메인 gameplay logic은 Character System 책임이 아니다.

## Source Scope

```text
Source/BathhouseSim/Public/Character/
  FirstPersonCharacter.h
  FirstPersonController.h
  FirstPersonMovementComponent.h

Source/BathhouseSim/Private/Character/
  FirstPersonCharacter.cpp
  FirstPersonController.cpp
  FirstPersonMovementComponent.cpp
```

## Key Classes

### AFirstPersonCharacter

`AFirstPersonCharacter`는 플레이어 Pawn의 composition root다.

- 기본 `CharacterMovementComponent` class를 `UFirstPersonMovementComponent`로 교체한다.
- `UFirstPersonCameraShakeComponent`를 기본 subobject로 생성한다.
- `FirstPersonCamera`를 capsule에 부착하고 pawn control rotation을 사용한다.
- `MoveAction`, `LookAction`, `JumpAction`, `SprintAction`을 Enhanced Input에 바인딩한다.
- 입력 scale 값(`MoveSpeedScale`, `LookSpeedScale`)을 적용한 뒤 Unreal movement/controller API로 전달한다.
- `bSprintToggle`에 따라 sprint 입력을 toggle 방식 또는 hold 방식으로 처리한다.
- `UPlayerInteractionComponent`, `UPlayerCarryComponent`와 camera 하위 `HeldKeyAnchor`를 조립한다.
- `InteractAction` E의 Started/Completed/Canceled를 primary begin/end intent로 전달한다. 기존 instant target은 Started에서 한 번 실행하고 hold target은 release까지 유지한다.
- `SecondaryInteractAction` F의 Started를 secondary intent로 전달한다.
- `DropCarryAction` G의 Started를 camera view와 함께 equipment drop intent로 전달한다.
- Character는 towel, stain, mop, basket과 key drop 가능 여부를 직접 판정하지 않는다.

### AFirstPersonController

`AFirstPersonController`는 로컬 플레이어 입력 mapping context 등록/해제만 담당한다.

- `DefaultMappingContext`가 설정되어 있으면 `UEnhancedInputLocalPlayerSubsystem`에 priority `0`으로 등록한다.
- `EndPlay`에서 등록한 `DefaultMappingContext`를 제거한다.
- gameplay state나 도메인 session state를 소유하지 않는다.

### UFirstPersonMovementComponent

`UFirstPersonMovementComponent`는 sprint 상태와 이동 속도 전환을 담당한다.

- `WalkingSpeed`와 `SprintingSpeed`를 소유한다.
- `StartSprinting`, `StopSprinting`, `SwitchSprinting`을 제공한다.
- 전방 가속 중이고 낙하 중이 아닐 때만 sprint를 시작한다.
- sprint 중 이동 가속이 완전히 사라지면 sprint를 종료한다. 달리는 중 점프해 falling 상태가 되어도 sprint 상태는 유지한다.

## Runtime Flow

### Startup

1. `AFirstPersonCharacter` constructor가 `UFirstPersonMovementComponent`를 기본 movement component로 설정한다.
2. `UFirstPersonCameraShakeComponent`를 기본 component로 생성한다.
3. `FirstPersonCamera`를 capsule에 부착하고 control rotation 기반 시점을 구성한다.
4. `AFirstPersonController::SetupInputComponent`가 `DefaultMappingContext`를 로컬 Enhanced Input subsystem에 등록한다.
5. `AFirstPersonCharacter::SetupPlayerInputComponent`가 설정된 input action들을 바인딩한다.
6. Interaction/Carry component에 camera와 held anchor context를 제공한다.

### Shutdown

1. `AFirstPersonController::EndPlay`가 등록한 `DefaultMappingContext`를 로컬 Enhanced Input subsystem에서 제거한다.

### Move

1. `MoveAction` triggered
2. `MoveInput`이 `FVector2D` 입력값에 `MoveSpeedScale`을 적용한다.
3. `DoMove`가 actor right/forward vector 기준으로 `AddMovementInput`을 호출한다.

### Look

1. `LookAction` triggered
2. `LookInput`이 `FVector2D` 입력값에 `LookSpeedScale`을 적용한다.
3. `DoLook`이 `AddControllerYawInput`, `AddControllerPitchInput`을 호출한다.

### Jump

1. `JumpAction` started 시 `DoJumpStart`가 `Jump()`를 호출한다.
2. `JumpAction` completed 시 `DoJumpEnd`가 `StopJumping()`을 호출한다.

### Sprint

1. `SprintAction` started 시 `SprintStartInput`이 실행된다.
2. `bSprintToggle=true`이면 `SwitchSprinting`을 호출한다.
3. `bSprintToggle=false`이면 `StartSprinting`을 호출하고, completed 시 `StopSprinting`을 호출한다.
4. `UFirstPersonMovementComponent`는 tick에서 sprint 유지 조건을 확인한다.

### Interact

1. `InteractAction` E Started에서 primary begin을 전달한다.
2. instant target은 기존 primary execute를 한 번 호출한다.
3. hold target은 E Completed/Canceled까지 동일 focus와 조건을 재검증한다.
4. `SecondaryInteractAction` F Started는 secondary execute를 한 번 호출한다.
5. `DropCarryAction` G Started는 carry domain에 held equipment release를 요청한다.

## Dependencies

- Character System -> Engine Character/Pawn/Movement
- Character System -> `Camera/CameraComponent`
- Character System -> Camera System
- Character System -> Interaction System
- Character System -> Enhanced Input
- Character System -> InputCore

현재 문서화된 책임 밖의 gameplay 시스템에 대한 의존성은 없다.

## Blueprint/API Contracts

Blueprint에서 접근 가능한 주요 API:

- `AFirstPersonCharacter::GetFirstPersonCamera`
- `AFirstPersonCharacter::GetFirstPersonMovement`
- `AFirstPersonCharacter::GetFirstPersonCameraShake`
- `AFirstPersonCharacter::DoMove`
- `AFirstPersonCharacter::DoLook`
- `AFirstPersonCharacter::DoJumpStart`
- `AFirstPersonCharacter::DoJumpEnd`
- `AFirstPersonCharacter::GetPlayerInteraction`
- `AFirstPersonCharacter::GetPlayerCarry`
- `UFirstPersonMovementComponent::StartSprinting`
- `UFirstPersonMovementComponent::StopSprinting`
- `UFirstPersonMovementComponent::SwitchSprinting`
- `UFirstPersonMovementComponent::IsSprinting`
- `UFirstPersonMovementComponent::IsForwardAccelerating`

Blueprint/Editor에서 설정해야 하는 주요 property:

- `AFirstPersonController::DefaultMappingContext`
- `AFirstPersonCharacter::MoveAction`
- `AFirstPersonCharacter::LookAction`
- `AFirstPersonCharacter::JumpAction`
- `AFirstPersonCharacter::SprintAction`
- `AFirstPersonCharacter::InteractAction`
- `AFirstPersonCharacter::SecondaryInteractAction`
- `AFirstPersonCharacter::DropCarryAction`
- `AFirstPersonCharacter::MoveSpeedScale`
- `AFirstPersonCharacter::LookSpeedScale`
- `AFirstPersonCharacter::bSprintToggle`
- `UFirstPersonMovementComponent::WalkingSpeed`
- `UFirstPersonMovementComponent::SprintingSpeed`

## Design Notes

- Character는 입력 라우팅과 pawn 조작만 담당하고, sprint의 실제 상태/속도는 MovementComponent가 소유한다.
- Character는 component composition과 input routing만 담당하고 focus/key transaction을 직접 소유하지 않는다.
- E/F/G는 intent mapping이며 concrete Cleaning/Towel 상태를 Character에 추가하지 않는다.
- serialized `HeldKeyAnchor` 이름은 기존 Blueprint 호환성을 위해 유지하되 key/mop/basket의 공용 held anchor로 사용한다. 이번 target에서 rename하지 않는다.
- Controller는 mapping context 등록/해제 외 책임을 갖지 않는다.
- Sprint 시작 조건은 전방 가속과 지상 상태를 요구한다.
- 현재 카메라는 capsule 기준 고정 offset을 사용한다. skeletal mesh socket 기반 카메라나 weapon/hand mesh는 별도 시스템이 생길 때 설계한다.
- 카메라 셰이크의 상태 기계와 shake class 재생은 Camera System이 담당한다.
- 현재 문서화된 Character/Camera 책임 밖의 도메인 기능은 기본 템플릿 범위 밖이다.

## Manual Review Points

- Editor 또는 Blueprint에서 `DefaultMappingContext`와 4개 input action이 모두 할당되어야 실제 입력이 동작한다.
- GameMode/Pawn/Controller 연결은 Config 또는 Blueprint에서 별도 확인이 필요하다.
- gamepad/mouse look 축 방향과 scale은 input asset 설정과 함께 플레이 테스트로 확인한다.
- `bSprintToggle=false` hold 방식에서는 `SprintAction` completed 이벤트가 반드시 발생해야 sprint가 종료된다.
- Controller 종료/교체 시 `DefaultMappingContext`가 제거되는지 확인한다.
- InteractAction이 Started 한 번마다 한 번만 실행되는지 확인한다.
- E hold target이 Completed/Canceled를 받으며 기존 instant interaction이 release 때 재실행되지 않는지 확인한다.
- F와 G가 각각 한 번만 routing되고 key에 G drop이 적용되지 않는지 확인한다.
- pawn 종료/교체 시 interaction focus와 held key attachment가 정리되는지 확인한다.
- Blueprint native parent rename이 필요한 경우 Core Redirect 필요 여부를 먼저 검토한다.
