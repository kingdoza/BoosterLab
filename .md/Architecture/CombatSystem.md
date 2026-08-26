# Combat System

## Implementation Status

이 문서는 단일 physical carry 계약을 사용하는 몽키스패너, 범용 LMB 장비 사용, 카메라 기준 근접 피격과 공용 체력의 현재 native 구현을 정의한다. wrench exact fixed slot, held-position free drop과 placement 전 active attack cancel도 [PhysicalCarrySystem.md](PhysicalCarrySystem.md)에 따라 구현되어 있다.

고객 전용 래그돌, 기립과 루틴 재시작은 [CustomerRecoverySystem.md](CustomerRecoverySystem.md)가 소유한다.

## Source Scope

```text
Source/BathhouseSim/Public/Combat/
  CombatTypes.h
  HealthComponent.h
  MeleeAttackComponent.h
  MonkeyWrenchActor.h

Source/BathhouseSim/Private/Combat/
  HealthComponent.cpp
  MeleeAttackComponent.cpp
  MonkeyWrenchActor.cpp
```

범용 장비 사용 interface, player use routing과 held transform 표현은 `Interaction/` Source에 둔다.

## Responsibilities

- 몽키스패너의 world/fixed-slot pickup, single carry와 G free drop
- LMB Started당 한 번의 공격 lifecycle
- 무기 World Mesh와 분리된 카메라 기준 multi shape trace
- 공격 1회 당 Actor 단위 중복 제거와 범위 내 모든 대상에 피해 전달
- 공용 체력, 피해, 회복과 depleted event
- 피격 방향·impulse를 보존하는 피해 context

Combat은 player input mapping, carry slot, 고객 StateTree, 래그돌 recovery, stain progress와 UI 상태를 소유하지 않는다.

## State And Execution Owners

| 책임 | Owner |
|---|---|
| LMB 입력 owner 선택 | Character System |
| 현재 소지 Actor | `UPlayerCarryComponent` |
| 장비 사용 lifecycle routing | `UPlayerEquipmentUseComponent` |
| held Actor transform 표현 | `UHeldEquipmentMotionComponent` |
| 몽키스패너 공격 상태·판정 | `UMeleeAttackComponent` |
| 체력·depleted terminal event | `UHealthComponent` |
| 고객 래그돌·루틴 중단 | Customer Recovery System |

## Generic Equipment Use Boundary

Interaction System의 `IHeldEquipmentUsable`은 concrete 무기와 청소 도구를 입력 계층에 결합하지 않고 다음을 제공한다.

- side-effect 없는 use query
- Begin/Update/End/Cancel lifecycle
- Started 단발 또는 Hold 사용 mode
- user, carry, camera origin/forward와 현재 focus hit를 포함한 context
- use result와 정확한 실패 이유

`UPlayerEquipmentUseComponent`는 현재 held Actor의 interface를 호출하지만 damage, cleaning progress와 domain transaction을 직접 변경하지 않는다. Computer focus중에는 동일 LMB를 Computer가 소유하며 equipment use는 시작하지 않는다.

## Held Equipment Motion

`UHeldEquipmentMotionComponent`는 carry 상태가 아닌 표현 lifecycle만 소유한다.

- `OneShot`: 몽키스패너 swing
- `LoopWhileInputHeld`: 물걸레 mopping
- use 시작 시 held Actor의 현재 relative transform을 baseline으로 snapshot
- curve 위치·회전 offset을 baseline에만 합성
- 정상 종료, cancel, drop, focus 전환과 EndPlay에서 baseline을 정확히 복구

`IPhysicalCarryable::GetHeldTransform()`의 authoring 값을 변경하거나 공용 anchor에 motion offset을 누적하지 않는다.

## Monkey Wrench

`AMonkeyWrenchActor`는 공통 carry Actor 상속 계층 없이 `IPlayerInteractable`, `IPhysicalCarryable`, `IHeldEquipmentUsable`을 직접 구현한다.

