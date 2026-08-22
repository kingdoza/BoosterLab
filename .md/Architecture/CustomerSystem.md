# Customer System

## Implementation Status

이 문서는 목욕탕 손님의 입장부터 퇴장까지 현재 구현된 C++ gameplay loop와 UE 5.8 StateTree 실행 계약을 정의한다. Bath collision-independent snap은 구현되었다. health 0 래그돌, soft interruption과 native Task restart는 [CustomerRecoverySystem.md](CustomerRecoverySystem.md)의 구현 target이다.

## Source Scope

```text
Source/BathhouseSim/Public/Customer/
  BathhouseCustomerTypes.h
  CustomerRoutineDefinition.h
  CustomerSessionComponent.h
  CustomerMontagePlaybackComponent.h
  BathhouseCustomerCharacter.h
  BathhouseCustomerAIController.h
  BathhouseCustomerSpawner.h
  StateTree/CustomerStateTreeTasks.h
  StateTree/CustomerStateTreeConditions.h
  StateTree/CustomerTowelStateTreeTasks.h

Source/BathhouseSim/Private/Customer/
  BathhouseCustomerTypes.cpp
  CustomerRoutineDefinition.cpp
  CustomerSessionComponent.cpp
  CustomerMontagePlaybackComponent.cpp
  BathhouseCustomerCharacter.cpp
  BathhouseCustomerAIController.cpp
  BathhouseCustomerSpawner.cpp
  StateTree/CustomerStateTreeTasks.cpp
  StateTree/CustomerStateTreeConditions.cpp
  StateTree/CustomerTowelStateTreeTasks.cpp

Source/BathhouseSim/Private/Tests/
  BathhouseDomainTests.cpp  # Bath snap cleanup, montage candidate와 playback token coverage
  CleaningTowelAutomationTests.cpp  # towel acquire/shortage/return/interruption coverage
```

## Responsibilities

- customer별 key/session, queue, facility reservation과 bath stay 상태
- StateTree 기반 routine orchestration과 gameplay event 전달
- check-in 60초 timeout과 미응대 퇴장
- 번호 시설, shower, random bath loop, checkout과 정상 퇴장
- 완료·timeout·기술 실패의 대칭 cleanup
- clean towel token 획득·사용·반납과 shortage fallback
- customer session satisfaction와 towel cleanup
- Combat health component 조립과 Customer Recovery soft-interruption 통합

Customer는 towel endpoint count/overflow, facility slot, key actor lifecycle, player carry, wallet과 UI 상태를 소유하지 않는다.

## State And Execution Owners

| 책임 | Owner |
|---|---|
| 현재 routine state와 transition | `ST_CustomerRoutine` StateTree |
| key number, key reference, timer와 runtime handles | `UCustomerSessionComponent` |
| navigation request | 현재 `FStateTreeMoveToTask`, target `FCustomerRestartableMoveToTask`와 AIController |
| queue | `ABathhouseCounterActor` |
| facility reservation/occupancy | `UBathhouseFacilitySlotComponent` |
| Bath approach/action snap 상태와 복구 | `UCustomerSessionComponent` |
| montage 재생, playback token과 종료 결과 | `UCustomerMontagePlaybackComponent` |
| key state | `ABathhouseKeyActor` |
| money | `UPlayerWalletComponent` |
| timed activity 실행 | native Customer StateTree Task |
| customer-held towel token과 satisfaction | `UCustomerSessionComponent` |
| towel count, overflow와 transfer | Towel System |
| health/depleted | Combat `UHealthComponent` |
| ragdoll/soft interruption/restart serial | Customer Recovery components |

## `UCustomerRoutineDefinition`

공용 Data Asset으로 NPC 행동 시간과 random 범위를 소유한다.

- `CheckInTimeoutSeconds = 60`
- `BathStayDurationSeconds = 60`
- `BathDwellMinSeconds = 10`
- `BathDwellMaxSeconds = 20`
- store shoes, undress, pre-shower, main shower 시간
- drying, towel return, dress, wear shoes 시간
- facility retry 간격과 navigation 최대 재시도
- `UsageFee = 10000`
- towel availability wait limit
- towel unavailable satisfaction penalty

