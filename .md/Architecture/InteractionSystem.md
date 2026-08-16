# Interaction System

## Implementation Status

이 문서는 현재 구현된 primary/secondary/hold interaction intent, key/wet mop/towel basket 단일 physical carry 계약과 공통 물리 드랍 트랜잭션을 정의한다. 아이템별 `HeldTransform`까지 Source와 automation에 구현되었으며 Computer focus 동안 world interaction을 멈추는 suppression 계약은 확정 구현 target이다.

## Source Scope

```text
Source/BathhouseSim/Public/Interaction/
  InteractionTypes.h
  PlayerInteractable.h
  PhysicalCarryable.h
  PlayerInteractionComponent.h
  PlayerCarryComponent.h
  BathhouseKeyActor.h
  BathhouseKeyHookActor.h

Source/BathhouseSim/Private/Interaction/
  PlayerInteractionComponent.cpp
  PlayerCarryComponent.cpp
  BathhouseKeyActor.cpp
  BathhouseKeyHookActor.cpp

Source/BathhouseSim/Private/Tests/
  BathhouseDomainTests.cpp  # single-key carry와 interaction attempt result coverage
  CleaningTowelAutomationTests.cpp  # mop/basket carry, hold cleaning과 key G rejection coverage
```

## Responsibilities

- first-person camera 중앙 line trace와 primary/secondary interaction 실행
- instant/hold primary lifecycle과 target/context 재검증
- side-effect 없는 상호작용 조회와 실행 직전 재검증
- query 상태와 별개인 interaction 실행 결과의 일회성 native notification
- inventory/hotbar 없는 0개 또는 1개의 physical key/mop/basket 소지
- 공용 held anchor와 아이템별 local held transform의 분리
- 번호 key actor의 유일한 lifecycle과 player held presentation
- focus와 held key 변화의 UI용 delegate
- 외부 focus mode가 활성화된 동안 active hold, trace와 prompt를 중단하는 C++ suppression 경계

Interaction은 cleaning progress, towel count/machine, customer routine, facility slot, queue와 player money를 소유하지 않는다.

## Interaction Contract

`IPlayerInteractable`은 다음 계약을 제공한다.

- `QueryInteraction(Context)`: 표시 여부, 실행 가능 여부, 대상명, 행동명과 실패 이유를 반환하며 상태를 바꾸지 않는다.
- `ExecuteInteraction(Context)`: 실행 직전에 조건을 다시 검증하고 성공/실패 결과를 반환한다.
- 기존 `bCanInteract`, `ActionName`, `FailureReason`은 primary 의미를 유지한다.
- target은 optional secondary 표시/가능 여부, action name과 failure reason을 추가로 반환할 수 있다.
- primary는 `Instant` 또는 `Hold` activation mode를 선언할 수 있다. 기존 target의 default는 `Instant`다.
- secondary는 이번 target에서 Started 한 번의 instant 실행만 사용한다.
- 기존 `ExecuteInteraction(Context)`는 primary API로 유지하고 optional secondary execute와 hold begin/update/cancel 계약을 추가한다.

`FPlayerInteractionContext`는 interactor, `UPlayerCarryComponent`, hit actor/component와 hit 정보를 가진다. `FPlayerInteractionQuery`와 결과 문구는 localization 가능한 `FText`를 사용한다.

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
- active hold는 target/input/focus/carry/EndPlay invalidation에서 정확히 한 번 cancel한다.
- pawn 종료·교체 시 focus를 지우고 query/result delegate를 정리한다.
- `SetInteractionSuppressed(true)`는 active hold를 transient failure 없이 한 번 cancel하고 current target/query를 지우며 suppress 중 trace와 public attempt의 mutation을 막는다.
- `SetInteractionSuppressed(false)`는 즉시 query를 refresh한다. 같은 값의 반복 설정은 lifecycle을 중복 실행하지 않는다.
- suppression은 generic 외부 focus 계약이며 Computer concrete type이나 computer session 상태를 판별하지 않는다.

