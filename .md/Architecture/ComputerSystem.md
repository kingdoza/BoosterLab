# Computer System

## Implementation Status

이 문서는 월드 모니터 기반 컴퓨터 상호작용의 현재 native 구현을 정의한다. Computer Actor, player session component, interaction suppression, sample widget과 focused automation은 Source에 구현되었고 Blueprint/Editor 연결은 후속 단계다.

이번 범위는 컴퓨터 한 대의 포커스 진입/이탈과 월드 `UWidgetComponent`에 표시되는 클릭 확인용 샘플 화면이다. 범용 운영체제, 프로그램 목록과 저장 시스템은 포함하지 않는다.

## Source Scope

```text
Source/BathhouseSim/Public/Computer/
  BathhouseComputerActor.h
  PlayerComputerUseComponent.h

Source/BathhouseSim/Private/Computer/
  BathhouseComputerActor.cpp
  PlayerComputerUseComponent.cpp

Source/BathhouseSim/Public/UI/
  ComputerSampleScreenWidget.h

Source/BathhouseSim/Private/UI/
  ComputerSampleScreenWidget.cpp
```

focused native automation은 `Source/BathhouseSim/Private/Tests`에 둔다.

## Responsibilities

- 월드 컴퓨터 mesh, world-space screen과 focus camera 조립
- 한 명의 유효한 local player에 대한 컴퓨터 사용권 예약/해제
- 빈손일 때만 사용 가능한 primary interaction query/execute
- 컴퓨터 사용 단계, 이전 view target, 이동 모드, 커서와 pointer lifecycle 관리
- 일반 1인칭/월드 interaction과 컴퓨터 pointer input의 상호 배타적 routing
- 포커스아웃 뒤 actor lifetime 동안 screen widget instance와 표시 상태 유지
- 컴퓨터 또는 player EndPlay와 focus transition 중단 시 안전한 복구

Computer는 physical carry state, 기존 interaction target 선정, 일반 이동/sprint 상태와 sample widget hierarchy를 소유하지 않는다.

## Responsibility Change

| 대상 | 기존 책임 | 신규 책임 | 최종 판단 |
|---|---|---|---|
| `AFirstPersonCharacter` | component 조립과 input intent routing | computer component/widget interaction 조립, 입력 mode 분기 | 세션 상태는 추가하지 않고 composition/routing만 확장 |
| `UPlayerInteractionComponent` | world focus와 E/F/G lifecycle | 외부 focus mode 동안 query/hold 억제 | 도메인 판정 없이 suppression 계약만 확장 |
| `ABathhouseComputerActor` | 없음 | 월드 표현, focus camera, use reservation | 신규 Actor가 컴퓨터 한 대의 composition root |
| `UPlayerComputerUseComponent` | 없음 | player별 focus/input/view session | lifecycle과 임시 상태가 응집되므로 신규 Component |
| `UComputerSampleScreenWidget` | 없음 | 클릭 연결과 표시용 클릭 확인 상태 | Native Widget policy에 따라 신규 C++ base |

범용 program manager, computer app base와 별도 screen state UObject는 현재 단일 샘플 화면에 필요하지 않아 만들지 않는다. 실제 gameplay 데이터가 추가되면 Widget 밖의 Computer domain owner로 분리한다.

## `ABathhouseComputerActor`

native default subobject:

- `ComputerMesh: UStaticMeshComponent`
- `ScreenWidget: UWidgetComponent`
- `FocusCamera: UCameraComponent`

Actor는 `IPlayerInteractable`을 직접 구현한다.

- `BeginPlay`에서 `ScreenWidget->InitWidget()`을 한 번 보장하고 실제 user widget 생성 여부를 사용 가능 조건에 포함한다.
- `QueryInteraction`은 carry context가 유효하고 `IsHandEmpty()`이며 screen/focus camera가 사용 가능하고 다른 user가 없을 때 `컴퓨터 사용`을 허용한다.
- 손에 key, wet mop 또는 towel basket이 있으면 `손에 든 물건을 내려놓아야 합니다`를 반환한다.
- `ExecuteInteraction`은 같은 조건을 다시 검증하고 interactor의 `UPlayerComputerUseComponent`에 시작을 요청한다.
- Actor reservation과 player session 시작은 한 transaction처럼 처리한다. session 시작 실패 시 reservation을 원복한다.
- current user identity를 검증한 release만 허용해 중복 종료나 다른 player의 해제를 막는다.
- `EndPlay`는 current user component에 actor unavailable을 통지하고 reservation을 지운다.

