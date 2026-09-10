# Placement System

## Implementation Status

배치 설비 Actor와 전용 `APlaceableFacilityItemActor`를 분리하고 새 Actor를 먼저 stage한 뒤 원본을 마지막에 제거하는 양방향 transaction은 Source와 native automation까지 구현되었다. Q Hold, ghost preview, zone/grid/snap/rotation, 회수 조건, 낙하 위치와 locker capacity/key-pool 계약을 유지한다. Clean Towel Stack과 Used Towel Bin은 towel token owner이므로 Actor 변환 대상에서 제외한다. Content authoring과 PIE 통합은 후속 Editor 단계다.

## Source Scope

```text
Source/BathhouseSim/Public/Placement/
  FacilityPlacementTypes.h
  FacilityPlacementPayload.h
  FacilityPlacementSettings.h
  PlaceableFacility.h
  FacilityPlacementDefinition.h
  FacilityPlacementComponent.h
  PlaceableFacilityItemActor.h
  FacilityPlacementZoneActor.h
  FacilityPlacementPreviewActor.h
  PlayerFacilityPlacementComponent.h

Source/BathhouseSim/Private/Placement/
  FacilityPlacementCollisionUtils.h/.cpp
  FacilityActorConversionTransaction.h/.cpp
  FacilityPlacementDefinition.cpp
  FacilityPlacementPayload.cpp
  FacilityPlacementSettings.cpp
  FacilityPlacementComponent.cpp
  PlaceableFacilityItemActor.cpp
  PlaceableFacilityItemCollision.cpp
  FacilityPlacementZoneActor.cpp
  FacilityPlacementPreviewActor.cpp
  PlayerFacilityPlacementComponent.cpp
  PlayerFacilityPlacementValidation.cpp

Source/BathhouseSim/Public/Facility/
  BathWaterStateComponent.h
  LockerActionSlotComponent.h
  LockerCapacitySubsystem.h
  BathhouseExpansionDefinition.h
  BathhouseExpansionAuthority.h

Source/BathhouseSim/Private/Facility/
  BathhouseFacilityPlacementDomain.cpp
  BathWaterStateComponent.cpp
  LockerActionSlotComponent.cpp
  LockerCapacitySubsystem.cpp
  BathhouseExpansionAuthority.cpp

Source/BathhouseSim/Public/Interaction/
  BathhouseKeyRackActor.h

Source/BathhouseSim/Private/Interaction/
  BathhouseKeyRackActor.cpp

Source/BathhouseSim/Private/Tests/
  FacilityPlacementAutomationTestProbe.h/.cpp
  FacilityPlacementAutomationTests.cpp
```

## Responsibilities

- 배치 설비 Actor와 전용 회수 아이템 Actor의 분리된 lifecycle 및 공용 Definition/payload
- held 설비 아이템의 즉시 preview, zone trace, grid/snap/rotation과 Actor 교체 commit
- 설치 설비의 Q Hold 회수 query/progress/cancel/commit
- zone 호환성, footprint 포함·충돌과 확장 단계 제한 검증
- 배치/회수에 따른 facility, NavMesh와 락커 수용량 등록 전환
- 확장 단계별 물리 열쇠 수와 최대 설치 락커 칸 정책
- 임시 락커 행동 슬롯과 customer capacity lease

Placement는 세탁기 수건 수량, 목욕탕 물 상태, customer 행동, key 상태와 UI hierarchy를 직접 변경하지 않는다. `IPlaceableFacility`을 통해 원래 domain owner의 side-effect-free query와 commit API를 조율한다.

## State Owners