## `UPlayerCarryComponent`

- generic `AActor` held reference 하나만 authoritative하게 소유한다.
- `IsHandEmpty`, `GetHeldKey`, 기존 key commit과 `OnHeldKeyChanged`를 호환 API로 유지한다.
- generic held object 조회/change delegate와 physical equipment take/release API를 추가한다.
- inventory array, stack, slot, selected hotbar와 item swap을 만들지 않는다.
- 빈손일 때만 key/mop/basket을 받을 수 있고 자동 swap하지 않는다.
- key는 대응하는 `ABathhouseKeyHookActor` 또는 승인된 customer/counter transaction으로만 release한다.
- key의 임의 바닥 drop과 모든 carryable의 임시 선반 보관은 현재 범위 밖이다. mop/basket의 G world drop은 아래 공통 계약으로 처리한다.
- key/mop/basket을 camera 하위 `HeldKeyAnchor`에 부착하고 held 중 world collision을 끈다.
- 기존 serialized `HeldKeyAnchor`는 모든 physical carryable의 공용 기준점으로 사용하며 rename하지 않는다.
- 아이템별 위치·회전 보정은 `IPhysicalCarryable::GetHeldTransform()`에서 조회하며 공용 anchor transform에 합쳐 저장하지 않는다.
- G release는 `IPhysicalCarryable`이 허용한 mop/basket만 처리하며 key는 held 상태를 유지하고 실패한다.
- free drop 전에 carryable root primitive의 월드 AABB로 held 위치부터 기존 목표 위치까지 box sweep을 수행한다.
- 기본 sweep channel은 `ECC_Visibility`이고 carry owner와 대상 actor를 무시한다. actor 원점과 bounds 중심 차이는 `BoundsOffset`으로 보정한다.
- blocking hit는 shape 중심인 `Hit.Location`과 clearance로 player 쪽 안전 위치를 계산한다. start penetration에서 투척 목표 쪽으로 진행하는 MTD는 벽 통과 후보이므로 거부하고, 나머지 후보만 segment 제한 후 동일 shape/channel overlap으로 재검증한다.
- 안전 위치 계산이 실패하면 attachment, `HeldObject`, `Carrier`, collision/physics와 presentation을 바꾸지 않는다.
- 안전 위치가 정해진 뒤 detach, 위치 적용, collision/physics 활성화, 장비별 impulse와 held reference 해제를 한 commit 경로에서 수행한다.
- 위치 적용 또는 physics 활성화 실패 시 이전 transform, collision과 held anchor 부착을 복원한다.
- carrier EndPlay에서 key는 기존 hook recovery, equipment는 last safe/initial transform recovery를 사용한다.

Cash는 carry 대상이 아니며 Economy System의 즉시 획득 interaction으로 처리한다.

## Generic Carryable Contract

`IPhysicalCarryable`은 concrete domain type을 Interaction에 결합하지 않고 다음을 제공한다.

- display name과 pickup query
- 공용 held anchor 기준 아이템별 local `HeldTransform`
- held presentation begin/end
- free world drop 허용 여부
- root physical primitive, 장비별 drop distance/impulse와 safe recovery
- 공통 물리 commit 이후 `Carrier`, last safe transform과 held presentation만 정리하는 완료 알림
- carried actor EndPlay cleanup

free drop을 허용하는 구현의 physical primitive는 actor root여야 한다. Detach, actor 위치 변경, physics 활성화와 impulse 적용은 `UPlayerCarryComponent`만 수행하며 concrete carryable은 이 동작을 반복하지 않는다.

Key는 기존 state transaction을 계속 정본으로 사용하며 G free drop을 거부한다. Wet mop과 towel basket은 world pickup과 G drop을 허용한다.

`GetHeldTransform()`의 기본 반환값은 Identity다. `ABathhouseKeyActor`, `AWetMopActor`, `ATowelBasketActor`가 각각 `EditDefaultsOnly`, `BlueprintReadOnly` `HeldTransform`을 소유하고 player-held 부착 경로에서만 사용한다. key hook, counter return slot과 customer assignment에는 적용하지 않는다.

