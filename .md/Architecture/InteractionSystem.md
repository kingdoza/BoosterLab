# Interaction System

## Implementation Status

이 문서는 현재 구현된 primary/secondary/hold interaction intent, Computer focus suppression, 범용 LMB equipment-use, held motion과 equipment prompt intent를 정의한다. single physical carry의 exact fixed slot과 held-position free drop은 [PhysicalCarrySystem.md](PhysicalCarrySystem.md)가 상세히 정의한다.

## Source Scope

```text
Source/BathhouseSim/Public/Interaction/
  InteractionTypes.h
  PlayerInteractable.h
  PhysicalCarryable.h
  PhysicalCarryFixedSlot.h
  PhysicalCarryFixedSlotActor.h
  HeldEquipmentUsable.h
  PlayerInteractionComponent.h
  PlayerCarryComponent.h
  PlayerEquipmentUseComponent.h
  HeldEquipmentMotionComponent.h
  BathhouseKeyActor.h
  BathhouseKeyHookActor.h

Source/BathhouseSim/Private/Interaction/
  PlayerInteractionComponent.cpp
  PhysicalCarryFixedSlotActor.cpp
  PhysicalCarryPlacementTransaction.h
  PhysicalCarryPlacementTransaction.cpp
  PlayerCarryComponent.cpp
  PlayerEquipmentUseComponent.cpp
  HeldEquipmentMotionComponent.cpp
  BathhouseKeyActor.cpp
  BathhouseKeyHookActor.cpp

Source/BathhouseSim/Private/Tests/
  BathhouseDomainTests.cpp  # single-key carry와 interaction attempt result coverage
  CleaningTowelAutomationTests.cpp  # mop/basket carry, hold cleaning과 physical placement coverage
  ComputerAutomationTests.cpp  # suppression, computer focus/input/pointer/cleanup coverage
  CombatRecoveryAutomationTests.cpp  # equipment routing, health, melee와 soft interruption coverage
```

## Responsibilities

- first-person camera 중앙 line trace와 primary/secondary interaction 실행
- instant/hold primary lifecycle과 target/context 재검증
- side-effect 없는 상호작용 조회와 실행 직전 재검증
- query 상태와 별개인 interaction 실행 결과의 일회성 native notification
- inventory/hotbar 없는 single physical carry의 입력·query 연결
- 공용 held anchor, item별 local held transform, fixed-slot/free-drop 계약 연결
- LMB 장비 사용의 side-effect-free query, Begin/Update/End/Cancel routing
- one-shot/hold equipment use 공통 lifecycle와 held Actor transform 표현
- E/F world target과 별도인 LMB equipment-use prompt/result 표시 데이터
- 번호 key actor의 domain lifecycle과 physical placement 연결
- focus와 held key 변화의 UI용 delegate
- 외부 focus mode가 활성화된 동안 active hold, trace와 prompt를 중단하는 C++ suppression 경계

Interaction은 cleaning progress, attack/damage/health, towel count/machine, customer routine, facility slot, queue와 player money를 소유하지 않는다.

## Interaction Contract

`IPlayerInteractable`은 다음 계약을 제공한다.

- `QueryInteraction(Context)`: 표시 여부, 실행 가능 여부, 대상명, 행동명과 실패 이유를 반환하며 상태를 바꾸지 않는다.
- `ExecuteInteraction(Context)`: 실행 직전에 조건을 다시 검증하고 성공/실패 결과를 반환한다.
- 기존 `bCanInteract`, `ActionName`, `FailureReason`은 primary 의미를 유지한다.
- target은 optional secondary 표시/가능 여부, action name과 failure reason을 추가로 반환할 수 있다.
- primary는 `Instant` 또는 `Hold` activation mode를 선언할 수 있다. 기존 target의 default는 `Instant`다.
- secondary는 현재 계약에서 Started 한 번의 instant 실행만 사용한다.
- 기존 `ExecuteInteraction(Context)`는 primary API로 유지하고 optional secondary execute와 hold begin/update/cancel 계약을 추가한다.

`FPlayerInteractionContext`는 interactor, `UPlayerCarryComponent`, hit actor/component와 hit 정보를 가진다. `FPlayerInteractionQuery`와 결과 문구는 localization 가능한 `FText`를 사용한다.

