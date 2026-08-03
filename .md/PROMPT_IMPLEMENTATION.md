# Implementation Prompt — Bath Approach Snap And Customer Montage Tasks

## 목적

기존 customer gameplay loop에 다음 두 변경만 native C++로 구현한다.

1. Bath slot의 NavMesh 위 `ApproachPoint`까지 이동한 뒤 NavMesh 밖일 수 있는 `ActionPoint`로 snap하고, 퇴탕 전에 approach로 복귀한다.
2. 유효 후보 중 하나를 EnterState에서 선택하는 one-shot과 duration-loop customer montage StateTree Task를 추가한다.

신발·의상 mesh, visibility, skin weight, AnimNotify와 appearance state는 이번 작업에서 제외한다.

기준 문서:

- `.md/AGENT_WORKFLOW.md`
- `.md/AGENT_IMPLEMENTATION.md`
- `.md/0_ARCHITECTURE.md`
- `.md/Architecture/CoreSystem.md`
- `.md/Architecture/FacilitySystem.md`
- `.md/Architecture/CustomerSystem.md`

이 파일은 기존 interaction/counter 구현 프롬프트를 전부 대체한다.

## 수용 기준

- Bath action point가 NavMesh 밖이어도 customer가 approach까지 보행하고 정확한 action transform으로 snap한다.
- 정상 dwell, `BathStayExpired`, StateTree interruption과 technical abort가 모두 approach 복귀와 movement 복구를 slot release보다 먼저 시도한다.
- 다음 Bath 또는 main shower `MoveTo`는 NavMesh 위 approach 위치에서 시작한다.
- 두 montage Task 모두 null 후보를 제외한 유효 후보가 0개면 실패, 1개면 그 하나, 여러 개면 균등 random 하나를 선택한다.
- 선택은 EnterState에서 한 번만 하며 loop Task는 duration 동안 같은 montage만 반복한다.
- one-shot은 실제 montage 정상 종료에 성공하고 interruption에는 실패한다.
- montage stop/exit는 playback token을 확인해 자신이 시작한 montage만 정리한다.
- 기존 reflected Task/property/component 이름을 rename 또는 delete하지 않고 timer-only activity 경로를 유지한다.
- 신발·의상 관련 Source와 Content를 추가하거나 변경하지 않는다.

## 책임 변화와 신규 타입

`UCustomerSessionComponent`는 기존 facility runtime owner 책임을 확장해 현재 Bath snap 여부, 예약 시점 approach/action transform과 movement 복구를 소유한다. 별도 Component로 나누면 reservation과 cleanup 순서를 분산시키므로 Session에 유지한다.

montage delegate, AnimInstance와 playback token은 session domain state와 독립된 lifecycle이다. `ABathhouseCustomerCharacter`에 새 `UCustomerMontagePlaybackComponent` default subobject를 조립하고 재생 책임을 분리한다.

StateTree는 순서와 Task parameter/binding을 소유한다. native Task는 session transaction과 montage playback API만 호출하며 Blueprint graph에 domain mutation을 추가하지 않는다.

## 구현 파일

신규:

- `Source/BathhouseSim/Public/Customer/CustomerMontagePlaybackComponent.h`
- `Source/BathhouseSim/Private/Customer/CustomerMontagePlaybackComponent.cpp`

수정:

- `Source/BathhouseSim/Public/Customer/BathhouseCustomerTypes.h`
- `Source/BathhouseSim/Public/Customer/CustomerSessionComponent.h`
- `Source/BathhouseSim/Private/Customer/CustomerSessionComponent.cpp`
- `Source/BathhouseSim/Public/Customer/BathhouseCustomerCharacter.h`
- `Source/BathhouseSim/Private/Customer/BathhouseCustomerCharacter.cpp`
- `Source/BathhouseSim/Public/Customer/StateTree/CustomerStateTreeTasks.h`
- `Source/BathhouseSim/Private/Customer/StateTree/CustomerStateTreeTasks.cpp`
- 필요한 focused native automation test

실제 구현 뒤 갱신할 정본:

- `.md/0_ARCHITECTURE.md`
- `.md/Architecture/FacilitySystem.md`
- `.md/Architecture/CustomerSystem.md`

정기 결과물:

- `.md/PROMPT_REVIEW.md`
- `.md/PROMPT_UNREAL.md`

