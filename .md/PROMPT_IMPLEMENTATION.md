# Implementation Prompt — World Computer Focus And Sample Screen

## 목적

현재 1인칭 조작, primary/secondary interaction, hold lifecycle, single physical carry와 interaction prompt 계약을 보존하면서 실제 월드 모니터에 표시되는 컴퓨터 샘플 화면을 native C++로 구현한다.

- 빈손으로 컴퓨터를 E 상호작용하면 monitor 정면 focus camera로 blend한다.
- focus 동안 1인칭 조작과 world interaction을 막고 mouse로 world-space widget을 클릭한다.
- 새 E Started로 focus-out하고 기존 1인칭 조작을 안전하게 복구한다.
- focus-out 뒤 같은 computer Actor가 살아 있는 동안 마지막 monitor 화면 상태를 유지한다.

기준 문서:

- `.md/AGENT_WORKFLOW.md`
- `.md/AGENT_IMPLEMENTATION.md`
- `.md/0_ARCHITECTURE.md`
- `.md/Architecture/CoreSystem.md`
- `.md/Architecture/CharacterSystem.md`
- `.md/Architecture/InteractionSystem.md`
- `.md/Architecture/UISystem.md`
- `.md/Architecture/ComputerSystem.md`
- `.md/QNA_ARCHITECTURE.md` Q31

이 파일은 이전 Carry/Stain/Towel Preview/Bath Snap 구현 프롬프트를 전부 대체한다.

## 보존 및 금지 범위

- 기존 E primary instant/hold, F secondary와 G equipment drop 의미를 바꾸지 않는다.
- key/wet mop/towel basket의 단일 physical carry와 `IPhysicalCarryable` 계약을 변경하지 않는다.
- computer 진입을 위해 held item을 자동 drop, 숨김, 보관 또는 swap하지 않는다. 빈손에서만 허용한다.
- `DisableInput()`으로 모든 입력을 끄거나 기존 mapping context를 제거하지 않는다.
- fullscreen viewport widget, game pause와 computer 사용 중 world/NPC 정지를 만들지 않는다.
- 범용 desktop/app framework, keyboard text input, scroll, 계정, 경제, 저장과 network replication을 추가하지 않는다.
- 기존 reflected symbol을 rename/delete하지 않고 Core Redirect/Config를 추가하지 않는다.
- 구현 단계에서 `Content/` asset을 생성, 수정 또는 resave하지 않는다.
- 새 module dependency를 추가하지 않는다. 기존 `UMG`, `InputCore`, `EnhancedInput`과 Engine API를 사용하고 direct Slate API는 사용하지 않는다.
- 사용자 소유 dirty Content와 `Reference/`를 건드리지 않는다.

## 1. Computer Actor

신규 파일:

- `Public/Computer/BathhouseComputerActor.h`
- `Private/Computer/BathhouseComputerActor.cpp`

`ABathhouseComputerActor : AActor, IPlayerInteractable`를 추가하고 다음 native default subobject를 안정적인 reflected 이름으로 생성한다.

- `ComputerMesh: UStaticMeshComponent` root
- `ScreenWidget: UWidgetComponent`
- `FocusCamera: UCameraComponent`

Editor authoring 값은 `FocusBlendInSeconds >= 0`, `FocusBlendOutSeconds >= 0`이며 native 기본값은 각각 `0.35`, `0.25`초다.

`QueryInteraction`과 `ExecuteInteraction`은 side-effect/commit 경계를 유지한다.

- carry context 또는 player computer component가 없으면 정확한 failure를 반환한다.
- `BeginPlay`에서 `ScreenWidget->InitWidget()`을 보장하고 실제 user widget이 없으면 사용 불가로 처리한다.
- `UPlayerCarryComponent::IsHandEmpty()`가 false이면 `손에 든 물건을 내려놓아야 합니다`를 반환한다.
- screen widget/focus camera가 유효하고 다른 user reservation이 없을 때만 `컴퓨터 사용`을 허용한다.
- execute는 모든 조건을 다시 검사하고 interactor의 `UPlayerComputerUseComponent`에 시작을 요청한다.
- reservation 성공 뒤 player session 시작이 실패하면 expected user guard로 reservation을 원복한다.
- 중복 begin/end와 다른 user release는 상태를 바꾸지 않는다.
- `EndPlay`는 current user component에 unavailable을 알리고 external reference를 지운다.

