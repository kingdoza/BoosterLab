# Code Review Prompt — Towel Presentation Blueprint Contract Rework

## Review Objective

UE 5.8 Towel presentation 구현의 기존 `BP_CleanTowelStack.StackVisual` 이름 충돌을 해결한 재작업을 Editor authoring 전에 리뷰한다. presentation behavior와 gameplay authority는 유지하고 새 reflected/default-subobject 계약명과 Blueprint/compiler 검증만 바로잡았다.

기준 문서:

- `.md/AGENT_WORKFLOW.md`
- `.md/AGENT_REVIEW.md`
- `.md/0_ARCHITECTURE.md`
- `.md/Architecture/TowelSystem.md`
- `.md/Architecture/TowelPresentationSystem.md`
- `.md/Architecture/CoreSystem.md`
- `.md/PROMPT_IMPLEMENTATION.md`
- `.md/PROMPT_IMPLEMENTATION_R.md`

## Original Failure

이전 전체 automation은 17 success를 보고했지만 같은 startup 로그에 다음 Blueprint compiler error가 2건 있었다.

```text
BP_CleanTowelStack: Tried to create a property StackVisual ...
but another object (/Script/BathhouseSim.CleanTowelStackActor:StackVisual) already exists there.
```

기존 Blueprint SCS가 `StackVisual`/`StackVisual_GEN_VARIABLE`을 직렬화했고 새 native parent도 같은 이름을 선언해 skeleton/generated class property 생성이 충돌했다.

## Implemented Correction

네 actor의 새 native member와 default-subobject 이름을 공통 `TowelPresentationVisual`로 교체했다.

| Actor | Component type | Final reflected/default-subobject name |
|---|---|---|
| `ACleanTowelStackActor` | `UTowelStackVisualComponent` | `TowelPresentationVisual` |
| `AUsedTowelBinActor` | `UTowelStackVisualComponent` | `TowelPresentationVisual` |
| `ATowelBasketActor` | `UTowelStackVisualComponent` | `TowelPresentationVisual` |
| `ATowelProcessingMachineActor` | `UTowelPileVisualComponent` | `TowelPresentationVisual` |

- 각 actor는 exactly one native presentation component를 소유한다.
- BeginPlay에는 자기 `Inventory`만 bind하고 EndPlay에는 unbind한다.
- profile/revision/count animation/ISM/Stack/Pile/Slot behavior는 변경하지 않았다.
- exact default-subobject name automation assertion도 최종 이름으로 변경했다.

## Read-Only Asset Symbol Inspection

다섯 target `.uasset`의 serialized strings를 저장 없이 검사했다.

| Asset | Existing relevant symbols | `TowelPresentationVisual` collision |
|---|---|---|
| `BP_CleanTowelStack` | `StackVisual`, `StackVisual_GEN_VARIABLE` | 없음 |
| `BP_UsedTowelBin` | `BinVisual`, `BinVisual_GEN_VARIABLE` | 없음 |
| `BP_TowelBasket` | 별도 custom visual symbol 없음 | 없음 |
| `BP_Washer` | `MachineVisual`, `MachineVisual_GEN_VARIABLE` | 없음 |
| `BP_Dryer` | `MachineVisual`, `MachineVisual_GEN_VARIABLE` | 없음 |

## Changed Files

Source contract/test:

- `Source/BathhouseSim/Public/Towel/CleanTowelStackActor.h`
- `Source/BathhouseSim/Private/Towel/CleanTowelStackActor.cpp`
- `Source/BathhouseSim/Public/Towel/UsedTowelBinActor.h`
- `Source/BathhouseSim/Private/Towel/UsedTowelBinActor.cpp`
- `Source/BathhouseSim/Public/Towel/TowelBasketActor.h`
- `Source/BathhouseSim/Private/Towel/TowelBasketActor.cpp`
- `Source/BathhouseSim/Public/Towel/TowelProcessingMachineActor.h`
- `Source/BathhouseSim/Private/Towel/TowelProcessingMachineActor.cpp`
- `Source/BathhouseSim/Private/Tests/TowelPresentationAutomationTests.cpp`

Canonical/handoff:

- `.md/0_ARCHITECTURE.md`
- `.md/Architecture/TowelSystem.md`
- `.md/Architecture/TowelPresentationSystem.md`
- `.md/PROMPT_REVIEW.md`
- `.md/PROMPT_UNREAL.md`

## Blueprint/API/Core Redirect

- 기존 reflected gameplay API와 Blueprint SCS component를 rename/delete하지 않았다.
- 폐기한 `StackVisual`/`PileVisual` native 이름은 Editor authoring을 통과하거나 Content에 저장된 계약이 아니므로 Core Redirect를 추가하지 않았다.
- redirect로 기존 live `BP_CleanTowelStack.StackVisual`을 흡수하면 잘못된 migration이 되므로 직접 collision-free 이름을 사용한다.
- `Config/`, `.uproject`, `BathhouseSim.Build.cs`는 변경하지 않았다.
- `Content/`는 수정하거나 resave하지 않았다.

## Verification Evidence

- UE 5.8 `BathhouseSimEditor Win64 Development` 정식 빌드 성공: 9 actions, compile/link/UHT error 0.
- allow-list `CompileAllBlueprints` read-only pass가 다섯 target asset을 각각 load/compile했다.
- Blueprint compile 결과: 0 errors, 0 warnings, 0 failed loads.
- compile 로그의 `LogBlueprint: Error`, `Internal Compiler Error`, `another object`, missing native parent/property/default subobject: 0건.
- focused `BathhouseSim.Towel.Presentation.StackPileSlotAndLifecycle`: 1 success, 0 fail, exit code 0.
- full `BathhouseSim`: 17 success, 0 fail, exit code 0.
- focused/full startup 로그의 동일 compiler 금지 진단: 각각 0건.
- 다섯 target asset의 실행 전/후 SHA-256 일치; Content 저장 없음.

## Review Focus

1. 네 UPROPERTY 이름과 `CreateDefaultSubobject` 이름이 모두 exact `TowelPresentationVisual`인가?
2. 기존 `StackVisual`, `BinVisual`, `MachineVisual` Blueprint symbol을 native code가 침범하지 않는가?
3. actor별 Stack/Pile concrete type과 attachment, bind/unbind lifecycle이 재작업 전과 같은가?
4. 테스트가 component type 탐색뿐 아니라 exact default-subobject 이름을 검증하는가?
5. Core Redirect 없음과 Content 무변경 판단이 asset inspection 결과에 부합하는가?
6. `.md/PROMPT_UNREAL.md`가 exact `TowelPresentationVisual`과 실제 pile property `MaxZJitter`를 사용하는가?
7. test count와 별도로 Blueprint/startup compiler 로그 scan이 증거에 포함됐는가?

## Out Of Scope

- inventory/transfer/machine authority 또는 presentation algorithm 변경
- drying-rack gameplay/asset
- 이번 코드 재작업 중 Blueprint 삭제, rename 또는 resave
- 신규 module/config/redirect

## Review Output

- finding은 severity와 exact file/line 근거로 작성한다.
- 문제가 없으면 코드 단계 승인 후 `.md/PROMPT_UNREAL.md`를 Editor 단계로 인계한다.
- Source 문제가 있으면 `.md/PROMPT_IMPLEMENTATION_R.md`에 최소 재작업 범위와 compiler-log 회귀 검증을 작성한다.