| 책임 | Owner |
|---|---|
| grid 크기, 휠 Yaw 간격, 공통 회수 시간/거리/낙하 Z 오프셋 | `UFacilityPlacementSettings` |
| 설비 tag, preview/placed/item class, item mesh, footprint cell 수 | `UFacilityPlacementDefinition` |
| 배치 설비의 Definition, footprint/NavModifier, stage/transition guard | `UFacilityPlacementComponent` |
| 회수 아이템 payload, Root Static Mesh physics, 공통 held 위치·회전 | `APlaceableFacilityItemActor` |
| preview session, 누적 Yaw, Q target/경과 시간과 Actor 교체 조율 | `UPlayerFacilityPlacementComponent` |
| zone-local grid origin/axes, bounds와 허용 tag | `AFacilityPlacementZoneActor` |
| 설비별 회수 조건 | `IPlaceableFacility` 구현 Actor와 해당 domain owner |
| 목욕탕 물 상태 | `UBathWaterStateComponent` |
| 현재 확장 단계 | `ABathhouseExpansionAuthority` |
| 물리 key/hook 구성 | `ABathhouseKeyRackActor` |
| 설치 락커 용량, 활성 lease와 임시 슬롯 후보 | `ULockerCapacitySubsystem` |
| 실제 held 회수 아이템 Actor | 기존 `UPlayerCarryComponent` |
| prompt 표시 | Interaction/UI |

## Global Settings

`UFacilityPlacementSettings : UDeveloperSettings`는 Project Settings의 공통 authoring 정본이다.

- `GridSizeCm = 10.0`
- `RotationStepDegrees`
- `RecoveryHoldSeconds`
- `RecoveryDropZOffsetCm = 100.0`
- `PlacementTraceDistance`
- `RecoveryTraceDistance`

설비 Actor, Definition과 player component에 이 값을 복제하지 않는다. 모든 zone은 같은 grid size를 사용하며 zone은 local origin/axes만 소유한다. Player component는 preview의 현재 누적 Yaw와 hold elapsed 같은 transient 값만 소유한다.

## Placeable Facility Contract

`IPlaceableFacility`은 설치된 domain Actor의 native 계약이고 `IPhysicalCarryable`은 전용 회수 아이템의 기존 소지 계약이다. 한 Actor가 두 계약을 동시에 수행하지 않는다.

- `IPlaceableFacility`: Component/Definition 조회, side-effect-free 배치·회수 query, 명시적 payload export/import와 silent register/unregister stage
- `UFacilityPlacementComponent`: 배치 설비의 Definition, footprint/NavModifier, recovery drop transform과 transaction guard
- `APlaceableFacilityItemActor`: payload를 소유하는 Placement 전용 physical item, E pickup과 G free drop
- `UPlayerCarryComponent`: 단일 held identity와 release/consume commit
- private conversion transaction: 새 Actor stage, rollback snapshot, 원본 제거와 최종 publication 순서

지원되는 `ABathhouseFacilityActor` 계열과 `ATowelProcessingMachineActor`는 `IPlaceableFacility`만 canonical runtime capability로 사용한다. `SupportsFacilityActorConversion()`이 false인 Clean Towel Stack/Used Towel Bin은 recovery row를 노출하지 않으며 해당 class를 가리키는 Definition은 runtime/Data Validation에서 실패한다. `APlaceableFacilityItemActor`는 `IPlayerInteractable`과 `IPhysicalCarryable`을 구현하고 `EPhysicalCarryKind::Facility` 및 `FreeDrop` capability만 반환한다. 설비 아이템에는 exact fixed slot, E store/take와 assigned-slot recovery를 구현하지 않는다. 모든 소지품을 위한 공통 Actor나 `UPhysicalCarryableComponent`는 만들지 않는다.

Actor class 자체가 상태를 구분한다.

- 배치 상태: Definition의 `PlacedFacilityClass` instance가 domain/facility/Navigation에 등록됨
- 회수 아이템 상태: Definition의 `RecoveryItemClass` instance가 free world 또는 held 상태로 존재
- transaction stage: 새 instance는 외부 registry/event와 collision/physics를 활성화하지 않은 채 검증됨

동일 Actor identity 보존은 계약이 아니다. 변환 성공 시 원본은 파괴되고 새 Actor가 authoritative instance가 된다. Held 여부는 계속 `UPlayerCarryComponent`만 소유한다. 플레이어 회수가 아닌 비정상적인 설비 `EndPlay`는 아이템을 생성하지 않는다.

## Definition And Components

