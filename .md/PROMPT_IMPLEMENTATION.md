# Implementation Prompt — Counter Queue Transform, Overflow Wander And Physical Key Return

## 목적

현재 player interaction, single physical carry, customer routine, facility, checkout cash와 recovery 계약을 보존하면서 다음 target을 native C++로 구현한다.

- check-in/checkout queue point의 Location/Yaw를 모두 적용하고, checkout visible capacity 초과 customer는 같은 FIFO 순번을 유지하며 전용 NavMesh volume을 배회한다.
- knockdown 기립 후 queue member는 최신 visible assignment로 복귀한 뒤 routine을 재개하며, checkout key는 단일 Counter drop point 주변에서 동일 key instance의 free-world physics를 활성화한다.

기준 문서:

- `.md/AGENT_WORKFLOW.md`
- `.md/AGENT_IMPLEMENTATION.md`
- `.md/0_ARCHITECTURE.md`
- `.md/Architecture/CoreSystem.md`
- `.md/Architecture/FacilitySystem.md`
- `.md/Architecture/CustomerSystem.md`
- `.md/Architecture/CustomerRecoverySystem.md`
- `.md/Architecture/PhysicalCarrySystem.md`

이 파일은 이전 physical-carry CCD 구현 프롬프트를 대체한다.

## 보존 및 금지 범위

- 구현 단계는 `Source/`와 native automation만 수정한다. `Content/`, `Config/`와 StateTree/Blueprint asset은 수정하지 않는다.
- check-in과 checkout은 독립 lane이며 front만 service interaction을 실행하는 기존 계약을 유지한다.
- checkout overflow를 별도 queue로 만들거나 FIFO entry를 dequeue/re-enqueue하지 않는다.
- Customer Session에 queue index, assignment 또는 wander position의 복제 상태를 추가하지 않는다.
- StateTree Blueprint graph, Blueprint Task, Tick 기반 Counter, 전체 Level NavMesh fallback을 추가하지 않는다.
- checkout key를 새로 spawn하거나 복제하지 않는다. check-in에서 받은 동일 `AssignedKey`를 사용한다.
- player G free drop, exact fixed slot, key number/hook, cash claim과 interaction prompt 계약을 변경하지 않는다.
- 공용 carry Actor/Component와 신규 runtime module dependency를 추가하지 않는다.

## 1. Facility Queue Assignment

`BathhouseFacilityTypes.h`에 reflected assignment type/struct를 추가한다.

- type: `Invalid`, `ServicePoint`, `QueuePoint`, `OverflowWander`
- data: full `FTransform`, logical index, queue point index, lane revision

`ABathhouseCounterActor`를 다음 규칙으로 변경한다.

- queue entry actor를 weak reference로 유지한다.
- lane mutation 전에 invalid actor entry를 compact한다.
- lane별 nonzero monotonic revision을 유지한다.
- index 0은 service point, index 1..N은 queue point 0..N-1이다.
- checkout index N+1 이상은 `OverflowWander`다.
- check-in 범위 초과는 `Invalid`이며 마지막 queue point clamp를 제거한다.
- `ResolveQueueAssignment(Lane, Actor, OutAssignment)`과 revision 조회 API를 제공한다.
- 기존 `OnQueueChanged`/native delegate는 유지하고 한 logical mutation에 한 번만 방송한다.
- `GetQueueTargetTransform`은 asset/source migration 기간의 deprecated visible-assignment wrapper로 보존한다.

queue point의 full transform을 보존하고 Customer가 Location/Yaw를 나눠 사용하게 한다. Pitch/Roll은 character facing에 적용하지 않는다.

## 2. Checkout Overflow Volume

`ACustomerQueueOverflowWanderVolume`을 Facility에 신규 작성한다.

- stable root와 authorable Box 범위
- NavMesh projection extent와 sample attempt count
- Box local random sample → NavMesh projection → volume containment/reachability 검증
- 유효 point를 찾지 못하면 false를 반환하고 Level 전체 NavMesh로 fallback하지 않음

Counter에 `EditInstanceOnly` overflow volume 배열을 추가하고 유효 reference만 사용한다. volume은 queue state나 AI request를 소유하지 않는다.

## 3. Queue Movement Settings

`UCustomerRoutineDefinition`에 다음 Editor authoring 값을 추가한다.

- queue acceptance radius, facing rotation speed degrees/second와 facing tolerance
- overflow wander acceptance radius와 pause min/max seconds

모든 값은 clamp/validation하고 min/max 역전은 명시적으로 정규화하거나 validation error로 처리한다. 구체적인 밸런스 값은 C++ 분기 안에 복제하지 않는다.

## 4. Customer Queue Navigation Component

