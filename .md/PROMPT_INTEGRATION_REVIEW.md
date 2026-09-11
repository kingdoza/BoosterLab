# 통합 검토 — 설비 배치 Content Migration·Preview·Dynamic Navigation·락커 ID

## 결론

상태는 **부분 완료**다. Content migration과 Blueprint authoring은 저장·재시작 검증까지 완료됐다. Project Settings Material reference, Recast `Dynamic`, 입력 기반 시각/Navigation 시나리오와 opt-out Definition의 native validation 결함이 남아 최종 통합 승인은 보류한다.

## 저장된 변경

### Definition 재저장

아래 9개를 UE 5.8 Editor에서 load/resave했다. 삭제된 `PreviewActorClass`, `FootprintCellsX/Y`는 reflected property 목록과 재시작 뒤 reference audit에서 사라졌다.

- `/Game/Bathhouse/Data/Placement/DA_FacilityPlacement_Bath`
- `/Game/Bathhouse/Data/Placement/DA_FacilityPlacement_Shower`
- `/Game/Bathhouse/Data/Placement/DA_FacilityPlacement_ClothesLocker_1`
- `/Game/Bathhouse/Data/Placement/DA_FacilityPlacement_ClothesLocker_4`
- `/Game/Bathhouse/Data/Placement/DA_FacilityPlacement_ClothesLocker_8`
- `/Game/Bathhouse/Data/Placement/DA_FacilityPlacement_Washer`
- `/Game/Bathhouse/Data/Placement/DA_FacilityPlacement_Dryer`
- `/Game/Bathhouse/Data/Placement/DA_FacilityPlacement_CleanTowelStack`
- `/Game/Bathhouse/Data/Placement/DA_FacilityPlacement_UsedTowelBin`

Stack/Bin의 `PlacedFacilityClass`, `RecoveryItemClass`, `RecoveryItemMesh`는 `None`으로 유지했다.

후속 공통 아이템 작업에서 활성 7개 Definition의 `RecoveryItemClass`를 `/Game/Bathhouse/Blueprints/Placement/BP_PlaceableFacilityItem.BP_PlaceableFacilityItem_C`로 통일했다. 새 class reference와 공통 Scale은 Editor 재시작 뒤에도 유지됐다.

### Blueprint authoring

아래 7개 placement Blueprint를 수정·Compile·저장했다.

- `/Game/Bathhouse/Blueprints/Facility/BP_Bath`
- `/Game/Bathhouse/Blueprints/Facility/BP_Shower`
- `/Game/Bathhouse/Blueprints/Facility/BP_ClothesLocker`
- `/Game/Bathhouse/Blueprints/Facility/BP_ClothesLocker_4`
- `/Game/Bathhouse/Blueprints/Facility/BP_ClothesLocker_8`
- `/Game/Bathhouse/Blueprints/Towel/BP_Washer`
- `/Game/Bathhouse/Blueprints/Towel/BP_Dryer`

공통 결과:

- `PlacementFootprint` bottom을 local `Z=0`에 맞췄다. Bath center Z는 40에서 38로 수정했고 나머지는 이미 extent Z와 일치했다.
- 모든 `PlacementFootprint`를 Collision `NoCollision`, Navigation 비활성으로 통일했다.
- 실제 body Static Mesh를 BlockAllDynamic과 Navigation 활성으로 저장했다.
- Washer/Dryer `MachineVisual`은 기존 `NoCollision`에서 `BlockAllDynamic`으로 수정했다.
- helper component는 Navigation 비활성 상태를 유지했다.
- 삭제된 `PlacementNavModifier`는 CDO와 조사한 external actor candidate에 존재하지 않았다.

`BP_CleanTowelStack`, `BP_UsedTowelBin`, `BP_FacilityPlacementZone`은 load/Compile했으나 serialized 변경이 없어 저장 diff는 생기지 않았다.

### Preview Material과 legacy asset

다음 Material 두 개를 생성·저장했다.

- `/Game/Bathhouse/Materials/Placement/MI_FacilityPreview_Valid`: green emissive, opacity 0.35, Translucent/Unlit
- `/Game/Bathhouse/Materials/Placement/MI_FacilityPreview_Invalid`: red emissive, opacity 0.35, Translucent/Unlit

Definition 재저장 뒤 reference 0과 Git 복구 가능성을 확인하고 `/Game/Bathhouse/Blueprints/Placement/Preview/BP_FacilityPreview_*` 9개를 삭제했다.

## Level과 전역 설정 확인