`UFacilityPlacementDefinition : UPrimaryDataAsset`은 stable id, facility Gameplay Tags, `PreviewActorClass`, `PlacedFacilityClass`, `RecoveryItemClass`, `RecoveryItemMesh`, X/Y footprint grid cell 수와 locker인 경우 `LockerSlotCount`를 가진다. class는 hard `TSubclassOf`로 유지하며 validation은 placed class의 `IPlaceableFacility` 및 conversion opt-in, item class의 정확한 native `APlaceableFacilityItemActor`, 서로 다른 class, preview class, stable id와 footprint를 검사한다. superclass가 `NotValidated`를 반환해도 자체 검사가 성공하면 `Valid`로 정규화한다. 모든 유효 Definition의 `RecoveryItemClass`는 같은 native 공통 class를 가리켜야 하며 설비별 item subclass는 허용하지 않는다.

`RecoveryItemMesh`는 설비 종류별로 다른 그림이 적용된 동일 규격 직육면체이며 mesh bounds와 일치하는 simple box collision 하나를 가진다. 초기 미지정 상태와 native test에서는 `/Engine/BasicShapes/Cube.Cube`를 fallback으로 사용한다. 별도 `RecoveryItemVisualScale`이나 collision extent property는 두지 않는다. `APlaceableFacilityItemActor`의 Root `UStaticMeshComponent` 기본 component scale이 모든 설비 아이템에 공통이며, collision은 선택된 mesh의 simple box와 Root scale을 그대로 따른다. complex-as-simple, box 이외/복수 simple shape, collision 누락과 physics 불가 mesh는 fail-closed validation 대상이다.

공통 `HeldTransform`은 item class default가 소유하되 일반 carryable처럼 location/rotation만 적용하고 scale은 항상 무시한다. pickup은 `SnapToTargetNotIncludingScale` 뒤 `SetRelativeLocationAndRotation`을 사용해 Root의 물리 scale을 보존한다. facility↔item 변환도 location/rotation만 전달하며 원본 Actor scale을 반대 class에 복사하지 않는다. spawn scale은 대상 class CDO Root scale에서 가져오고 recovery item의 이 값이 모든 설비 아이템의 공통 크기다.

실제 `PlacementFootprint`는 배치 설비 하위 `UBoxComponent` reference로 resolve한다. X/Y 크기는 전역 grid size의 정수배이고 Definition cell 수와 일치해야 한다. 회수 아이템 collision과 설치 footprint는 서로 대체하지 않는다. 기존 설비의 reflected `PackagePhysicalRoot`, `Mode`, `HeldTransform`, release 값과 `EPlaceableFacilityMode` ordinal은 asset migration을 위해 한 cycle 보존하지만 canonical 변환과 물리에 사용하지 않는다.

## Placement Payload

`FFacilityPlacementPayload`는 `Definition`과 item을 Outer로 하는 하나의 `UFacilityPlacementInstanceData` 파생 객체를 가진다. Placement는 opaque instance data를 해석하지 않고 item에 보관한 뒤 새 placed Actor에 전달한다. domain Actor는 recovery item을 deferred spawn한 뒤 해당 item을 Outer로 자신의 typed data object를 생성하며, placement 성공 전까지 item이 유일한 owner다.

- `UBathhouseFacilityPlacementInstanceData`: `FacilityType`, legacy `FacilityNumber`, `SelectionWeight`, `bEnabled`
- `UTowelMachinePlacementInstanceData`: `MachineKind`, `ProcessingDurationSeconds`
- contents, processing progress, bath water, slot reservation/occupancy, customer reference와 registry pointer는 포함하지 않는다.

각 domain Actor가 자신의 정확한 data class와 값 범위를 export/import하고 null, 잘못된 Outer/type 및 direct/nested/container의 Actor·ActorComponent reference를 재귀 검증해 거부한다. soft Actor/Component property와 delegate도 fail-closed다. data object는 runtime-only이고 Blueprint가 생성·변경하지 않는다. 회수 가능한 domain의 gate가 비영속 runtime 상태를 안전한 초기 조건으로 제한하며 새 Actor는 empty/waiting/available 상태로 시작한다. component topology, locker slot ID/transform과 presentation 구성은 `PlacedFacilityClass` class default가 공급한다. 임의의 Level instance component override나 전체 Actor serialization은 보존 계약이 아니므로 회수 가능한 설비의 영속 authoring은 Definition, placed class default 또는 위 명시 payload에 둔다.

