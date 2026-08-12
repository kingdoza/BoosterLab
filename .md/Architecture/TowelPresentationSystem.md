# Towel Presentation System

## Target Status

이 문서는 기존 Towel inventory snapshot을 Static Mesh Instance로 표현하는 Stack/Pile/Slot 공통 기능의 확정 구현 target을 정의한다. 현재 Towel circulation, washer/dryer와 Blueprint actor는 존재하지만 이 native quantity presentation 계층은 아직 구현되지 않았다.

이번 연결 범위:

- Stack: 기존 clean towel stack, used towel bin, carried towel basket
- Pile: 기존 washer와 dryer를 함께 표현하는 `ATowelProcessingMachineActor`
- Slot: component/API/validation/Editor preview까지만 구현하고 gameplay actor에는 연결하지 않음

수건 건조대 Actor, inventory, interaction, processing과 Content asset은 이번 범위 밖이다.

## Source Target

```text
Source/BathhouseSim/Public/Towel/Presentation/
  TowelVisualTypes.h
  TowelVisualMeshProfile.h
  TowelQuantityVisualComponent.h
  TowelStackVisualComponent.h
  TowelPileVisualComponent.h
  TowelSlotVisualComponent.h

Source/BathhouseSim/Private/Towel/Presentation/
  # matching implementation files
```

Towel System 내부 표현 하위 계층이며 새 top-level module이나 dependency를 만들지 않는다.

## Responsibilities

- towel state/count/revision snapshot의 transient world presentation
- state별 Static Mesh variant 선택과 unique-mesh ISM bucket 관리
- 최신 authoritative snapshot으로 count animation 수렴
- vertical stack, random layered pile과 ordered authored slot transform 생성
- inventory delegate bind/unbind와 runtime instance cleanup
- Slot reference validation과 Editor preview

Presentation은 towel state/count/capacity, machine state, transfer와 interaction을 변경하지 않는다.

## State And Execution Owners

| 책임 | Owner |
|---|---|
| authoritative state/count/revision | `UTowelInventoryComponent` |
| target/displayed presentation state | `UTowelQuantityVisualComponent` |
| state별 mesh variants | `UTowelVisualMeshProfile` Data Asset |
| per-index transform | Stack/Pile/Slot subclass |
| washer/dryer process animation/sound | existing Machine Blueprint |
| interaction collision | existing Actor/port component |

## Mesh Profile

`FTowelStateMeshVariants`:

- `ETowelState State`
- `TArray<TObjectPtr<UStaticMesh>> MeshVariants`

`UTowelVisualMeshProfile`은 `TArray<FTowelStateMeshVariants> StateVariants` 하나를 노출한다.

- `None` entry는 허용하지 않는다.
- 같은 state 중복은 validation 오류다.
- null mesh는 runtime 후보에서 제외한다.
- 유효 후보 0개면 해당 state visual을 만들지 않고 경고한다.
- 후보 1개면 random 호출 없이 사용한다.
- 여러 후보는 배열 entry 기준 균등 random이며 동일 mesh 중복은 의도한 가중치로 인정한다.

Profile은 필요한 state만 가진다.

- shelf profile: Clean entry만
- used-bin profile: Used entry만
- basket profile: Used/Wet/Clean
- washer profile: Used/Wet
- dryer profile: Wet/Clean

Z spacing, pile bounds와 slot references는 container geometry이므로 profile이 아니라 각 component property다.

## Common Quantity Visual

`UTowelQuantityVisualComponent`는 abstract `USceneComponent`다.

Public contract:

- `BindInventorySource(UTowelInventoryComponent*)`
- `UnbindInventorySource()`
- `SetTargetPresentation(State, Count, Revision, bAnimate)`
- `SynchronizeImmediately()`
- displayed/target state, count와 applied revision 조회

Runtime state:

- bound inventory weak reference와 delegate handle
- target/displayed state/count
- latest applied `int64` revision
- active step timer
- ordered layer records
- unique mesh별 transient `UInstancedStaticMeshComponent` bucket
- initialized random stream

Bind 시 current snapshot을 즉시 적용한다. stale revision은 무시하고 같은 revision의 다른 payload는 ensure/log 후 authoritative current snapshot으로 재동기화한다.

Count animation:

- `DisplayedCount < TargetCount`: 한 step마다 새 top/index visual 추가
- `DisplayedCount > TargetCount`: 마지막 index부터 제거
- 새 revision은 기존 timer를 재사용하되 target을 즉시 교체한다.
- disable/rebind/reconstruction에서 `SynchronizeImmediately`로 최신 snapshot에 맞춘다.
- state가 바뀌면 count와 layout transform을 유지하고 새 state profile로 mesh bucket만 재구축한다.

각 새 index는 생성 시 mesh variant를 한 번 random 선택한다. 기존 index는 count만 바뀌어도 다시 추첨하지 않으며 제거 후 같은 index가 다시 생성되면 새로 추첨한다.

## ISM Rendering Contract

한 towel마다 `UStaticMeshComponent`를 만들지 않고 unique mesh asset마다 transient `UInstancedStaticMeshComponent` bucket 하나를 만든다.

