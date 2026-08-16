# UI System

## Status And Scope

UI System은 BathhouseSim의 native C++ Widget과 Widget Blueprint 사이의 공통 책임 경계를 정의한다.

native UI Source는 primary/secondary action, 의도별 실패와 hold progress를 같은 interaction prompt widget에서 처리한다. world-space computer sample screen의 native base와 두 필수 BindWidget도 구현되어 있다.

```text
Source/BathhouseSim/Public/UI/
Source/BathhouseSim/Private/UI/
```

Current classes:

- `ABathhouseHUD`: local prompt 생성·제거와 possessed pawn context 연결
- `UInteractionPromptWidget`: primary/secondary query/result lifecycle, hold progress, transient failure timer와 필수 `BindWidget` 표시 갱신
- `UComputerSampleScreenWidget`: world monitor의 sample button lifecycle과 클릭 확인 표시

`Content/`의 Widget Blueprint와 UI asset은 serialized project data다. 명시적인 Editor 작업 없이 수정하거나 resave하지 않는다.

## Responsibilities

- native C++ Widget과 Widget Blueprint의 책임 경계 정의
- 런타임 상태, 외부 context와 delegate lifecycle 관리 기준 제공
- 입력, drag/drop, validation과 domain API routing 기준 제공
- 표시 데이터 변환과 Blueprint 표현 계약 정의
- root, slot, row, operation과 domain owner 사이의 책임 분리
- interaction prompt의 local lifecycle과 표현 계약
- primary E, secondary F와 hold progress의 분리 표시
- world-space monitor sample click과 actor lifetime 화면 상태 유지 계약

UI는 gameplay domain 상태를 소유하거나 domain mutation 규칙을 다시 구현하지 않는다. UI는 사용자 의도를 전달하고 결과를 표현하며, 실제 상태 변경은 해당 domain의 상태 owner가 수행한다.

## Native Widget Policy

다음 중 하나가 있으면 Editor 작업 전에 native C++ `UUserWidget` base를 구현한다.

- 런타임 상태 또는 외부 context 보관
- delegate 구독과 해제
- `NativeConstruct`, `NativeDestruct`, cancel 또는 end-play cleanup
- 입력 판단, drag/drop 또는 validation
- 표시 데이터 변환과 반복 가능한 분기
- gameplay domain API 호출

C++이 소유하는 범위:

- 상태와 lifecycle
- 입력 의도와 drag/drop routing
- domain API 호출과 실패 처리
- 표시용 데이터 가공
- Blueprint에 전달할 최소 event와 API

Widget Blueprint가 소유하는 범위:

- widget hierarchy와 `BindWidget` 대상 배치
- layout, style와 animation
- icon, font, material과 기타 표현 asset 연결
- C++이 갱신한 상태에 반응하는 선택적 시각 이벤트

Blueprint-only Widget은 상태, delegate, 입력 또는 domain API 호출이 없는 표현 전용일 때만 허용한다. 표현 전용 Widget에 의미 없는 native class를 추가하지 않는다.

## Responsibility Split

root widget 하나에 모든 책임을 모으지 않는다.

- root widget: 화면 단위 context 주입과 하위 widget 조립
- slot 또는 row widget: 단일 항목 context와 표시, 해당 항목의 입력 의도 전달
- operation object: drag payload처럼 한 상호작용 동안 유지되는 임시 상태
- domain component 또는 subsystem: 실제 gameplay 상태와 mutation 규칙
- presentation-only child widget: 전달받은 표시 데이터의 layout과 style

Blueprint에서 동적으로 row를 생성하는 것은 표현 데이터 렌더링 범위에서 허용한다. Blueprint row는 domain 규칙이나 런타임 상태의 owner가 되어서는 안 된다.

## Lifecycle And Delegate Policy

- delegate를 구독한 native Widget은 대칭적인 해제 경로를 가진다.
- 재구성, 화면 닫기, drag cancel, owner 교체와 end play에서 임시 상태와 외부 참조를 정리한다.
- 초기화 API는 필요한 context를 명시적으로 주입하고, 초기화 전 호출과 중복 초기화를 안전하게 처리한다.
- Widget이 world actor나 provider를 반복 검색하지 않는다. Controller, component 또는 상위 root가 필요한 context를 resolve해 주입한다.
- 표시 값이 변하지 않았으면 불필요한 Blueprint event나 전체 UI rebuild를 반복하지 않는다.

## Input And Drag/Drop Policy

- C++은 입력 버튼, drag mode, source와 target context, payload 유효성을 판정한다.
- drag operation은 상호작용 중 필요한 payload와 preview 상태만 소유한다.
- drop routing은 source/target 조합을 판정하되 실제 domain mutation은 상태 owner API에 위임한다.
- drop 성공, 실패와 cancel 모두 source preview와 active operation을 정리하는 경로를 가진다.
- partial preview나 수량 표시가 있으면 source, drag visual과 실제 결과의 일관성을 검증한다.
- quick action이나 target 선택 규칙이 domain 정책으로 성장하면 UI에서 domain owner로 이동한다.

