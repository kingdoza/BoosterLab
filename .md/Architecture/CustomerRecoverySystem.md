# Customer Recovery System

## Implementation Status

이 문서는 customer health depleted 후 래그돌, 설정 시간 뒤 제자리 즉시 기립과 C++ 루틴 실행 계층의 soft interruption/restart를 정의한다. queue-pose recovery gate와 native StateTree queue Task Source까지 구현되었고 asset 교체는 후속 Editor 단계다.

Soft interruption은 StateTree 종료, technical abort 또는 customer death가 아니다. [CustomerSystem.md](CustomerSystem.md)의 정상·timeout·technical cleanup과 구분한다.

## Source Scope

```text
Source/BathhouseSim/Public/Customer/
  CustomerKnockdownComponent.h
  CustomerRoutineInterruptionComponent.h
  CustomerQueueNavigationComponent.h
  StateTree/CustomerStateTreeTasks.h        # restartable MoveTo/Task 계약 확장
  StateTree/CustomerQueueStateTreeTasks.h

Source/BathhouseSim/Private/Customer/
  CustomerKnockdownComponent.cpp
  CustomerRoutineInterruptionComponent.cpp
  CustomerQueueNavigationComponent.cpp
  StateTree/CustomerStateTreeTasks.cpp
  StateTree/CustomerQueueStateTreeTasks.cpp
```

`UCustomerSessionComponent`, `UCustomerMontagePlaybackComponent`, `ABathhouseCustomerCharacter`와 focused automation test를 함께 확장한다.

## Responsibilities

- health depleted를 customer knockdown으로 단 한 번 전환
- root physics body의 피격 방향 impulse와 래그돌 lifecycle
- 래그돌 최종 위치에서 기상 montage 없이 즉시 기립
- StateTree `ExitState()` cleanup을 실행하지 않는 soft pause/restart
- session 타이머 잔여 시간, key/towel/cash/queue/facility reservation 보존
- 미완료 국소 행동의 시작 지점 재실행
- queue member의 기립 후 최신 service/queue transform 복귀와 Yaw 정렬
- 이미 commit된 원자적 transaction의 중복 방지

Customer Recovery는 피해 검출, player 공격 판정, facility slot authoritative state와 StateTree transition graph를 소유하지 않는다.

## State And Execution Owners

| 책임 | Owner |
|---|---|
| health/depleted | Combat `UHealthComponent` |
| ragdoll/recovery timer·physics snapshot | `UCustomerKnockdownComponent` |
| soft pause, restart serial·active operation | `UCustomerRoutineInterruptionComponent` |
| session timer 잔여·domain resource | `UCustomerSessionComponent` |
| routine state/transition | `ST_CustomerRoutine` |
| restartable navigation request | native `FCustomerRestartableMoveToTask`과 AIController |
| queue navigation·recovery pose | `UCustomerQueueNavigationComponent`와 Counter assignment |
| local activity/montage restart | 기존 native Customer StateTree Task |
| facility reservation/occupancy | `UBathhouseFacilitySlotComponent` |

## Customer Composition

`ABathhouseCustomerCharacter`는 다음 private default subobject를 조립한다.

- 기존 `CustomerSession`
- 기존 `CustomerMontagePlayback`
- `CustomerQueueNavigation`
- `Health`
- `CustomerKnockdown`
- `CustomerRoutineInterruption`

신규 component는 `VisibleAnywhere`, `BlueprintReadOnly`, `AllowPrivateAccess="true"`를 사용하고 외부 C++은 getter로만 접근한다. Customer Actor는 composition과 Blueprint 표현 event만 담당하고 physics, timer와 StateTree restart 상태를 직접 소유하지 않는다.

## Knockdown Contract

health가 0이 되면 `UCustomerKnockdownComponent`가 다음을 수행한다.

1. active knockdown guard commit
2. Routine Interruption에 soft pause 요청
3. AI movement, customer-owned montage와 CharacterMovement 중단
4. capsule/mesh collision, movement mode와 mesh relative transform snapshot
5. Skeletal Mesh ragdoll collision/physics 전환
6. configured root bone physics body에 damage direction + vertical impulse를 질량과 무관한 velocity change로 적용
7. authorable knockdown duration timer 시작

기립 전 추가 damage와 impulse는 무시하고 recovery timer를 다시 시작하지 않는다.

`RootBoneName`은 Skeletal Mesh와 PhysicsAsset에 실제 physics body가 있어야 한다. data validation과 runtime에서 없으면 명시적 오류로 처리하고 pelvis 등으로 silent fallback하지 않는다.

## Immediate Recovery

Knockdown duration 만료 시:

