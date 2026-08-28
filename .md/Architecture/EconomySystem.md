# Economy System

## Implementation Status

이 문서는 현재 구현된 player money와 checkout cash 획득 계약을 정의한다.

## Source Scope

```text
Source/BathhouseSim/Public/Economy/
  BathhousePlayerState.h
  PlayerWalletComponent.h
  BathhouseCashPaymentActor.h

Source/BathhouseSim/Private/Economy/
  BathhousePlayerState.cpp
  PlayerWalletComponent.cpp
  BathhouseCashPaymentActor.cpp

Source/BathhouseSim/Private/Tests/
  BathhouseEconomyTests.cpp
```

## Responsibilities

- player money의 authoritative runtime state
- checkout cash interaction의 일회성 금액 지급
- money 변경 delegate와 Blueprint 조회 API

Economy는 customer routine, key, counter queue와 money HUD를 소유하지 않는다.

## `ABathhousePlayerState`

- `UPlayerWalletComponent`의 composition root다.
- player money가 Pawn 교체와 분리되도록 Character가 아닌 PlayerState에 둔다.
- 저장·로드와 multiplayer replication은 현재 범위 밖이다.

GameMode/Blueprint는 local player에 `ABathhousePlayerState`가 사용되도록 연결해야 한다.

## `UPlayerWalletComponent`

- `int32 CurrentMoney`의 state owner다.
- 첫 구현의 기본값은 `0`이다.
- `TryAddMoney(Amount)`는 양수만 허용하고 overflow를 방지한다.
- `GetCurrentMoney`와 `OnMoneyChanged`를 Blueprint에 제공한다.
- 임의 Blueprint setter나 subtract API는 현재 추가하지 않는다.

## `ABathhouseCashPaymentActor`

- customer가 checkout에서 player에게 제시하는 cash 표현·interaction actor다.
- `PaymentAmount` 기본값은 `10000`원이다.
- cash는 player 손에 들어가지 않고 `UPlayerCarryComponent`와 무관하다.
- `IPlayerInteractable` query는 아직 claim되지 않았고 유효한 wallet이 있을 때 `현금 받기`를 제공한다.
- execute는 동일 actor에서 한 번만 `TryAddMoney`를 호출한다.
- wallet mutation보다 먼저 `bClaimed` one-shot guard를 commit하여 동기 `OnMoneyChanged` callback 재진입을 거부한다.
- wallet add 실패 시 guard를 rollback하고 actor와 money를 모두 유지한다.
- 성공 시에만 `OnCashClaimed`를 방송한 뒤 actor를 제거한다.

Customer는 concrete callback 없이 `OnCashClaimed` delegate를 구독해 checkout 완료와 퇴장을 진행한다.

## Checkout Cash Flow

1. Checkout front customer의 동일 assigned key가 Counter drop point 주변에서 physical `OnCounter` 상태로 commit된다.
2. Customer가 `ABathhouseCashPaymentActor`를 cash offer point에 생성·표시한다.
3. Player가 cash actor와 상호작용한다.
4. Cash actor가 `ABathhousePlayerState`의 wallet을 resolve한다.
5. Wallet에 `10000`원을 정확히 한 번 추가한다.
6. Cash actor가 claimed event를 방송하고 제거된다.
7. Customer가 checkout queue를 떠나 exit로 이동한다.

Player가 key actor를 집거나 hook에 반환하는 것은 customer 퇴장 조건이 아니다.

## Blueprint/API Contracts

Blueprint API/event:

- `UPlayerWalletComponent::GetCurrentMoney`
- `UPlayerWalletComponent::OnMoneyChanged`
- `ABathhouseCashPaymentActor::OnCashClaimed`
- `ABathhouseCashPaymentActor::OnCashAvailable`

Cash mesh, material과 제시 표현은 Blueprint 책임이지만 이번 단계에서는 실제 animation asset을 요구하지 않는다.

## Dependencies

- Economy -> Interaction interface
- Economy -> Engine PlayerState/ActorComponent
- Customer -> Economy cash actor
- UI는 필요하면 wallet delegate를 구독할 수 있으나 첫 구현에 money HUD는 포함하지 않는다.

## Manual Review Points

- cash actor를 반복 interact해도 money가 한 번만 증가하는지 확인한다.
- wallet resolve 또는 add 실패 시 cash actor가 사라지지 않는지 확인한다.
- Pawn 교체 후에도 PlayerState money가 유지되는지 확인한다.
- cash가 held key를 밀어내거나 carry 상태에 들어가지 않는지 확인한다.
- customer는 cash가 실제 claim되기 전에 checkout queue를 떠나지 않는지 확인한다.