값은 Editor에서 조정하지만 runtime 도중 시설이나 Widget이 변경하지 않는다. 향후 성격·만족도 같은 인과요인은 별도 modifier로 추가하며 현재는 고정 bath stay 값을 그대로 사용한다.

## `UCustomerSessionComponent`

- assigned `ABathhouseKeyActor`와 `KeyNumber`
- current counter lane/queue handle
- current facility slot reservation
- current Bath action-point snap 여부, 예약 당시 발바닥 기준 approach/action transform과 collision-independent snap/return
- last used bath actor
- bath stay end time와 expiry timer
- current logical activity와 service interaction gate
- cash claimed, departure reason와 cleanup guard
- optional `FTowelUseHandle`, towel-use stage와 towel cleanup guard
- current satisfaction value
- check-in wait와 checkout offer의 non-interruptible queue-service guard

Session은 domain runtime state owner이며 StateTree transition graph를 복제하지 않는다. StateTree Task가 session API를 통해 transaction을 수행한다.

Bath timer가 만료되면 session은 `Customer.Event.BathStayExpired` event를 StateTree에 전달한다.

## `ABathhouseCustomerCharacter`

- customer Pawn의 composition root다.
- private `CustomerSession` default subobject로 `UCustomerSessionComponent`를 생성하고 외부 C++에는 `GetCustomerSession()` 접근만 제공한다.
- private `CustomerMontagePlayback` default subobject로 montage lifecycle을 조립하며 gameplay session state와 분리한다.
- target `Health`, `CustomerKnockdown`, `CustomerRoutineInterruption` private default subobject를 조립하고 각 책임을 getter로만 노출한다.
- `CustomerSession`은 `VisibleAnywhere`, `BlueprintReadOnly`, `AllowPrivateAccess` 계약으로 Blueprint와 StateTree의 읽기 binding을 유지한다.
- check-in 중 `IPlayerInteractable`을 구현하고 session에 query/execute를 위임한다.
- logical activity 변경을 Blueprint 표현 event로 전달한다.

Blueprint event:

- `OnActivityStarted(ActivityType)`
- `OnActivityFinished(ActivityType)`
- `OnCustomerPresentationStateChanged(PresentationState)`
- `OnCustomerSatisfactionChanged(PreviousSatisfaction, NewSatisfaction)`

`UCustomerMontagePlaybackComponent`는 AnimInstance, 현재 montage, monotonic playback token과 종료 결과를 소유한다. StateTree Task는 token으로 자신이 시작한 montage만 조회·중단한다. AnimNotify, Motion Warping, prop animation과 신발·의상 전환은 이번 target에 포함하지 않는다.

## `ABathhouseCustomerAIController`

- UE 5.8 `UStateTreeAIComponent`를 brain component로 사용한다.
- `UStateTreeAIComponentSchema` 기반 `ST_CustomerRoutine` asset을 실행한다.
- navigation과 StateTree lifecycle만 담당한다.
- key, facility, timer와 cash 상태를 소유하지 않는다.

필요 module/plugin:

- `StateTree`
- `GameplayStateTree`
- `StateTreeModule`
- `GameplayStateTreeModule`
- `AIModule`, `GameplayTasks`, `NavigationSystem`, `GameplayTags`

## StateTree Boundary

StateTree asset이 소유하는 것:

- state hierarchy와 transition
- native Task/Condition 배치와 binding
- key received, timeout, facility available, bath expired, cash claimed event 전이
- towel available과 towel wait expired event 전이
- Bath approach 이동, action snap, montage 실행과 approach 복귀 순서

Native C++이 소유하는 것:

- Task/Condition 구현
- session transaction과 cleanup
- cached Bath 발바닥 transform에 scaled capsule half height를 한 번 더하는 actor/capsule-center 변환과 unswept teleport
- queue/facility/key/wallet API 호출
- gameplay event 발행
- montage 후보 검증, 단일 선택과 실제 playback 종료 판정
- target soft interruption serial, restartable MoveTo와 기존 Task local restart

