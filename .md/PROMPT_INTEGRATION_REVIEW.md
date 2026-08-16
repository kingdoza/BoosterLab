# World Computer Focus And Sample Screen — Unreal Integration Review

## 작업 상태

- 상태: 완료
- 기준 작업서: `.md/PROMPT_UNREAL.md`
- 검증 엔진: Unreal Engine 5.8
- native reservation/session/input ownership 로직은 Blueprint로 옮기지 않았다.
- 이번 Editor 단계에서는 Source, Config, 기존 StateTree를 수정하지 않았다.
- 기존 dirty worktree의 다른 Source/Content/문서는 저장하거나 되돌리지 않았다.

## 생성·수정·저장한 에셋

| 구분 | 에셋 |
|---|---|
| 생성 | `/Game/Input/Actions/IA_ComputerClick` |
| 생성 | `/Game/Bathhouse/UI/WBP_ComputerSampleScreen` |
| 생성 | `/Game/Bathhouse/Blueprints/Computer/BP_BathhouseComputer` |
| 수정 | `/Game/Input/IMC_FirstPerson` |
| 수정 | `/Game/FirstPersonCharacter/BP_FirstPersonCharacter` |
| 배치 | `/Game/Maps/DefaultMap` |

- World Partition placement로 external actor package `Content/__ExternalActors__/Maps/DefaultMap/7/EH/E4FLO971KSWUJ40H7W7PHK.uasset` 1개가 생성됐다.
- 위 대상 외 Content package는 저장하지 않았다.

## Computer Click Input

- `IA_ComputerClick` Value Type: `Digital (bool)`.
- Trigger: 0개, Modifier: 0개.
- `IMC_FirstPerson`에 `Left Mouse Button -> IA_ComputerClick` mapping을 정확히 1개 추가했다.
- 기존 mapping을 유지했으며 최종 mapping 수는 4개다.
- `BP_FirstPersonCharacter` Class Defaults의 `ComputerClickAction`은 `/Game/Input/Actions/IA_ComputerClick`을 참조한다.
- Blueprint Event Graph에 pointer press/release 로직을 추가하지 않았다.

## WBP_ComputerSampleScreen

- native parent: `UComputerSampleScreenWidget`.
- Designer 기준 크기: `1024 x 576`.
- Event Graph node: 0개.
- exact hierarchy:

```text
RootOverlay (Overlay)
  ScreenPanel (Border)
    ScreenColumn (VerticalBox)
      ClickResultText (TextBlock)
      TestButton (Button)
        TestButtonLabel (TextBlock)
```

- `ClickResultText` 초기 문자열: `버튼을 클릭하세요`.
- `TestButtonLabel` 문자열: `테스트 클릭`.
- `TestButton`은 hit-test 가능한 `Visible` 상태다.
- required `BindWidget` 이름과 타입을 정확히 유지했고, click delegate와 runtime 문자열 변경은 native C++ 소유로 유지했다.

## BP_BathhouseComputer

- native parent: `ABathhouseComputerActor`.
- inherited component만 사용: `ComputerMesh`, `ScreenWidget`, `FocusCamera`.
- 대체/중복 component 및 Blueprint Event Graph 로직을 추가하지 않았다.

### ComputerMesh

- sample placeholder: `/Engine/BasicShapes/Cube.Cube`.
- relative scale: `(0.12, 1.2, 0.7)`.
- collision: `Query And Physics`, `Visibility = Block`.

### ScreenWidget

- Widget Class: `/Game/Bathhouse/UI/WBP_ComputerSampleScreen`.
- Space: `World`.
- Draw Size: `1024 x 576`.
- Geometry Mode: `Plane`.
- Pivot: `(0.5, 0.5)`.
- Receive Hardware Input: `false`.
- collision: `Query Only`, 다른 채널은 Ignore, `Visibility = Block`.
- relative location: `(51.6667, 0, 0)`.
- relative rotation: `(0, 0, 0)`.
- relative scale: `(0.833333, 0.083333, 0.142857)`.
- 위 relative 값은 root mesh의 비균일 scale을 상쇄한다. 실제 화면 offset은 monitor 전면 방향 약 `6.2 cm`, world scale은 약 `(0.1, 0.1, 0.1)`이다.

### FocusCamera

- relative location: `(1333.3333, 0, 14.2857)`.
- 저장된 relative rotation: `(Pitch 0, Yaw 180, Roll -180)`; 결과 view는 monitor 정면을 향한다.
- FOV: `70`.
- `FocusBlendInSeconds = 0.35`.
- `FocusBlendOutSeconds = 0.25`.
- root의 비균일 scale을 상쇄한 실제 camera offset은 monitor 정면 약 `160 cm`, 위쪽 `10 cm`다.