신규 `UCustomerQueueNavigationComponent`를 Customer Character의 private default subobject로 조립한다. `VisibleAnywhere`, `BlueprintReadOnly`, `AllowPrivateAccess="true"`와 public C++ getter를 사용한다.

Component가 소유할 실행 책임:

- Counter queue-change delegate와 active `UAITask_MoveTo` lifecycle, request/result token과 stale callback 차단
- latest assignment resolve와 material target 변화 비교
- service/queue point MoveTo 후 yaw-only smooth turn
- queue point 도착 후 revision 대기
- checkout overflow destination 선택·MoveTo·pause 반복과 promotion 시 active wander 취소
- visible assignment 이동과 CharacterMovement 회전 flag snapshot/restore
- knockdown suspend와 paused-StateTree queue-pose recovery gate

정상 queue 실행은 service point의 위치·Yaw 정렬 후에만 완료된다. queue point 정렬은 완료가 아니라 대기 상태다. overflow sample 실패는 현재 위치에서 authorable retry를 기다리며 navigation failure count를 소비하지 않는다.

Exit/EndPlay/technical failure에서는 AI task, Tick/timer, delegate, token과 임시 movement flag를 대칭 정리한다. Counter FIFO/index는 Component에 저장하지 않는다.

## 5. Native StateTree Queue Task

이미 큰 `CustomerStateTreeTasks.h/.cpp`에 신규 queue lifecycle을 누적하지 않는다. 신규 파일을 사용한다.

- `Public/Customer/StateTree/CustomerQueueStateTreeTasks.h`
- `Private/Customer/StateTree/CustomerQueueStateTreeTasks.cpp`

`FCustomerMoveToCurrentQueueAssignmentTask`를 구현한다.

- Context/Parameter: Customer, Session, expected lane
- Enter/Tick: membership·lane·component 검증 후 시작하고 service 정렬 완료에만 `Succeeded`
- interruption중 `Running` 유지
- Exit: 자신의 execution token만 취소

기존 parent `FCustomerQueueTask`가 queue membership lifecycle을 계속 소유한다. 기존 `FCustomerQueueTargetTask` reflected type은 deprecated compatibility로 남겨 Content 교체 전에 asset load를 깨지 않는다.

## 6. Knockdown Queue-Pose Gate

`BeginSoftInterruption`에서 queue navigation의 active move/turn을 suspend하고, `UCustomerRoutineInterruptionComponent::EndSoftInterruption` 흐름을 다음처럼 확장한다.

- queue member가 아니면 기존처럼 timer/brain을 즉시 resume한다.
- current assignment가 `OverflowWander`면 즉시 resume하고 active queue Task가 새 wander point를 고른다.
- service/queue assignment면 routine timer와 StateTree를 paused 상태로 유지한 채 queue navigation recovery를 시작한다.
- recovery 이동·Yaw 완료 callback에서 timer와 brain을 정확히 한 번 resume한다.
- recovery중 revision 변경은 latest assignment로 request를 교체한다.
- invalid counter/assignment 또는 retry exhaustion은 stale request를 성공시키지 말고 기존 technical-abort 경계로 전달한다.

ragdoll 종료와 `OnCustomerRecovered` presentation event는 기존 의미를 유지한다. queue recovery는 get-up montage가 아니라 기립 후 보행·회전 gate다. queue membership, key/towel/cash, timers와 StateTree hierarchy를 cleanup하지 않는다.

## 7. Single Physical Checkout Key Drop

Counter에 stable `ReturnedKeyDropPoint` default subobject와 authorable local XY extent/attempt count를 추가한다. 후보 순서는 exact point가 첫 번째이고 이후만 local XY random offset이다.

기존 runtime returned-slot array, reservation/occupancy와 player pickup slot release는 canonical path에서 제거한다.

- `ReturnedKeyPointReferences`와 `OnReturnedKeySlotsChanged`는 deprecated reflected compatibility로 한 migration cycle 보존한다.
- deprecated property/event를 canonical runtime에서 읽거나 호출하지 않는다.
- 신규 Blueprint presentation event `OnReturnedKeyDropped(AActor* ReturnedKey)`를 성공 후 한 번 호출한다.

`ABathhouseKeyActor`의 checkout placement를 변경한다.

1. `AssignedToCustomer`와 expected customer identity 검증
2. key `KeyPhysicsRoot` bounds로 WorldStatic/WorldDynamic blocking overlap 검사
3. transaction snapshot 뒤 동일 Actor를 후보 transform으로 teleport
4. Counter forward × key forward velocity + world up × key upward velocity 계산
5. `FPhysicalCarryPlacementTransaction::ApplyFreeWorld`로 detach, Pawn Ignore, CCD, QueryAndPhysics와 simulate physics 적용
6. 성공 후에만 `AssignedToCustomer -> OnCounter`, Counter owner와 session guard commit
7. key visibility와 presentation event를 commit 뒤 공개