Blueprint StateTree Task와 Blueprint graph에 domain mutation을 구현하지 않는다.

## Native StateTree Tasks

- queue join/leave와 front 도착 대기
- check-in key 대기와 timeout 시작·취소
- facility/slot 선택·예약·release
- 현재 built-in `FStateTreeMoveToTask`에 목적지 binding 제공, target은 동일 binding의 native restartable MoveTo로 교체
- Bath action/approach point의 collision-independent snap과 movement mode 복구
- logical activity begin, finish와 timer-only fallback
- 후보 중 하나를 한 번 선택해 실제 종료를 기다리는 one-shot montage
- 후보 중 하나를 한 번 선택해 같은 montage만 지정 시간 동안 반복하는 duration-loop montage
- bath stay timer 시작과 random bath loop
- checkout key 배치와 cash actor 생성·claim 대기
- normal/timeout/technical cleanup
- clean towel acquire/wait/fallback, mark-used와 used return Task

조건은 session과 owner API를 읽기만 하고 상태를 바꾸지 않는다.

Queue removal은 session membership와 wait/service guard를 먼저 지운 뒤 counter에서 dequeue한다. Counter의 동기 lane broadcast는 남은 customer에게 계속 전달하지만, 이미 떠나는 customer와 active check-in wait/checkout offer에는 `QueueChanged` StateTree event를 보내지 않는다. Check-in wait 시작은 idempotent하여 StateTree reselect가 기존 timeout을 다시 시작하지 않는다. Knockdown soft pause는 StateTree reselect/exit가 아니며 queue, facility와 checkout cleanup을 호출하지 않는다.

## Full Routine Flow

1. Spawner가 entry에 customer를 생성하고 routine definition/counter를 주입한다.
2. Customer가 check-in lane에 enqueue하고 할당 queue point로 이동한다.
3. Front service point 도착 후 `WaitingForKey`가 되고 60초 timeout을 시작한다.
4. Player가 유효한 번호 key를 주면 key/session을 commit하고 timeout을 취소한다.
5. Timeout이면 check-in lane을 떠나 exit로 이동한 뒤 소멸한다.
6. 성공하면 같은 번호 shoe locker slot에서 store-shoes timed activity를 수행한다.
7. 같은 번호 clothes locker slot에서 undress timed activity를 수행한다.
8. `TowelShelf` slot에서 clean towel 한 장 획득을 시도한다.
9. 없으면 authorable limit 동안 availability를 기다리고 만료 시 towel 없이 진행하며 satisfaction을 감소시킨다.
10. Shower slot에서 pre-shower timed activity를 수행한다.
11. Pre-shower 완료 순간 고정 60초 bath stay timer를 시작한다.
12. Available bath 중 random slot을 예약하고 NavMesh 위 approach point까지 이동한다.
13. 이동을 정지하고 발바닥 action point를 capsule-center actor transform으로 변환한 뒤 blocking collision 사전 검사 없이 unswept snap하여 입욕을 시작한다.
14. 탕마다 `10~20초` random dwell 동안 EnterState에서 선택한 montage 하나만 반복하고 시간이 남으면 다른 available bath를 선택한다.
15. 다른 bath가 있으면 직전 bath를 제외하며 모든 bath가 점유 중이면 reservation 없이 availability event를 기다린다.
16. dwell 완료 또는 60초 만료 시 montage를 중단하고 approach point로 복귀한 뒤 slot을 release한다.
17. Shower slot에서 main-shower activity를 수행한다.
18. towel handle이 있으면 Drying에서 Used로 mark하고 기존 `TowelBasket` facility의 used bin에 반환한다.
19. used bin full이면 bin 주변 valid floor의 individual used towel로 commit하고, 즉시 spawn 불가면 PendingSpill ledger로 이전한다.
20. towel handle이 없으면 towel-dependent drying/return을 건너뛴다.
21. clothes locker와 shoe locker에서 dress/wear-shoes activity를 수행한다.
22. Checkout lane에 enqueue하고 key 배치, cash claim을 처리한다.
23. Cash claim 성공 즉시 checkout lane을 떠나 exit로 이동하고 소멸한다.