`FPlayerInteractionQuery`는 기존 E primary/F secondary 필드를 유지하고 optional LMB equipment row의 visibility/can-use/action/failure, activation mode와 progress를 추가한다. `EPlayerInteractionIntent::EquipmentUse`와 `EPhysicalCarryKind::MonkeyWrench`는 기존 reflected ordinal을 보존하도록 각 enum 끝에 추가한다.

## Held Equipment Use Contract

`IHeldEquipmentUsable`은 concrete type을 Character/Interaction에 결합하지 않고 다음을 제공한다.

- `QueryEquipmentUse(Context)`: 실행 가능, action name, failure, mode를 반환하고 상태를 변경하지 않음
- `BeginEquipmentUse(Context)`: 입력 시작 owner를 commit
- `UpdateEquipmentUse(Context, DeltaTime)`: Hold use와 progress 갱신
- `EndEquipmentUse(Context)`: 정상 release
- `CancelEquipmentUse(Context)`: drop, target/tool invalidation, focus mode/EndPlay cleanup

context는 user/carry, camera origin/forward과 현재 focus hit를 제공한다. held equipment가 실제 domain owner API를 호출하며 `UPlayerEquipmentUseComponent`는 concrete wrench/mop을 cast하지 않는다.

Equipment row 합성은 현재 held Actor가 `IHeldEquipmentUsable`이면 해당 query를 authoritative하게 사용한다. 사용 가능한 held equipment가 없을 때만 focus target이 `물걸레가 필요합니다` 같은 disabled equipment action/failure를 광고할 수 있다. 두 source를 두 LMB row로 동시 표시하지 않으며 focus target은 target name/hit context를 제공한다.

## `UPlayerInteractionComponent`

- local player camera 기준 configurable distance/channel line trace를 수행한다.
- actor 또는 hit component에서 `IPlayerInteractable`을 찾는다.
- target 또는 query 결과가 바뀔 때만 `OnInteractionQueryChanged`를 방송한다.
- `TryInteract()`에서 대상을 다시 trace/query한 뒤 execute한다.
- 기존 `TryInteract()`는 instant primary 호환 wrapper로 유지한다.
- E Started/Completed/Canceled를 primary begin/end로 받고 active hold 동안 같은 target, focus, carry와 query 조건을 매 Tick 재검증한다.
- F Started는 secondary query/execute를 호출하고 target에 secondary가 없으면 mutation하지 않는다.
- G Started는 view intent를 carry component에 전달하고 반환 결과를 동일 attempt notification으로 방송한다.
- `TryInteract()`의 대상 없음, query 실행 불가, execute 성공·실패는 모두 `FPlayerInteractionResult` 하나를 반환하고 `OnInteractionAttemptFinishedNative`를 정확히 한 번 방송한다.
- execute 뒤에는 query를 먼저 refresh한 다음 attempt result를 방송하므로 UI는 최신 지속 상태 위에 일시 실행 피드백을 표시할 수 있다.
- key, mop, basket, towel, stain, customer, cash 같은 구체 domain type을 직접 판별하지 않는다.
- focus target의 world query와 held Actor의 equipment query를 합성해 E/F/LMB row의 단일 `FPlayerInteractionQuery`를 방송한다.
- equipment-use attempt result를 `EquipmentUse` intent로 받아 기존 query/result delegate에 합성하되 domain mutation을 대행하지 않는다.
- active hold는 target/input/focus/carry/EndPlay invalidation에서 정확히 한 번 cancel한다.
- pawn 종료·교체 시 focus를 지우고 query/result delegate를 정리한다.
- `SetInteractionSuppressed(true)`는 active hold를 transient failure 없이 한 번 cancel하고 current target/query를 지우며 suppress 중 trace와 public attempt의 mutation을 막는다.
- `SetInteractionSuppressed(false)`는 즉시 query를 refresh한다. 같은 값의 반복 설정은 lifecycle을 중복 실행하지 않는다.
- suppression은 generic 외부 focus 계약이며 Computer concrete type이나 computer session 상태를 판별하지 않는다.

## `UPlayerCarryComponent`

