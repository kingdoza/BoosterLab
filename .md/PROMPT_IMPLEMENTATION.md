# 구현 프롬프트 — 전용 설비 회수 아이템과 Actor 교체 Transaction

## 목표

배치된 설비 Actor 자체를 `Packaged` physical item으로 바꾸는 현재 구현을 폐기한다. 배치 설비와 회수 아이템을 별도 class/instance로 분리하고 `APlaceableFacilityItemActor` 하나가 모든 설비 회수 아이템을 Definition 기반으로 표현하게 한다. 기존 Q Hold, 회수 조건, 낙하 위치, ghost preview, grid/snap/rotation, single carry와 G free-drop 경로는 유지한다.

## 정본 입력

- `.md/0_ARCHITECTURE.md`
- `.md/Architecture/PlacementSystem.md`
- `.md/Architecture/PhysicalCarrySystem.md`
- `.md/Architecture/FacilitySystem.md`
- `.md/Architecture/TowelSystem.md`
- `.md/Architecture/CoreSystem.md`
- `.md/QNA_ARCHITECTURE.md` Q89~Q94

## 고정 결정

- 배치 설비와 회수 아이템은 서로 다른 Actor class와 instance다.
- 공통 native `APlaceableFacilityItemActor` 하나를 Definition으로 구분한다.
- Definition이 `PlacedFacilityClass`, `RecoveryItemClass`, `RecoveryItemMesh`와 기존 preview/footprint 데이터를 함께 소유한다.
- 양방향 변환은 새 Actor를 먼저 deferred/staged 생성하고 검증한 뒤 원본을 마지막에 제거한다.
- 설비 아이템은 `IPhysicalCarryable`의 `FreeDrop` capability만 지원한다. exact fixed slot은 범위 밖이다.
- 회수 성공은 자동 pickup이나 impulse를 발생시키지 않고 기존 held item을 바꾸지 않는다.
- Root Static Mesh의 scale이 공통 외형/물리 scale이며 별도 `RecoveryItemVisualScale`과 collision extent를 만들지 않는다.
- `HeldTransform`은 모든 설비 아이템에 공통이고 location/rotation만 적용한다. scale은 무시한다.
- facility↔item 변환은 location/rotation만 전달하고 서로의 Actor/Root scale을 복사하지 않는다.
- Content를 수정하지 않으며 Definition mesh 미지정 시 Engine 기본 `/Engine/BasicShapes/Cube.Cube`로 동작한다.

## 신규 타입

### `FacilityPlacementPayload.h`

- `UFacilityPlacementInstanceData`: abstract runtime-only UObject base. Tick/delegate와 domain mutation을 갖지 않는다.
- `FFacilityPlacementPayload`: `Definition`과 item을 Outer로 하는 instanced `UFacilityPlacementInstanceData` 포인터.
- Placement는 instance data 내용을 해석하지 않는다.
- null/wrong Outer/wrong domain type과 Actor/Component runtime reference를 fail-closed한다.

Domain별 data class:

- `UBathhouseFacilityPlacementInstanceData`: `FacilityType`, legacy `FacilityNumber`, `SelectionWeight`, `bEnabled`
- `UTowelMachinePlacementInstanceData`: `MachineKind`, `ProcessingDurationSeconds`

contents, machine state/progress, bath water, slot reservation/occupancy, customer와 registry pointer는 payload에 넣지 않는다. `StructUtils`/experimental plugin, 전체 Actor serialization과 문자열 property bag은 사용하지 않는다.

### `APlaceableFacilityItemActor`

- `AActor + IPlayerInteractable + IPhysicalCarryable`
- stable Root `UStaticMeshComponent` 하나가 표시, simple collision, physics와 CCD를 담당
- staged/free-world/held/placement-consumed 내부 lifecycle과 payload, carrier weak reference, last-safe transform 소유
- `EPhysicalCarryKind::Facility`, capability는 `FreeDrop`만 반환
- E pickup과 기존 `UPlayerCarryComponent` single-held 계약 사용
- G는 기존 actual-held-pose transaction, Pawn Ignore, CCD, 질량 무시 약한 velocity change 사용
- fixed-slot getter/bind/store/recovery는 제공하지 않거나 항상 fail-closed
- 정상 placement consumption과 일반 EndPlay/fall recovery를 구분

