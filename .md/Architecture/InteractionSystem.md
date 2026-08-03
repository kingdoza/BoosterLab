# Interaction System

## Implementation Status

이 문서는 현재 구현된 player interaction과 단일 physical key 소지 경계를 정의한다.

## Source Scope

```text
Source/BathhouseSim/Public/Interaction/
  InteractionTypes.h
  PlayerInteractable.h
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
```

## Responsibilities

- first-person camera 중앙 line trace와 primary interaction 실행
- side-effect 없는 상호작용 조회와 실행 직전 재검증
- query 상태와 별개인 interaction 실행 결과의 일회성 native notification
- inventory/hotbar 없는 0개 또는 1개의 key 소지
- 번호 key actor의 유일한 lifecycle과 player held presentation
- focus와 held key 변화의 UI용 delegate

Interaction은 customer routine, facility slot, queue와 player money를 소유하지 않는다.

## Interaction Contract

`IPlayerInteractable`은 다음 계약을 제공한다.

- `QueryInteraction(Context)`: 표시 여부, 실행 가능 여부, 대상명, 행동명과 실패 이유를 반환하며 상태를 바꾸지 않는다.
- `ExecuteInteraction(Context)`: 실행 직전에 조건을 다시 검증하고 성공/실패 결과를 반환한다.
- 한 대상은 현재 하나의 primary interaction만 제공한다.

`FPlayerInteractionContext`는 interactor, `UPlayerCarryComponent`, hit actor/component와 hit 정보를 가진다. `FPlayerInteractionQuery`와 결과 문구는 localization 가능한 `FText`를 사용한다.

## `UPlayerInteractionComponent`

- local player camera 기준 configurable distance/channel line trace를 수행한다.
- actor 또는 hit component에서 `IPlayerInteractable`을 찾는다.
- target 또는 query 결과가 바뀔 때만 `OnInteractionQueryChanged`를 방송한다.
- `TryInteract()`에서 대상을 다시 trace/query한 뒤 execute한다.
- `TryInteract()`의 대상 없음, query 실행 불가, execute 성공·실패는 모두 `FPlayerInteractionResult` 하나를 반환하고 `OnInteractionAttemptFinishedNative`를 정확히 한 번 방송한다.
- execute 뒤에는 query를 먼저 refresh한 다음 attempt result를 방송하므로 UI는 최신 지속 상태 위에 일시 실행 피드백을 표시할 수 있다.
- key, customer, cash 같은 구체 domain type을 직접 판별하지 않는다.
- pawn 종료·교체 시 focus를 지우고 query/result delegate를 정리한다.

## `UPlayerCarryComponent`

- `ABathhouseKeyActor` 하나만 authoritative held reference로 소유한다.
- `IsHandEmpty`, `GetHeldKey`, `TryTakeKey`, `TryReleaseKey`를 제공한다.
- inventory array, stack, slot, selected hotbar와 item swap을 만들지 않는다.
- 빈손일 때만 key를 받을 수 있다.
- key는 대응하는 `ABathhouseKeyHookActor` 또는 승인된 customer/counter transaction으로만 release한다.
- 바닥 drop과 임시 선반 보관은 현재 범위 밖이다.
- held key를 camera 하위 `HeldKeyAnchor`에 부착하고 world collision을 끈다.

Cash는 carry 대상이 아니며 Economy System의 즉시 획득 interaction으로 처리한다.

## `ABathhouseKeyActor`

- 변경되지 않는 `KeyNumber`와 원래 `KeyHook`을 가진다.
- 같은 key number의 유일한 물리 token이다.
- mesh/collision과 first-person 표시를 겸하지만 key state는 actor 하나에만 존재한다.
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
- `InteractAction`을 `Started`에 바인딩하고 `TryInteract()`로 전달

Character는 focus 규칙과 key transaction을 직접 구현하지 않는다. PlayerController는 mapping context 등록·해제 책임을 유지한다.

## Blueprint/API Contracts

Blueprint 조회·표현 API:

- `UPlayerInteractionComponent::GetCurrentInteractionQuery`
- `UPlayerInteractionComponent::TryInteract`
- `UPlayerInteractionComponent::OnInteractionQueryChanged`는 Blueprint 표시 갱신 계약이다.
- `UPlayerInteractionComponent::OnInteractionAttemptFinishedNative`는 C++ 전용 실행 결과 계약이며 BlueprintAssignable로 노출하지 않는다.
- `UPlayerCarryComponent::IsHandEmpty`
- `UPlayerCarryComponent::GetHeldKey`
- `ABathhouseKeyActor::OnKeyStateChanged`
- `ABathhouseKeyActor::OnHeldPresentationChanged`

Editor authoring 값:

- `AFirstPersonCharacter::InteractAction`
- trace 거리와 collision channel
- `HeldKeyAnchor` transform
- key actor mesh/number presentation
- key hook의 번호와 key actor 연결

## Dependencies

- Interaction -> Engine actor/component/collision
- Interaction -> Facility registry의 번호 검증 API
- Character -> Interaction
- UI -> Interaction
- Interaction은 Customer와 UI concrete class에 의존하지 않는다.

## Manual Review Points

- 어떤 경로에서도 player가 두 key를 동시에 들지 않는지 확인한다.
- query가 상태를 바꾸지 않고 execute가 조건을 재검증하는지 확인한다.
- 같은 query에서 UI delegate가 매 Tick 반복되지 않는지 확인한다.
- 한 번의 `TryInteract()`에서 result delegate가 중복 방송되지 않고 반환값과 동일한 성공·실패 이유를 전달하는지 확인한다.
- key가 복제·소실되거나 허용되지 않은 번호 hook에 반환되지 않는지 확인한다.
- player/customer 비정상 종료 시 key가 원래 hook으로 복구되는지 확인한다.
- key number가 HUD text가 아니라 first-person 3D key에 표시되는지 확인한다.