- generic held `AActor` 하나와 호환 key getter/delegate를 authoritative하게 소유한다.
- 빈손 pickup만 허용하며 inventory array, hotbar, item swap을 만들지 않는다.
- E fixed-slot take/store와 G free drop을 concrete item cast 없이 public interface로 조율한다.
- active equipment use를 placement 전에 한 번 cancel하고 성공 후에만 held reference를 해제한다.
- attach, slot, collision, CCD 또는 physics 실패 시 snapshot 전체를 rollback한다.
- `HeldKeyAnchor`와 기존 key commit API를 rename/delete하지 않는다.
- detailed state, release physics, key extension과 recovery는 [PhysicalCarrySystem.md](PhysicalCarrySystem.md)를 따른다.

Cash는 carry 대상이 아니며 Economy System의 즉시 획득 interaction으로 처리한다.

## Generic Carryable Contract

`IPhysicalCarryable`은 concrete domain type을 Interaction에 결합하지 않는 native 계약이다. 기본 `FreeDrop|FixedSlot` capability, exact-slot binding, actual-held-pose release, free-world CCD, key state 확장과 reflected compatibility는 [PhysicalCarrySystem.md](PhysicalCarrySystem.md)를 따른다.

`GetHeldTransform()`의 기본은 Identity이며 location/rotation만 사용하고 scale은 `(1,1,1)`로 유지한다. 새 공통 carry Actor 또는 `UPhysicalCarryableComponent`를 만들지 않는다.

`UHeldEquipmentMotionComponent`는 carryable 공통화가 아닌 표현 도우미다. current relative transform을 baseline으로 저장하고 one-shot/hold-loop curve offset을 적용하며 end/cancel/drop에서 baseline을 복구한다. carry reference, pickup/drop transaction와 domain progress를 소유하지 않는다.

## Key And Fixed-Slot Target

`ABathhouseKeyActor`의 number/customer/counter transaction과 `ABathhouseKeyHookActor`의 topology validation은 유지한다. target은 `DroppedInWorld`을 enum 끝에 추가하고 key hook도 fixed-slot interface를 구현한다. generic equipment slot과 key physics root를 포함한 상세 계약은 [PhysicalCarrySystem.md](PhysicalCarrySystem.md)를 따른다.

## Character Integration

`AFirstPersonCharacter`는 composition root로 다음을 추가한다.

- `UPlayerInteractionComponent`
- `UPlayerCarryComponent`
- `UPlayerEquipmentUseComponent`
- first-person camera 하위 `HeldKeyAnchor`
- existing instant primary 호환을 유지하면서 E Started/Completed/Canceled, F Started, G Started와 LMB Started/Triggered/Completed/Canceled를 Interaction/Equipment/Carry intent API에 전달한다.
- computer session이 input을 capture하면 해당 session이 E lifecycle을 소비하고 Interaction에는 전달하지 않는다.

Character는 focus 규칙과 key transaction을 직접 구현하지 않는다. PlayerController는 mapping context 등록·해제 책임을 유지한다.

## Blueprint/API Contracts

Blueprint 조회·표현 API:

- `UPlayerInteractionComponent::GetCurrentInteractionQuery`
- `UPlayerInteractionComponent::TryInteract`
- primary begin/end, secondary attempt와 equipment drop attempt API
- equipment-use begin/update/end/cancel API와 C++ result integration
- `UPlayerInteractionComponent::OnInteractionQueryChanged`는 Blueprint 표시 갱신 계약이다.
- `UPlayerInteractionComponent::OnInteractionAttemptFinishedNative`는 C++ 전용 실행 결과 계약이며 BlueprintAssignable로 노출하지 않는다.
- `UPlayerInteractionComponent::SetInteractionSuppressed`, `IsInteractionSuppressed`는 외부 focus owner가 사용하는 C++ 전용 계약이며 Blueprint에 노출하지 않는다.
- `UPlayerCarryComponent::IsHandEmpty`
- `UPlayerCarryComponent::GetHeldKey`
- generic held object와 held kind 조회, `OnHeldObjectChanged`
- exact fixed-slot take/store와 actual-held-pose free-drop result
- combined equipment-use query/result의 optional LMB action/failure/mode/progress
- `ABathhouseKeyActor::OnKeyStateChanged`
- `ABathhouseKeyActor::OnHeldPresentationChanged`
- `APhysicalCarryFixedSlotActor::OnSlotOccupancyChanged`