## Placement Zone And Preview

`AFacilityPlacementZoneActor`는 zone-local grid origin/axes, bounds와 allowed facility Gameplay Tags를 소유한다.

- 카메라 중앙 trace가 compatible zone을 맞히면 zone local plane에서 후보 transform을 계산한다.
- grid는 preview 동안 LCtrl 상태와 무관하게 항상 표시한다.
- LCtrl Hold 중에만 local X/Y를 `GridSizeCm`에 quantize한다.
- Mouse Wheel 값마다 누적 Yaw에 `RotationStepDegrees`를 곱해 더한다.
- LCtrl을 놓아도 rotation 누적값은 유지한다.

Preview Actor는 표현 전용 ghost다. collision/domain Tick, facility/Navigation/locker 등록을 하지 않는다. C++이 transform, validity와 failure를 제공하고 Blueprint는 valid/invalid material, grid와 효과만 표현한다.

preview class 누락, spawn 실패 또는 session 도중 preview 파괴는 fail-closed다. session은 disabled placement prompt와 실패 사유를 유지하거나 안전하게 종료하며, live preview가 없는 상태에서는 commit할 수 없다. preview와 recovery target은 weak reference 및 `OnDestroyed`로 추적하고, 활성 preview/recovery session이 없으면 player placement component Tick은 꺼진다.

## Placement Validation And Commit

LMB commit 직전에 다음을 같은 frame 상태로 다시 검증한다.

1. local player가 preview를 시작한 동일 `APlaceableFacilityItemActor`를 실제로 들고 있고 session owner가 일치하며 computer/suppression이 활성화되지 않았다.
2. 정확히 하나의 compatible zone이 resolve된다.
3. footprint의 네 world corner를 zone local space로 변환했을 때 전체가 실제 scaled/rotated zone bounds 안에 포함된다.
4. Definition footprint가 blocking object와 겹치지 않고 유효 floor support를 가진다. non-blocking trigger overlap은 배치를 막지 않으며 PhysicsBody 등 실제 blocking channel은 막는다.
5. footprint 크기와 Definition cell 수가 전역 grid 계약을 만족한다.
6. payload, `PlacedFacilityClass`와 설비 domain precondition 및 확장 단계 설치 제한을 만족한다.

성공 후보에서는 `PlacedFacilityClass`를 candidate location/rotation과 해당 class CDO Root scale로 `OverrideRootScale` deferred spawn하고 payload를 import한 뒤 외부 등록이 없는 staged 상태로 `FinishSpawning`한다. 후보 footprint도 CDO Root 기준 상대 transform을 사용하므로 preview/validation과 실제 spawn scale이 일치한다. held item Root scale은 placed Actor로 복사하지 않는다. 새 Actor의 Definition/footprint/component topology와 domain 조건을 다시 검사하고 facility/locker/NavModifier registration을 silent stage한다. 그 다음 carry owner가 held item reference를 silent clear하고 item을 placement-consumed로 표시한 뒤 제거한다. 모든 내부 상태가 확정된 후 facility/capacity와 held-change event를 각 한 번 발행한다.

새 facility 초기화·등록, held clear 또는 item 제거 전 단계가 실패하면 staged facility 등록을 취소하고 Actor를 파괴하며 원래 item의 held identity, attachment, Root scale, collision/physics snapshot과 preview를 유지한다. 정상 placement로 소비되는 item의 `EndPlay`는 free-world recovery나 추가 facility spawn을 실행하지 않는다. notification 재진입은 transaction guard가 거부한다. publication callback이 새 Actor를 파괴하면 captured subsystem만으로 invalid registry를 compact하고 제거 event/capacity revision을 보상한 뒤 Actor/component를 다시 접근하지 않는다.

## Input Ownership

- free-world 설비 아이템을 E로 들면 즉시 placement preview 시작
- LMB: 유효 후보 배치 commit
- LCtrl Hold: 위치 grid snap
- Mouse Wheel: 전역 간격 Yaw 회전
- E: free-world 설비 아이템 pickup에만 사용하며 held 설비 아이템의 fixed-slot 동작은 없음
- G: 기존 held-position 약한 free drop; 성공 시 preview 종료
- ESC: 설비 아이템 preview에서는 no-op
- Q Hold: 응시 중인 설치 설비 회수; 현재 held item과 무관

