# Placement System

## Target Status

이 문서는 아직 Source에 없는 설비 배치·회수, 확장 단계와 락커 수용량의 확정 target을 정의한다. 이번 범위는 packaged 설비의 신규 배치와 설치된 설비의 Q Hold 회수만 포함한다. 설치된 원본을 유지한 별도 이동 프리뷰는 만들지 않는다.

## Planned Source Scope

```text
Source/BathhouseSim/Public/Placement/
  FacilityPlacementTypes.h
  FacilityPlacementSettings.h
  PlaceableFacility.h
  FacilityPlacementDefinition.h
  FacilityPlacementComponent.h
  FacilityPlacementZoneActor.h
  FacilityPlacementPreviewActor.h
  PlayerFacilityPlacementComponent.h

Source/BathhouseSim/Private/Placement/
  FacilityPlacementSettings.cpp
  FacilityPlacementComponent.cpp
  FacilityPlacementZoneActor.cpp
  FacilityPlacementPreviewActor.cpp
  PlayerFacilityPlacementComponent.cpp

Source/BathhouseSim/Public/Facility/
  BathWaterStateComponent.h
  LockerActionSlotComponent.h
  LockerCapacitySubsystem.h
  BathhouseExpansionDefinition.h
  BathhouseExpansionAuthority.h

Source/BathhouseSim/Private/Facility/
  BathWaterStateComponent.cpp
  LockerActionSlotComponent.cpp
  LockerCapacitySubsystem.cpp
  BathhouseExpansionAuthority.cpp

Source/BathhouseSim/Public/Interaction/
  BathhouseKeyRackActor.h

Source/BathhouseSim/Private/Interaction/
  BathhouseKeyRackActor.cpp

Source/BathhouseSim/Private/Tests/
  FacilityPlacementAutomationTests.cpp
```

## Responsibilities

- 설비의 `Placed`/`Packaged` 모드와 공용 Definition
- held 설비의 즉시 preview, zone trace, grid/snap/rotation과 배치 commit
- 설치 설비의 Q Hold 회수 query/progress/cancel/commit
- zone 호환성, footprint 포함·충돌과 확장 단계 제한 검증
- 배치/회수에 따른 facility, NavMesh와 락커 수용량 등록 전환
- 확장 단계별 물리 열쇠 수와 최대 설치 락커 칸 정책
- 임시 락커 행동 슬롯과 customer capacity lease

Placement는 세탁기 수건 수량, 목욕탕 물 상태, customer 행동, key 상태와 UI hierarchy를 직접 변경하지 않는다. `IPlaceableFacility`을 통해 원래 domain owner의 side-effect-free query와 commit API를 조율한다.

## State Owners

| 책임 | Owner |
|---|---|
| grid 크기, 휠 Yaw 간격, 공통 회수 시간/거리 | `UFacilityPlacementSettings` |
| 설비 tag, preview class, footprint cell 수 | `UFacilityPlacementDefinition` |
| 설비 `Placed`/`Packaged`, footprint/physical root/fixed-slot binding | `UFacilityPlacementComponent` |
| preview session, 누적 Yaw, Q target/경과 시간과 transaction | `UPlayerFacilityPlacementComponent` |
| zone-local grid origin/axes, bounds와 허용 tag | `AFacilityPlacementZoneActor` |
| 설비별 회수 조건 | `IPlaceableFacility` 구현 Actor와 해당 domain owner |
| 목욕탕 물 상태 | `UBathWaterStateComponent` |
| 현재 확장 단계 | `ABathhouseExpansionAuthority` |
| 물리 key/hook 구성 | `ABathhouseKeyRackActor` |
| 설치 락커 용량, 활성 lease와 임시 슬롯 후보 | `ULockerCapacitySubsystem` |
| 실제 held Actor | 기존 `UPlayerCarryComponent` |
| prompt 표시 | Interaction/UI |

## Global Settings

`UFacilityPlacementSettings : UDeveloperSettings`는 Project Settings의 공통 authoring 정본이다.

- `GridSizeCm = 10.0`
- `RotationStepDegrees`
- `RecoveryHoldSeconds`
- `PlacementTraceDistance`
- `RecoveryTraceDistance`

