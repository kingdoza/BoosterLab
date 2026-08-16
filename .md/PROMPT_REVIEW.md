# Code Review Prompt — World Computer Focus And Sample Screen

## Review Objective

UE 5.8의 기존 first-person 이동, E primary instant/hold, F secondary, G physical carry drop과 native interaction prompt 계약을 보존하면서 추가한 world computer focus/session/sample-screen 구현을 Editor authoring 전에 리뷰한다.

기준 문서:

- `.md/AGENT_WORKFLOW.md`
- `.md/AGENT_REVIEW.md`
- `.md/0_ARCHITECTURE.md`
- `.md/Architecture/CharacterSystem.md`
- `.md/Architecture/InteractionSystem.md`
- `.md/Architecture/UISystem.md`
- `.md/Architecture/ComputerSystem.md`
- `.md/QNA_ARCHITECTURE.md` Q31
- `.md/PROMPT_IMPLEMENTATION.md`

## Acceptance Contract

- computer 진입은 `UPlayerCarryComponent::IsHandEmpty()`인 local player만 허용하고 held item을 drop/swap/hide하지 않는다.
- `ABathhouseComputerActor`가 actor-local screen/focus camera와 expected-user reservation을 소유한다.
- `UPlayerComputerUseComponent`가 phase, view target, movement/custom mode, cursor, timer와 pointer-down 상태의 단일 owner다.
- Character는 default subobject 조립과 input intent routing만 하며 computer phase/reservation을 복제하지 않는다.
- Interaction은 Computer concrete type을 모르는 generic suppression API만 제공한다.
- focus-in/out은 ScreenWidget class/instance를 remove/recreate하지 않는다.
- sample WBP는 `TestButton`, `ClickResultText` hierarchy/style만 소유하고 click logic은 native다.
- Content/Config/module dependency/Core Redirect를 변경하지 않는다.

## Implemented Changes

### Computer actor and reservation

- `ABathhouseComputerActor`가 exact native subobject `ComputerMesh`, `ScreenWidget`, `FocusCamera`를 생성한다.
- `BeginPlay`가 `ScreenWidget->InitWidget()`을 보장하고 실제 user widget, focus camera, carry/player component와 reservation을 query/execute에서 재검증한다.
- key/mop/basket을 든 경우 exact failure `손에 든 물건을 내려놓아야 합니다`를 반환한다.
- execute는 expected user로 reserve한 뒤 session 시작 실패 시 같은 identity guard로 rollback한다.
- 다른 user release와 반복 release는 no-op이며 Actor EndPlay는 current user에게 unavailable을 통지한다.

### Player focus session

- `UPlayerComputerUseComponent`가 `Inactive`, `FocusingIn`, `Active`, `FocusingOut`을 구분한다.
- Character가 first-person camera, movement, interaction, carry와 mouse-source widget interaction context를 명시적으로 configure한다.
- 진입은 local pawn/controller와 빈손을 재검증하고 기존 view/movement/custom/cursor를 snapshot한 뒤 sprint/jump/movement를 멈추고 interaction을 suppress한다.
- blend 완료 뒤에만 `FInputModeGameAndUI`, cursor와 hit testing을 켜며 pointer press는 `Active`에서만 허용한다.
- 종료/강제 cleanup은 pointer를 한 번 release하고 이전 view, movement/custom, `FInputModeGameOnly`, saved cursor, interaction query와 reservation을 복구한다.
- computer EndPlay, invalid target/controller와 transition 중 exit는 같은 cleanup state를 사용한다.

### Character input routing

- `PlayerComputerUse`, `ComputerWidgetInteraction`, `ComputerClickAction`을 추가했다.
- 진입 E와 종료 E가 각각 자신의 Completed/Canceled까지 소비하도록 explicit press-owner bool을 둔다.
- capture 중 Move/Look/Jump/Sprint/F/G와 공개 `DoMove`, `DoLook`, `DoJumpStart`가 기존 domain 호출 전에 return한다.
- click Started가 pointer press를 실제 시작한 경우에만 Completed/Canceled가 release한다.

### Interaction suppression and sample widget

- `SetInteractionSuppressed(bool)`은 true 전환 시 active hold를 transient failure 없이 한 번 cancel하고 primary latch/current target/query를 지운다.
- suppress 중 Tick refresh와 E/F/G public attempt는 trace, domain mutation과 attempt-result broadcast를 만들지 않는다.
- false 전환은 query를 즉시 refresh하고 반복 설정은 no-op이다.
- `UComputerSampleScreenWidget`은 construct/destruct에서 button delegate를 대칭 연결하며 widget-instance `bWasClicked`로 `버튼을 클릭하세요`/`클릭 확인`을 적용한다.

## Changed Source