1. root body의 최종 world transform snapshot
2. physics simulation 종료
3. root의 최종 XY를 유지하고 아래 floor trace로 capsule Z만 보정
4. actor/capsule, mesh relative transform과 collision snapshot 복구
5. configured recovery health ratio로 health 회복
6. CharacterMovement를 복구하고 routine restart 준비
7. queue member면 최신 assignment를 resolve해 queue-pose recovery gate 실행

별도 get-up montage와 Motion Warping은 추가하지 않는다. 일반 routine은 ragdoll 최종 XY에서 재개하지만, active queue member의 visible assignment는 예외로 현재 service/queue point까지 이동하고 authored Yaw로 정렬한 뒤 routine을 재개한다. floor trace가 실패하면 파괴된 physics 값을 사용하지 않도록 최종 유효 actor/root transform을 안전 값으로 사용한다.

## Soft Interruption Boundary

Knockdown은 StateTree에 stop/restart를 요청하지 않는다. UE 5.8 `UStateTreeComponent::PauseLogic()`은 `bIsPaused`를 설정하고 Tick을 비활성하며 execution context의 `Stop()`/`ExitState()`를 호출하지 않는다. recovery에서 `ResumeLogic()`으로 기존 instance data의 scheduled tick을 재개한다. `StopLogic()`/`RestartLogic()`은 destructive Task exit을 유발하므로 soft interruption에서 금지한다.

중단 시 보존:

- assigned key/number
- towel use handle/stage
- cash offer/physical key return commit/cash claimed state
- queue membership와 current lane
- current facility reservation
- satisfaction, completed transaction guard
- StateTree active hierarchy

`EndSoftInterruption()`은 queue recovery gate를 먼저 검사한다. queue member가 아니거나 checkout `OverflowWander` assignment면 즉시 resume한다. visible service/queue assignment면 StateTree와 routine timer를 paused 상태로 유지하고 shared queue navigation component가 이동·회전을 완료한 뒤 한 번만 timer와 brain을 resume한다. queue revision이 바뀌면 과거 point가 아니라 최신 assignment로 active request를 교체한다.

정상 StateTree exit, technical abort와 EndPlay는 기존 cleanup을 실행하며 soft interruption guard로 이를 막지 않는다.

## Timer Policy

`UCustomerSessionComponent::PauseRoutineTimers`/재개 경계가 다음 TimerManager handle의 잔여 시간을 저장한다.

- check-in wait
- total bath stay
- clean towel availability wait

기립 후 남은 시간으로 한 번만 재설정한다. knockdown recovery timer는 routine timer가 아니므로 계속 진행한다. Task instance의 local timed activity/montage duration은 보존하지 않고 재시작 대상이다.

## Restart Serial And Native Tasks

`UCustomerRoutineInterruptionComponent`는 monotonic `InterruptionSerial`과 현재 restartable operation의 weak/tokenized registration을 소유한다. 이는 전체 routine phase를 복제하는 C++ state machine이 아니라 active Task의 transient 실행 checkpoint다.

기존 native Task는 EnterState에서 serial을 snapshot하고 resume 후 변경을 감지하면:

- timer-only activity: authored local duration으로 초기화
- montage once: 유효 후보를 다시 선택하고 처음부터 재생
- montage loop: 후보 하나를 다시 선택하고 local duration 초기화
- wait: session timer의 복구된 잔여 시간 관찰
- checkout: 기존 key/cash actor를 재사용하고 새 offer를 중복 생성하지 않음

Bath local dwell은 새로 시작하지만 total bath stay 잔여 시간을 넘지 않는다. total timer가 먼저 만료하면 현재 montage/activity를 종료하고 기존 main-shower transition을 사용한다.

## Restartable Navigation

내장 `FStateTreeMoveToTask`는 interruption serial을 인식하지 못하므로 native `FCustomerRestartableMoveToTask`로 교체한다.

- 기존 destination, facing, acceptance/routing binding을 유지
- knockdown중 request/tick을 중지
- recovery 후 현재 destination을 다시 resolve하고 새 path request 시작
- old request completion/callback이 새 request를 성공/실패로 변경하지 못하도록 request token guard
- replacement에 의해 token이 superseded된 Task는 자신의 request와 local token을 정리하고 `Failed`로 기존 retry/cleanup 계약에 이관하며 무기한 `Running`하지 않음
- navigation 실패는 기존 retry/technical-abort 계약으로 전달

`ST_CustomerRoutine`에는 행동별 checkpoint state나 recovery transition을 추가하지 않는다. 일반 목적지는 기존 MoveTo를 동일 binding의 restartable native Task로 교체하고, check-in/checkout은 queue 전용 native Task로 교체한다.

## Queue Pose Recovery

