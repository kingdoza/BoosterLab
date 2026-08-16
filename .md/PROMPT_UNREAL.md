# Unreal Prompt — Carry/Stain/Towel Preview/Bath Snap Integration

## Status

**Native 구현, UE 5.8 build와 BathhouseSim automation 17/17 완료. 아래 Content authoring, Blueprint compile/reload와 Editor/PIE 통합 검증이 필요하다.**

이 단계는 C++ domain logic을 Blueprint로 옮기지 않는다. 작업 전 모든 Editor/Live Coding 세션을 닫고 최신 `BathhouseSimEditor Win64 Development` binary로 새 Editor를 연다.

## Native Preflight

1. startup에서 `LogBlueprint: Error`, missing native property/component/event와 duplicate property가 0인지 확인한다.
2. 아래 target Blueprint를 저장 전 개별 Compile한다.
3. native load/compiler 오류가 있으면 어떤 asset도 저장하지 말고 코드 리뷰 단계로 돌린다.
4. 기존 사용자 dirty asset과 의도하지 않은 map/external actor는 저장하지 않는다.

## 1. Carry HeldTransform Authoring

| Asset | Parent | Property |
|---|---|---|
| `/Game/Bathhouse/Blueprints/Interaction/BP_BathhouseKey` | `ABathhouseKeyActor` | `HeldTransform` |
| `/Game/Bathhouse/Blueprints/Cleaning/BP_WetMop` | `AWetMopActor` | `HeldTransform` |
| `/Game/Bathhouse/Blueprints/Towel/BP_TowelBasket` | `ATowelBasketActor` | `HeldTransform` |

각 Blueprint Class Defaults에서 공용 `HeldKeyAnchor` 기준으로 손에 자연스럽게 보이는 local location/rotation을 설정한다.

- scale은 반드시 `(1,1,1)`로 유지한다. non-unit 값은 runtime에서 무시되고 Data Validation warning 대상이다.
- `/Game/FirstPersonCharacter/BP_FirstPersonCharacter`의 기존 `HeldKeyAnchor` 이름을 바꾸거나 아이템별 offset을 anchor에 합치지 않는다.
- key hook/customer/counter/recovery, mop/basket world scale, collision와 throw 값은 수정하지 않는다.
- 세 Blueprint를 Data Validation하고 HeldTransform scale warning 0을 확인한다.

PIE 확인:

1. Identity가 기존 anchor snap과 같은지 확인한다.
2. 세 아이템의 서로 다른 location/rotation이 player-held 상태에서만 보인다.
3. key hook take와 counter take에는 offset이 적용되고 hook return/counter placement/customer assignment/recovery에는 적용되지 않는다.
4. mop/basket G drop 위치, bounds, physics scale과 재획득/실패 rollback이 기존과 같다.

## 2. Water Stain Variation Authoring

Target: `/Game/Bathhouse/Blueprints/Cleaning/BP_WaterStain` (`AWaterStainActor`)

1. 기존 Blueprint-owned `StainVisual` 이름은 유지한다.
2. Components hierarchy에서 `StainVisual`을 inherited `StainVisualRoot` 아래로 재부착한다.
3. `InteractionCollision`은 root로 유지하고 relative transform/collision을 바꾸지 않는다.
4. Class Defaults에 `MaterialVariants`, `MinXYScale`, `MaxXYScale`, `MinYawDegrees`, `MaxYawDegrees`를 authoring한다.
5. scale 네 값은 모두 양수로 두고 의도한 경우에만 min/max를 뒤집어 runtime normalization을 확인한다.
6. `Event Apply Stain Material Variant`에서 전달된 material을 기존 `StainVisual`의 material element/slot `0`에 적용한다.

Event Graph 금지:

- random material/index/yaw/scale 재계산
- `StainVisualRoot` 또는 `InteractionCollision` world transform 변경
- cleaning state/progress, spawn count 또는 registry mutation
- Tick/Delay를 이용한 variation 재적용

Editor/PIE 확인:

1. `BP_WaterStain` Compile/Data Validation error 0, scale warning 0이다.
2. 후보 0개면 기존 material, 1개면 해당 material, 여러 개면 spawn별 variation이 보인다.
3. 한 stain은 lifetime 중 material/yaw/scale을 바꾸지 않는다.
4. X/Y만 범위 안에서 독립 변화하고 Z scale은 `1`이다.
5. decal/mesh 회전과 scale이 interaction trace 위치, floor alignment, spacing과 청소 hold를 바꾸지 않는다.

## 3. Towel Editor Preview

Inherited component exact name은 모두 `TowelPresentationVisual`이다.

