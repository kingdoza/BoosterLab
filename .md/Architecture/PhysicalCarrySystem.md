# Physical Carry System

## Implementation Status

Q47~Q52의 exact fixed slot과 actual-held-position free drop이 Source와 native automation에 구현되었다. key/wet mop/towel basket/monkey wrench는 공통 single carry coordinator를 사용하고 기본적으로 전용 fixed slot과 G free drop을 지원한다. free drop은 camera-origin 목표 위치로 이동하지 않고 실제 held world pose에서 질량 독립 `120/15 cm/s` velocity change를 적용하며, 모든 physical carry root는 free-world physics에서 CCD를 사용한다. checkout key도 같은 physical transaction으로 단일 Counter drop point 주변에 동일 인스턴스를 반환한다.

equipment slot Blueprint/instance 배치, exact `AssignedItem`/anchor, key physics bounds와 기존 Blueprint release velocity 값은 코드 리뷰 후 Editor 단계에서 authoring한다.

## Source Scope

```text
Source/BathhouseSim/Public/Interaction/
  PhysicalCarryable.h
  PhysicalCarryFixedSlot.h             # 신규 fixed-slot native interface
  PhysicalCarryFixedSlotActor.h        # 신규 equipment slot actor
  PlayerCarryComponent.h
  BathhouseKeyActor.h
  BathhouseKeyHookActor.h

Source/BathhouseSim/Private/Interaction/
  PhysicalCarryFixedSlotActor.cpp
  PhysicalCarryPlacementTransaction.h  # private non-UObject snapshot/rollback helper
  PhysicalCarryPlacementTransaction.cpp
  PlayerCarryComponent.cpp
  BathhouseKeyActor.cpp
  BathhouseKeyHookActor.cpp

Source/BathhouseSim/Private/Tests/
  CleaningTowelAutomationTests.cpp
  CombatRecoveryAutomationTests.cpp
  PhysicalCarryFixedSlotAutomationTests.cpp
```

Concrete carryable 확장 대상은 `Cleaning/WetMopActor`, `Towel/TowelBasketActor`, `Combat/MonkeyWrenchActor`다. Source 폴더를 이동하거나 공통 carry Actor/Component를 만들지 않는다.

## Responsibilities

- inventory/hotbar 없는 0개 또는 1개의 physical Actor 소지
- 모든 일반 carryable의 G free drop과 전용 fixed slot 기본 capability
- item instance와 fixed slot의 일대일 authoring·runtime binding
- fixed slot↔held와 held→free world의 원자적 transaction
- 실제 held world transform을 보존하는 physics release
- 질량 무시 약한 forward/upward velocity-change impulse
- 모든 physical carry root의 free-world CCD 기본 활성화
- fixed-slot 우선 비정상 복구와 last-safe fallback
- key의 number/customer/counter lifecycle과 물리 placement의 공존

Physical Carry는 input mapping, cleaning/damage/towel count, customer routine와 UI layout을 소유하지 않는다.

## State And Execution Owners

| 책임 | Owner |
|---|---|
| 현재 held Actor | `UPlayerCarryComponent` |
| exact assigned item·anchor·점유 | fixed slot Actor |
| equipment carrier·last-safe transform | concrete equipment Actor |
| key number/domain state | `ABathhouseKeyActor` |
| take/store/free-drop transaction guard | `UPlayerCarryComponent` |
| E/G intent routing·결과 방송 | Interaction/Character |
| mesh·slot 표현 | Blueprint |

## Class Growth Decision

구현 전 `PlayerCarryComponent.h/.cpp`는 약 `88/388`줄이었고 구현 후 약 `91/414`줄이다. 기존 safe-location sweep resolver는 제거했으며 fixed-slot snapshot/rollback의 기계적 세부사항은 Component에 누적하지 않는다.

`FPhysicalCarryPlacementTransaction` private non-UObject helper가 parent/socket, transform, collision, CCD, physics와 slot snapshot의 적용·rollback을 응집해 관리한다. Tick, delegate, reflected property와 독립 gameplay state는 갖지 않는다. `UPlayerCarryComponent`는 `HeldObject`, reentrancy guard, transition 검증과 최종 commit owner로 유지한다. 별도 `UPhysicalCarryableComponent`나 public transaction UObject는 만들지 않는다.

## Default Capability Contract

`IPhysicalCarryable`은 공통 Actor나 상태 저장 Component가 아니라 native 계약으로 유지한다. 기본 capability는 다음 두 가지다.

- `FreeDrop`: held 상태에서 G free drop
- `FixedSlot`: 지정된 exact fixed slot에 E store/take

