# Unreal Prompt — World Computer Focus And Sample Screen Integration

## Status

**Native 구현, UE 5.8 build, focused automation과 BathhouseSim 전체 automation 18/18 완료. 아래 Content authoring, Blueprint compile/reload와 Editor/PIE 통합 검증이 필요하다.**

이 단계는 C++ session, reservation, interaction suppression 또는 click state logic을 Blueprint로 옮기지 않는다. 작업 전 모든 Editor/Live Coding 세션을 닫고 최신 `BathhouseSimEditor Win64 Development` binary로 새 Editor를 연다.

## Verified Content Scan

현재 존재하는 연결 대상:

- `/Game/FirstPersonCharacter/BP_FirstPersonCharacter`
- `/Game/Input/IMC_FirstPerson`
- `/Game/Input/Actions/IA_Interact`
- `/Game/Input/Actions/IA_SecondaryInteract`
- `/Game/Input/Actions/IA_DropCarry`
- `/Game/Maps/DefaultMap`

현재 Content에는 `Computer`, `Monitor`, `IA_ComputerClick` 이름의 project asset이 없다. 다음 exact target 경로를 신규 생성한다.

- `/Game/Bathhouse/Blueprints/Computer/BP_BathhouseComputer`
- `/Game/Bathhouse/UI/WBP_ComputerSampleScreen`
- `/Game/Input/Actions/IA_ComputerClick`

project computer/monitor static mesh도 발견되지 않았다. sample 검증은 `/Engine/BasicShapes/Cube.Cube` placeholder를 사용할 수 있지만, production mesh를 임의 경로로 추정하거나 새 외부 asset을 import하지 않는다. production mesh가 제공되면 실제 사용 경로를 completion report에 기록한다.

## Native Preflight

1. startup에서 `LogBlueprint: Error`, missing native class/property/component/event와 duplicate property가 0인지 확인한다.
2. `/Game/FirstPersonCharacter/BP_FirstPersonCharacter`를 저장 전 Compile하여 inherited `PlayerComputerUse`, `ComputerWidgetInteraction`, `ComputerClickAction`이 보이는지 확인한다.
3. native load/compiler 오류가 있으면 어떤 asset도 저장하지 말고 코드 리뷰 단계로 돌린다.
4. 기존 사용자 dirty asset과 의도하지 않은 map/external actor는 저장하지 않는다.

## 1. Computer Click Input

1. `/Game/Input/Actions/IA_ComputerClick`을 생성한다.
2. Value Type은 `Digital (bool)`로 두고 별도 Trigger/Modifier를 추가하지 않는다.
3. `/Game/Input/IMC_FirstPerson`에 `Left Mouse Button -> IA_ComputerClick` mapping을 하나 추가한다.
4. 기존 E/F/G/Move/Look/Jump/Sprint mapping과 mapping context priority를 변경하지 않는다.
5. `/Game/FirstPersonCharacter/BP_FirstPersonCharacter` Class Defaults의 `ComputerClickAction`에 `/Game/Input/Actions/IA_ComputerClick`을 지정한다.
6. `ComputerWidgetInteraction`은 inherited component를 그대로 사용한다. Blueprint Event Graph에서 Press/Release Pointer Key를 호출하지 않는다.

## 2. Native Sample Screen WBP

`/Game/Bathhouse/UI/WBP_ComputerSampleScreen`을 생성하고 native parent를 `UComputerSampleScreenWidget`으로 설정한다.

권장 hierarchy:

```text
RootOverlay (Overlay)
  ScreenPanel (Border)
    ScreenColumn (VerticalBox)
      ClickResultText (TextBlock)   [required BindWidget]
      TestButton (Button)           [required BindWidget]
        TestButtonLabel (TextBlock)
```

필수 계약:

- 이름과 타입은 정확히 `TestButton: Button`, `ClickResultText: TextBlock`이다.
- `TestButtonLabel`의 표시 문자열은 예: `테스트 클릭`이며 계약 이름이 아니다.
- Designer preview의 `ClickResultText`는 `버튼을 클릭하세요`로 둘 수 있으나 runtime text owner는 native C++이다.
- root desired size는 `1024 x 576` 기준으로 만들고 button이 mouse hit-testable한 `Visible` 상태인지 확인한다.
- Event Graph에 OnClicked, text 변경, focus-in/out, actor/session 호출, Tick/Delay를 추가하지 않는다.
- native construct/destruct가 delegate를 연결하므로 Blueprint binding을 중복 추가하지 않는다.

Compile 후 required BindWidget 오류가 0인지 확인한다.

## 3. Computer Actor Blueprint

`/Game/Bathhouse/Blueprints/Computer/BP_BathhouseComputer`를 생성하고 native parent를 `ABathhouseComputerActor`로 설정한다.

inherited component 이름을 바꾸거나 대체 component를 추가하지 않는다.

- `ComputerMesh` root
- `ScreenWidget`
- `FocusCamera`

### ComputerMesh

- sample 단계에서는 `/Engine/BasicShapes/Cube.Cube`를 placeholder로 지정하고 monitor/desk 크기로 scale한다.
- Visibility trace가 Actor를 찾을 수 있도록 collision은 Query 가능, Visibility Block으로 둔다.
- production computer mesh가 제공되면 placeholder만 교체하고 native component 이름/부모 관계는 유지한다.

### ScreenWidget