| Blueprint | Type | Mesh profile |
|---|---|---|
| `/Game/Bathhouse/Blueprints/Towel/BP_CleanTowelStack` | Stack | `/Game/Bathhouse/Data/Towel/DA_TowelVisual_Shelf` |
| `/Game/Bathhouse/Blueprints/Towel/BP_UsedTowelBin` | Stack | `/Game/Bathhouse/Data/Towel/DA_TowelVisual_UsedBin` |
| `/Game/Bathhouse/Blueprints/Towel/BP_TowelBasket` | Stack | `/Game/Bathhouse/Data/Towel/DA_TowelVisual_Basket` |
| `/Game/Bathhouse/Blueprints/Towel/BP_Washer` | Pile | `/Game/Bathhouse/Data/Towel/DA_TowelVisual_Washer` |
| `/Game/Bathhouse/Blueprints/Towel/BP_Dryer` | Pile | `/Game/Bathhouse/Data/Towel/DA_TowelVisual_Dryer` |

각 placed/default preview 대상에서:

1. profile, `RandomSeed`, Stack offset/Z spacing 또는 Pile bounds/layer/jitter/rotation을 확인한다.
2. `PreviewState`와 `PreviewCount`를 설정하고 `RebuildPreview`를 누른다.
3. 같은 seed/input으로 clear/rebuild했을 때 mesh와 Pile transform이 동일한지 확인한다.
4. `ClearPreview`가 instance를 모두 제거하는지 확인한다.
5. preview instance가 transient이며 asset inventory count/state/revision을 바꾸지 않는지 확인한다.

`UTowelSlotVisualComponent`는 별도 test Actor에서만 ordered same-owner slot reference, rebuild/clear를 확인하고 gameplay actor/`BP_DryingSpot`에는 연결하지 않는다. drying-rack Actor/inventory/interaction을 만들지 않는다.

PIE 확인:

- preview button 호출은 Game/PIE presentation과 revision을 바꾸지 않는다.
- BeginPlay가 남은 preview를 지우고 각 Actor의 authoritative inventory snapshot을 즉시 표시한다.
- transfer/state swap/count animation, basket carry/drop와 washer/dryer process 회귀가 없다.
- 종료/재시작 뒤 중복 ISM, timer 또는 delegate 반응이 없다.

## 4. Bath Snap Integration

Targets:

- `/Game/Bathhouse/Blueprints/Facility/BP_Bath`
- `/Game/Bathhouse/Blueprints/Customer/BP_BathhouseCustomer`
- `/Game/Bathhouse/AI/ST_CustomerRoutine`은 compile/behavior 검증만 하며 graph를 수정하지 않는다.

별도 native property authoring은 없다. `BP_Bath`의 ApproachPoint/ActionPoint는 기존 발바닥 기준 transform을 유지한다.

1. 임시 blocking collision Actor를 ActionPoint의 capsule 위치에 겹치게 배치한다.
2. 고객이 ApproachPoint까지 navigation한 뒤 정확한 cached ActionPoint로 snap하는지 확인한다.
3. snap 후 movement가 `MOVE_None`이고 capsule/Actor collision enabled와 responses가 그대로인지 확인한다.
4. blocking overlap이 navigation retry/technical abort를 만들지 않는지 확인한다.
5. 정상 dwell 종료, interruption과 technical abort가 cached ApproachPoint로 복귀하고 movement mode/slot을 복구하는지 확인한다.
6. 임시 blocker는 검증 후 삭제하고 의도하지 않았다면 map을 저장하지 않는다.

## Compile, Save And Reload

1. 수정한 Blueprint/Data Asset만 개별 Compile/Data Validation한다.
2. error/warning 0인 경우에만 의도한 asset을 저장한다.
3. Editor를 재시작하고 inherited properties/component hierarchy/event binding/preview authoring 값이 유지되는지 확인한다.
4. target asset 외 dirty package가 생기면 저장하지 않는다.

## Forbidden Blueprint Logic

- carry ownership/drop transaction 또는 HeldTransform scale 처리
- stain random seed/variation/cleaning state 계산
- towel inventory/revision/timer/ISM lifecycle 계산
- Customer snap collision 검사, teleport, movement mode 또는 facility state 변경
- reflected symbol rename/delete, Core Redirect/Config/module 변경

## Completion Report

- 생성/수정/저장한 exact asset 경로
- 세 HeldTransform 최종 location/rotation과 unit scale validation
- stain material 후보/range, `StainVisualRoot` hierarchy와 slot-0 event 연결
- 다섯 towel preview의 state/count/seed/layout, rebuild/clear와 same-seed 결과
- blocked Bath snap/return/collision-state PIE 결과
- Blueprint Compile/Data Validation/reload 결과와 의도하지 않은 dirty asset 유무