Player의 key pickup/rack 반환은 customer 퇴장 조건이 아니다.

## Check-In Transaction

- check-in lane front customer만 key query를 제공한다.
- player가 `HeldByPlayer` key를 들고 있고 key hook/두 numbered facility가 valid해야 한다.
- 성공 시 key actor를 `AssignedToCustomer`로 전이하고 player hand를 비운 뒤 session에 저장한다.
- key 번호는 player가 선택한 번호이며 customer가 미리 배정받지 않는다.
- key receive와 timeout이 같은 frame에 경쟁하면 game thread에서 먼저 commit한 terminal event만 유효하다.

## Customer Towel Transaction

- clean towel 획득은 stack count 감소와 session `FTowelUseHandle` 생성이 한 transaction이다.
- handle은 token owner, original stack, used 여부와 terminal cleanup guard를 가진다.
- clean shortage wait 만료는 gameplay fallback이며 technical abort가 아니다.
- fallback은 towel-dependent 상태를 건너뛰고 authorable satisfaction penalty를 한 번 적용한다.
- used bin capacity는 clean acquire를 막지 않는다. full return은 individual floor overflow 또는 PendingSpill로 보존한다.
- session interruption 전 사용하지 않은 token은 original stack, 사용한 token은 bin/overflow/recovery ledger로 한 번만 이전한다.

## Activity And Montage Contract

animation을 사용하는 논리 행동은 다음 순서를 사용한다.

1. slot reserve
2. navigation target으로 이동하고 Bath면 발바닥 action point에 capsule 높이를 한 번 적용해 collision-independent unswept snap
3. `BeginActivity`와 `OnActivityStarted`
4. StateTree montage Task가 유효 후보를 필터링하고 EnterState에서 정확히 하나 선택
5. one-shot은 실제 montage 정상 종료, duration-loop는 같은 선택 montage의 지정 시간 반복을 완료 기준으로 사용
6. logical completion commit과 `OnActivityFinished`
7. Bath면 발바닥 approach point에 같은 capsule 높이 변환을 적용해 복귀
8. slot release

후보가 하나면 random 호출 없이 그 montage를 사용하고 후보가 없거나 재생할 수 없으면 Task가 실패한다. Loop Task는 정상 실행중 후보를 다시 선택하지 않는다. 정상 StateTree exit은 token owner의 montage를 blend-out하고 session cleanup을 실행한다. Knockdown soft interruption은 cleanup 없이 playback을 중지하고 기립 후 후보를 다시 선택해 local action을 처음부터 재시작한다. animation이 없는 상태는 기존 timer-only activity를 사용할 수 있다.

## Bath Snap Collision Policy

Bath ActionPoint와 ApproachPoint는 reservation-time cached 발바닥 transform을 사용하고 scaled capsule half height를 정확히 한 번 적용한다. ActionPoint snap은 `IsActionTransformClear` 또는 capsule overlap 사전 검사를 수행하지 않는다.

`SnapToCurrentFacilityActionPoint()`는 유효한 reservation/slot, cached transform, Character, capsule과 movement component만 검증한다. AI와 movement를 정지하고 movement mode를 저장한 뒤 `SetActorLocationAndRotation`의 `bSweep=false`, `ETeleportType::TeleportPhysics`로 정확한 transform을 적용한다. Blocking Volume, facility mesh 또는 다른 collision과 겹쳐도 그 사실만으로 snap을 실패시키거나 navigation failure를 증가시키지 않는다.

Snap 중 capsule/Actor collision enabled 상태와 response는 변경하지 않는다. 요구사항은 placement 검사 무시이며 고객을 facility 사용 전체 동안 ghost actor로 바꾸지 않는다. movement는 기존처럼 `MOVE_None`으로 유지하므로 사용 중 CharacterMovement가 위치를 수정하지 않는다.