LMB owner 우선순위는 `Computer > Placement > Equipment`다. Placement가 LMB를 소유하는 동안 equipment use를 시작하지 않는다. Q recovery는 Computer focus/suppression 중에는 실행하지 않는다.

## Recovery Transaction

설비 Actor는 포커스된 일반 `IPlayerInteractable` query에 recovery 표시·가능 여부·실패 사유를 supplemental row로 합친다. 따라서 회수 prompt는 별도 player-global recovery trace 결과에 의존하지 않는다. `UPlayerFacilityPlacementComponent`는 활성 Q hold의 target과 elapsed만 소유하고 같은 combined query의 progress를 보충한다.

Q Started에서 현재 target을 고정하고 활성 recovery session의 Component Tick에서 elapsed를 누적한다. `Elapsed / RecoveryHoldSeconds`가 prompt progress이며 기준 시간에 도달한 Tick에서 release를 기다리지 않고 즉시 한 번 commit한다. 기준 시간 전 Q release, gaze/range 이탈, suppression, target/owner EndPlay 또는 회수 조건 변경은 session을 cancel하고 progress를 0으로 만든다. target `OnDestroyed`는 다음 Tick을 기다리지 않고 즉시 session을 취소하며, 자동 commit 직전에도 local owner와 suppression을 재검증한다.

회수 precondition:

- `SupportsFacilityActorConversion() == true`; Clean Towel Stack/Used Towel Bin은 항상 제외
- Washer/Dryer: inventory count 0, processing state `Waiting`
- Bath: 모든 use slot `Available`, `UBathWaterStateComponent::IsEmpty()`
- Locker bank: 모든 action slot `Available`, 회수 뒤 설치 용량이 활성 lease 수 이상
- 공통: 유효 Definition/footprint와 placed/item class mapping, export 가능한 payload, transaction 미실행

회수 낙하 transform은 현재 `PlacementFootprint` 월드 중심의 X/Y와 월드 bounds 바닥 Z에 공통 `RecoveryDropZOffsetCm`를 더한 위치, 원본의 회전과 recovery item class CDO Root scale을 사용한다. 원본 설비 Actor scale은 item에 복사하지 않는다. passive query와 commit 재검사는 같은 Definition collision-query builder를 사용한다. mesh bounds origin과 단일 simple box center/rotation은 0이어야 하고 bounds/box 크기가 일치해야 한다. 원본 설비 Actor는 overlap query ignore 목록에만 넣으며 다른 WorldStatic/WorldDynamic blocker는 그대로 회수를 막는다.

회수 commit은 새 item을 collision/physics 비활성 staged 상태로 완전히 준비한 다음 원본 facility/domain/NavModifier 등록을 silent 해제한다. 이어 item을 예정 위치의 free-world physics로 활성화하고 선형·각속도를 0으로 유지한 뒤 원본 설비를 마지막에 파괴한다. `Destroy()`까지 성공해야 새 item과 registry event를 publish한다. 자동 pickup, held item 변경과 impulse는 없다.

item 준비·collision, payload export, silent unregister, physics 활성화 또는 원본 파괴가 실패하면 staged item을 제거하고 원본 facility registration/NavModifier를 복원한다. 회수 target destruction callback은 transaction이 소유한 정상 파괴와 외부 파괴를 구분한다. 외부 `EndPlay`, world teardown과 `FellOutOfWorld`는 player recovery가 아니므로 item을 생성하지 않는다. 같은 Q 입력, 재진입 또는 완료/EndPlay 경합은 최대 하나의 원본과 하나의 최종 authoritative Actor만 남겨야 한다.

## Bath Water State

`UBathWaterStateComponent`는 최소 `Empty`, `Filling`, `Filled`, `Draining` 상태와 선택적 정규화 수량을 소유한다. `IsEmpty()`만 회수 gate에 제공하며 mesh visibility와 Blueprint bool은 authoritative하지 않다. Blueprint는 상태 변경 event로 물 표현만 갱신한다.

## Locker Bank And Capacity

