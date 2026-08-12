# Implementation Prompt — Towel Stack, Pile And Slot Presentation

## 목적

이미 구현된 Towel inventory/transfer와 기존 washer/dryer를 변경하지 않고 towel quantity world presentation을 native C++로 구현한다.

- Stack runtime 연결: `ACleanTowelStackActor`, `AUsedTowelBinActor`, `ATowelBasketActor`
- Pile runtime 연결: 기존 `ATowelProcessingMachineActor` 하나를 통해 `BP_Washer`, `BP_Dryer`
- Slot: component/API/reference validation/Editor preview까지만 구현, gameplay actor에는 연결하지 않음

기준 문서:

- `.md/AGENT_WORKFLOW.md`
- `.md/AGENT_IMPLEMENTATION.md`
- `.md/0_ARCHITECTURE.md`
- `.md/Architecture/TowelSystem.md`
- `.md/Architecture/TowelPresentationSystem.md`
- `.md/Architecture/CoreSystem.md`

이 파일은 이전 Cleaning/Towel gameplay 구현 프롬프트를 전부 대체한다.

## 기존 상태

다음을 신규 구현하지 않는다.

- `UTowelInventoryComponent`, transfer/circulation subsystem
- `ATowelProcessingMachineActor`
- `BP_Washer`, `BP_Dryer`
- clean stack, used bin, basket와 overflow actor

기존 `OnInventoryChanged` snapshot의 state/count/capacity/revision을 presentation 입력으로 사용한다. Source/Content의 기존 reflected type/property/event를 rename/delete하지 않는다.

## 수용 기준

- authoritative towel state/count/revision은 기존 inventory만 소유한다.
- visual component는 snapshot을 읽고 domain mutation을 수행하지 않는다.
- 상태별 Data Asset은 필요한 state entry만 가지며 null/None/duplicate state를 안전하게 검증한다.
- Stack은 pivot local +Z로 일정 간격, Pile은 bounded lower-first layered random, Slot은 ordered authored reference transform을 사용한다.
- 새 visual index 생성 시 state 후보 중 mesh 하나를 random 선택하고 기존 index는 unrelated count update에서 재추첨하지 않는다.
- stale revision을 무시하고 rapid bulk count change/rebind 후 최신 snapshot으로 수렴한다.
- state conversion은 count/layout transform을 유지하고 mesh profile만 교체한다.
- runtime mesh instance는 collision, overlap, navigation과 interaction trace에 영향을 주지 않는다.
- 세 Stack Actor와 기존 processing machine만 inventory에 bind한다.
- Slot은 어떤 inventory/gameplay actor에도 bind하지 않고 drying-rack Actor/asset을 만들지 않는다.

## Source Target

신규:

```text
Source/BathhouseSim/Public/Towel/Presentation/
  TowelVisualTypes.h
  TowelVisualMeshProfile.h
  TowelQuantityVisualComponent.h
  TowelStackVisualComponent.h
  TowelPileVisualComponent.h
  TowelSlotVisualComponent.h

Source/BathhouseSim/Private/Towel/Presentation/
  # matching cpp
```

수정:

- `Towel/CleanTowelStackActor.h/.cpp`
- `Towel/UsedTowelBinActor.h/.cpp`
- `Towel/TowelBasketActor.h/.cpp`
- `Towel/TowelProcessingMachineActor.h/.cpp`
- focused native automation tests

새 module dependency는 추가하지 않는다. `Content/`, `Config/`와 `.uproject`는 수정하지 않는다.

## 1. Mesh Profile

`FTowelStateMeshVariants`는 `ETowelState State`와 `TArray<TObjectPtr<UStaticMesh>> MeshVariants`를 가진다.

`UTowelVisualMeshProfile : UDataAsset`은 `StateVariants` 배열을 노출한다.

- `None` entry와 duplicate state는 validation 오류
- null candidate 제외
- 유효 0개는 warning/표현 없음, 1개는 random 호출 없음, 여러 개는 배열 entry 균등 random
- 동일 mesh 중복 entry는 가중치로 유지
- lookup은 매 layer 생성마다 배열 전체를 불필요하게 재가공하지 않도록 validation/cache 경계를 둔다.

## 2. Common Quantity Visual

`UTowelQuantityVisualComponent : USceneComponent`는 abstract common base다.

- explicit `BindInventorySource`/`UnbindInventorySource`
- `SetTargetPresentation(State, Count, Revision, bAnimate)`
- `SynchronizeImmediately`
- target/displayed state/count와 applied revision
- step timer, seeded random stream, ordered layer records
- unique mesh asset별 transient `UInstancedStaticMeshComponent` bucket

Bind 시 current snapshot을 즉시 적용하고 delegate를 대칭 해제한다. owner/component search로 source를 추측하지 않는다.

Count 증가는 한 step마다 새 마지막 index를 추가하고 감소는 마지막 index부터 제거한다. 새 revision은 active animation의 target만 교체한다. state 변경은 기존 layer local transform을 보존하고 bucket/mesh selection을 새 profile state로 rebuild한다.