- Widget Class: `/Game/Bathhouse/UI/WBP_ComputerSampleScreen`
- Space: `World`
- Draw Size: `1024 x 576`
- Geometry Mode: `Plane`
- Pivot: `(0.5, 0.5)`
- Receive Hardware Input: `false`
- collision/hit testing은 mouse-source `ComputerWidgetInteraction`의 world hit가 screen plane을 찾을 수 있도록 Query Only와 Visibility Block을 확인한다.
- monitor 앞면과 z-fighting이 없도록 local transform을 조정한다.
- Blueprint에서 Widget Class를 set/remove하거나 user widget을 recreate하는 graph를 만들지 않는다.

### FocusCamera and blends

- `FocusCamera`를 monitor 정면 중앙을 바라보게 배치한다.
- 화면 가장자리와 주변 목욕탕 일부가 함께 보이도록 relative transform과 FOV를 조정한다.
- `FocusBlendInSeconds` 기본 `0.35`, `FocusBlendOutSeconds` 기본 `0.25`를 시작값으로 사용한다.
- 카메라가 ComputerMesh 또는 ScreenWidget 뒤/안쪽에 놓이지 않았는지 viewport preview로 확인한다.

## 4. Level Placement

Target map: `/Game/Maps/DefaultMap`

1. `BP_BathhouseComputer` 한 대를 player가 정면에서 E trace 가능한 위치에 배치한다.
2. player camera와 ComputerMesh/ScreenWidget 사이에 Visibility를 먼저 막는 장식 collision이 없는지 확인한다.
3. FocusCamera view에서 monitor가 정면이고 주변 목욕탕이 일부 보이는지 확인한다.
4. actor를 복수 배치해도 각 ScreenWidget instance와 reservation이 actor별로 분리되는지 확인한다.
5. placement 검증 중 생성된 의도하지 않은 external actor package는 저장하지 않는다.

## 5. PIE Acceptance

### Entry and empty-hand gate

1. 빈손 E Started로 focus-in하고 `0.35`초 뒤 cursor와 screen click이 활성화되는지 확인한다.
2. 진입 E release가 즉시 focus-out하지 않는지 확인한다.
3. key, wet mop, towel basket을 각각 든 상태에서 `손에 든 물건을 내려놓아야 합니다`가 표시되고 진입하지 않는지 확인한다.
4. 실패 시 held item이 자동 drop/hide/swap되지 않는지 확인한다.

### Input capture and pointer

1. focus 중 Move/Look/Jump/Sprint/F/G가 player/world 상태를 바꾸지 않는지 확인한다.
2. Left Mouse Button으로 `TestButton`을 클릭하면 `ClickResultText`가 `클릭 확인`으로 바뀌는지 확인한다.
3. mouse down 상태에서 E로 focus-out해도 button stuck, duplicate click 또는 이후 world click 누수가 없는지 확인한다.
4. focus-in blend 중 mouse click은 작동하지 않지만 새 E Started focus-out은 작동하는지 확인한다.

### Exit and restoration

1. 새 E Started가 focus-out을 시작하고 그 E Completed가 기존 primary hold에 전달되지 않는지 확인한다.
2. 이전 first-person view, movement mode, cursor와 GameOnly 입력이 복구되는지 확인한다.
3. focus-out 직후 기존 interaction prompt가 현재 target 기준으로 다시 나타나는지 확인한다.
4. 재진입하면 같은 widget instance의 `클릭 확인` 상태가 유지되는지 확인한다.
5. world/NPC simulation은 computer 사용 중에도 계속 진행되고 game pause/fullscreen viewport widget이 생기지 않는지 확인한다.

### Cleanup and repetition

1. 빠른 E 반복, focus-in 중 exit, focus-out 중 반복 exit에서 camera/input/reservation이 잠기지 않는지 확인한다.
2. 사용 중 computer Actor를 PIE에서 파괴하거나 level transition을 수행했을 때 first-person view/input이 복구되는지 확인한다.
3. player/controller 종료 또는 possess 변경 뒤 cursor, pointer, movement와 interaction suppression이 남지 않는지 확인한다.
4. focus-out/re-entry가 `WBP_ComputerSampleScreen`을 재생성하거나 click state를 초기화하지 않는지 확인한다.

## Compile, Save And Reload

1. `IA_ComputerClick`, `IMC_FirstPerson`, `WBP_ComputerSampleScreen`, `BP_BathhouseComputer`, `BP_FirstPersonCharacter`를 개별 Compile/Data Validation한다.
2. error/warning 0인 경우에만 의도한 asset과 `/Game/Maps/DefaultMap` placement를 저장한다.
3. Editor를 재시작하고 native parent, inherited component 이름, WBP BindWidget, action assignment, widget class와 camera/blend 값이 유지되는지 확인한다.
4. target asset 외 dirty package가 생기면 저장하지 않는다.

## Forbidden Blueprint Logic

- computer reservation/phase/timer/view target/movement/input mode/cursor 처리
- interaction suppression, E/F/G gate와 pointer press ownership
- held item 자동 drop/swap/hide
- WBP OnClicked text 상태 또는 widget recreate/remove
- fullscreen viewport widget, pause, mapping context 교체
- reflected symbol rename/delete, Core Redirect/Config/module 변경

## Completion Report

- 생성/수정/저장한 exact asset 경로
- 실제 ComputerMesh 경로 또는 `/Engine/BasicShapes/Cube.Cube` placeholder 사용 여부
- ScreenWidget draw size/space/geometry/collision, WBP class와 exact hierarchy
- FocusCamera relative transform/FOV와 blend 값
- `IA_ComputerClick`/`IMC_FirstPerson`/player property assignment
- empty/key/mop/basket gate, E ownership, capture gate, pointer forced release와 state 유지 PIE 결과
- actor/player/controller cleanup, Blueprint Compile/Data Validation/reload 결과
- 의도하지 않은 dirty asset/external actor 유무