새 carryable은 둘 다 지원하는 것이 기본이며 예외만 명시적으로 opt-out한다. `CanFreeDrop`은 현재 상태까지 재검증하고, fixed-slot query도 assigned item, 점유, held identity와 Actor validity를 side effect 없이 검사한다.

공통 계약은 다음 정보를 제공한다.

- display name, kind와 `HeldTransform`
- pickup/held presentation lifecycle
- physical root primitive
- free-drop 허용 여부와 forward/upward velocity-change 값
- assigned fixed slot binding과 store/take commit notification
- last-safe/fixed-slot recovery

각 concrete Actor는 자신의 domain state와 reflected authoring 값을 직접 소유한다.

## Exact Fixed Slot

신규 `APhysicalCarryFixedSlotActor`는 equipment용 world interaction target이다.

```text
SceneRoot
├─ InteractionCollision
└─ ItemAnchor
```

Editor authoring:

- `AssignedItem`: 정확한 Actor instance 하나
- `SlotDisplayName`
- `bStartOccupied`: 기본 `true`
- interaction collision과 `ItemAnchor` transform

`AssignedItem`은 `EditInstanceOnly`이므로 얇은 slot Blueprint의 CDO에서는 비어 있는 것이 정상이다. template/CDO data validation은 stable default subobject 구조만 검사하고, exact item 누락·capability·중복 참조 검증은 배치된 non-template slot instance와 runtime initialization에서 수행한다.

`AssignedItem`은 `IPhysicalCarryable`과 fixed-slot capability를 가져야 한다. 같은 kind의 다른 instance나 임의의 carryable은 받을 수 없다. duplicate slot assignment와 누락·자기참조·invalid primitive는 data validation/runtime initialization에서 명시적으로 실패시킨다.

slot은 BeginPlay initialization에서 assigned item과 runtime 양방향 weak binding을 구성한다. 여러 slot이 같은 item을 bind하면 하나를 임의로 선택하지 않고 충돌한 slot 전체를 disabled 처리한다. concrete item은 이 binding을 recovery 조회에만 사용하고 점유 정본을 복제하지 않는다.

슬롯이 occupied일 때 item collision/physics는 꺼지고 `ItemAnchor`에 snap된다. interaction trace는 slotted item이 아니라 `InteractionCollision`을 맞히며 slot이 take/store prompt와 실패 이유를 제공한다. Blueprint는 `OnSlotOccupancyChanged`로 mesh/animation/sound를 표현할 수 있지만 점유를 변경하지 않는다.

## Fixed Slot Interaction

Q49에 따라 E는 fixed placement, G는 free drop으로 분리한다.

- occupied slot + empty hand: `물건 가져가기`
- empty slot + exact assigned item held: `물건 놓기`
- empty slot + empty hand: `놓을 물건이 없습니다.`
- occupied slot + occupied hand: `이미 다른 물건을 들고 있습니다.`
- empty slot + wrong item: `이 슬롯에 놓는 물건이 아닙니다.`
- invalid assignment/state: 연결 또는 상태 오류를 정확한 `FText`로 반환

G는 slot proximity를 검사하거나 자동 snap하지 않는다. slot query는 side effect가 없고 execute가 모든 조건을 다시 검증한다.

## Atomic Transfer

`UPlayerCarryComponent`가 다음 transition의 transaction coordinator다.

```text
FixedSlot -> Held
Held -> FixedSlot
Held -> FreeWorld
```

Commit 전에 item/slot/carry identity, active transaction guard, attachment, primitive와 placement 조건을 검증한다. attach, collision, CCD, physics 또는 domain commit이 실패하면 held reference, slot occupancy, parent/socket, relative/world transform, collision, CCD와 physics를 이전 snapshot으로 복구한다.

store/free-drop 시 active equipment use와 held motion을 먼저 한 번 cancel한다. 이후 placement가 실패해도 use는 취소된 상태로 유지하지만 item ownership과 위치는 유지한다. 성공 후에만 `HeldObject`를 지우고 change delegate를 한 번 방송한다.

## Held-Position Free Drop

`HeldPosition`은 새 property가 아니라 active held motion을 cancel한 뒤의 실제 Actor/physical primitive world transform이다.

```text
HeldAnchor WorldTransform × Item HeldTransform
```

새 free-drop 흐름:

1. equipment use/motion cancel과 held baseline 복구
2. current primitive world pose와 rollback snapshot 저장
3. 해당 pose에서 world blocking overlap 검사
4. `KeepWorldTransform` detach
5. 별도 `SetActorLocation` 없이 CCD와 collision/physics 활성화
6. 선형·각속도 초기화 후 약한 velocity-change impulse 적용
7. concrete item free-world state와 carry release commit

