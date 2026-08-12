# Unreal Prompt — Towel Presentation Authoring After Contract Rework

## Status

**C++ collision 재작업과 read-only Blueprint compile 완료, profile/layout Content authoring과 PIE 검증 필요.**

모든 새 inherited native presentation component의 exact 이름은 `TowelPresentationVisual`이다. Stack actor에서는 `UTowelStackVisualComponent`, washer/dryer에서는 `UTowelPileVisualComponent` 타입이다.

## Native Preflight

1. 모든 BathhouseSim Editor와 Live Coding을 닫는다.
2. UE 5.8 `Build.bat`으로 `BathhouseSimEditor Win64 Development -WaitMutex -NoHotReloadFromIDE`를 빌드한다.
3. Editor를 새로 열고 `LogBlueprint: Error`, duplicate property, missing native property/default subobject가 없는지 확인한다.
4. C++ 단계 증거는 build 성공, target Blueprint compile 0 error/0 warning, focused 1 success, 전체 17 success/0 fail이다.
5. native load/compiler 오류가 보이면 asset을 저장하지 말고 Editor를 닫아 코드 리뷰 단계로 돌린다.

## Native Contract

| Blueprint | Parent | Inherited component | Concrete type |
|---|---|---|---|
| `/Game/Bathhouse/Blueprints/Towel/BP_CleanTowelStack` | `ACleanTowelStackActor` | `TowelPresentationVisual` | Stack |
| `/Game/Bathhouse/Blueprints/Towel/BP_UsedTowelBin` | `AUsedTowelBinActor` | `TowelPresentationVisual` | Stack |
| `/Game/Bathhouse/Blueprints/Towel/BP_TowelBasket` | `ATowelBasketActor` | `TowelPresentationVisual` | Stack |
| `/Game/Bathhouse/Blueprints/Towel/BP_Washer` | `ATowelProcessingMachineActor` | `TowelPresentationVisual` | Pile |
| `/Game/Bathhouse/Blueprints/Towel/BP_Dryer` | `ATowelProcessingMachineActor` | `TowelPresentationVisual` | Pile |

기존 Blueprint symbol `StackVisual`, `BinVisual`, `MachineVisual`은 native 계약명이 아니다. 임의 rename하지 않는다.

## Create Mesh Profile Data Assets

`UTowelVisualMeshProfile` Data Asset을 `/Game/Bathhouse/Data/Towel/`에 만든다.

| Asset | State entries |
|---|---|
| `DA_TowelVisual_Shelf` | Clean |
| `DA_TowelVisual_UsedBin` | Used |
| `DA_TowelVisual_Basket` | Used, Wet, Clean |
| `DA_TowelVisual_Washer` | Used, Wet |
| `DA_TowelVisual_Dryer` | Wet, Clean |

- `None`/duplicate state와 null mesh를 넣지 않는다.
- 같은 mesh 중복은 가중치이므로 의도한 경우에만 사용한다.
- Content Browser Data Validation에서 다섯 profile 모두 error/warning 0을 확인한다.

## Configure Stack Blueprints

각 inherited `TowelPresentationVisual`에 profile, `BaseLocalOffset`, towel 두께에 맞는 `ZSpacing`, `RandomSeed`, `CountStepInterval`, `bAnimateInventoryChanges`를 설정한다.

| Blueprint | MeshProfile | Attachment contract |
|---|---|---|
| `BP_CleanTowelStack` | `DA_TowelVisual_Shelf` | native actor root |
| `BP_UsedTowelBin` | `DA_TowelVisual_UsedBin` | native actor root |
| `BP_TowelBasket` | `DA_TowelVisual_Basket` | native `WorldMesh` |

`BP_CleanTowelStack`의 기존 Blueprint `StackVisual` StaticMeshComponent는 native 계약과 다른 레거시 표현이다. 새 native profile/layout을 설정해 동등한 외형을 확인한 뒤 중복 표현이면 이 Editor 단계에서만 삭제하고 Compile한다. 삭제 전 mesh/material/relative transform을 기록하며, 다른 Blueprint의 `BinVisual`/`MachineVisual`은 삭제하지 않는다.

## Configure Washer And Dryer

각 inherited `TowelPresentationVisual`에 다음을 설정한다.

| Blueprint | MeshProfile | Required states |
|---|---|---|
| `BP_Washer` | `DA_TowelVisual_Washer` | Used -> Wet |
| `BP_Dryer` | `DA_TowelVisual_Dryer` | Wet -> Clean |

정확한 reflected property 이름:

- relative transform 또는 `BaseLocalOffset`
- `PileHalfExtent`
- `ItemsPerLayer`
- `LayerSpacing`
- `MaxZJitter`
- constrained rotation min/max
- `RandomSeed`, `CountStepInterval`, `bAnimateInventoryChanges`

machine kind/timer/transfer port/control/delegate는 수정하지 않는다. drum animation 포함 여부는 기존 Blueprint presentation hierarchy의 attachment로만 조정한다.

## Slot Boundary

- `UTowelSlotVisualComponent`를 gameplay actor에 추가하지 않는다.
- drying-rack actor/Blueprint, inventory, transfer, timer와 interaction을 만들지 않는다.
- existing `BP_DryingSpot`을 수정하지 않는다.

## Compile And Save

1. profile 다섯 개를 Data Validation한다.
2. target Blueprint 다섯 개를 개별 Compile한다.
3. compiler/load 오류가 0인 경우에만 의도한 asset을 저장한다.
4. Editor 재시작 후 inherited `TowelPresentationVisual`, profile reference와 layout 값이 유지되는지 확인한다.
5. 의도하지 않은 dirty asset은 저장하지 않는다.

예상 Content 변경은 profile Data Asset 5개와 target Blueprint 5개다. `BP_CleanTowelStack.StackVisual`을 실제로 제거했다면 completion report에 명시한다.

## PIE Acceptance

1. 다섯 actor의 초기 inventory 수량이 즉시 표시된다.
2. E/F transfer 후 visible count가 최신 inventory count로 수렴한다.
3. 기존 layer는 unrelated revision에서 mesh/transform이 바뀌지 않는다.
4. basket Used/Wet/Clean과 washer Used->Wet, dryer Wet->Clean state swap이 count/transform을 보존한다.
5. Pile은 drum bounds 안의 아래 layer부터 채워지고 같은 seed에서 재현된다.
6. instance가 collision/overlap/navigation/interaction trace에 참여하지 않는다.
7. basket carry/drop, machine process와 used-bin overflow가 기존과 동일하다.
8. `BP_CleanTowelStack`에 레거시 mesh와 native instance의 중복 표시가 없다.
9. PIE 종료/재시작 뒤 중복 instance, timer 또는 delegate 반응이 없다.

## Forbidden Blueprint Logic

- inventory count/state 재계산
- Event Graph ISM 생성/삭제
- native delegate bind lifecycle 대체
- machine/transfer gameplay 변경

## Completion Report

- 생성/수정/저장한 exact asset 경로
- profile state별 mesh와 component layout 최종값
- `BP_CleanTowelStack.StackVisual` 보존/삭제 결정과 이관 값
- Data Validation/Blueprint compile/재로드 결과
- PIE acceptance pass/fail과 실패 재현 절차
- 의도하지 않은 dirty asset 유무