- `Public/Computer/BathhouseComputerActor.h`
- `Private/Computer/BathhouseComputerActor.cpp`
- `Public/Computer/PlayerComputerUseComponent.h`
- `Private/Computer/PlayerComputerUseComponent.cpp`
- `Public/UI/ComputerSampleScreenWidget.h`
- `Private/UI/ComputerSampleScreenWidget.cpp`
- `Public/Character/FirstPersonCharacter.h`
- `Private/Character/FirstPersonCharacter.cpp`
- `Public/Interaction/PlayerInteractionComponent.h`
- `Private/Interaction/PlayerInteractionComponent.cpp`
- `Private/Tests/ComputerAutomationTests.cpp`
- `Private/Tests/BathhouseCleaningTowelTestProbe.h/.cpp`

관련 architecture 정본과 `.md/PROMPT_UNREAL.md`도 실제 native 계약으로 갱신했다.

## Class Growth Check

| Owner | Header lines | CPP lines | 판단 |
|---|---:|---:|---|
| FirstPersonCharacter | 113→135 | 218→308 | composition과 입력 분기만 확장, session 상태는 component에 유지 |
| PlayerInteraction | 99→103 | 358→416 | generic suppression lifecycle만 추가, Computer include 없음 |
| BathhouseComputerActor | 신규 60 | 신규 152 | actor-local 표현/reservation/query transaction으로 응집 |
| PlayerComputerUseComponent | 신규 99 | 신규 395 | phase/view/input/cleanup이 함께 움직이는 player session owner |
| ComputerSampleScreenWidget | 신규 34 | 신규 42 | 두 BindWidget의 delegate/state 표시만 소유 |

`UPlayerComputerUseComponent` CPP 증가는 정상/강제 종료의 대칭 복구와 invalidation 방어를 한 owner에 모은 결과다. 범용 desktop/app framework로 분리하지 않는다.

## Blueprint/API/Core Redirect Impact

신규 reflected 계약:

- `ABathhouseComputerActor`와 `ComputerMesh`, `ScreenWidget`, `FocusCamera`, focus blend 값
- `AFirstPersonCharacter::PlayerComputerUse`, `ComputerWidgetInteraction`, `ComputerClickAction`, `GetPlayerComputerUse`
- `UComputerSampleScreenWidget::TestButton`, `ClickResultText`

기존 reflected symbol rename/delete가 없어 Core Redirect는 없다. `BathhouseSim.Build.cs`, `.uproject`, `Config/`와 `Content/`를 변경하지 않았다.

## Verification Evidence

- UE 5.8 `Build.bat` `BathhouseSimEditor Win64 Development`: UHT/compile/link success
- focused `BathhouseSim.Computer.FocusSessionSuppressionAndSampleScreen`: success, exit code 0
- full `Automation RunTests BathhouseSim`: 18 success, 0 fail, exit code 0
- focused coverage: missing carry/component/screen, empty/key/mop/basket gate, occupied/rollback/duplicate begin; zero/non-zero phase와 transition exit; E ownership, movement/F/G gate, pointer forced release; view/movement/cursor/interaction/reservation 복구; controller loss와 Actor EndPlay; widget instance/state 유지; suppression hold 1회 cancel/empty query 1회/direct no-op

## Review Focus

1. Actor query가 side effect 없이 실패 이유를 정하고 execute가 reservation/session transaction을 정확히 rollback하는가?
2. focus-in 실패, focus-out, Actor/Component/Controller invalidation에서 timer, pointer, input mode, view와 reservation이 중복 또는 누락 없이 정리되는가?
3. `FocusingIn` 중 E exit와 zero blend reentrancy가 `Phase`/snapshot을 잘못 비우지 않는가?
4. pointer release가 focus-out과 later input Completed에서 이중 발생하거나 stuck 상태를 만들지 않는가?
5. Character의 explicit E/click press ownership이 기존 instant/hold begin/end를 오염시키지 않는가?
6. suppression direct-call guard가 attempt broadcast/domain mutation을 만들지 않고 해제 시 query를 즉시 복구하는가?
7. sample widget state가 actor lifetime 동안 유지되면서 SaveGame/domain state로 성장하지 않았는가?
8. `.md/PROMPT_UNREAL.md`가 native logic을 Blueprint에 복제하지 않고 실제 존재/부재 asset 경로를 정확히 반영하는가?

## Review Output

- finding은 severity와 exact file/line 근거로 작성한다.
- 문제가 없으면 코드 단계 승인 후 `.md/PROMPT_UNREAL.md`를 Editor 단계로 인계한다.
- Source 문제가 있으면 `.md/PROMPT_IMPLEMENTATION_R.md`에 최소 재작업 범위와 회귀 검증을 작성한다.