`FocusBlendInSeconds`, `FocusBlendOutSeconds`는 음수가 아닌 Editor authoring 값이며 native 기본값은 각각 `0.35`, `0.25`초다. `FocusCamera`의 relative transform과 FOV는 monitor를 정면 중앙에 두되 주변 world가 보이도록 Blueprint class/instance에서 조정한다.

## `UPlayerComputerUseComponent`

player별 authoritative transient session owner다. 내부 phase는 `Inactive`, `FocusingIn`, `Active`, `FocusingOut`을 사용한다.

시작 전 검증:

- owner가 locally controlled pawn이고 유효한 player controller를 가진다.
- active computer/session이 없다.
- `UPlayerCarryComponent`가 빈손이다.
- target reservation이 성공한다.

진입 시 다음을 한 commit/rollback 경계에서 수행한다.

1. 이전 view target, movement mode/custom mode와 cursor 표시값을 저장한다.
2. sprint와 jump를 끝내고 movement를 즉시 정지한 뒤 `MOVE_None`으로 전환한다.
3. `UPlayerInteractionComponent`를 suppress해 active hold를 조용히 한 번 cancel하고 prompt/query를 지운다.
4. computer Actor를 `SetViewTargetWithBlend` 대상으로 설정한다.
5. blend가 끝나면 `Active`로 전환하고 mouse cursor, `FInputModeGameAndUI`와 widget pointer hit testing을 활성화한다.
6. UE 5.8의 CameraComponent에는 view별 AA method override가 없으므로, active computer focus 동안 태그가 붙은 `r.AntiAliasingMethod=FXAA` override를 적용해 world-widget의 TSR history 잔상을 피한다.

focus-in 중 E를 다시 누르면 focus-out으로 전환할 수 있다. pointer click은 `Active`에서만 허용한다.

종료 시 pointer press가 남았다면 release하고 hit testing과 cursor를 먼저 끈다. focus용 AA override를 태그로 unset해 이전 project/runtime 값을 복구하고, 저장한 view target으로 blend한 뒤 movement mode, 1인칭 입력과 interaction query를 복구하고 expected user reservation을 해제한다. blend가 0이면 같은 frame에 완료한다.

컴퓨터 또는 player/controller가 유효하지 않게 되면 timer와 pointer state를 정리하고 가능한 player pawn/view target, `FInputModeGameOnly`, 저장된 movement mode와 interaction을 즉시 복구한다. begin/end/unavailable은 반복 호출해도 상태를 복제하거나 잠금을 남기지 않는다.

## Input Routing

`AFirstPersonCharacter`는 `UPlayerComputerUseComponent`와 `UWidgetInteractionComponent`를 default subobject로 조립한다. Widget interaction은 평상시에 hit testing을 하지 않는다.

- E Started: computer phase가 `Inactive`가 아니면 focus-out intent로 소비하고, 아니면 기존 primary begin으로 전달한다.
- E Completed/Canceled: computer가 현재 press를 소유하면 소비한다. 진입에 사용한 E release가 즉시 focus-out을 일으키지 않는다.
- `ComputerClickAction` Started/Completed/Canceled: `Active`일 때만 left pointer press/release로 전달한다.
- Move/Look/Jump/Sprint/F/G: computer session이 input을 capture하는 동안 Character의 input-facing handler와 공개 `DoMove`/`DoLook`/`DoJumpStart` 경로에서 domain 호출 전에 차단한다.

전체 `DisableInput()`은 종료 E와 pointer 입력까지 막으므로 사용하지 않는다. 기존 mapping context는 교체하지 않으며 `ComputerClickAction`을 같은 `IMC_FirstPerson`에 연결한다.

## Interaction Suppression

`UPlayerInteractionComponent`는 C++ 전용 `SetInteractionSuppressed(bool)`과 조회 API를 제공한다.

- suppress 시작은 active hold를 transient failure 없이 정확히 한 번 cancel하고 focus/current query를 empty로 commit한다.
- suppress 중 Tick trace와 E/F/G public attempt는 target/domain mutation을 하지 않는다.
- suppress 해제는 즉시 `RefreshInteractionQuery()`를 실행한다.
- Interaction은 Computer concrete class를 include하거나 active computer state를 직접 판정하지 않는다.

