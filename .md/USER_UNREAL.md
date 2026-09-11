# 사용자 Unreal 후속 작업 — 설비 배치 Content Migration

## 상태

Definition/Blueprint migration, footprint·body Navigation authoring, preview Material 생성, 구형 Preview Blueprint 삭제는 저장·재시작 확인까지 완료됐다. 아래 항목은 현재 Unreal MCP가 Project Settings 또는 World Partition external actor를 디스크에 저장하지 못하거나, 실제 플레이어 입력·시각 판정이 필요해 남아 있다.

`Save All`은 사용하지 않는다. 아래에서 지정한 설정과 `/Game/Maps/DefaultMap` 관련 external actor만 저장한다.

## 1. Project Settings 연결

1. **Edit > Project Settings > Game > Facility Placement**를 연다.
2. `Facility Item Held Transform`을 다음 값으로 둔다.
   - Location `(0,0,0)`
   - Rotation `(0,0,0)`
   - Scale `(1,1,1)`
3. `Valid Preview Material`에 `/Game/Bathhouse/Materials/Placement/MI_FacilityPreview_Valid`을 지정한다.
4. `Invalid Preview Material`에 `/Game/Bathhouse/Materials/Placement/MI_FacilityPreview_Invalid`를 지정한다.
5. Editor를 닫았다가 다시 열어 두 reference가 유지되는지 확인한다.

현재 확인 상태: 두 Material은 Translucent/Unlit로 저장돼 있지만 Project Settings의 두 reference는 재시작 후 `None`으로 돌아왔다. MCP property setter는 메모리만 바꾸고 config를 저장하지 않았다.

## 2. RecastNavMesh Dynamic 저장

1. PIE를 중지하고 `/Game/Maps/DefaultMap`을 연다.
2. Outliner에서 `RecastNavMesh`를 선택한다.
3. **Runtime Generation**을 `Dynamic`으로 바꾼다. `Dynamic Modifiers Only`가 아니다.
4. 해당 Recast external actor와 필요한 Map 패키지만 저장한다.
5. `DefaultMap`을 닫았다가 다시 열고 값이 `Dynamic`으로 유지되는지 확인한다.

현재 확인 상태: MCP 메모리에서는 `Dynamic` 적용이 됐지만 World Partition actor 저장 호출이 external actor 패키지를 에셋으로 찾지 못했다. 재시작 후 저장값은 다시 `Dynamic Modifiers Only`였다.

## 3. Definition Data Validation 네이티브 차단점

다음 두 opt-out Definition은 의도대로 `PlacedFacilityClass=None`, `RecoveryItemClass=None` 상태지만 현재 native `UFacilityPlacementDefinition::IsDataValid()`가 opt-out을 구분하지 않아 각각 세 개의 오류를 낸다.

- `/Game/Bathhouse/Data/Placement/DA_FacilityPlacement_CleanTowelStack`
- `/Game/Bathhouse/Data/Placement/DA_FacilityPlacement_UsedTowelBin`

오류는 `Placed Facility Class must implement IPlaceableFacility`, `Recovery Item Class must derive from APlaceableFacilityItemActor`, `Placed and recovery item classes must be different`다.

Editor에서 class를 임의로 채우지 않는다. 그러면 Stack/Bin의 placement opt-out 계약이 깨진다. 구현 단계로 되돌려 `IsDataValid()`가 conversion opt-out Definition에는 placement/recovery class 검사를 적용하지 않도록 수정하고 코드 리뷰를 다시 받아야 한다. 그 뒤 Definition 9개를 Data Validation한다.

활성 Definition 7개는 helper collision/Navigation 오류 없이 검증됐고, `RecoveryItemMesh=None`에 대한 native Cube fallback 경고만 남았다.

## 4. 직접 플레이 검증

1~3번을 마친 뒤 PIE에서 Bath, Shower, Locker 1/4/8, Washer, Dryer를 각각 확인한다.

1. 설비를 들었을 때 class-default의 모든 body mesh가 preview에 나타나는지 확인한다.
2. 설치 가능 위치는 초록, 불가 위치는 빨강 반투명 재질이 모든 mesh slot에 적용되는지 확인한다.
3. 모든 footprint bottom이 Zone의 `PlacementFloor`와 일치하고 부양·매몰되지 않는지 확인한다.
4. LCtrl snap, wheel yaw, containment, blocking overlap, 네 corner floor support를 확인한다.
5. confirm 전 preview가 collision/NavMesh를 만들지 않고, confirm 뒤 body collision에 따라 NavMesh가 갱신되는지 확인한다.
6. 빈 설비 회수 시 source collision/NavMesh가 사라지고, rollback이면 복구되며, 회수 item 재배치 뒤 NavMesh가 다시 생성되는지 확인한다.
7. Clean Towel Stack과 Used Towel Bin에 placement/recovery prompt가 생기지 않는지 확인한다.
8. 설비 회수 아이템이 공통 `/Game/Bathhouse/Blueprints/Placement/BP_PlaceableFacilityItem`의 `ItemRoot` Scale `(0.3,0.3,0.3)`로 생성되고 E pickup/G drop 뒤에도 같은 크기를 유지하는지 확인한다.

깨끗한 재시작 상태에서 기본 PIE 2회는 이미 통과했으며 duplicate RegistrationId, locker capacity/expansion 오류, removed property/component, Blueprint compile 오류와 Ensure는 없었다.

## 재개 조건

- Project Settings의 두 Material reference가 재시작 후 유지된다.
- Recast `RuntimeGeneration=Dynamic`이 재시작 후 유지된다.
- opt-out Definition 두 개의 native Data Validation 오류가 수정된다.
- Definition 9개와 pre-placed Locker 두 개의 Data Validation이 통과한다.
- 4번 직접 플레이 검증 결과를 기록한다.

완료 후 `.md/PROMPT_INTEGRATION_REVIEW.md`의 미완료 항목을 최종 통합 리뷰에서 다시 판정한다.
