# Implementation Prompt — Primary Equipment Use, Combat And Customer Recovery

## 목적

기존 E/F/G interaction, single physical carry, key/towel/customer/facility transaction, computer focus/pointer와 interaction prompt 계약을 보존하며 다음을 native C++로 구현한다.

- LMB를 범용 `PrimaryUseAction`으로 이관하고 Computer pointer 또는 held equipment use로 배타적 routing
- single carry를 사용하는 몽키스패너, one-shot held motion과 camera-based multi-target attack
- 물걸레 LMB Hold mopping state/motion과 정면 water stain에만 제거 progress
- 공용 health와 customer health 0의 root-body ragdoll, 설정 시간 뒤 제자리 즉시 기립
- UE 5.8 StateTree soft pause, session 보존과 미완료 국소 행동 C++ restart

기준 문서:

- `.md/AGENT_WORKFLOW.md`
- `.md/AGENT_IMPLEMENTATION.md`
- `.md/0_ARCHITECTURE.md`
- `.md/Architecture/CoreSystem.md`
- `.md/Architecture/CharacterSystem.md`
- `.md/Architecture/InteractionSystem.md`
- `.md/Architecture/UISystem.md`
- `.md/Architecture/CleaningSystem.md`
- `.md/Architecture/CombatSystem.md`
- `.md/Architecture/CustomerSystem.md`
- `.md/Architecture/CustomerRecoverySystem.md`
- `.md/Architecture/FacilitySystem.md`
- `.md/QNA_ARCHITECTURE.md` Q27~Q46

이 파일은 이전 Computer 구현 프롬프트를 전부 대체한다.

## 보존 및 금지 범위

- Source와 native automation만 수정하고 `Content/`, `Config/`, `ST_CustomerRoutine` asset을 수정/resave하지 않는다.
- `IPhysicalCarryable`을 유지하고 공통 carry Actor/상속 계층 또는 `UPhysicalCarryableComponent`를 만들지 않는다.
- E world primary, F secondary, G free equipment drop과 key free-drop 금지를 바꾸지 않는다.
- inventory, hotbar, weapon slot, auto-repeat/input buffer, weapon mesh collision damage를 추가하지 않는다.
- customer death/despawn/departure reason, get-up montage, player health/attack AI를 추가하지 않는다.
- knockdown soft interruption에 `StopLogic`, `RestartLogic`, StateTree reselect/exit와 technical abort를 사용하지 않는다.
- 기존 reflected type/property/event/component를 rename/delete하지 않고 enum 값은 끝에만 추가한다.
- 신규 runtime module dependency, replication, SaveGame을 추가하지 않는다.

## 1. Interaction And Character

대상/target:

- `Interaction/InteractionTypes.h`, `PhysicalCarryable.h`
- 신규 `HeldEquipmentUsable.h`
- 신규 `PlayerEquipmentUseComponent.h/.cpp`
- 신규 `HeldEquipmentMotionComponent.h/.cpp`
- `PlayerInteractionComponent.h/.cpp`, `PlayerCarryComponent.h/.cpp`
- `Character/FirstPersonCharacter.h/.cpp`

계약:

- `EPlayerInteractionIntent::EquipmentUse`, `EPhysicalCarryKind::MonkeyWrench`를 끝에 추가한다.
- equipment context/query/result와 `IHeldEquipmentUsable` Query/Begin/Update/End/Cancel를 native interface로 구현한다.
- `FPlayerInteractionQuery`에 optional LMB visibility/can-use/action/failure/mode/progress를 추가하고 `Equals`를 완전히 갱신한다.
- held usable query가 equipment row의 authoritative source다. 없을 때만 focus target이 disabled requirement를 표시하며 LMB row를 두 개 만들지 않는다.
- Equipment Use component는 camera/focus/carry context를 구성하고 interface만 호출하며 wrench/mop을 cast하거나 domain state를 변경하지 않는다.
- Motion component는 one-shot/hold-loop curve offset만 소유하고 begin baseline을 end/cancel/drop/EndPlay에 정확히 복구한다.
- Character에 `PlayerEquipmentUse` subobject/getter와 `PrimaryUseAction`을 추가한다.
- 기존 `ComputerClickAction`은 deprecated fallback으로 보존한다. Primary가 설정되면 Primary만, 없으면 fallback만 binding한다.
- explicit press owner `None/Computer/Equipment`로 Started와 Completed/Canceled를 같은 owner에 한 번만 전달한다.
- Computer capture중 LMB는 기존 pointer press/release만 사용하고 equipment use/query mutation을 만들지 않는다.
- G drop은 active equipment use를 한 번 cancel한 후 기존 swept commit을 사용한다. drop 실패에도 held reference/transform을 보존한다.