Character의 정상 input gate가 1차 경계이고 suppression은 stale prompt, active hold와 외부 API 호출을 막는 2차 경계다.

## Screen And UI Contract

`ScreenWidget`은 World Space로 표시되고 `UWidgetInteractionComponent`의 mouse source/hit test를 받는다. `bReceiveHardwareInput` 방식과 동시에 사용하지 않아 중복 클릭을 막는다.

`UComputerSampleScreenWidget`은 다음 필수 `BindWidget`만 가진다.

- `TestButton: UButton`
- `ClickResultText: UTextBlock`

native C++은 construct/destruct의 대칭 delegate 연결, 클릭 여부와 text 갱신을 소유한다. Widget Blueprint는 hierarchy, layout, style와 선택적 animation만 소유한다. 초기 표시는 `버튼을 클릭하세요`, 클릭 뒤 표시는 `클릭 확인`이다.

focus-out은 `ScreenWidget`이나 user widget을 remove/recreate하지 않는다. 따라서 마지막 클릭 상태는 같은 computer Actor lifetime 동안 유지되고 재진입 시 그대로 보인다. Actor 파괴 또는 level reload 뒤의 영속 저장은 현재 범위 밖이다.

## Dependencies

- Character -> Computer
- Computer -> Interaction public query/carry 계약
- Computer -> Engine Camera/CharacterMovement/PlayerController
- Computer -> UMG `UWidgetComponent`, `UWidgetInteractionComponent`
- UI sample widget -> UMG
- Interaction은 Computer concrete type에 의존하지 않는다.

현재 `UMG`, `InputCore`와 `EnhancedInput` module dependency로 구현한다. direct Slate API 사용처가 없으므로 `Slate`, `SlateCore`를 추가하지 않는다.

## Blueprint/API Contracts

신규 reflected 계약:

- `ABathhouseComputerActor` native parent와 `ComputerMesh`, `ScreenWidget`, `FocusCamera`
- `AFirstPersonCharacter::PlayerComputerUse`, `ComputerWidgetInteraction`, `ComputerClickAction`
- `UComputerSampleScreenWidget` native parent와 `TestButton`, `ClickResultText` BindWidget
- computer focus camera/blend와 widget interaction distance/debug authoring 값

기존 reflected symbol을 rename/delete하지 않으므로 Core Redirect는 필요하지 않다. Content asset 생성과 assignment는 Unreal 단계에서 수행한다. 현재 Content scan에는 computer/monitor asset과 `IA_ComputerClick`이 없고 기존 player/input asset만 존재한다.

## Out Of Scope

- fullscreen viewport UI와 game pause
- 범용 desktop, app window, keyboard text input와 scroll
- 계정, 결제, 경제 또는 저장 데이터
- 여러 player의 network replication
- level reload를 넘는 monitor 상태 저장
- 컴퓨터 사용 중 held item 자동 drop 또는 보관

## Manual Review Points

- 빈손일 때만 진입하며 실패 query와 execute가 동일한 이유를 반환하는지 확인한다.
- 진입에 사용한 E release는 유지되고 새로운 E Started만 focus-out을 시작하는지 확인한다.
- focus-in/out과 반복 E에서도 reservation, timer와 input lock이 정확히 한 번 정리되는지 확인한다.
- 사용 중 Move/Look/Jump/Sprint/F/G와 world trace가 mutation을 만들지 않는지 확인한다.
- mouse press/release와 focus-out 강제 release가 button stuck 또는 double click을 만들지 않는지 확인한다.
- monitor가 정면으로 보이면서 주변 목욕탕이 viewport에 남는지 플레이 테스트한다.
- focus-out/re-entry 뒤 `클릭 확인` 상태가 유지되고 widget construct가 반복되지 않는지 확인한다.
- active focus에서는 FXAA가 적용되고 정상 종료와 강제 cleanup 뒤에는 진입 전 AA method가 복구되는지 확인한다.
- computer/player EndPlay 뒤 view target, cursor, input mode, movement와 interaction prompt가 복구되는지 확인한다.
- world/NPC simulation이 computer 사용 중 pause되지 않는지 확인한다.