설비 Actor, Definition과 player component에 이 값을 복제하지 않는다. 모든 zone은 같은 grid size를 사용하며 zone은 local origin/axes만 소유한다. Player component는 preview의 현재 누적 Yaw와 hold elapsed 같은 transient 값만 소유한다.

## Placeable Facility Contract

`IPlaceableFacility`과 `IPhysicalCarryable`은 Component가 아닌 Actor native interface다. 실제 공통 Component는 `UFacilityPlacementComponent` 하나다.

- `IPlaceableFacility`: Component/Definition 조회, side-effect-free placement/recovery query, 최종 mode commit
- `UFacilityPlacementComponent`: mode, footprint와 package primitive reference, exact fixed slot binding, transaction guard
- `IPhysicalCarryable`: 기존 single carry, `HeldTransform`, E fixed-slot, G held-position free drop

`ABathhouseFacilityActor` 계열과 기존 `ATowelProcessingMachineActor`가 두 interface를 직접 구현하고 Component에 공통 처리를 위임한다. `ABathhouseFacilityActor`는 package E pickup용 `IPlayerInteractable`도 제공하며 기존 derived player-interactable은 placed domain query를 유지하고 packaged mode만 base 경로로 보낸다. Definition/component 설정이 없는 facility는 query가 숨겨져 placeable이 아니다. 모든 소지품을 위한 공통 Actor나 `UPhysicalCarryableComponent`는 만들지 않는다.

동일 Actor instance가 두 mode를 전환한다.

- `Placed`: facility/domain/Navigation 등록, 설치 표현/충돌 활성, package physics 비활성
- `Packaged`: facility/domain/Navigation 해제, package 표현과 pickup/fixed-slot/free-drop 활성

Held 여부는 `UPlayerCarryComponent`가 소유하고 placement mode에 복제하지 않는다. 회수 때 새 Actor를 spawn하거나 원본을 destroy하지 않는다. Definition과 영구 authoring 값은 같은 instance에 유지하고 runtime contents/reservation은 회수 조건으로 비어 있음을 보장한다.

## Definition And Components

`UFacilityPlacementDefinition : UPrimaryDataAsset`은 stable id, facility Gameplay Tags, preview Actor class, X/Y footprint grid cell 수와 locker인 경우 `LockerSlotCount`를 가진다.

실제 `PlacementFootprint`는 설비 하위 `UBoxComponent` reference로 resolve한다. X/Y 크기는 전역 grid size의 정수배이고 Definition cell 수와 일치해야 한다. `PackagePhysicalRoot`는 설비 Blueprint 하위 primitive reference이며 설치 표현과 별개다. exact fixed slot binding은 같은 Actor instance에 계속 유지된다.

## Placement Zone And Preview

`AFacilityPlacementZoneActor`는 zone-local grid origin/axes, bounds와 allowed facility Gameplay Tags를 소유한다.

- 카메라 중앙 trace가 compatible zone을 맞히면 zone local plane에서 후보 transform을 계산한다.
- grid는 preview 동안 LCtrl 상태와 무관하게 항상 표시한다.
- LCtrl Hold 중에만 local X/Y를 `GridSizeCm`에 quantize한다.
- Mouse Wheel 값마다 누적 Yaw에 `RotationStepDegrees`를 곱해 더한다.
- LCtrl을 놓아도 rotation 누적값은 유지한다.

Preview Actor는 표현 전용 ghost다. collision/domain Tick, facility/Navigation/locker 등록을 하지 않는다. C++이 transform, validity와 failure를 제공하고 Blueprint는 valid/invalid material, grid와 효과만 표현한다.

## Placement Validation And Commit

LMB commit 직전에 다음을 같은 frame 상태로 다시 검증한다.

1. player가 해당 `Packaged` Actor를 실제로 들고 있고 session owner가 일치한다.
2. 정확히 하나의 compatible zone이 resolve된다.
3. footprint 전체가 그 zone bounds 안에 포함된다.
4. footprint가 WorldStatic/WorldDynamic, 다른 설비와 player/customer Pawn에 겹치지 않고 유효 floor support를 가진다.
5. footprint 크기와 Definition cell 수가 전역 grid 계약을 만족한다.
6. 설비 domain precondition과 확장 단계 설치 제한을 만족한다.