Bucket은 component에 attach하고 NoCollision, overlap off, navigation off, tick off로 설정한다. EndPlay/unregister에서 timer, delegate, bucket과 records를 정리하며 inventory는 변경하지 않는다.

## 3. Stack

`UTowelStackVisualComponent`는 `BaseLocalOffset`, `ZSpacing`을 authoring한다.

```text
Location(Index) = BaseLocalOffset + LocalUp * ZSpacing * Index
```

다음 기존 Actor에 `StackVisual` named native default subobject를 추가한다.

- `ACleanTowelStackActor`
- `AUsedTowelBinActor`
- `ATowelBasketActor`

property는 `VisibleAnywhere`, `BlueprintReadOnly`다. Actor BeginPlay/EndPlay가 자기 existing Inventory를 명시적으로 bind/unbind한다. 기존 interaction, carry, overflow와 physics behavior를 바꾸지 않는다.

Used bin 내부 instance는 container presentation-only다. `AWorldUsedTowelActor`는 Stack에 포함하거나 변경하지 않는다.

## 4. Pile

`UTowelPileVisualComponent`는 `PileHalfExtent`, `BaseLocalOffset`, `ItemsPerLayer`, `LayerSpacing`, bounded Z jitter와 constrained rotation range를 authoring한다.

- X/Y는 local extent 안 seeded random
- Z는 lower-first `Index / ItemsPerLayer` layer에 spacing/jitter 적용 후 extent clamp
- 동일 index transform은 count 유지/state swap에서 보존

기존 `ATowelProcessingMachineActor`에 `PileVisual` named native default subobject 하나를 추가하고 existing Inventory에 bind한다. 이 class를 상속하는 기존 Washer/Dryer 양쪽이 사용한다.

machine kind, state, processing timer, transfer port/control와 delegate 의미를 변경하지 않는다. 처리 완료 inventory commit이 기존 revision event로 Pile state mesh를 바꾸게 한다. drum animation/rotation은 Blueprint presentation 책임이며 C++에서 새 기계 actor를 만들지 않는다.

## 5. Slot Component Only

`UTowelSlotVisualComponent`는 ordered `TArray<FComponentReference> SlotReferences`를 authoring한다.

- 같은 owner의 non-null SceneComponent만 resolve
- duplicate/foreign/unresolved reference 제외와 진단
- 배열 순서 유지, name/type/world scan fallback 금지
- 앞 N개 slot 표시, 감소는 마지막 visible slot부터 제거
- slot world transform을 visual component local transform으로 변환
- count > valid slots이면 presentation만 clamp하고 gameplay value는 변경하지 않음

`PreviewState`, `PreviewCount`, `RebuildPreview()` CallInEditor와 transient preview cleanup을 구현한다. runtime `SetTargetPresentation` API는 공통 base에서 제공하되 이번 단계에는 source inventory를 bind하지 않는다.

금지:

- `ATowelDryingRackActor`
- `BP_TowelDryingRack`
- slot inventory/transfer/interaction/processing
- 기존 `BP_DryingSpot` 수정

## Blueprint/API/Core Redirect

- 기존 API rename/delete가 없으므로 Core Redirect를 추가하지 않는다.
- 새 default subobject 이름 `StackVisual`, `PileVisual`은 향후 Content 계약이므로 안정적으로 정한다.
- 구현 단계는 profile Data Asset을 만들거나 existing Blueprint를 resave하지 않는다.
- 후속 Unreal 작업은 Stack 세 Blueprint와 기존 Washer/Dryer의 inherited component transform/profile만 authoring한다.
- Slot은 Content asset을 만들지 않고 native test/CallInEditor API까지만 인계한다.

## Tests And Verification

- profile None/duplicate/null/0·1·multiple candidate와 weighted duplicate를 검증한다.
- Stack index transform, top removal, remove/re-add reroll과 existing-layer stability를 검증한다.
- Pile transform이 authored bounds/lower layer 안이고 seed로 재현되는지 검증한다.
- state swap이 count/transform을 유지하며 mesh selection만 교체하는지 검증한다.
- stale/equal-divergent revision, rapid target reversal, immediate sync와 bind/unbind cleanup을 검증한다.
- ISM collision/navigation/overlap 설정과 unique-mesh bucket 정리를 검증한다.
- Slot order, foreign/duplicate/unresolved rejection, capacity clamp와 preview cleanup을 검증한다.
- 세 Stack Actor와 machine 초기 snapshot/delegate lifecycle을 검증한다.
- existing towel transfer/machine/customer tests를 유지한다.
- `git diff --check` 후 UE 5.8 `Build.bat`을 첫 시도부터 승인된 sandbox 밖에서 실행한다.

완료 후 `.md/PROMPT_REVIEW.md`와 `.md/PROMPT_UNREAL.md`만 정기 결과물로 작성한다. Unreal 프롬프트에는 profile Data Asset과 기존 Stack 세 Blueprint/Washer/Dryer 설정만 포함하고 drying-rack asset이나 기능은 포함하지 않는다.