## Presentation Data Policy

- C++은 domain 값을 표시 전용 값과 구조로 변환하고 Blueprint에는 최소 데이터만 전달한다.
- normalize, format, validation과 반복 가능한 상태 분기는 C++에 둔다.
- layout, 정렬, 색상, disabled style과 animation은 Widget Blueprint가 담당한다.
- runtime 위치 계산이나 viewport clamp처럼 상태에 의존하는 계산은 C++ 책임으로 둘 수 있고, 실제 hierarchy와 시각 반응은 Blueprint가 담당한다.
- row 내부 widget 이름과 구체 UMG hierarchy는 필요하지 않은 한 C++ `BindWidget` 계약으로 강제하지 않는다.

## Interaction Prompt

- `ABathhouseHUD`는 local player에서 `UInteractionPromptWidget`을 생성해 viewport에 추가한다.
- HUD는 possessed pawn의 `UPlayerInteractionComponent`를 resolve해 widget에 주입하고 pawn 교체 시 재연결한다.
- `UInteractionPromptWidget`은 query delegate를 구독·해제하고 현재 prompt data를 cache한다.
- widget은 C++ 전용 interaction attempt result delegate도 `FDelegateHandle`로 구독하며 component 교체, null context와 destruct에서 대칭 해제한다.
- native construct 뒤 initial query가 empty여도 bound widget 상태를 정확히 한 번 적용하며, unpossession 또는 null context 주입 시 즉시 hidden/empty presentation으로 지운다.
- native widget은 표시 여부, 대상명, primary/secondary 실행 가능 여부·행동명·실패 이유와 hold progress를 직접 bound widget에 적용한다.
- 기존 필수 계약 `PromptRoot`, `TargetNameText`, `ActionNameText`, `FailureReasonText`는 유지하며 `ActionNameText`/`FailureReasonText`를 primary E row로 사용한다.
- target 확장 필수 계약은 `SecondaryActionNameText: UTextBlock`, `SecondaryFailureReasonText: UTextBlock`, `InteractionProgressBar: UProgressBar`다.
- primary/secondary input label은 각각 E/F presentation 값이며 domain action name에 hard-code하지 않는다.
- query 실패 이유는 query가 유지되는 동안 지속 표시하고, 실행 실패와 대상 없음 결과는 기본 1.5초 동안 transient failure로 우선 표시한다.
- `FailureDisplayDurationSeconds`는 `EditDefaultsOnly`, 최소 0.1초인 presentation authoring 값이다.
- transient failure는 실패 result마다 timer를 재시작하며 성공 result, query 변경, context 해제, timer 만료와 widget destruct에서 지운다.
- query가 없어도 transient failure가 있으면 root를 표시하고 target/action text는 비운다. root enabled 상태는 transient failure가 아니라 현재 visible primary/secondary 중 하나라도 실행 가능한지를 따른다.
- `PromptRoot`와 기존 primary bindings의 visibility/enabled/text, effective failure 우선순위와 timer는 C++이 갱신한다.
- secondary row visibility/enabled/text, hold progress visibility/percent와 intent별 transient failure도 C++이 갱신한다.
- Widget Blueprint는 정확한 `BindWidget` 이름의 hierarchy, crosshair 주변 layout, style와 선택적 animation만 담당하며 Event Graph 없이도 정상 동작해야 한다.
- `OnInteractionPromptChanged`는 native 표시 적용 뒤 호환성과 선택적 표현 반응을 위해 호출하는 hook이며 prompt 정확성이 이 event 구현에 의존하지 않는다.
- 기존 `OnInteractionPromptChanged` signature는 primary 호환 hook으로 유지하고 secondary/progress용 새 표현 hook을 추가한다. 기존 reflected event signature를 변경하지 않는다.
- G equipment drop 실패는 target query와 별개인 transient attempt reason으로 기존 failure 영역에 표시한다.
- held key number는 HUD text가 아니라 first-person 3D key actor에 표시한다.
- inventory, hotbar, item slot과 money HUD는 현재 범위에 포함하지 않는다.
- towel count HUD를 별도로 만들지 않으며 basket/stack/machine world presentation과 target prompt만 사용한다.

## Computer Sample Screen

- `ABathhouseComputerActor`의 `UWidgetComponent`가 World Space로 Widget Blueprint instance를 생성하고 유지한다.
- `UComputerSampleScreenWidget`은 `TestButton: UButton`, `ClickResultText: UTextBlock` 두 필수 `BindWidget`만 요구한다.
- C++은 construct/destruct의 대칭 button delegate, 클릭 여부와 text 갱신을 소유한다.
- 초기 text는 `버튼을 클릭하세요`, 클릭 뒤 text는 `클릭 확인`이다.
- Widget Blueprint는 screen hierarchy, layout, style와 animation만 담당하며 Event Graph 없이 클릭 확인이 동작해야 한다.
- player의 `UWidgetInteractionComponent`가 mouse-source pointer를 전달한다. Widget의 hardware input 수신을 동시에 사용하지 않는다.
- focus-out은 widget을 remove/recreate하지 않는다. 같은 computer Actor lifetime 동안 마지막 화면 상태를 유지하고 재진입 시 그대로 표시한다.
- sample 클릭 여부는 표현 확인용 local widget state다. 실제 gameplay computer 데이터가 생기면 별도 Computer domain owner가 소유하고 Widget에는 표시값만 전달한다.