ActionPoint에서 나올 때도 cached ApproachPoint로 unswept teleport하고 저장했던 movement mode를 복원한다. invalid owner/cache/component 또는 transform 적용 자체 실패만 technical failure다. release, normal StateTree exit, technical abort와 EndPlay cleanup은 같은 return/restore 경로를 사용한다. Knockdown은 이 cleanup 경로를 사용하지 않고 reservation을 유지한 채 ragdoll 최종 위치에서 ApproachPoint로 다시 이동한다.

## Technical Abort

Navigation이 설정된 횟수만큼 반복 실패하면 gameplay 분기가 아니라 technical abort로 처리한다.

- active timer와 StateTree wait 취소
- Bath action point에 있으면 collision 사전 검사 없이 cached approach point 복귀와 movement mode 복구
- current slot release
- queue entry 제거
- assigned key를 원래 hook으로 복구
- towel handle을 used stage에 따라 clean stack 또는 used bin/overflow/recovery ledger로 정리
- cash actor가 있으면 제거하되 이미 지급된 money는 되돌리지 않음
- 오류 기록 후 exit 이동 시도와 소멸

Check-in 외 gameplay timeout은 두지 않는다.

## Spawner

`ABathhouseCustomerSpawner`는 customer class, routine definition, counter, entry transform, spawn interval과 max active count를 authoring한다. Customer 완료 delegate로 active count를 정리하며 개별 routine phase를 직접 제어하지 않는다.

## Dependencies

- Customer -> Facility
- Customer -> Interaction
- Customer -> Economy
- Customer -> Towel
- Customer -> Combat health/damage public 계약
- Customer -> UE 5.8 GameplayStateTree/AI/Navigation
- Customer montage playback -> Engine Animation/AnimInstance
- Customer는 UI concrete class에 의존하지 않는다.

## Manual Review Points

- check-in timeout이 front 도착 후 시작되고 key 수령 시 취소되는지 확인한다.
- player가 준 key 번호와 두 numbered facility가 전체 routine에서 일치하는지 확인한다.
- bath timer가 pre-shower 완료 시 시작하고 정확히 60초에 current montage를 중단한 뒤 approach 복귀와 release를 수행하는지 확인한다.
- blocking collision이 action point를 점유해도 snap이 성공하고 정확한 cached transform, `MOVE_None`과 기존 collision enabled 상태를 유지하는지 확인한다.
- blocked action snap 후 정상 release/technical abort가 cached approach로 복귀하고 movement mode를 복원하는지 확인한다.
- bath random dwell과 다른 bath 선택이 고정 체류시간 종료를 지연하지 않는지 확인한다.
- 모든 StateTree exit/abort에서 queue, slot, timer와 key가 정리되는지 확인한다.
- key 배치와 cash claim 뒤 NPC가 key 회수를 기다리지 않고 퇴장하는지 확인한다.
- montage 후보가 0/1/여러 개인 경우 각각 failure/단일 선택/random 단일 선택으로 동작하는지 확인한다.
- duration-loop가 처음 선택한 montage만 반복하고 StateTree exit에서 다른 playback을 중단하지 않는지 확인한다.
- montage가 없는 timer-only 상태의 기존 logical loop가 유지되는지 확인한다.
- 신발·의상 mesh, visibility, AnimNotify와 appearance state가 이번 범위에 추가되지 않았는지 확인한다.
- `CustomerSession`이 외부 C++에서 직접 접근되지 않고 Blueprint/StateTree 읽기 binding과 public getter가 유지되는지 확인한다.
- clean towel shortage가 authorable wait 뒤 routine을 계속하고 satisfaction penalty를 한 번만 적용하는지 확인한다.
- used bin full이 customer를 막지 않고 individual overflow/PendingSpill로 token을 보존하는지 확인한다.
- customer StateTree exit/EndPlay에서 towel token owner가 중복되거나 사라지지 않는지 확인한다.
- knockdown soft pause가 StateTree `ExitState()`를 발생시키지 않고 session timer/자원/예약을 보존하는지 [CustomerRecoverySystem.md](CustomerRecoverySystem.md)의 수용 기준으로 확인한다.
