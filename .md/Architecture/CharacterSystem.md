# Character System

## Implementation Status

1인칭 이동, sprint, camera, Interaction/Carry 조립, E primary, F secondary, 모든 현재 physical carryable의 G free drop과 LMB `PrimaryUseAction`의 Computer pointer/held equipment 배타적 routing이 현재 Source에 구현되어 있다. G는 item kind를 판정하지 않고 camera forward만 carry coordinator에 전달한다.

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
- player computer-use component와 widget interaction 조립
- computer focus 중 1인칭 입력 gate와 click action 라우팅
- player equipment-use component 조립과 LMB Started/Triggered/Completed/Canceled routing

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
- `DropCarryAction` G의 Started를 camera forward와 함께 generic held-item free-drop intent로 전달한다.
- `UPlayerComputerUseComponent`와 mouse-source `UWidgetInteractionComponent`를 조립한다.
- `UPlayerEquipmentUseComponent`를 조립하고 camera, carry와 interaction query/result context를 주입한다.
- computer session이 capture 중이면 E Started는 focus-out으로, LMB primary use는 widget pointer로 보내고 Move/Look/Jump/Sprint/F/G를 차단한다.
- 일반 상태 LMB는 현재 held Actor의 generic equipment-use lifecycle로 전달하며 Character가 wrench/mop을 cast하지 않는다.
- 진입에 사용한 E의 Completed/Canceled가 즉시 focus-out 또는 기존 hold lifecycle로 재전달되지 않도록 press ownership을 보존한다.
- Character는 towel, stain, mop, basket, monkey wrench의 사용 가능 여부와 key drop 가능 여부를 직접 판정하지 않는다.

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
7. Computer use component에 first-person camera, movement, interaction, carry와 widget interaction context를 제공한다.
8. Equipment use component에 camera, carry와 interaction context를 제공한다.

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
5. `DropCarryAction` G Started는 carry domain에 held item free drop을 요청한다.
6. computer session이 E press를 소유하면 기존 primary begin/end로 전달하지 않는다.

### Computer Use

1. 일반 상태의 E primary가 computer Actor 실행에 성공하면 Computer component가 즉시 movement와 world interaction을 잠근다.
2. focus-in 완료 뒤 `PrimaryUseAction`을 world widget left pointer press/release로 전달한다.
3. 새 E Started는 focus-out을 시작하고 해당 press lifecycle을 소비한다.
4. focus-out 완료 뒤 movement와 일반 interaction을 복구한다.

### Primary Equipment Use

1. `PrimaryUseAction` Started에서 현재 input owner를 Computer 또는 Equipment으로 정확히 한 번 결정한다.
2. Computer Active이면 pointer press를 시작하고 해당 press의 Completed/Canceled만 pointer release로 소비한다.
3. 일반 상태면 `UPlayerEquipmentUseComponent` Begin/Update/End로 전달한다. 몽키스패너는 Started 한 번, 물걸레는 Hold lifecycle을 사용한다.
4. press owner를 중간에 바꾸지 않고 End/Cancel을 시작 owner에만 전달한다.

Native reflected property 이관은 `PrimaryUseAction`을 최우선으로 사용하되 기존 `ComputerClickAction`을 deprecated fallback으로 한 migration cycle 보존한다. 둘 다 설정되면 `PrimaryUseAction`만 binding하여 LMB를 중복 처리하지 않는다.

## Dependencies

- Character System -> Engine Character/Pawn/Movement
- Character System -> `Camera/CameraComponent`
- Character System -> Camera System
- Character System -> Interaction System
- Character System -> Computer System
- Character System -> Enhanced Input
- Character System -> InputCore
- Character System -> UMG widget interaction

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
- `AFirstPersonCharacter::GetPlayerEquipmentUse`
- `AFirstPersonCharacter::GetPlayerComputerUse`
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
- `AFirstPersonCharacter::PrimaryUseAction`
- `AFirstPersonCharacter::ComputerClickAction` (deprecated Editor migration fallback)
- `AFirstPersonCharacter::MoveSpeedScale`
- `AFirstPersonCharacter::LookSpeedScale`
- `AFirstPersonCharacter::bSprintToggle`
- `UFirstPersonMovementComponent::WalkingSpeed`
- `UFirstPersonMovementComponent::SprintingSpeed`

## Design Notes

- Character는 입력 라우팅과 pawn 조작만 담당하고, sprint의 실제 상태/속도는 MovementComponent가 소유한다.
- Character는 component composition과 input routing만 담당하고 focus/key transaction을 직접 소유하지 않는다.
- computer의 phase, camera blend, cursor/input mode와 reservation은 Computer component/Actor가 소유하며 Character에 상태를 복제하지 않는다.
- E/F/G는 intent mapping이며 fixed slot, key, Cleaning/Towel 상태를 Character에 추가하지 않는다.
- LMB도 intent mapping이며 Character에 wrench attack, mop cleaning 또는 prompt domain state를 추가하지 않는다.
- `ComputerClickAction` property를 즉시 rename/delete하지 않고 `PrimaryUseAction` 이관 후 후속 제거 단계에서 Property Redirect를 검토한다.
- serialized `HeldKeyAnchor` 이름은 기존 Blueprint 호환성을 위해 유지하되 key/mop/basket/monkey wrench의 공용 held anchor로 사용한다. rename하지 않는다.
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
- F와 G가 각각 한 번만 routing되고 G가 concrete kind 판단 없이 key를 포함한 held carryable에 도달하는지 확인한다.
- computer 사용 중 Move/Look/Jump/Sprint/F/G가 기존 component나 domain에 도달하지 않는지 확인한다.
- `PrimaryUseAction`과 deprecated fallback이 동시에 LMB를 중복 binding하지 않는지 확인한다.
- Computer pointer press와 Equipment use press owner가 섞이지 않고 Completed/Canceled가 시작 owner에 한 번만 도달하는지 확인한다.
- 진입 E release와 종료 E press/release가 각각 한 번만 소비되며 world interaction을 오발하지 않는지 확인한다.
- pawn 종료/교체 시 interaction focus와 held key attachment가 정리되는지 확인한다.
- Blueprint native parent rename이 필요한 경우 Core Redirect 필요 여부를 먼저 검토한다.