reservation은 현재 single local-player 범위의 transient weak/component reference로 구현하고 replicated 상태를 만들지 않는다.

## 2. Player Computer Use Component

신규 파일:

- `Public/Computer/PlayerComputerUseComponent.h`
- `Private/Computer/PlayerComputerUseComponent.cpp`

`UPlayerComputerUseComponent : UActorComponent`가 player별 transient session의 단일 owner다. internal phase는 `Inactive`, `FocusingIn`, `Active`, `FocusingOut`을 구분한다.

Character composition root가 camera/interaction/carry/movement/widget interaction context를 명시적으로 연결한다. Component는 active computer, 이전 view target, movement mode/custom mode, cursor 표시값, transition timer와 pointer-down 여부를 소유한다.

진입:

1. locally controlled owner, player controller, 빈손과 inactive 상태를 재검증한다.
2. 이전 view target/movement/cursor를 snapshot한다.
3. sprint와 jump를 끝내고 movement를 즉시 정지한 뒤 `MOVE_None`으로 바꾼다.
4. interaction suppression을 시작한다.
5. computer Actor에 `SetViewTargetWithBlend`한다.
6. blend 종료 시에만 `Active`로 전환하고 cursor, `FInputModeGameAndUI`와 widget hit testing을 켠다.

`FInputModeGameAndUI`에는 `ScreenWidget->GetUserWidgetObject()`를 focus 대상으로 사용하고 cursor를 viewport 안에서 정상 클릭할 수 있게 설정한다. focus-in 중 E focus-out은 허용하지만 pointer click은 `Active`에서만 허용한다. blend 시간이 0이면 즉시 완료한다.

종료:

1. pointer가 down이면 left mouse를 정확히 한 번 release한다.
2. widget hit testing과 cursor를 끄고 저장한 view target으로 blend한다.
3. blend 종료 후 저장한 movement mode/custom mode를 복구한다.
4. interaction suppression을 해제해 query를 즉시 refresh한다.
5. expected computer/user reservation을 해제하고 session snapshot을 지운다.
6. 정상 first-person 상태는 `FInputModeGameOnly`로 복구하되 저장한 cursor 표시값을 복원한다.

computer/player/controller EndPlay, target invalidation과 transition 중 재요청은 timer/pointer/reservation/input lock을 남기지 않는 단일 cleanup 경로를 사용한다. view target이 사라졌으면 owner pawn/controller의 유효한 view target으로 즉시 복구한다. begin/end/unavailable은 멱등적이어야 한다.

focus-out은 `ScreenWidget`의 widget class를 다시 설정하거나 user widget을 remove/recreate하지 않는다.

## 3. Character Input Integration

대상:

- `Character/FirstPersonCharacter.h/.cpp`

다음 default subobject/property를 추가한다.

- `PlayerComputerUse: UPlayerComputerUseComponent`
- `ComputerWidgetInteraction: UWidgetInteractionComponent`
- `ComputerClickAction: UInputAction` (`EditDefaultsOnly`, `BlueprintReadOnly`)

Widget interaction은 mouse source를 사용하고 default hit testing은 false다. debug는 기본 false이며 필요한 interaction distance는 Editor에서 조정 가능하게 한다.

입력 routing:

- E Started: computer component가 capture 중이면 focus-out을 요청하고 소비, 아니면 기존 `BeginPrimaryInteraction`.
- E Completed/Canceled: 해당 E press owner가 computer이면 소비, 아니면 기존 `EndPrimaryInteraction`.
- `ComputerClickAction` Started: `Active`일 때만 `PressPointerKey(EKeys::LeftMouseButton)`.
- Completed/Canceled: press를 시작한 경우만 정확히 한 번 release.
- Move/Look/Jump/Sprint/F/G handler와 공개 `DoMove`/`DoLook`/`DoJumpStart`는 computer가 capture 중이면 기존 movement/interaction/domain 호출 전에 return한다.

진입 E의 release가 즉시 focus-out하지 않고, 종료 E의 후속 release가 기존 hold를 건드리지 않도록 explicit press-consumed state를 둔다. Character는 computer phase/view/reservation을 복제하지 않고 component 조회만 사용한다.

BlueprintPure getter는 Editor/검증에 필요한 최소 `GetPlayerComputerUse()`만 추가한다. Widget interaction을 Blueprint logic에서 구동하지 않는다.