- DefaultMap의 Placement Zone은 한 개이며 Location `(600,-100,0)`, Bounds Extent `(1400,900,10)`, `Facility.Placeable`, floor plane world `Z=0`이다.
- pre-placed 1-slot Clothes Locker 두 개의 `RegistrationId`는 valid/unique다.
- 조사한 Bath 2, Locker 2, Washer 1, Dryer 1 external actor에 stale `PlacementNavModifier`가 없다.
- Blueprint body Navigation 변경은 재시작 뒤 Level instance에도 반영됐다.
- Project Settings Material reference는 MCP 메모리에서 설정됐지만 config에 저장되지 않아 재시작 뒤 `None`이었다.
- Recast는 MCP 메모리에서 `Dynamic`으로 변경됐지만 external actor 저장이 실패해 재시작 뒤 `DynamicModifiersOnly`였다.

Project Settings와 World Partition 저장은 [USER_UNREAL.md](USER_UNREAL.md) 1~2번으로 인계했다. Map 또는 external actor 디스크 변경은 이번 pass에 포함되지 않는다.

## Compile·Validation·PIE

- 재시작 뒤 Blueprint 10개를 warnings-as-errors로 Compile했고 모두 성공했다.
- 대상 allowlist asset dirty 상태는 모두 false다.
- 활성 Definition 7개는 footprint/helper navigation 오류 없이 validation됐다. `RecoveryItemMesh=None` native Cube fallback 경고는 유지된다.
- Clean Towel Stack과 Used Towel Bin Definition은 native `IsDataValid()`가 opt-out을 구분하지 않아 각각 3개 오류가 발생한다. Content를 임의로 활성화하지 않았으며 구현 단계 차단점으로 기록했다.
- 깨끗한 Editor 재시작 후 PIE를 편집 없이 2회 실행·종료했다. 두 실행 모두 duplicate RegistrationId, locker capacity/expansion, navigation retry, removed property/component, Blueprint compile 오류, Error/Ensure 없이 시작됐다.
- 작업 중 PIE가 실행된 상태에서 Blueprint를 Compile한 한 번의 잘못된 검증 순서에서는 `ATowelProcessingMachineActor::BeginPlay()` delegate 중복 바인딩 Ensure가 발생했다. 깨끗한 재시작과 정상 순서의 PIE 2회에서는 재현되지 않아 제품 수용 결과에서 제외했다.

## 미수행 수용 기준

MCP에는 플레이어 입력 주입과 NavMesh 시각 비교 기능이 없어 다음을 완료로 판정하지 않았다.

- 7종 설비의 multi-mesh preview 외관과 모든 material slot 치환
- valid/invalid 색 전환, LCtrl snap, wheel yaw, containment/overlap/four-corner support
- preview/confirm/recovery/rollback/replacement 전후 collision 및 NavMesh 변화
- Stack/Bin placement/recovery opt-out의 실제 prompt 판정
- pre-placed Locker actor Data Validation의 Editor UI 실행

exact 절차는 [USER_UNREAL.md](USER_UNREAL.md) 3~4번에 있다.

## Unreal 정본과 작업 경계

- 생성: `.md/Unreal/PlacementSystem.md`, `.md/Unreal/FacilitySystem.md`, `.md/Unreal/WorldSystem.md`
- 갱신: `.md/Unreal/0_UNREAL.md`
- 최초 Unreal migration pass에서는 Source와 Config를 수정하지 않았다. 후속 공통 아이템 작업에서는 Blueprint subclass 허용 Source와 `FacilityItemHeldTransform.Scale=1` 정규화 Config를 수정했다.
- 작업 시작 전에 존재하던 Source/Architecture 문서 변경은 보존했다.
- Unreal asset을 셸로 편집하거나 `Save All`을 사용하지 않았다.

## 공통 설비 아이템 Blueprint 후속 결과

- `/Game/Bathhouse/Blueprints/Placement/BP_PlaceableFacilityItem`을 `APlaceableFacilityItemActor` parent로 생성했다.
- 기존에 HeldTransform에 입력됐지만 무시되던 Scale `(0.3,0.3,0.3)`을 상속된 `ItemRoot` 기본 Scale로 이전했다.
- runtime/Data Validation이 네이티브 base 또는 Blueprint 파생 class를 허용하도록 수정했다.
- UE 5.8 Editor 빌드 성공, `BathhouseSim.Placement` 4/4 및 전체 `BathhouseSim` 36/36 자동화 성공을 확인했다.
- 새 DLL 재시작 뒤 Blueprint compile, 공통 Scale, Definition 7개 reference와 dirty 0을 확인했다.
- 실제 화면 크기와 E/G 조작 감각은 입력 기반 직접 PIE 검증으로 남는다.