- 빈손 player가 E로 pickup
- key, mop, basket 또는 다른 wrench를 들고 있으면 실패
- held중 world collision/physics 비활성과 개별 `HeldTransform` 적용
- exact assigned slot에 E로 take/store
- G로 actual held pose에서 질량 무시 약한 velocity change를 적용하는 free-drop transaction 사용
- falling out of world와 carrier EndPlay에서 fixed-slot 우선 recovery 사용

`EPhysicalCarryKind::MonkeyWrench`는 기존 ordinal을 보존하도록 enum 끝에 추가한다.

## Melee Attack Contract

LMB `Started`가 공격을 한 번 시작한다. 공격중 추가 Started와 Hold는 무시하고 input buffer/auto repeat를 만들지 않는다.

1. one-shot held motion 시작
2. authorable `HitTimeSeconds` 도달
3. camera origin/forward를 다시 snapshot
4. `CameraOrigin + Forward * AttackDistance`를 중심으로 단일 sphere multi trace
5. owner/weapon을 제외하고 Actor identity로 중복 제거
6. 범위 내 모든 active `UHealthComponent`에 각각 한 번 피해
7. motion 종료 후 다음 공격 허용

무기 mesh, socket, collision overlap과 시각적 타격점은 authoritative 피격 판정이 아니다. player가 의도하지 않은 대상도 동일 범위에 있으면 피격된다.

## Damage And Health

`FCombatDamageContext`는 다음을 보존한다.

- instigator/causer
- damage amount
- camera-based origin, normalized direction
- 질량과 무관한 velocity-change impulse strength와 optional vertical impulse

`UHealthComponent`는 `MaxHealth`, `CurrentHealth`, active/depleted guard와 health/depleted delegate를 소유한다. 음수 피해, 중복 depleted broadcast와 max를 넘는 회복을 허용하지 않는다.

비치명 피해는 체력과 optional Blueprint 표현만 갱신하고 고객 루틴을 중단하지 않는다. 체력 0은 Combat actor 파괴나 death event가 아니라 Customer Recovery에 전달하는 depleted event다.

## Blueprint/API Contracts

Editor authoring:

- wrench mesh/collision, `HeldTransform`, fixed slot과 약한 forward/upward release velocity
- swing position/rotation curve, attack duration와 hit time
- attack distance/radius, trace channel, damage, impulse와 vertical impulse
- `UHealthComponent::MaxHealth`

Blueprint presentation:

- wrench held/drop presentation
- attack start/hit/end optional event
- health changed/nonlethal hit optional event

Blueprint는 attack timing, trace, 중복 제거, damage commit과 health/depleted state를 변경하지 않는다.

## Dependencies

- Combat -> Interaction/Physical Carry public carry/fixed-slot/equipment-use 계약
- Combat -> Engine collision, curve, ActorComponent
- Customer -> Combat health/damage public 계약
- Character -> Interaction input routing
- Combat은 Customer StateTree와 UI concrete class에 의존하지 않는다.

신규 runtime module dependency는 필요하지 않다.

## Failure And Cleanup

- 사용중 wrench slot store/drop/EndPlay: attack과 motion cancel, baseline 복구 후 atomic placement/cleanup
- invalid camera/context: attack 시작 전 실패, trace/damage 없음
- 중간에 target EndPlay: weak target를 commit 직전 재검증
- trace component 중복: Actor 단위 한 번만 damage
- attack중 computer focus: 빈손만 computer를 시작할 수 있으므로 normal flow에서 발생하지 않음
- EndPlay와 cancel은 delegate, timer/Tick, active use owner를 남기지 않음

## Manual Review Points

- single carry가 key/mop/basket/wrench 중 하나만 허용하는지 확인한다.
- LMB Started 한 번에 attack 하나만 시작하고 공격중 입력을 무시하는지 확인한다.
- 무기 mesh 위치와 관계없이 camera 기준 거리/반경의 모든 target이 한 번씩 맞는지 확인한다.
- nonlethal damage가 루틴을 중단하지 않고 depleted만 Customer Recovery를 시작하는지 확인한다.
- drop/cancel 후 held actor transform과 다음 use가 정상 복구되는지 확인한다.
- exact slot take/store가 attack을 시작하지 않고 wrong instance를 거부하는지 확인한다.
