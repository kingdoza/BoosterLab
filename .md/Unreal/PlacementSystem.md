# Placement Editor Authoring

## 전역 설정과 Preview 재질

- 전역 grid 기본값은 `10cm`다.
- `UFacilityPlacementSettings.FacilityItemHeldTransform`은 위치·회전만 담당하며 런타임은 scale을 1로 정규화한다. 현재 저장값은 Location `(30,-50,0)`, Rotation identity, Scale `(1,1,1)`이다.
- 아래 Material 두 개가 저장돼 있다.
  - `/Game/Bathhouse/Materials/Placement/MI_FacilityPreview_Valid`: Surface, Translucent, Unlit, Two Sided, Emissive `(0.05,1,0.05)`, Opacity `0.35`
  - `/Game/Bathhouse/Materials/Placement/MI_FacilityPreview_Invalid`: Surface, Translucent, Unlit, Two Sided, Emissive `(1,0.03,0.03)`, Opacity `0.35`
- 현재 Config의 `ValidPreviewMaterial`과 `InvalidPreviewMaterial`은 각각 `/Game/Material/MI_Preview_Valid`, `/Game/Material/MI_Preview_Invalid`를 참조한다.

## Definition 계약

다음 Definition에는 `PreviewActorClass`, `FootprintCellsX`, `FootprintCellsY`가 없으며 저장·재로드가 완료됐다.

| Definition | Placed Class | Locker Slots | 상태 |
|---|---|---:|---|
| `/Game/Bathhouse/Data/Placement/DA_FacilityPlacement_Bath` | `BP_Bath` | 0 | placement 활성 |
| `/Game/Bathhouse/Data/Placement/DA_FacilityPlacement_Shower` | `BP_Shower` | 0 | placement 활성 |
| `/Game/Bathhouse/Data/Placement/DA_FacilityPlacement_ClothesLocker_1` | `BP_ClothesLocker` | 1 | placement 활성 |
| `/Game/Bathhouse/Data/Placement/DA_FacilityPlacement_ClothesLocker_4` | `BP_ClothesLocker_4` | 4 | placement 활성 |
| `/Game/Bathhouse/Data/Placement/DA_FacilityPlacement_ClothesLocker_8` | `BP_ClothesLocker_8` | 8 | placement 활성 |
| `/Game/Bathhouse/Data/Placement/DA_FacilityPlacement_Washer` | `BP_Washer` | 0 | placement 활성 |
| `/Game/Bathhouse/Data/Placement/DA_FacilityPlacement_Dryer` | `BP_Dryer` | 0 | placement 활성 |
| `/Game/Bathhouse/Data/Placement/DA_FacilityPlacement_CleanTowelStack` | `None` | 0 | placement opt-out |
| `/Game/Bathhouse/Data/Placement/DA_FacilityPlacement_UsedTowelBin` | `None` | 0 | placement opt-out |

활성 7개 Definition의 `RecoveryItemClass`는 공통 `/Game/Bathhouse/Blueprints/Placement/BP_PlaceableFacilityItem.BP_PlaceableFacilityItem_C`, `RecoveryItemMesh`는 `None`이며 native Cube fallback을 사용한다. Stack과 Bin은 `PlacedFacilityClass`, `RecoveryItemClass`, `RecoveryItemMesh`가 모두 `None`인 opt-out 상태다.

공통 설비 아이템 Blueprint의 parent는 `/Script/BathhouseSim.PlaceableFacilityItemActor`다. 상속된 `ItemRoot`의 기본 Relative Scale은 `(0.3,0.3,0.3)`이며, 설비 회수 아이템의 공통 크기는 이 값에서 조정한다. Project Settings의 `FacilityItemHeldTransform.Scale`로는 크기를 조정하지 않는다.

구형 `/Game/Bathhouse/Blueprints/Placement/Preview/BP_FacilityPreview_*` 9개는 Definition 재저장 뒤 참조가 0임을 확인하고 삭제됐다. 현재 preview는 native `AFacilityPlacementPreviewActor` 경로만 사용한다.

## Blueprint footprint와 body

모든 footprint는 actor local install floor `Z=0`을 기준으로 한다. `PlacementFootprint`는 Navigation 비활성, Collision `NoCollision`이다. scaled full X/Y는 10cm grid의 양의 정수배다.

| Blueprint | Parent Class | Footprint Extent | Footprint Relative Z | body mesh 계약 |
|---|---|---|---:|---|
| `/Game/Bathhouse/Blueprints/Facility/BP_Bath` | `/Script/BathhouseSim.BathhouseFacilityActor` | `(145,120,38)` | 38 | `FacilityVisual`, `SM_Bath_old`, BlockAllDynamic, Navigation 활성 |
| `/Game/Bathhouse/Blueprints/Facility/BP_Shower` | `/Script/BathhouseSim.BathhouseFacilityActor` | `(40,55,28)` | 28 | `FacilityVisual`, Engine Cube, BlockAllDynamic, Navigation 활성 |
| `/Game/Bathhouse/Blueprints/Facility/BP_ClothesLocker` | `/Script/BathhouseSim.BathhouseFacilityActor` | `(30,20,50)` | 50 | `FacilityVisual`, Engine Cube, BlockAllDynamic, Navigation 활성 |
| `/Game/Bathhouse/Blueprints/Facility/BP_ClothesLocker_4` | `/Script/BathhouseSim.BathhouseFacilityActor` | `(30,70,50)` | 50 | `FacilityVisual`과 `LockerVisual02~04`, BlockAllDynamic, Navigation 활성 |
| `/Game/Bathhouse/Blueprints/Facility/BP_ClothesLocker_8` | `/Script/BathhouseSim.BathhouseFacilityActor` | `(30,140,50)` | 50 | `FacilityVisual`과 `LockerVisual02~08`, BlockAllDynamic, Navigation 활성 |
| `/Game/Bathhouse/Blueprints/Towel/BP_Washer` | `/Script/BathhouseSim.TowelProcessingMachineActor` | `(30,25,40)` | 40 | `MachineVisual`, Engine Cube, BlockAllDynamic, Navigation 활성 |
| `/Game/Bathhouse/Blueprints/Towel/BP_Dryer` | `/Script/BathhouseSim.TowelProcessingMachineActor` | `(30,25,40)` | 40 | `MachineVisual`, Engine Cube, BlockAllDynamic, Navigation 활성 |

`PackagePhysicalRoot`, slot, interaction, action/approach point, towel presentation, water/contents helper는 Navigation에 관여하지 않는다. Bath의 `SM_Bath_old`에는 generated convex collision과 NavCollision이 존재한다.

## Placement Zone

- `/Game/Bathhouse/Blueprints/Placement/BP_FacilityPlacementZone`
- Parent Class: `/Script/BathhouseSim.FacilityPlacementZoneActor`
- inherited `ZoneBounds`와 `PlacementFloor`를 사용한다.
- `PlacementFloor`의 ZoneBounds 상대 transform은 identity이며 Navigation 비활성이다.
- 실제 Level instance의 floor plane은 world `Z=0`이다. Zone Bounds의 두께는 설치 높이 계산의 기준이 아니다.

Recast와 Project Settings의 미저장 전역값, 실제 입력 기반 preview/배치/회수 판정은 [USER_UNREAL.md](../USER_UNREAL.md)를 따른다.