Definition mesh가 유효하면 Root에 적용하고 없으면 native Cube fallback을 유지한다. 모든 회수 mesh는 동일 규격 직육면체이며 bounds와 일치하는 simple box collision 하나만 허용한다. complex-as-simple, box 이외/복수 shape, physics 불가 mesh는 validation 실패다.

pickup은 `SnapToTargetNotIncludingScale`과 `SetRelativeLocationAndRotation`만 사용한다. `SetRelativeTransform(HeldTransform)`으로 Root scale을 덮어쓰지 않는다. held/drop/rollback 전체에서 Root scale을 보존한다.

### Private conversion helper

`FacilityActorConversionTransaction.h/.cpp` 같은 private non-UObject helper로 staged Actor, 원본/신규 weak identity, carry/physics snapshot, silent domain registration과 rollback을 응집한다. Tick, reflected 상태나 domain 규칙은 넣지 않는다. `UPlayerFacilityPlacementComponent`는 session과 상위 commit 순서만 조율한다.

## `UFacilityPlacementDefinition`

추가:

- `TSubclassOf<AActor> PlacedFacilityClass`
- `TSubclassOf<APlaceableFacilityItemActor> RecoveryItemClass`
- `TObjectPtr<UStaticMesh> RecoveryItemMesh`

validation:

- placed class는 `IPlaceableFacility` 구현
- recovery class는 전용 item class 파생
- 현재 모든 Definition의 recovery class는 동일한 native 공통 class여야 하며 설비별 subclass는 거부
- placed/recovery class가 서로 다름
- preview class, stable id, footprint와 locker slot count 기존 검증 유지
- mesh가 있으면 동일 직육면체/simple box/physics 계약 검사; null은 이번 migration에서 Cube fallback 경고만 허용

## Placed Facility 책임 변경

- `ABathhouseFacilityActor`와 `ATowelProcessingMachineActor`는 canonical `IPlaceableFacility`만 사용한다.
- 각 Actor가 자신의 typed instance data를 item Outer에 export하고 staged 새 Actor에서 import/범위 검증한다.
- bath/locker는 기존 모든 slot Available, bath water Empty와 locker capacity gate를 유지한다.
- washer/dryer는 inventory 0, state Waiting gate를 유지한다.
- 새 Actor import 후 runtime 내용물은 empty/waiting/available로 시작한다.
- class/default subobject topology, locker slot ID/transform과 presentation은 `PlacedFacilityClass` CDO가 공급한다.
- Level instance의 임의 component override를 serialize/copy하지 않는다.

`IPlaceableFacility`에는 side-effect-free query와 payload export/import, silent register/unregister stage, rollback과 최종 publication을 분리하는 native 계약을 제공한다. 외부 event는 양쪽 Actor 상태가 확정되기 전에 발행하지 않는다.

## Q Hold 회수 흐름

1. 기존 focus supplemental recovery row, Q Started/Triggered/Completed/Canceled와 target 고정을 유지한다.
2. 시작 및 완료 직전에 domain 조건, Definition/class/payload와 낙하 위치를 재검증한다.
3. passive query collision은 Definition mesh bounds/simple box와 recovery item CDO Root scale에서 derived box를 계산한다.
4. `RecoveryItemClass`를 예정 낙하 location/rotation과 item class CDO Root scale로 deferred spawn하고 collision/physics가 꺼진 staged 상태로 초기화한다. 원본 facility scale은 복사하지 않는다.
5. 원본 설비가 staged item을 Outer로 typed payload data를 생성하고 item이 payload를 소유한다.
6. `FinishSpawning` 뒤 실제 Root mesh collision을 원본 설비만 ignore하여 다시 검사한다.
7. 원본 facility/domain/locker/NavModifier를 silent unregister한다.
8. item을 free-world physics로 활성화하고 선형·각속도를 0으로 둔다. impulse는 주지 않는다.
9. 원본 설비를 마지막에 `Destroy()`한다.
10. 성공이 확정된 후 registry/capacity와 필요한 presentation event를 한 번 publish한다.

어느 단계든 실패하면 staged item을 파괴하고 원본 등록/NavModifier를 복원한다. 정상 transaction이 실행한 target Destroy와 외부 Destroy callback을 구분한다. 외부 EndPlay/world teardown/FellOutOfWorld는 설비 item을 생성하지 않는다.

## LMB 배치 흐름