Layer record는 mesh bucket identity, instance index와 local transform을 저장한다. count 감소는 항상 global 마지막 layer부터 수행하므로 해당 bucket에서도 마지막 instance를 제거하게 유지한다. full rebuild/state swap은 모든 transform을 보존한 채 bucket mapping만 다시 만든다.

모든 bucket:

- `NoCollision`
- overlap event off
- `CanEverAffectNavigation=false`
- tick off
- interaction trace 대상 아님
- visual component에 attachment
- unregister/EndPlay에서 timer와 함께 대칭 정리

## Stack Layout

`UTowelStackVisualComponent` property:

- `BaseLocalOffset`
- `ZSpacing >= 0`
- common animation interval/seed/profile

index transform은 component local 기준 `BaseLocalOffset + FVector::UpVector * ZSpacing * Index`다. rotation/scale random은 이번 target에 추가하지 않는다.

Native default subobject 연결:

- `ACleanTowelStackActor::StackVisual`
- `AUsedTowelBinActor::StackVisual`
- `ATowelBasketActor::StackVisual`

각 Actor가 BeginPlay에 자기 `Inventory`를 명시적으로 bind하고 EndPlay에 unbind한다. owner/component search로 암묵 연결하지 않는다. Used bin 내부 instance는 표현 전용이고 overflow `AWorldUsedTowelActor`와 무관하다.

## Pile Layout

`UTowelPileVisualComponent` property:

- local `PileHalfExtent`
- `BaseLocalOffset`
- `ItemsPerLayer >= 1`
- `LayerSpacing >= 0`
- position Z jitter와 constrained random rotation range
- common animation interval/seed/profile

index는 `LayerIndex = Index / ItemsPerLayer`를 사용한다. X/Y는 authored extent 안 random, Z는 base + layer spacing + bounded jitter로 계산하고 extent 밖으로 clamp한다. 적은 count가 공중에 뜨지 않게 아래 layer부터 생성한다.

기존 `ATowelProcessingMachineActor`에 `PileVisual` native default subobject 하나만 추가한다. `BP_Washer`와 `BP_Dryer`가 이를 상속하고 각각 drum 안 pivot/bounds와 profile을 authoring한다. existing `Inventory`에 bind하므로 process completion의 Used->Wet 또는 Wet->Clean commit도 같은 revision event로 표현된다.

기존 machine timer, transfer port, control, state delegate와 Blueprint process animation은 변경하지 않는다. Pile instance를 drum 회전에 포함할지는 Blueprint attachment/presentation으로 결정한다.

## Slot Layout Target

`UTowelSlotVisualComponent`는 ordered `TArray<FComponentReference> SlotReferences`를 가진다.

BeginPlay/preview resolve 조건:

- non-null `USceneComponent`
- visual component와 같은 owner Actor
- 중복 component가 아님
- 입력 배열 순서 유지
- 이름/type/world scan fallback 금지

count N은 valid slot의 앞 N개를 표시하고 감소는 마지막 visible slot부터 제거한다. target count가 valid slot count보다 크면 valid capacity까지만 표시하고 진단 로그를 남기며 gameplay count는 변경하지 않는다. slot world transform은 visual component local transform으로 변환해 ISM instance에 사용한다.

이번 구현은 API와 Editor preview까지만 포함한다.

- `PreviewState`
- `PreviewCount`
- `RebuildPreview()` `CallInEditor`
- preview/runtime instance의 transient cleanup

`ATowelDryingRackActor`, `BP_TowelDryingRack`, inventory bind, transfer, drying process와 interaction은 만들지 않는다. existing `BP_DryingSpot`도 수정하지 않는다.

## Blueprint/API Contracts

기존 actor에 추가되는 `StackVisual`/`PileVisual`은 `VisibleAnywhere`, `BlueprintReadOnly` default subobject다. 기존 reflected component/property/event를 rename하거나 삭제하지 않는다.

Editor authoring:

- visual component `MeshProfile`, seed와 count step interval
- Stack pivot/offset/Z spacing
- Washer/Dryer pile pivot, bounds/layer/jitter
- Slot owner component references와 preview state/count

Blueprint Event Graph는 inventory count를 다시 계산하거나 instance를 생성하지 않는다. optional sound/material response만 기존 inventory/machine event에 연결한다.

## Dependencies

- Towel Presentation -> Towel inventory snapshot/delegate
- Towel Presentation -> Engine Scene/ISM/DataAsset/Timer
- 기존 Towel gameplay -> concrete UI/Widget 의존 없음
- UI System/Interaction System -> Towel Presentation 의존 없음

## Manual Review Points

- 기존 Stack/Pile index의 mesh가 unrelated revision에서 재추첨되지 않는지 확인한다.
- state swap이 count/layout을 유지하고 profile mesh만 교체하는지 확인한다.
- stale revision, rapid bulk increase/decrease와 rebind 뒤 displayed count가 최신 snapshot으로 수렴하는지 확인한다.
- 모든 ISM이 collision/navigation/interaction trace에 영향을 주지 않는지 확인한다.
- Stack 세 Actor와 기존 Washer/Dryer만 runtime inventory에 bind되는지 확인한다.
- Slot이 어느 gameplay actor에도 연결되지 않고 건조대 기능/asset이 생성되지 않는지 확인한다.