## 4. Interaction Suppression

대상:

- `Interaction/PlayerInteractionComponent.h/.cpp`
- focused interaction automation

C++ 전용 `SetInteractionSuppressed(bool bSuppressed)`, `IsInteractionSuppressed() const` API를 추가한다.

true 전환:

- active hold를 transient failure 없이 정확히 한 번 cancel
- primary-held flag와 current target/query 정리
- empty query delegate를 필요한 경우 한 번 broadcast
- Tick trace와 refresh 중단

suppress 중 `BeginPrimaryInteraction`, `TryInteract`, `TrySecondaryInteract`, `TryDropCarry` 직접 호출은 trace/domain mutation과 attempt-result broadcast를 만들지 않는다. `EndPrimaryInteraction`은 stale input flag만 안전하게 정리한다.

false 전환은 즉시 `RefreshInteractionQuery()`를 호출한다. 같은 값의 반복 설정은 cancel/refresh/delegate를 반복하지 않는다. Interaction code는 Computer header를 include하거나 concrete computer 상태를 판정하지 않는다.

## 5. Native Sample Screen Widget

신규 파일:

- `Public/UI/ComputerSampleScreenWidget.h`
- `Private/UI/ComputerSampleScreenWidget.cpp`

`UComputerSampleScreenWidget : UUserWidget`에는 `TestButton: UButton`, `ClickResultText: UTextBlock` 두 필수 `BindWidget`을 추가한다.

native construct에서 button delegate를 중복 없이 연결하고 destruct에서 대칭 해제한다. presentation-only `bWasClicked`를 widget instance에 유지하고 현재 상태를 bound text에 적용한다.

초기 표시는 `버튼을 클릭하세요`, 클릭 뒤 표시는 `클릭 확인`이다.

focus-out은 widget lifecycle을 끝내지 않으므로 재진입 뒤 클릭 상태가 유지되어야 한다. Actor/level lifetime을 넘는 SaveGame은 구현하지 않는다. WBP Event Graph에 클릭 로직을 요구하지 않는다.

## Blueprint/API와 Unreal 인계

신규 reflected symbol은 `ComputerSystem.md` 계약 이름을 그대로 사용한다. 기존 symbol rename/delete가 없으므로 Core Redirect는 없다.

구현 완료 후 `.md/PROMPT_UNREAL.md`에는 Content를 실제 scan한 정확한 asset 경로와 함께 다음을 기록한다.

- `BP_BathhouseComputer` 생성, native parent/mesh/screen/focus camera와 blend authoring
- world-space ScreenWidget의 draw size, geometry, collision/hit test와 sample WBP class 연결
- `WBP_ComputerSampleScreen` native parent와 정확한 `TestButton`, `ClickResultText` hierarchy 구성
- `IA_ComputerClick` 생성 및 기존 `IMC_FirstPerson`의 left mouse mapping
- player Blueprint의 `ComputerClickAction` assignment
- level 배치, 정면 focus 구도와 주변 목욕탕 시야 검증

구현 Agent는 Content를 직접 수정하지 않는다.

## Native Tests와 검증

- empty hand 성공, key/mop/basket held 실패와 query/execute 재검증
- screen/component 누락, occupied reservation과 begin failure rollback
- interaction suppression의 hold 1회 cancel, empty query, direct attempt no-op와 해제 refresh
- 진입 E Started/Completed, 새 종료 E Started/Completed의 정확한 press ownership
- capture 중 Move/Look/Jump/Sprint/F/G가 기존 상태를 바꾸지 않음
- focus phase 전이, zero/non-zero blend, repeated begin/end와 transition 중 exit
- pointer press/release, focus-out forced release와 double-click/stuck 방지
- 이전 view target, movement mode, cursor/input mode와 interaction 복구
- computer/player/controller EndPlay와 invalid target cleanup
- focus-out이 widget instance/클릭 표시 상태를 초기화하지 않음
- 기존 interaction/carry/cleaning/towel/customer automation 회귀 없음

`git diff --check` 후 UE 5.8 `Build.bat`을 `.md/AGENT_WORKFLOW.md` 정책대로 첫 시도부터 승인된 sandbox 밖에서 실행한다. 구현 완료 후 `.md/PROMPT_REVIEW.md`와 `.md/PROMPT_UNREAL.md`만 정기 결과물로 작성한다.