## 2. Interaction Prompt Native Extension

대상: `UI/InteractionPromptWidget.h/.cpp`

- 필수 `BindWidget` `EquipmentActionNameText`, `EquipmentFailureReasonText`, `EquipmentProgressBar`를 추가한다.
- 기존 7 bindings, `OnInteractionPromptChanged`, `OnInteractionPromptDetailsChanged` signature를 변경하지 않는다.
- optional `OnEquipmentUsePromptChanged` hook을 신규 정보로 추가하되 native binding 적용이 정확성을 소유한다.
- E/F/LMB row, primary/equipment progress와 intent별 transient failure를 독립 표시한다.
- root enabled는 visible E/F/LMB 중 하나라도 실행 가능한지로 판정한다.
- WBP와 Event Graph는 구현 단계에서 수정하지 않는다.

## 3. Cleaning LMB Migration

대상: `Cleaning/WetMopActor.h/.cpp`, `WaterStainActor.h/.cpp`

- Wet Mop이 `IHeldEquipmentUsable` Hold를 구현하고 LMB중 target 유무와 관계없이 `bIsMopping`/loop motion을 유지한다.
- Water Stain은 E hold를 제공하지 않고 side-effect-free LMB target/failure query와 native begin/update/cancel cleaning API를 제공한다.
- Hold중 camera focus가 새 stain으로 바뀌면 기존 stain progress/lock을 0으로 cancel하고 새 stain을 시작한다.
- target이 없으면 progress만 없고 mopping/motion은 유지한다. LMB release/drop/EndPlay은 전체 use를 정리한다.
- 기존 `RemovalDurationSeconds`, cleaner identity, progress/completion/variation/spawn/Blueprint event를 보존한다.
- optional `OnMoppingStateChanged` 표현 event를 추가하되 Blueprint가 progress를 변경하지 못하게 한다.

## 4. Combat

신규:

- `Combat/CombatTypes.h`
- `Combat/HealthComponent.h/.cpp`
- `Combat/MeleeAttackComponent.h/.cpp`
- `Combat/MonkeyWrenchActor.h/.cpp`

계약:

- Wrench는 common Actor base 없이 interactable/carryable/equipment-usable를 직접 구현하고 E pickup, G swept free drop, 개별 HeldTransform을 사용한다.
- LMB Started당 공격 하나만 시작하고 attack중 입력을 무시하며 buffer/auto repeat를 만들지 않는다.
- authorable hit time에 카메라 origin/forward를 새로 snapshot하고 `Origin + Forward * Distance`의 단일 sphere multi trace를 수행한다.
- weapon owner/actor를 제외하고 Actor identity로 dedupe한 모든 active HealthComponent target에 각각 한 번 damage를 적용한다. intended target 우선/수량 제한은 없다.
- weapon World Mesh/collision/socket은 authoritative hit source가 아니다.
- damage context에 instigator/causer, damage, camera origin/direction, impulse/vertical impulse를 보존한다.
- HealthComponent는 clamp, health/depleted delegate, duplicate depleted guard와 configured recovery를 소유한다.

## 5. Customer Knockdown And Soft Restart

신규/target:

- `Customer/CustomerKnockdownComponent.h/.cpp`
- `Customer/CustomerRoutineInterruptionComponent.h/.cpp`
- `Customer/BathhouseCustomerCharacter.h/.cpp`
- `Customer/CustomerSessionComponent.h/.cpp`
- `Customer/CustomerMontagePlaybackComponent.h/.cpp`
- `Customer/StateTree/CustomerStateTreeTasks.h/.cpp`

계약:

- Customer가 private `Health`, `CustomerKnockdown`, `CustomerRoutineInterruption` default subobject/getter를 조립하고 기존 `CustomerSession` AllowPrivateAccess를 보존한다.
- nonlethal damage는 health/표현만 갱신하고 StateTree를 중단하지 않는다.
- health 0은 depleted를 한 번 commit하고 StateTree `PauseLogic`, movement/montage 중단, collision/movement/mesh transform snapshot, ragdoll을 수행한다.
- configured root bone PhysicsAsset body를 data/runtime validate하고 해당 body에 camera direction + vertical impulse를 적용한다. pelvis silent fallback은 금지한다.
- knockdown중 추가 damage/impulse와 recovery timer reset을 무시한다.
- timer 만료 시 root 최종 XY와 floor-aligned Z에서 physics/collision/mesh/movement를 복구하고 health를 authorable ratio(기본 1.0)로 회복한다. get-up montage와 NavMesh 수평 복귀는 없다.
- Session은 check-in, total bath stay, towel wait의 TimerManager 잔여 시간을 idempotent pause/resume한다. knockdown timer는 계속 진행한다.
- interruption component는 monotonic serial과 active restartable operation token만 소유하고 StateTree phase graph를 C++에 복제하지 않는다.
- `PauseLogic/ResumeLogic`를 사용하여 active hierarchy/instance data를 보존한다. soft pause는 Queue/Facility/Checkout Task `ExitState()`를 호출하지 않는다.
- timed activity는 local duration 초기화, once/loop montage는 후보 재선택·처음부터 재생, wait는 session 잔여 시간을 사용한다. Bath local restart는 total bath remaining을 넘지 않는다.
- native `FCustomerRestartableMoveToTask`를 추가하고 stale request token을 무시하며 recovery 후 current destination으로 새 path request를 시작한다. Content StateTree 교체는 Editor 인계다.
- facility action은 `EndUse`로 `Occupied -> Reserved`만 적용하고 reservation/cache를 보존한다. 기립 후 ragdoll 위치에서 ApproachPoint로 이동, Bath snap/BeginUse 후 local action을 재시작한다.
- key/towel handle/cash offer/queue/facility reservation/completed atomic transaction을 rollback/replay하지 않는다. facility/owner invalidation은 기존 retry/technical cleanup을 사용한다.

## Blueprint/API And Editor Handoff

- 신규 component는 stable reflected name, `VisibleAnywhere`, `BlueprintReadOnly`, `AllowPrivateAccess=true`를 사용한다.
- `ComputerClickAction`을 보존하므로 이번 구현에 Core Redirect/Config를 추가하지 않는다.
- 구현 후 `.md/PROMPT_UNREAL.md`에 실제 Content scan 경로와 다음을 기록한다: `IA_ComputerClick -> IA_PrimaryUse` 이관/redirector 정리, `IMC_FirstPerson` LMB, player property, `WBP_InteractionPrompt` 3 bindings, Wrench Blueprint/배치, WetMop curve, Customer health/ragdoll/root PhysicsAsset authoring, `ST_CustomerRoutine` MoveTo Task 교체.
- 구현 Agent는 Content를 직접 수정하지 않는다.

## Native Tests And Validation

- single carry key/mop/basket/wrench mutual exclusion, wrench E pickup/G rollback-safe drop
- PrimaryUse preferred/fallback 단일 binding, Computer/Equipment press owner와 forced cancel
- equipment query merge, LMB row/progress/failure, 기존 E/F/G/hold prompt 회귀
- wrench Started 단일 attack, hit timing, camera sphere multi, Actor dedupe, 범위 내 모든 health target과 mesh 비의존성
- mop no-target mopping/motion, stain focus enter/switch/leave, release/drop/EndPlay cleanup과 completion 일회성
- nonlethal routine 유지, depleted 단일 knockdown, root body validation/impulse, repeated-hit ignore, configured health recovery
- `PauseLogic`이 Task exit/queue/facility/checkout cleanup을 유발하지 않고 `ResumeLogic`이 기존 instance를 유지함
- check-in/bath/towel timer 잔여 재개, local timer/montage restart, total bath cap
- move/queue/facility/checkout 중단과 resource/reservation/atomic commit 무손실·무복제
- customer/controller/facility/tool EndPlay의 정상 cleanup과 delegate/timer/request token 정리
- 기존 interaction/carry/computer/cleaning/towel/customer automation 회귀

`git diff --check` 후 UE 5.8 `Build.bat`을 `.md/AGENT_WORKFLOW.md` 정책대로 첫 시도부터 승인된 sandbox 밖에서 실행한다. 구현 완료 후 `.md/PROMPT_REVIEW.md`와 `.md/PROMPT_UNREAL.md`만 정기 결과물로 작성한다.
