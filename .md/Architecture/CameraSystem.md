# Camera System

## Scope

- `Source/BoosterLab/Public/Camera/FirstPersonCameraShakeComponent.h`
- `Source/BoosterLab/Private/Camera/FirstPersonCameraShakeComponent.cpp`
- `Source/BoosterLab/Public/Camera/FirstPersonCameraManager.h`
- `Source/BoosterLab/Private/Camera/FirstPersonCameraManager.cpp`

## Responsibilities

- 이동 상태 기반 idle/walk/sprint camera shake 전환
- falling/landing 상태 감지와 landing shake 재생
- 강제 위치 보정 등 외부 이동 직후 landing shake 1회 억제 API 제공
- 비로컬 플레이어에서 Tick 비용 저감
- 재생 중인 이동 camera shake 정리
- player camera manager 기반 상하 시야각 제한 기본값 제공

## Key Classes

- `UFirstPersonCameraShakeComponent`: 카메라 셰이크 상태 기계
- `EFirstPersonCameraMoveState`: idle/walk/sprint 이동 상태 enum
- `AFirstPersonCameraManager`: Blueprint 파생 class에서 `ViewPitchMin`/`ViewPitchMax`를 조정할 수 있는 camera manager 기본 class

## Runtime Flow

1. `AFirstPersonCharacter`가 `FirstPersonCameraShake` 기본 subobject를 생성한다.
2. `AFirstPersonController`가 기본 `PlayerCameraManagerClass`로 `AFirstPersonCameraManager`를 사용한다.
3. `AFirstPersonCameraManager`는 기본 pitch limit을 제공하며, Blueprint 파생 class에서 `ViewPitchMin`/`ViewPitchMax`를 에디터 값으로 조정할 수 있다.
4. `BeginPlay`에서 owner character와 `UFirstPersonMovementComponent`를 캐시한다.
5. 비로컬 플레이어면 이동 shake를 중단하고 Tick interval을 낮춘다.
6. falling 중에는 이동 shake를 중지하고 landing 후보 상태를 기록한다.
7. falling에서 grounded로 전환되는 프레임에 landing shake를 재생하거나 suppress flag를 소비한다.
8. 수평 속도와 sprint 상태를 기준으로 idle/walk/sprint shake class를 전환한다.

## Dependencies

- Character System
- `AFirstPersonCharacter`
- `UFirstPersonMovementComponent`
- `APlayerCameraManager`
- `UCameraShakeBase`

Camera System은 현재 문서화된 Camera 책임 밖의 도메인 시스템에 의존하지 않는다.

## Blueprint/API Contracts

Blueprint에서 접근 가능한 주요 API:

- `UFirstPersonCameraShakeComponent::StopAllCameraShakes`
- `UFirstPersonCameraShakeComponent::SuppressNextLandingShake`
- `AFirstPersonCharacter::GetFirstPersonCameraShake`
- `AFirstPersonCameraManager`

Blueprint/Editor에서 설정해야 하는 주요 property:

- `IdleCameraShakeClass`
- `WalkCameraShakeClass`
- `SprintCameraShakeClass`
- `LandingCameraShakeClass`
- `IdleSpeedThreshold`
- `AFirstPersonCameraManager` 또는 Blueprint 파생 class의 `ViewPitchMin`
- `AFirstPersonCameraManager` 또는 Blueprint 파생 class의 `ViewPitchMax`

## Design Notes

- Camera System은 이동 상태를 직접 소유하지 않고 `UFirstPersonMovementComponent::IsSprinting()`과 owner velocity를 읽어 shake 상태만 결정한다.
- move shake는 상태 전환마다 기존 shake class instance를 중지한 뒤 새 shake class를 재생한다.
- landing shake는 movement shake와 별도로 `PlayerCameraManager->StartCameraShake`를 호출한다.
- 비로컬 최적화는 actor/component 제거가 아니라 Tick interval 조정과 shake 중단으로 처리한다.
- `SuppressNextLandingShake()`는 teleport, scripted reposition 같은 외부 이동 이후 불필요한 landing shake를 한 번만 막는 API다.
- 기본 pitch limit은 C++에서 `-80..80`으로 제공한다. 프로젝트별 조정은 `AFirstPersonCameraManager`를 부모로 한 Blueprint class에서 수행하고, PlayerController의 `PlayerCameraManagerClass`에 해당 Blueprint를 지정한다.

## Manual Review Points

- idle/walk/sprint shake class가 설정된 Blueprint에서 상태 전환 시 이전 shake가 중복 재생되지 않는지 확인한다.
- landing 직후 `LandingCameraShakeClass`가 한 번만 재생되는지 확인한다.
- `SuppressNextLandingShake()` 호출 직후 첫 landing shake만 억제되는지 확인한다.
- local/non-local possession 전환 시 stale `PlayerCameraManager` 참조가 남지 않는지 확인한다.
- sprint 상태와 camera shake sprint 상태가 `UFirstPersonMovementComponent::IsSprinting()` 기준으로 동기화되는지 확인한다.
- Blueprint camera manager에서 `ViewPitchMin`/`ViewPitchMax` 조정 후 실제 look input pitch 제한이 기대 범위와 맞는지 확인한다.