성공 시 carry/Actor/registry snapshot을 잡고 후보 transform, `Placed` mode, facility/NavModifier/locker 등록을 적용한 뒤에만 held reference를 해제한다. 어느 단계든 실패하면 attachment, transform, collision, held reference, mode와 registry를 모두 rollback한다.

## Input Ownership

- packaged 설비를 들면 즉시 placement preview 시작
- LMB: 유효 후보 배치 commit
- LCtrl Hold: 위치 grid snap
- Mouse Wheel: 전역 간격 Yaw 회전
- E: 기존 exact fixed slot 보관; 성공 시 preview 종료
- G: 기존 held-position 약한 free drop; 성공 시 preview 종료
- ESC: packaged 설비 preview에서는 no-op
- Q Hold: 응시 중인 설치 설비 회수; 현재 held item과 무관

LMB owner 우선순위는 `Computer > Placement > Equipment`다. Placement가 LMB를 소유하는 동안 equipment use를 시작하지 않는다. Q recovery는 Computer focus/suppression 중에는 실행하지 않는다.

## Recovery Transaction

Q Started에서 현재 target을 고정하고 Triggered에서 elapsed를 누적한다. `Elapsed / RecoveryHoldSeconds`가 prompt progress다. Q release, gaze/range 이탈, suppression, target/owner EndPlay 또는 회수 조건 변경 시 정확히 한 번 cancel하고 progress를 0으로 만든다.

회수 precondition:

- Washer/Dryer: inventory count 0, processing state `Waiting`
- Bath: 모든 use slot `Available`, `UBathWaterStateComponent::IsEmpty()`
- Locker bank: 모든 action slot `Available`, 회수 뒤 설치 용량이 활성 lease 수 이상
- 공통: `Placed`, 유효 Definition/footprint/package primitive/fixed-slot binding, transaction 미실행

성공 시 설비 위치에서 domain/facility/NavModifier 등록을 해제하고 같은 Actor를 `Packaged` free-world physics로 전환한다. 자동 pickup, held item 변경과 impulse는 없다. package collision 활성화나 registry 전환에 실패하면 모두 `Placed`로 rollback한다.

## Bath Water State

`UBathWaterStateComponent`는 최소 `Empty`, `Filling`, `Filled`, `Draining` 상태와 선택적 정규화 수량을 소유한다. `IsEmpty()`만 회수 gate에 제공하며 mesh visibility와 Blueprint bool은 authoritative하지 않다. Blueprint는 상태 변경 event로 물 표현만 갱신한다.

## Locker Bank And Capacity

1/4/8칸 락커는 하나의 `ABathhouseFacilityActor`와 자식 `ULockerActionSlotComponent` N개다. 각 component는 기존 slot 예약/점유 계약을 재사용하고 내부 stable `LockerSlotId`만 가지며 플레이어 번호와 key number는 갖지 않는다.

`ULockerCapacitySubsystem`은 `Placed`인 locker bank의 operational action slot만 등록한다.

- `InstalledLockerCapacity = 등록된 action slot 총수`
- `ActiveLeaseCount = 체크인 승인 후 아직 종료되지 않은 lease 수`
- 정상 commit에서 `ActiveLeaseCount <= InstalledLockerCapacity`
- 확장 제한은 bank Actor 수가 아니라 Definition의 `LockerSlotCount` 합계

check-in key 전달 transaction에서 provisional lease를 먼저 확보하고 key/session commit이 실패하면 rollback한다. checkout, timeout, technical cleanup과 EndPlay는 idempotent release를 호출한다. lease 없는 check-in timeout은 안전한 no-op다.

locker Actor의 비정상 EndPlay는 player recovery로 처리하거나 package item을 생성하지 않는다. slot은 registry에서 제거하되 기존 customer lease는 보존하고 신규 check-in을 차단하며, 용량이 lease보다 작아진 invariant fault를 기록하고 locker 재배치 또는 customer cleanup까지 유지한다.