## Blueprint/API Contracts

- Blueprint가 사용해야 하는 API와 event만 `BlueprintCallable`, `BlueprintPure` 또는 `BlueprintImplementableEvent`로 노출한다.
- C++ 내부 연결용 함수는 불필요하게 Blueprint에 노출하지 않는다.
- `BindWidget`은 native 로직에 반드시 필요한 최소 widget에만 사용한다.
- 신규 secondary/progress widget은 runtime 정확성에 필요하므로 `BindWidget`으로 요구하고 WBP migration 단계에서 hierarchy를 함께 갱신한다.
- computer sample의 button/text도 native 동작에 필요하므로 필수 `BindWidget`이며 정확한 이름과 타입으로 WBP에 구성한다.
- Blueprint event는 domain object 전체보다 표현에 필요한 data를 전달한다.
- native parent, reflected type, function, property 또는 component 이름은 Content asset 계약으로 취급한다.
- 공개 Blueprint 계약을 rename하거나 제거할 때는 asset migration, Core Redirect 필요 여부, Blueprint compile/save와 post-migration scan을 함께 계획한다.
- legacy wrapper는 migration 기간에만 유지하고 새 Blueprint가 사용할 대체 API와 제거 조건을 문서화한다.

## Dependencies

- UI System은 필요한 gameplay domain의 public API에 의존할 수 있다.
- 현재 interaction prompt UI는 Interaction System에만 의존한다.
- computer sample widget은 UMG에만 의존하고 gameplay domain mutation을 수행하지 않는다.
- UI는 Cleaning/Towel concrete class를 판별하지 않고 확장된 Interaction query/result만 소비한다.
- gameplay domain은 구체 Widget class에 의존하지 않는다. 필요하면 event, interface 또는 presentation data 경계를 사용한다.
- `UMG`, `Slate`, `SlateCore` 같은 모듈 의존성은 실제 Source include와 사용처가 생길 때만 `BathhouseSim.Build.cs`에 추가한다.
- UI가 다른 시스템의 상태를 복제해 별도 정본으로 만들지 않는다.

## Manual Review Points

- 새 Widget Blueprint가 runtime 상태, delegate, 입력 또는 domain API 호출을 Event Graph에 소유하지 않는지 확인한다.
- C++ root widget에 slot, row, operation과 domain mutation 책임이 다시 집중되지 않는지 확인한다.
- 화면을 반복해서 열고 닫거나 owner를 교체해도 delegate 중복 구독과 stale reference가 남지 않는지 확인한다.
- drag/drop 성공, 실패와 cancel 뒤 preview와 active operation이 복구되는지 확인한다.
- 같은 표시 상태에서 Blueprint event나 rebuild가 불필요하게 반복되지 않는지 확인한다.
- 기존 네 `BindWidget`과 신규 세 `BindWidget`의 이름과 타입이 `WBP_InteractionPrompt`에서 정확히 일치하는지 확인한다.
- 신규 secondary 두 TextBlock과 ProgressBar의 이름/타입이 일치하고 E/F row가 독립적으로 표시되는지 확인한다.
- `WBP_InteractionPrompt` Event Graph가 visibility, text, 실패 표시 또는 실행 가능 판단을 다시 구현하지 않는지 확인한다.
- 실패 result 연속 수신 시 1.5초 timer가 재시작되고 query 변경·성공·context 해제·destruct에서 stale failure가 남지 않는지 확인한다.
- transient execution failure가 query failure보다 우선하며 timer 만료 뒤 현재 query failure로 복귀하는지 확인한다.
- native class와 reflected API 변경 시 관련 Widget Blueprint를 compile/save하고 참조 오류를 검사한다.
- layout, style, animation과 asset 연결이 C++으로 불필요하게 이동하지 않았는지 확인한다.
- pawn 교체와 HUD 종료에서 query/result delegate가 정확히 한 번 해제되는지 확인한다.
- focus, held key와 target 상태 변화에 따라 prompt가 즉시 갱신되는지 확인한다.
- hold cancel/completion, towel capacity/state, machine state와 individual overflow towel에 따라 primary/secondary failure가 즉시 갱신되는지 확인한다.
- computer sample WBP의 `TestButton`, `ClickResultText` 이름/타입과 Event Graph 부재를 확인한다.
- focus-out/re-entry에서 widget instance와 `클릭 확인` 상태가 유지되고 actor/level 종료에서만 수명이 끝나는지 확인한다.