실패하면 key transform, hidden/collision/physics와 owner/state를 그대로 유지한다. `OnCounter` 재호출은 idempotent success이며 같은 key를 다시 투하하지 않는다. `TryTakeFromCounter`는 slot API 없이 key state와 Player Carry transaction만으로 회수한다.

`UCustomerSessionComponent::TryPlaceCheckoutKey`는 slot index를 저장하지 않고 key-owned transaction을 호출한다. key가 `OnCounter`가 된 뒤에만 기존 cash offer를 만든다. checkout offer retry와 cash claim/leave flow는 유지한다.

기계적 candidate/overlap/placement 코드는 private non-UObject helper로 분리해 이미 큰 Key Actor와 Session cpp의 성장을 제한한다. helper는 gameplay owner state를 저장하지 않는다.

## 8. Blueprint/API와 Migration

신규 reflected 계약:

- queue assignment enum/struct
- `ACustomerQueueOverflowWanderVolume`
- `UCustomerQueueNavigationComponent`
- `FCustomerMoveToCurrentQueueAssignmentTask`
- `ReturnedKeyDropPoint`, drop search settings와 `OnReturnedKeyDropped`
- `UCustomerRoutineDefinition` queue movement settings

기존 UCLASS/USTRUCT/UENUM 이름과 enum ordinal을 rename/delete하지 않는다. deprecated symbol을 보존하므로 Core Redirect는 추가하지 않는다. implementation은 Content를 resave하지 않는다.

## 9. Native Automation

기존 focused test를 교체·확장한다.

- service/queue assignment가 component의 full Location/Rotation과 revision을 반환하고 checkout `1 + N` 이후 entry는 clamp 없이 FIFO overflow가 된다.
- dequeue/invalid actor compact 후 가장 빠른 overflow entry가 정확한 queue point로 promotion된다.
- check-in 범위 초과가 방어적으로 invalid다.
- overflow point가 configured volume/NavMesh 밖으로 나오지 않고 sample failure가 queue를 변경하지 않는다.
- queue navigation이 move 완료 후 target Yaw tolerance까지 회전해야 service success한다.
- knockdown중 revision 변경 후 latest point 복귀·회전 전에는 routine resume가 발생하지 않는다.
- overflow recovery는 과거 wander point 복귀 없이 queue Task를 재개한다.
- checkout은 spawn 증가 없이 기존 assigned key 하나를 Pawn Ignore·CCD·질량 독립 velocity의 `OnCounter` physics로 전환한다.
- blocked exact/random 후보는 key/session/physics를 rollback하고 cash를 만들지 않는다.
- 여러 반환 key가 collision-free 후보를 사용하고 player pickup이 Counter slot mutation을 요구하지 않는다.
- 기존 check-in timeout, queue cleanup, cash claim, key hook/free drop/fixed slot과 recovery test가 회귀하지 않는다.

테스트 전용 public gameplay API나 Content fixture를 추가하지 않는다.

## 10. 검증과 후속 산출물

- UE 5.8 `Build.bat` Editor target을 `.md/AGENT_WORKFLOW.md`의 exact command로 첫 시도부터 승인 실행한다.
- 관련 focused automation과 가능한 전체 `BathhouseSim` suite를 실행한다.
- `.md/PROMPT_REVIEW.md`에 변경 파일, build/test 결과, FIFO·token·rollback·class growth 검토점을 기록한다.
- `.md/PROMPT_UNREAL.md`에는 다음 Editor 작업만 정확한 asset/instance 경로와 함께 요청한다.
  - Counter queue point Location/Yaw와 `ReturnedKeyDropPoint`/search 설정
  - checkout overflow volume 배치와 Counter reference
  - `ST_CustomerRoutine` check-in/checkout의 기존 Queue Target + MoveTo를 신규 queue Task로 교체·binding
  - Customer/Key/Counter Blueprint compile-save-reload와 PIE 검증
- StateTree 예상 Editor 작업량은 낮음이다. 기존 두 queue 이동 구간의 Task 교체와 context/lane binding이며 신규 state/transition은 없다.
- 자동화 불가능한 사용자 조작이 실제로 확인된 경우에만 `.md/USER_UNREAL.md`를 작성한다.

## 완료 조건

- 모든 visible check-in/checkout customer가 고유 point의 위치와 Yaw를 사용한다.
- checkout overflow가 FIFO를 유지하며 configured volume에서만 배회하고 빈자리 순서대로 promotion된다.
- queue member knockdown은 최신 visible assignment 복귀·회전 뒤에만 routine을 재개한다.
- checkout key가 동일 인스턴스·동일 번호/원래 hook identity로 collision-free physical `OnCounter` 상태가 된다.
- 실패·EndPlay·동시 revision/interaction에서 customer, queue, key와 cash가 소실·복제되지 않는다.