1. held object change에서 동일 facility item instance와 payload를 고정하고 기존 ghost preview를 즉시 시작한다.
2. preview validation은 Definition footprint, zone, floor/blocking, 확장 한계를 사용한다.
3. confirm 직전에 local owner, suppression, held identity, preview, payload와 후보를 재검증한다.
4. `PlacedFacilityClass`를 candidate location/rotation과 placed class CDO Root scale로 deferred spawn하고 payload import 및 staged-start flag를 설정한다. held item Root scale은 복사하지 않는다.
5. `FinishSpawning`은 BeginPlay의 자동 domain registration을 억제하고 외부 event를 발행하지 않는다.
6. 새 facility component/topology/domain 조건을 검증한 뒤 facility/locker/NavModifier를 silent register한다.
7. carry의 held reference를 silent clear하고 item을 placement-consumed로 표시한 뒤 원본 item을 제거한다.
8. 모든 상태가 확정된 후 held/facility/capacity event를 각 한 번 publish하고 preview를 종료한다.

실패하면 새 facility 등록을 취소하고 staged Actor만 파괴한다. 원본 item의 held identity, parent/socket, location/rotation, Root scale, collision/physics/CCD와 preview를 정확히 유지한다.

## 호환성과 제거 금지

- 기존 `EPlaceableFacilityMode` ordinal을 변경하지 않는다.
- `UFacilityPlacementComponent::Mode`, `HeldTransform`, release 값, `OnModeChanged`와 placed Actor의 `PackagePhysicalRoot` reflected 이름/default subobject를 한 migration cycle 유지한다.
- legacy placed Actor physical-carry query와 `Packaged` 전환은 fail-closed이며 신규 경로에서 호출하지 않는다.
- 기존 BlueprintCallable/Assignable 및 component 이름을 rename/delete하지 않는다. Core Redirect는 추가하지 않는다.
- `EPhysicalCarryKind::Facility`, `HeldKeyAnchor`, `UPlayerCarryComponent`와 기존 free-drop transaction을 유지한다.

## 대상 파일

- `Source/BathhouseSim/BathhouseSim.Build.cs`는 신규 dependency가 필요 없는지 확인만 하며 `StructUtils`를 추가하지 않는다.
- `Source/BathhouseSim/Public|Private/Placement/*`
- `Source/BathhouseSim/Public|Private/Facility/BathhouseFacilityActor*`, placement domain과 신규 typed instance data
- `Source/BathhouseSim/Public|Private/Towel/TowelProcessingMachineActor*`와 신규 typed instance data
- 필요한 `Interaction/PlayerCarryComponent*`, private physical transaction의 최소 확장
- `Source/BathhouseSim/Private/Tests/FacilityPlacementAutomationTests.cpp`
- 필요하면 carry scale/capability 회귀 테스트 파일

`Content/`, `Config/`, `.uproject`는 수정하지 않는다.

## 자동화 수용 기준

- 회수 성공: 원본 facility 0개, payload가 같은 item 1개, 지정 낙하 위치, physics/CCD/Pawn Ignore, 속도 0, 기존 held item 불변
- 회수 spawn/class/payload/collision/silent unregister/Destroy 실패: 원본 facility 1개, item 0개, registry/NavModifier/lease 변화 없음
- 배치 성공: 원본 item 0개, 새 placed facility 1개, empty hand와 최종 registry가 event observer에 보임
- 배치 spawn/import/domain/carry commit 실패: 기존 item 1개가 계속 held, staged facility 0개, preview와 전체 physical snapshot 유지
- 반복 입력, reentrant delegate, target/item/owner EndPlay 경합에서 복제·손실·stale registry 없음
- 설비 item fixed-slot 거부, E pickup/G drop 성공과 actual held pose 유지
- authored non-unit `HeldTransform.Scale` 무시 및 pickup/drop/rollback Root scale 보존
- facility Actor scale→item, item Root scale→새 facility 누수 없음
- Cube fallback과 Definition mesh simple-box validation
- bath/machine/locker 기존 회수 gate 및 locker capacity/key-pool 회귀

## 검증과 인계

- `git diff --check`
- UE 5.8 Build.bat 정책으로 `BathhouseSimEditor Win64 Development` 빌드
- 관련 `BathhouseSim.Placement`, physical carry automation 실행
- 구현 완료 후 `.md/PROMPT_REVIEW.md`와 `.md/PROMPT_UNREAL.md`를 현재 작업만으로 작성
- Editor 인계에는 Definition별 class/mesh 연결, 기존 Blueprint package 표현 비활성 확인과 PIE 양방향 변환 검증을 포함