## DefaultMap 배치

- actor label: `Computer`.
- class: `BP_BathhouseComputer`.
- transform: Location `(-470, 0, 160)`, Rotation `(0, 0, 0)`.
- `Bathhouse_` 접두사를 사용하지 않았다.
- 동일 class actor는 정확히 1개다.
- player eye에서 computer 방향으로 실행한 `Visibility` trace가 약 `263.75 cm`에서 computer/screen 전면을 hit했다. 사이에 먼저 막는 장식 collision은 없었다.
- focus 중 camera 위치는 약 `(-310, 0, 170)`이며 screen과 약 `154 cm` 거리다.

## PIE 통합 검증

실제 Enhanced Input action injection으로 다음을 확인했다.

- 빈손 E 진입 후 movement가 `MOVE_None`, cursor가 visible, `ComputerWidgetInteraction` hit testing이 true가 됐다.
- focus view는 약 `(-310, 0, 170)`으로 전환됐고 world pause는 false였다.
- focus 중 `IA_Move`를 주입해도 pawn 위치가 변하지 않아 movement capture gate가 작동했다.
- 새 E Started로 focus-out하면 movement가 `MOVE_Walking`, cursor가 hidden, hit testing이 false가 되고 first-person view가 복구됐다.
- focus-out 후 재진입해도 runtime `WBP_ComputerSampleScreen` instance 경로가 같았다.
- runtime `TestButton.OnClicked` delegate를 실행하면 `ClickResultText`가 정확히 `클릭 확인`으로 바뀌었고, focus-out/re-entry 뒤에도 같은 값이 유지됐다.
- focus 중 computer actor를 파괴하면 first-person view, `MOVE_Walking`, cursor hidden, pointer hit testing false가 복구됐고 world pause는 false였다.
- 실행 중 fullscreen viewport widget 또는 world pause는 생성되지 않았다.

native focused/full automation 18/18 사전 결과로 다음 계약도 함께 충족된 상태다.

- key/wet mop/towel basket held gate와 held item 비변조.
- E Started/Completed ownership 및 primary interaction 누수 방지.
- focus-in/out 반복, pointer forced release, actor/player/controller cleanup.
- movement/look/world interaction suppression과 reservation 정리.

### 물리 마우스 클릭 확인 범위

- 현재 Editor는 `RenderOffscreen/Unattended` 세션이라 OS/Slate hardware cursor가 실제 viewport로 전달되지 않는다.
- 따라서 `Mouse` source의 화면 좌표 이동과 실제 Left Mouse Button으로 `TestButton`을 누르는 시각 검증은 자동화하지 못했다.
- 대신 action mapping/property, world widget collision, native click delegate, 문자열 갱신과 상태 유지까지 각각 확인했다.
- visible Editor에서 최종 수동 확인 시 빈손 E 진입 후 `테스트 클릭`을 Left Mouse Button으로 한 번 눌러 `클릭 확인`이 보이는지만 확인하면 된다.

## Compile, Data Validation, Save, Reload

- `WBP_ComputerSampleScreen`, `BP_BathhouseComputer`, `BP_FirstPersonCharacter`를 warnings-as-errors로 compile: 3/3 성공.
- `IA_ComputerClick`, `IMC_FirstPerson`, `WBP_ComputerSampleScreen`, `BP_BathhouseComputer`, `BP_FirstPersonCharacter` Data Validation: 5/5 `VALID`, asset error/warning 0.
- 의도한 5개 Content asset과 `DefaultMap` placement만 저장했다.
- 저장 후 target package를 디스크에서 reload하고 `DefaultMap`도 다시 load했다.
- reload 후 native parent, exact hierarchy, inherited component 이름, action assignment, widget class, collision, camera/blend 값과 actor 1개 placement가 모두 유지됐다.
- reload 후 Blueprint compile 3/3, Data Validation 5/5를 다시 통과했다.
- 최종 target content dirty package: 0개, dirty map: 0개.

## 최종 범위 확인

- 신규 영구 Content asset: 3개.
- 수정한 기존 Content asset: 3개(`IMC_FirstPerson`, `BP_FirstPersonCharacter`, `DefaultMap`).
- 신규 external actor package: 1개, 의도하지 않은 external actor: 0개.
- production computer mesh는 제공되지 않아 Engine Cube placeholder를 사용했다.
- Source, Config, StateTree graph, 기존 다른 map actor는 이번 Editor 단계에서 수정하지 않았다.
- 별도 `USER_UNREAL.md` 후속 문서는 필요하지 않다.