canonical 경로는 camera origin, `ThrowSpawnDistance`, 목표 위치 sweep과 안전지점 teleport를 사용하지 않는다. Q51에 따라 current held pose가 WorldStatic/WorldDynamic blocker와 겹치면 drop은 실패하고 item은 계속 held된다.

## Release Velocity And Collision

기본값:

```text
Forward velocity change: 120 cm/s
Upward velocity change: 15 cm/s
```

합성값은 camera forward와 world up을 사용하고 `AddImpulse(..., bVelChange=true)`로 적용한다. 질량은 결과 속도에 영향을 주지 않는다. 값은 item class default에서 조정할 수 있지만 현재 네 carryable의 기본값은 모두 약하게 통일한다.

기존 reflected `ThrowImpulseStrength`는 rename/delete하지 않고 forward velocity-change 값으로 유지한다. `ThrowSpawnDistance`, `DropSweepChannel`, `DropSweepClearance`는 호환을 위해 deprecated 상태로 한 migration cycle 보존하되 canonical placement 계산에는 사용하지 않는다. upward 값은 신규 reflected authoring property로 추가한다.

Q50 B에 따라 free-world carryable은 `Pawn` channel을 영구 `Ignore`한다. player와 customer Pawn 모두 물리 충돌하지 않으며 WorldStatic/WorldDynamic collision은 유지한다. held/fixed-slot 상태에서는 기존처럼 collision 전체를 끈다.

모든 `IPhysicalCarryable::GetPhysicalCarryPrimitive()`는 CCD가 기본이다. key의 `KeyPhysicsRoot`와 wet mop/towel basket/monkey wrench의 `WorldMesh` native default 및 Blueprint component template에서 `bUseCCD=true`를 유지한다. 공통 free-world transaction은 serialized Blueprint 값이나 이전 상태에 의존하지 않고 physics 활성화 직전에 CCD를 다시 켠다. `FellOutOfWorld`의 fixed-slot 불가 시 concrete last-safe `SetWorldPhysics(true)` 경로도 같은 플래그를 복구한다. 따라서 신규 carryable은 common transaction과 recovery helper에서 같은 규칙을 구현하며, 아이템별 opt-out 분기는 두지 않는다.

held/fixed-slot 상태는 collision과 physics가 꺼져 있으므로 CCD 플래그를 지울 필요가 없다. placement가 commit 전에 실패하면 transaction snapshot의 이전 CCD 값을 복구하여 원자성을 보존한다. CCD는 빠르고 얇은 rigid body의 discrete-step tunneling을 줄이는 보조 계약이며, 유효한 simple collision과 WorldStatic/WorldDynamic block 응답을 대체하지 않는다.

## Key Extension

`ABathhouseKeyHookActor`는 번호 topology와 customer flow 때문에 generic equipment slot로 교체하지 않고 `IPhysicalCarryFixedSlot`을 추가 구현한다. 기존 `KeyNumber`, `KeyActor`, `KeyAnchor`와 facility validation을 보존한다.

`EBathhouseKeyState` 끝에 `DroppedInWorld`를 추가한다.

| 현재 | 행위 | 다음 |
|---|---|---|
| `AtHook` | E take | `HeldByPlayer` |
| `HeldByPlayer` | G free drop | `DroppedInWorld` |
| `DroppedInWorld` | item을 보고 E take | `HeldByPlayer` |
| `HeldByPlayer` | 원래 hook을 보고 E store | `AtHook` |
| `HeldByPlayer` | check-in transfer | `AssignedToCustomer` |
| `AssignedToCustomer` | checkout physical return | `OnCounter` |
| `OnCounter` | key를 보고 E take | `HeldByPlayer` |

free drop은 `KeyNumber`, original `KeyHook`과 unique token identity를 바꾸지 않는다. dropped key를 hook에 자동 반환하지 않고 player가 다시 들고 E로 반환한다. checkout도 새 key를 spawn하지 않고 같은 hidden `AssignedKey`를 Counter의 후보 transform으로 옮긴 뒤 `FPhysicalCarryPlacementTransaction::ApplyFreeWorld` 계약으로 physics를 켠다. velocity change는 player free drop과 동일한 key authoring 값을 사용하되 방향은 Counter drop point forward와 world up이다.

checkout commit 전에 key root bounds로 WorldStatic/WorldDynamic blocking overlap을 검사한다. exact point를 먼저 시도하고 Counter의 authorable local XY 범위에서 제한 횟수만 탐색한다. 성공해야 `AssignedToCustomer -> OnCounter`와 customer checkout key-return guard를 commit하며, 실패하면 transform, visibility, collision/physics와 key/session state를 모두 유지한다. Counter slot reservation이나 occupancy는 사용하지 않는다.