`Content/`, `Config/`, `.uproject`와 `BathhouseSim.Build.cs`는 수정하지 않는다. `Engine` 의존성으로 Animation/AnimInstance API를 사용하며 private GameplayInteractions montage Task를 include하거나 복제하지 않는다.

## 1. Bath Approach/Action Snap

- `UBathhouseFacilitySlotComponent`의 기존 component transform을 action point, `ApproachOffset` 결과를 approach point로 유지한다.
- slot reflected property를 rename하거나 새 이동 상태를 Facility에 추가하지 않는다.
- facility 예약 성공 시 현재 slot의 approach/action transform을 Session에 캐시한다.
- Session에 `bSnappedToFacilityActionPoint`와 snap/복구 API를 추가한다.
- action 진입은 AIController 이동 취소, CharacterMovement velocity 정지, 기존 movement mode 보관, movement 비활성화, action 위치/회전 적용 순서다.
- approach 복귀는 cached approach 위치/회전 적용 후 보관한 movement mode를 복원하되 walking customer의 기본 fallback은 `MOVE_Walking`이다.
- snap은 capsule이 blocking geometry와 겹치는지 검증하고 실패 시 위치를 변경하지 않은 채 false와 진단 로그를 반환한다.
- 일반 facility는 기존 이동과 activity 동작을 유지하며 새 snap을 자동 적용하지 않는다.
- `ReleaseCurrentFacility`, `AbortActivity`, `TechnicalAbort`, EndPlay cleanup은 snap 상태면 approach 복귀를 먼저 시도한 뒤 slot을 release한다.
- 정상 경로에는 명시적 snap-out Task를 사용하고 cleanup의 복귀는 누락·중단에 대한 fallback으로 둔다.

`ECustomerFacilitySnapTarget`은 `ActionPoint`, `ApproachPoint` 두 값만 가진다. `FCustomerFacilitySnapTask`는 Session context와 Target parameter를 받고 EnterState에서 즉시 성공/실패를 반환한다.

## 2. Montage Playback Component

- `UCustomerMontagePlaybackComponent`는 tick하지 않는 native ActorComponent다.
- Character constructor에서 `CustomerMontagePlayback` 이름의 private default subobject로 생성한다.
- property는 `VisibleAnywhere`, `BlueprintReadOnly`, `AllowPrivateAccess`를 사용하고 public getter를 제공한다.
- owner `USkeletalMeshComponent`의 AnimInstance를 runtime에 조회하며 asset을 소유하지 않는다.
- monotonic playback token, 현재 montage와 `Playing/Succeeded/Interrupted` 결과를 저장한다.
- `Montage_Play` 반환값이 0 이하이거나 AnimInstance/montage가 없으면 시작에 실패한다.
- `FOnMontageEnded`의 `bInterrupted`로 정상 종료와 중단을 구분한다. montage 길이 타이머로 one-shot 완료를 추정하지 않는다.
- 새 playback이 기존 것을 대체하면 기존 token은 interrupted로 끝난다.
- stop/query는 token 일치 여부를 검사해 이전 Task가 새 montage를 중단하지 못하게 한다.
- component EndPlay와 owner 종료에서 delegate와 현재 playback을 안전하게 정리한다.

## 3. Montage StateTree Tasks

두 Task의 instance data는 customer context, `TArray<TObjectPtr<UAnimMontage>> MontageCandidates`, play rate, section/ blend 값과 선택된 montage/token runtime 값을 분리한다.

공통 후보 규칙:

- null을 제외한 유효 후보 배열을 만든다.
- 0개는 오류 로그와 `Failed`다.
- 1개는 random API를 호출하지 않고 index 0을 사용한다.
- 2개 이상은 편향 없는 균등 random으로 하나를 선택한다.
- 선택 결과는 instance data에 저장하고 Tick/reselect 도중 다시 선택하지 않는다.

`FPlayCustomerMontageOnceTask` (`DisplayName="Play Customer Montage Once"`):

- EnterState에서 후보 하나를 선택하고 재생해 `Running`이 된다.
- playback token 결과가 `Succeeded`면 Task 성공, `Interrupted`/invalid면 실패한다.
- ExitState에서 아직 자신이 소유한 playback만 configured blend-out으로 중단한다.

`FPlaySelectedMontageLoopForDurationTask` (`DisplayName="Play Selected Montage Loop For Duration"`):