Editor authoring 값:

- `AFirstPersonCharacter::InteractAction`
- `AFirstPersonCharacter::SecondaryInteractAction`
- `AFirstPersonCharacter::DropCarryAction`
- `AFirstPersonCharacter::PrimaryUseAction`
- trace 거리와 collision channel
- `HeldKeyAnchor` transform
- key, wet mop, towel basket, monkey wrench Blueprint class default의 개별 `HeldTransform`
- item별 약한 forward/upward velocity change, 기본 `120/15 cm/s`
- deprecated `ThrowSpawnDistance`, `DropSweepChannel`, `DropSweepClearance`는 호환용으로만 보존
- equipment fixed slot의 exact `AssignedItem`, `bStartOccupied`, `ItemAnchor`
- key `KeyPhysicsRoot` bounds와 collision
- key actor mesh/number presentation
- key hook의 번호와 key actor 연결

## Dependencies

- Interaction -> Engine actor/component/collision
- Interaction -> Facility registry의 번호 검증 API
- Cleaning -> Interaction public query/equipment-use/motion/carry 계약
- Combat -> Interaction public carry/equipment-use/motion 계약
- Towel -> Interaction public intent/carry 계약
- Character -> Interaction
- Computer -> Interaction public query/carry/suppression 계약
- UI -> Interaction
- Interaction은 Computer concrete class에 의존하지 않는다.
- Interaction은 Combat/Cleaning/Customer/UI concrete class에 의존하지 않는다.

## Manual Review Points

- 어떤 경로에서도 player가 두 key를 동시에 들지 않는지 확인한다.
- 어떤 경로에서도 key/mop/basket/monkey wrench를 둘 이상 동시에 들지 않는지 확인한다.
- E hold cancel과 F/G attempt가 기존 primary result를 중복 방송하지 않는지 확인한다.
- key의 기존 state transition과 GetHeldKey/OnHeldKeyChanged 계약이 generic carry 확장 뒤에도 유지되는지 확인한다.
- query가 상태를 바꾸지 않고 execute가 조건을 재검증하는지 확인한다.
- 같은 query에서 UI delegate가 매 Tick 반복되지 않는지 확인한다.
- 한 번의 `TryInteract()`에서 result delegate가 중복 방송되지 않고 반환값과 동일한 성공·실패 이유를 전달하는지 확인한다.
- dropped key가 복제·소실되거나 허용되지 않은 번호 hook에 반환되지 않는지 확인한다.
- player/customer 비정상 종료 시 key가 원래 hook으로 복구되는지 확인한다.
- key number가 HUD text가 아니라 first-person 3D key에 표시되는지 확인한다.
- 네 carryable의 slot/store/free-drop이 같은 carry component commit 경로를 사용하고 concrete actor가 transaction을 복제하지 않는지 확인한다.
- Identity `HeldTransform`이 기존 anchor 부착을 보존하고, 네 아이템의 서로 다른 location/rotation이 player-held 상태에만 적용되는지 확인한다.
- held transform이 hook/counter/world drop transform과 physical bounds scale을 오염시키지 않는지 확인한다.
- suppression 시작이 hold를 한 번만 cancel하고 query/prompt를 지우며, 해제 직후 최신 target을 다시 조회하는지 확인한다.
- LMB use press owner가 Computer/Equipment 사이에서 섞이지 않고 active use가 release/cancel/drop/EndPlay에 한 번만 종료되는지 확인한다.
- equipment query/result가 E/F row를 덮어쓰지 않고 empty hand/invalid target의 정확한 실패 이유를 제공하는지 확인한다.
- suppress 중 E/F/G 직접 호출도 domain mutation이나 stale attempt feedback을 만들지 않는지 확인한다.
- free drop이 camera target으로 teleport하지 않고 actual held pose에서 시작하는지 확인한다.
- held pose world overlap 실패가 attachment, carrier와 presentation을 보존하는지 확인한다.
- free-world item이 Pawn을 영구 무시하고 질량과 무관한 약한 velocity change 및 CCD를 받는지 확인한다.