이번 계약에서 `HeldTransform`의 authoritative 값은 location과 rotation이며 scale은 `(1,1,1)`로 유지한다. 물건의 월드 물리 크기와 drop bounds를 held presentation 때문에 바꾸지 않는다. 새 공통 carry Actor 또는 `UPhysicalCarryableComponent`는 만들지 않으며 기존 Actor가 interface 계약을 직접 구현한다.

## `ABathhouseKeyActor`

- 변경되지 않는 `KeyNumber`와 원래 `KeyHook`을 가진다.
- 같은 key number의 유일한 물리 token이다.
- mesh/collision과 first-person 표시를 겸하지만 key state는 actor 하나에만 존재한다.
- player-held 상태에서만 자기 `HeldTransform`을 공용 anchor 기준으로 적용한다.
- Blueprint presentation은 key number를 3D actor에 표시할 수 있다.

`EBathhouseKeyState`:

- `AtHook`
- `HeldByPlayer`
- `AssignedToCustomer`
- `OnCounter`
- `Recovering`

허용 transition:

| 현재 | 행위 | 다음 |
|---|---|---|
| `AtHook` | 빈손 player가 key hook에서 획득 | `HeldByPlayer` |
| `HeldByPlayer` | check-in front customer에게 전달 | `AssignedToCustomer` |
| `HeldByPlayer` | 원래 key hook에 반환 | `AtHook` |
| `AssignedToCustomer` | checkout counter 반환 slot에 배치 | `OnCounter` |
| `OnCounter` | 빈손 player가 획득 | `HeldByPlayer` |
| 유효한 비정상 상태 | technical cleanup | `Recovering` 후 `AtHook` |

transition은 현재 state, expected holder와 target identity를 검증하고 실패 시 양쪽 상태를 유지한다.

Technical recovery는 transition 전 실제 `StateOwner`를 snapshot하고 optional expected-owner guard를 검증한다. 실제 owner가 player carry이면 해당 carry의 동일 key reference를 먼저 대칭 정리하며, counter slot 해제와 hook 재부착을 한 idempotent recovery 경로에서 수행한다. Key actor 자체의 EndPlay는 파괴 중인 actor를 재부착하지 않고 carry/counter의 외부 참조만 정리한다.

## `ABathhouseKeyHookActor`

- 하나의 `KeyNumber`와 해당 `ABathhouseKeyActor`를 연결한다.
- 같은 번호의 shoe locker와 clothes locker가 정확히 하나씩 있을 때만 enabled다.
- 빈손 player에게 `키 가져가기` interaction을 제공한다.
- 대응 key를 든 player에게만 `키 반환하기` interaction을 제공한다.
- 시설 누락·중복 또는 key가 hook에 없으면 이유를 반환하고 상태를 바꾸지 않는다.

## Character Integration

`AFirstPersonCharacter`는 composition root로 다음을 추가한다.

- `UPlayerInteractionComponent`
- `UPlayerCarryComponent`
- first-person camera 하위 `HeldKeyAnchor`
- existing instant primary 호환을 유지하면서 E Started/Completed/Canceled, F Started와 G Started를 Interaction/Carry intent API에 전달한다.
- computer session이 input을 capture하면 해당 session이 E lifecycle을 소비하고 Interaction에는 전달하지 않는다.

Character는 focus 규칙과 key transaction을 직접 구현하지 않는다. PlayerController는 mapping context 등록·해제 책임을 유지한다.

## Blueprint/API Contracts

Blueprint 조회·표현 API:

- `UPlayerInteractionComponent::GetCurrentInteractionQuery`
- `UPlayerInteractionComponent::TryInteract`
- primary begin/end, secondary attempt와 equipment drop attempt API
- `UPlayerInteractionComponent::OnInteractionQueryChanged`는 Blueprint 표시 갱신 계약이다.
- `UPlayerInteractionComponent::OnInteractionAttemptFinishedNative`는 C++ 전용 실행 결과 계약이며 BlueprintAssignable로 노출하지 않는다.
- `UPlayerInteractionComponent::SetInteractionSuppressed`, `IsInteractionSuppressed`는 외부 focus owner가 사용하는 C++ 전용 계약이며 Blueprint에 노출하지 않는다.
- `UPlayerCarryComponent::IsHandEmpty`
- `UPlayerCarryComponent::GetHeldKey`
- generic held object와 held kind 조회, `OnHeldObjectChanged`
- `ABathhouseKeyActor::OnKeyStateChanged`
- `ABathhouseKeyActor::OnHeldPresentationChanged`

Editor authoring 값:

- `AFirstPersonCharacter::InteractAction`
- `AFirstPersonCharacter::SecondaryInteractAction`
- `AFirstPersonCharacter::DropCarryAction`
- trace 거리와 collision channel
- `HeldKeyAnchor` transform
- key, wet mop, towel basket Blueprint class default의 개별 `HeldTransform`
- `UPlayerCarryComponent`의 `DropSweepChannel`, `DropSweepClearance`
- wet mop/towel basket의 기존 `ThrowSpawnDistance`, `ThrowImpulseStrength`
- key actor mesh/number presentation
- key hook의 번호와 key actor 연결

## Dependencies

- Interaction -> Engine actor/component/collision
- Interaction -> Facility registry의 번호 검증 API
- Cleaning -> Interaction public hold/carry 계약
- Towel -> Interaction public intent/carry 계약
- Character -> Interaction
- Computer -> Interaction public query/carry/suppression 계약
- UI -> Interaction
- Interaction은 Computer concrete class에 의존하지 않는다.
- Interaction은 Customer와 UI concrete class에 의존하지 않는다.

## Manual Review Points

- 어떤 경로에서도 player가 두 key를 동시에 들지 않는지 확인한다.
- 어떤 경로에서도 key/mop/basket을 둘 이상 동시에 들지 않는지 확인한다.
- E hold cancel과 F/G attempt가 기존 primary result를 중복 방송하지 않는지 확인한다.
- key의 기존 state transition과 GetHeldKey/OnHeldKeyChanged 계약이 generic carry 확장 뒤에도 유지되는지 확인한다.
- query가 상태를 바꾸지 않고 execute가 조건을 재검증하는지 확인한다.
- 같은 query에서 UI delegate가 매 Tick 반복되지 않는지 확인한다.
- 한 번의 `TryInteract()`에서 result delegate가 중복 방송되지 않고 반환값과 동일한 성공·실패 이유를 전달하는지 확인한다.
- key가 복제·소실되거나 허용되지 않은 번호 hook에 반환되지 않는지 확인한다.
- player/customer 비정상 종료 시 key가 원래 hook으로 복구되는지 확인한다.
- key number가 HUD text가 아니라 first-person 3D key에 표시되는지 확인한다.
- mop/basket drop이 같은 carry component commit 경로를 사용하고 concrete actor가 직접 위치·physics·impulse를 적용하지 않는지 확인한다.
- Identity `HeldTransform`이 기존 anchor 부착을 보존하고, 세 아이템의 서로 다른 location/rotation이 player-held 상태에만 적용되는지 확인한다.
- held transform이 hook/counter/world drop transform과 physical bounds scale을 오염시키지 않는지 확인한다.
- suppression 시작이 hold를 한 번만 cancel하고 query/prompt를 지우며, 해제 직후 최신 target을 다시 조회하는지 확인한다.
- suppress 중 E/F/G 직접 호출도 domain mutation이나 stale attempt feedback을 만들지 않는지 확인한다.
- 벽, 비스듬한 면과 모서리에서 swept AABB가 벽 내부나 너머에 배치되지 않는지 확인한다.
- start penetration 또는 안전 공간 없음 실패가 held attachment, carrier와 presentation을 보존하고 빈 방향 재시도를 허용하는지 확인한다.