- `Duration >= 0.1`, 유효 `LoopSection`과 재생 가능한 montage를 요구한다.
- EnterState에서 선택한 montage의 loop section을 자기 자신으로 연결하고 한 번만 재생한다.
- Tick은 remaining duration만 감소시키며 후보를 다시 뽑거나 montage를 교체하지 않는다.
- duration 전에 montage가 정상/비정상 종료되면 malformed loop로 실패한다.
- duration 만료 시 자신이 소유한 montage를 configured blend-out하고 성공한다.
- external StateTree exit도 동일 token ownership으로 정리한다.

## 4. Activity Lifecycle Integration

- 기존 `FCustomerActivityTask`와 `Timed Customer Activity` 이름은 timer-only/serialized 호환 경로로 유지한다.
- 새 `FCustomerBeginActivityTask`는 `Session`, `Activity`를 받아 `BeginActivity`를 호출하고 계산된 `ResolvedDuration` output을 제공한다.
- 새 `FCustomerFinishActivityTask`는 현재 Activity가 parameter와 일치할 때 `FinishActivity`를 commit한다. facility release는 수행하지 않는다.
- animation StateTree 경로는 `Begin Activity -> montage Task -> Finish Activity` 순서를 사용한다.
- duration-loop Task의 Duration은 앞선 Begin Task의 `ResolvedDuration`에 bind할 수 있어야 한다.
- `FCustomerFacilityTask::ExitState` fallback은 active Activity abort, snap 복귀, wait 해제와 slot release를 순서대로 수행한다.
- Bath dwell 자연 완료와 `BathStayExpired`는 모두 Activity를 정리하고 snap-out한 다음 facility parent를 빠져나가게 Editor 프롬프트에 명시한다.
- one-shot action의 완료 기준은 montage 정상 종료다. 기존 duration property는 삭제하지 않고 timer-only fallback에 남긴다.

## Blueprint/API/Core Redirect 영향

- 신규 component와 Task만 추가하며 기존 reflected type/property를 rename/delete하지 않으므로 Core Redirect는 추가하지 않는다.
- `BP_BathhouseCustomer`는 inherited `CustomerMontagePlayback`을 갖지만 별도 Blueprint logic이 필요하지 않다.
- `ST_CustomerRoutine`은 새 Task 배치와 context/output binding이 필요하다.
- Bath Blueprint의 각 slot action transform은 탕 내부, `ApproachOffset`은 NavMesh 위에 authoring해야 한다.
- Montage asset, AnimBP Slot node와 loop section은 Editor 작업이며 구현 단계는 Content를 수정하지 않는다.
- 신발·의상 mesh/component, visibility, AnimNotify, hand prop과 Motion Warping은 `PROMPT_UNREAL.md`에도 포함하지 않는다.

## 검증

- candidate helper는 0/1/여러 후보와 null 혼합을 검증한다.
- one-shot 정상 종료/interrupt, loop 단일 후보/다중 후보/조기 종료/duration 종료와 stale token stop을 검증한다.
- Bath snap은 action 진입, approach 복귀, release-before-return 방지와 cleanup idempotency를 검증한다.
- 기존 facility exclusivity, bath timer와 technical cleanup test를 유지한다.
- reflected type/property 이름과 외부 `CustomerSession` 직접 접근을 focused search로 확인한다.
- `git diff --check`를 실행한다.
- UE 5.8 `Build.bat`으로 `BathhouseSimEditor Win64 Development -WaitMutex -NoHotReloadFromIDE` 링크 빌드를 수행한다.
- 구현 결과로 `PROMPT_REVIEW.md`에는 class growth, delegate/token cleanup, 테스트와 빌드 결과를 기록한다.
- `PROMPT_UNREAL.md`에는 Bath approach/action authoring, AnimBP Slot/Montage 준비와 StateTree 수동 구성·binding·PIE 검증만 기록한다.

## 금지사항

- NavMesh를 action point까지 억지로 확장하거나 action point를 runtime navigation projection으로 대체하지 않는다.
- action point에서 slot을 먼저 release한 뒤 다음 MoveTo를 시작하지 않는다.
- montage 후보를 Tick마다 재추첨하거나 loop 도중 다른 montage로 shuffle하지 않는다.
- one-shot 성공을 montage asset 길이 기반 단순 timer로 판정하지 않는다.
- montage asset 참조를 `UCustomerSessionComponent` 또는 `UCustomerRoutineDefinition`에 저장하지 않는다.
- 신발·옷, modular mesh, skin weight, AnimNotify와 appearance system을 구현하지 않는다.
- UI, Interaction, Economy, input mapping과 unrelated gameplay를 변경하지 않는다.