key의 `KeyPhysicsRoot: UBoxComponent`만 free-world collision/physics를 담당하고 기존 `SceneRoot`/`WorldMesh`는 그 아래에서 presentation을 담당한다. 기존 reflected component 이름은 삭제·rename하지 않으며 Blueprint hierarchy/relative transform은 Editor에서 재검증한다.

## Equipment Extension

Wet mop, towel basket과 monkey wrench는 exact `APhysicalCarryFixedSlotActor`에 store/take할 수 있다.

- Wet mop store/drop은 cleaning progress, mopping state와 loop motion을 cancel한다.
- Monkey wrench store/drop은 active swing/hit lifecycle과 one-shot motion을 cancel한다.
- Towel basket store/drop은 inventory state/count/revision과 Stack presentation을 그대로 유지한다.

fixed-slot placement는 cleaning, combat 또는 towel transaction이 아니다. slot/carry ownership만 변경한다.

## Recovery And EndPlay

carrier EndPlay와 `FellOutOfWorld`는 유효하고 비어 있는 exact fixed slot을 먼저 사용하고, 불가능하면 last-safe world transform에 physics 상태로 복구한다. 새 Actor를 spawn하지 않는다.

slot이 runtime에 파괴될 때 stored item이 유효하고 world teardown이 아니면 anchor world transform에서 free-world 상태로 전환하고 fixed binding을 지운다. 실제 item Actor의 `EndPlay`가 시작된 뒤에는 같은 instance를 되살릴 수 없으므로 carry/slot external reference만 정리한다. runtime gameplay는 carryable에 직접 `Destroy()`를 호출하지 않고 recoverable failure를 `FellOutOfWorld` 이전 경로에서 처리한다.

EndPlay, fall recovery와 transaction retry는 delegate, timer, attachment와 slot occupancy를 중복 변경하지 않아야 한다.

## Blueprint/API And Editor Contracts

신규 reflected 계약:

- `APhysicalCarryFixedSlotActor`
- stable default subobject `SceneRoot`, `InteractionCollision`, `ItemAnchor`
- `AssignedItem`, `SlotDisplayName`, `bStartOccupied`
- `OnSlotOccupancyChanged`
- key `KeyPhysicsRoot`
- item별 upward velocity-change authoring 값
- 네 carryable physical root의 `BodyInstance.bUseCCD=true`

보존 계약:

- `IPhysicalCarryable`, `HeldTransform`, `HeldKeyAnchor`
- `UPlayerCarryComponent`와 `OnHeldObjectChanged`/`OnHeldKeyChanged`
- `DropCarryAction`, 기존 key/hook API와 state ordinal
- `ThrowImpulseStrength`, `ThrowSpawnDistance`, drop sweep property

기존 symbol rename/delete가 없으므로 Core Redirect는 추가하지 않았다. Source 구현은 Content를 수정하지 않았으며, Editor 단계에서 equipment slot Blueprint/instances, assigned item, anchors, key physics bounds와 기존 Blueprint velocity 값을 authoring한다.

## Dependencies

- Physical Carry는 Interaction Source package와 Engine collision/physics를 사용한다.
- Cleaning/Towel/Combat은 concrete item state에서 Physical Carry public 계약을 구현한다.
- Character는 Interaction을 통해 E/G intent만 전달한다.
- UI는 Interaction query/result만 표시한다.
- 신규 runtime module dependency는 필요하지 않다.

## Manual Review Points

- key/mop/basket/wrench가 모두 exact slot과 G free drop을 지원하는지 확인한다.
- 같은 kind의 다른 instance와 duplicate assigned slot이 거부되는지 확인한다.
- G가 slot 근처에서 snap하지 않고 actual held pose에서 출발하는지 확인한다.
- wall overlap 실패가 attachment, carry reference와 presentation을 보존하는지 확인한다.
- 모든 질량에서 약한 동일 velocity change를 얻고 free-world item이 Pawn을 영구 무시하는지 확인한다.
- 네 carryable class default와 Blueprint physical root에서 CCD가 켜지고, free drop 및 slot 파괴 free-world 전환에서도 유지되는지 확인한다.
- 강제된 late free-drop 실패가 이전 attachment/collision/physics와 함께 이전 CCD 값도 복구하는지 확인한다.
- dropped key의 number/hook/customer/counter transaction이 보존되는지 확인한다.
- non-empty basket의 inventory와 presentation revision이 slot 이동으로 바뀌지 않는지 확인한다.
- store/drop이 active mop/wrench use를 한 번 cancel하고 다음 사용이 정상 복구되는지 확인한다.
- carrier/slot/item/fall cleanup에서 item과 slot occupancy가 소실·복제되지 않는지 확인한다.