`UCustomerQueueNavigationComponent`는 정상 queue Task와 paused-StateTree recovery gate가 공유하는 async 실행 owner다. Counter의 FIFO/index를 복제하지 않고 매 resolve마다 session의 lane과 owner actor로 최신 assignment를 얻는다.

- 정상 실행: queue point 이동·Yaw 정렬·revision 대기, checkout overflow wander와 promotion을 처리하고 service point 정렬 후 성공한다.
- knockdown 시작: active AI request와 turn을 중단하되 queue membership은 유지한다.
- 기립 후 visible assignment: latest point로 이동·회전한 뒤 interruption component에 gate 완료를 알린다.
- 기립 후 overflow: 고정 복귀점이 없으므로 brain을 resume하고 queue Task가 새 wander destination을 선택한다.
- invalid counter/assignment 또는 반복 navigation failure: 오래된 request를 성공 처리하지 않고 기존 technical-abort 경계로 전달한다.

queue movement의 request token, delegate, Tick과 임시 CharacterMovement 회전 flag는 이 Component가 대칭 정리한다. `UCustomerSessionComponent`와 이미 비대한 공용 StateTree Task 파일에 async lifecycle을 추가하지 않는다.

## Facility Activity Recovery

시설 행동중 knockdown되면:

1. current slot에 `EndUse(customer)`를 호출해 `Occupied -> Reserved`로 전환
2. reservation owner, cached approach/action transform은 유지
3. ragdoll 위치에서 기립한 후 current ApproachPoint로 navigation
4. Bath는 기존 collision-independent ActionPoint snap, 일반 시설은 action facing/사용 계약 복구
5. `BeginUse(customer)`로 다시 Occupied
6. current logical activity를 완료로 commit하지 않고 local action/montage만 재시작

시설이 중단중 파괴되어 reservation이 유효하지 않으면 존재하지 않는 slot을 복제하지 않고 기존 retry 또는 technical abort로 이관한다.

## Transaction Boundary

다음 원자적 commit이 soft interruption보다 먼저 완료됐으면 rollback/replay하지 않는다.

- check-in key transfer
- clean towel token acquire
- mark towel used/used towel return
- checkout assigned key의 physical `OnCounter` commit
- cash offer creation/cash claim
- activity completion에 결합된 domain side effect

같은 game-thread frame에서 먼저 commit한 terminal guard가 승리한다. 미완료 행동만 처음부터 다시 실행한다.

## Blueprint/API Contracts

Editor authoring:

- `KnockdownDurationSeconds`
- `RecoveryHealthRatio` 기본 `1.0`
- `RootBoneName`
- ragdoll mesh collision profile
- recovery floor trace channel/distance

Blueprint presentation event:

- `OnCustomerKnockdownStarted`
- `OnCustomerRecovered`

Blueprint는 health, recovery timer, physics state, session timer, reservation과 restart serial을 변경하지 않는다. 기상 montage는 이번 범위에 없다.

## Dependencies

- Customer Recovery -> Combat health/damage context
- Customer Recovery -> Customer Session/Montage/StateTree
- Customer Recovery -> Facility slot public reservation API
- Customer Recovery -> AI/Navigation/CharacterMovement/PhysicsAsset
- Combat은 Customer Recovery에 의존하지 않는다.

## Manual Review Points

- nonlethal damage는 StateTree를 중단하지 않고 health 0만 knockdown을 한 번 시작하는지 확인한다.
- root physics body에 카메라 방향 impulse가 질량과 무관한 velocity change로 적용되고 body가 없으면 validation이 실패하는지 확인한다.
- knockdown중 추가 damage/impulse와 timer reset이 없는지 확인한다.
- final ragdoll XY에서 즉시 기립하고 configured ratio로 health를 회복하는지 확인한다.
- check-in/bath/towel wait는 남은 시간을 재개하고 local activity/montage는 처음부터 재시작하는지 확인한다.
- queue/facility/key/towel/cash와 completed transaction이 손실·복제되지 않는지 확인한다.
- facility action은 예약을 유지한 채 ragdoll 위치에서 approach로 다시 이동하고 action을 재시작하는지 확인한다.
- soft pause가 Queue/Facility/Checkout Task의 destructive `ExitState()`를 호출하지 않는지 확인한다.
- actor/controller/facility EndPlay에서 soft interruption이 정상 cleanup을 막지 않는지 확인한다.
- service/queue point에서 쓰러진 customer가 기립 후 최신 point 위치·Yaw 복구를 완료하기 전에 StateTree/timer가 재개되지 않는지 확인한다.
- overflow customer는 기립 후 과거 wander point로 강제 복귀하지 않고 FIFO를 보존한 새 wander 실행을 재개하는지 확인한다.