탈의와 착의는 각각 random available locker action slot을 행동 동안만 reserve/use/release한다. 같은 슬롯일 필요가 없고 `CustomerSession`이 `ClothesStored`만 소유한다. 회수 대상 bank에 Reserved/Occupied slot이 하나라도 있으면 실패한다. Draining, 자동 회수와 montage 강제 중단은 만들지 않는다.

## Expansion And Key Pool

`UBathhouseExpansionDefinition : UPrimaryDataAsset`의 각 단계는 `KeyPoolSize`와 `MaxInstalledLockerSlots`를 가진다. 모든 단계에서 `KeyPoolSize >= MaxInstalledLockerSlots`를 asset validation한다.

`ABathhouseExpansionAuthority`는 current tier의 단일 runtime owner다. 이번 범위는 initial tier와 단계 상승 API만 제공하며 구매/경제/UI와 runtime downgrade는 제외한다.

`ABathhouseKeyRackActor`는 단계의 `KeyPoolSize`만큼 기존 key-hook pair를 구성한다. 락커 배치·회수는 key 수, key 번호나 이미 key를 받은 customer를 변경하지 않는다. key number는 물리 token/hook 식별과 3D 표시에만 쓰고 locker lookup에는 사용하지 않는다.

## Compatibility

- `EBathhouseFacilityType::ShoeLocker` ordinal은 한 migration cycle 보존하되 신규 runtime에서 사용하지 않는다.
- `FacilityNumber`, numbered lookup과 locker topology validation API는 deprecated wrapper로 보존하되 신규 customer/key flow가 호출하지 않는다.
- `EBathhouseCustomerActivity::StoreShoes`, `WearShoes`와 duration property는 asset migration 동안 이름/ordinal을 보존하되 신규 StateTree에서 제거한다.
- 이번 Source 단계에서 reflected symbol을 rename/delete하지 않으므로 Core Redirect는 추가하지 않는다.

## Blueprint/API Contracts

Editor authoring:

- 설비별 Definition, footprint/package primitive, package/placed 표현과 exact fixed slot
- zone bounds/local axes/allowed tags
- Project Settings의 grid/Yaw/recovery/time/distance 값
- bath water initial state
- locker bank action slots와 Definition `LockerSlotCount`
- expansion Definition/initial tier와 key rack anchors/classes

Blueprint는 preview/grid, mode 변경, recovery progress, water state와 capacity/key-pool 변경을 표현할 수 있다. Blueprint는 배치 판정, 회수 조건, lease와 key pool 수량을 변경하지 않는다.

## Dependencies

- Placement -> Interaction carry/query/result와 supplemental intent-source contract
- Facility/Towel -> Placement의 placeable-facility contract
- Interaction key rack -> Facility expansion authority query
- Customer -> Facility locker capacity/slot API
- UI -> Interaction combined prompt data
- Placement/Facility -> GameplayTags, NavigationSystem
- `UFacilityPlacementSettings` 사용을 위해 runtime `DeveloperSettings` module을 추가한다.

## Manual Review Points

- grid가 항상 보이고 LCtrl이 위치 quantization만 바꾸는지 확인한다.
- zone-local axes, 10cm 기본 grid, footprint full containment와 collision failure를 확인한다.
- LMB 실패가 held Actor와 registry를 보존하고 성공만 설치 상태로 전환하는지 확인한다.
- Q 취소 조건과 held item 독립성, 동일 Actor의 무충격 package 전환을 확인한다.
- washer/dryer contents, bath water/customer, locker action/capacity 감소가 각각 회수를 막는지 확인한다.
- 1/4/8 bank component 수, Definition slot 수와 확장 제한 합계가 일치하는지 확인한다.
- check-in 경쟁에서 lease/key가 함께 commit/rollback되고 모든 종료 경로가 lease를 한 번만 반환하는지 확인한다.
- locker 비정상 EndPlay가 item/lease를 복제하지 않고 admission을 차단하는지 확인한다.
- 탈의/착의가 독립 random slot을 사용하고 `ClothesStored`는 session에만 남는지 확인한다.
- key pool이 확장 단계에만 반응하고 locker 변경으로 key/customer 번호가 바뀌지 않는지 확인한다.
- Nav Modifier와 `Dynamic Modifiers Only`가 필요한 영역만 갱신하는지 확인한다.