1/4/8칸 락커는 하나의 `ABathhouseFacilityActor`와 자식 `ULockerActionSlotComponent` N개다. 각 component는 기존 slot 예약/점유 계약을 재사용하고 author가 명시한 non-empty stable `LockerSlotId`만 가지며 플레이어 번호와 key number는 갖지 않는다. component 이름 fallback은 허용하지 않고 count/ID 중복/누락/확장 상한을 side-effect-free preparation에서 모두 검증한다.

`ULockerCapacitySubsystem`은 `Placed`인 locker bank의 operational action slot만 등록한다.

- `InstalledLockerCapacity = 등록된 action slot 총수`
- `ActiveLeaseCount = 체크인 승인 후 아직 종료되지 않은 lease 수`
- 정상 commit에서 `ActiveLeaseCount <= InstalledLockerCapacity`
- 확장 제한은 bank Actor 수가 아니라 Definition의 `LockerSlotCount` 합계

check-in key 전달 transaction에서 provisional lease를 먼저 확보하고 key/session commit이 실패하면 rollback한다. checkout, timeout, technical cleanup과 EndPlay는 idempotent release를 호출한다. lease 없는 check-in timeout은 안전한 no-op다.

locker Actor의 비정상 EndPlay는 player recovery로 처리하거나 package item을 생성하지 않는다. weak registry compaction은 사라진 bank/slot을 용량에서 제거하되 유효 customer lease는 보존하고 신규 check-in을 차단하며, 용량이 lease보다 작아진 invariant fault를 기록하고 locker 재배치 또는 customer cleanup까지 유지한다. customer owner가 사라진 lease는 compaction으로 제거되고, facility registry 역시 invalid weak entry를 외부 query에 노출하지 않는다.

탈의와 착의는 각각 random available locker action slot을 행동 동안만 reserve/use/release한다. 같은 슬롯일 필요가 없고 `CustomerSession`이 `ClothesStored`만 소유한다. 회수 대상 bank에 Reserved/Occupied slot이 하나라도 있으면 실패한다. Draining, 자동 회수와 montage 강제 중단은 만들지 않는다.

## Expansion And Key Pool

`UBathhouseExpansionDefinition : UPrimaryDataAsset`의 각 단계는 `KeyPoolSize`와 `MaxInstalledLockerSlots`를 가진다. 모든 단계에서 `KeyPoolSize >= MaxInstalledLockerSlots`를 asset validation한다.

`ABathhouseExpansionAuthority`는 current tier의 단일 runtime owner다. 이번 범위는 initial tier와 단계 상승 API만 제공하며 구매/경제/UI와 runtime downgrade는 제외한다.

`ABathhouseKeyRackActor`는 단계의 `KeyPoolSize`만큼 기존 key-hook pair를 구성한다. 락커 배치·회수는 key 수, key 번호나 이미 key를 받은 customer를 변경하지 않는다. key number는 물리 token/hook 식별과 3D 표시에만 쓰고 locker lookup에는 사용하지 않는다.

## Compatibility

- `EBathhouseFacilityType::ShoeLocker` ordinal은 한 migration cycle 보존하되 신규 runtime에서 사용하지 않는다.
- `FacilityNumber`, numbered lookup과 locker topology validation API는 deprecated wrapper로 보존하되 신규 customer/key flow가 호출하지 않는다.
- `EBathhouseCustomerActivity::StoreShoes`, `WearShoes`와 duration property는 asset migration 동안 이름/ordinal을 보존하되 신규 StateTree에서 제거한다.
- 기존 `EPlaceableFacilityMode`, `UFacilityPlacementComponent::Mode/HeldTransform`, release 값, `PackagePhysicalRoot` subobject 이름과 `OnModeChanged`는 한 migration cycle 보존한다. placed Actor의 legacy physical-carry query와 `Packaged` 전환은 fail-closed이며 canonical path가 호출하지 않는다.
- `APlaceableFacilityItemActor`, `FFacilityPlacementPayload`와 Definition class/mesh property는 신규 symbol이다. 기존 reflected symbol을 rename/delete하지 않으므로 Core Redirect는 추가하지 않는다.

## Blueprint/API Contracts

Editor authoring:

- 설비별 Definition의 placed/item/preview class와 `RecoveryItemMesh`
- placed facility의 footprint/NavModifier와 placed 표현
- 공통 회수 아이템 Root Static Mesh scale 및 위치·회전 전용 `HeldTransform`
- zone bounds/local axes/allowed tags
- Project Settings의 grid/Yaw/recovery/time/distance 값
- bath water initial state
- locker bank action slots와 Definition `LockerSlotCount`
- expansion Definition/initial tier와 key rack anchors/classes

Blueprint는 preview/grid, recovery progress, water state와 capacity/key-pool 변경을 표현할 수 있다. 회수 아이템 mesh 선택, collision/physics와 payload 변환은 native가 적용한다. Blueprint는 배치 판정, 회수 조건, Actor 생성·파괴 transaction, lease와 key pool 수량을 변경하지 않는다. 최초 구현은 Content 변경 없이 Engine 기본 Cube fallback으로 동작하며 후속 Editor 단계에서 같은 규격의 설비별 직육면체 mesh를 Definition에 연결한다.

Clean Towel Stack/Used Towel Bin용 Placement Definition은 authoring하지 않는다. 두 Actor의 placement/recovery opt-out은 native 고정 계약이며 Blueprint에서 우회하지 않는다.

## Dependencies

- Placement -> Interaction carry/query/result와 supplemental intent-source contract
- Facility/Towel -> Placement의 placeable-facility contract
- Interaction key rack -> Facility expansion authority query
- Customer -> Facility locker capacity/slot API
- UI -> Interaction combined prompt data
- Placement/Facility -> GameplayTags, NavigationSystem
- `UFacilityPlacementSettings`를 위해 기존 `DeveloperSettings` runtime module을 사용하며 payload 때문에 새 module이나 plugin을 추가하지 않는다.

## Manual Review Points

- grid가 항상 보이고 LCtrl이 위치 quantization만 바꾸는지 확인한다.
- zone-local axes, 10cm 기본 grid, footprint full containment와 collision failure를 확인한다.
- scaled/rotated zone과 recovery item Root mesh simple collision 기준 blocking/non-blocking overlap을 확인한다.
- missing/lost preview가 fail-closed이고 LMB 실패가 held item의 identity, Root scale, 물리 snapshot과 registry를 보존하는지 확인한다.
- 성공 notification에서 observer가 empty hand와 완성된 registry만 보고, 동기 재진입/파괴에도 stale entry나 invalid access가 없는지 확인한다.
- Q 취소 조건과 기존 held item 독립성, 무충격 전용 item 생성 및 원본 facility의 마지막 파괴를 확인한다.
- 회수 실패는 원본 facility만, 배치 실패는 원본 item만 남기며 성공 시 반대쪽 Actor가 정확히 하나만 남는지 확인한다.
- item의 `HeldTransform` scale이 무시되고 pickup/drop 뒤에도 Root mesh scale과 simple collision이 보존되는지 확인한다.
- 원본 facility scale이 item으로, item Root scale이 새 facility로 누수되지 않는지 확인한다.
- 설비 item이 G free drop만 지원하고 generic exact fixed slot에는 저장되지 않는지 확인한다.
- washer/dryer contents, bath water/customer, locker action/capacity 감소가 각각 회수를 막는지 확인한다.
- Clean Towel Stack/Used Towel Bin에 Q recovery row가 없고 해당 class Definition이 validation에서 거부되는지 확인한다.
- 1/4/8 bank component 수, Definition slot 수와 확장 제한 합계가 일치하는지 확인한다.
- check-in 경쟁에서 lease/key가 함께 commit/rollback되고 모든 종료 경로가 lease를 한 번만 반환하는지 확인한다.
- locker 비정상 EndPlay가 item/lease를 복제하지 않고 admission을 차단하는지 확인한다.
- 탈의/착의가 독립 random slot을 사용하고 `ClothesStored`는 session에만 남는지 확인한다.
- key pool이 확장 단계에만 반응하고 locker 변경으로 key/customer 번호가 바뀌지 않는지 확인한다.
- Nav Modifier와 `Dynamic Modifiers Only`가 필요한 영역만 갱신하는지 확인한다.
