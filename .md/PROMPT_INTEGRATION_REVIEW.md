# Carry/Stain/Towel Preview/Bath Snap — Unreal Integration Review

## 작업 상태

- 상태: 완료
- 기준 작업서: `.md/PROMPT_UNREAL.md`
- 검증 엔진: Unreal Engine 5.8.1
- 이번 단계에서는 Source, Config, StateTree graph, map 및 World Partition external actor를 수정하거나 저장하지 않았다.
- 기존 dirty worktree의 다른 Source/Content/문서는 보존했다.

## 수정 및 저장한 에셋

| 종류 | 에셋 |
|---|---|
| HeldTransform | `/Game/Bathhouse/Blueprints/Interaction/BP_BathhouseKey` |
| HeldTransform | `/Game/Bathhouse/Blueprints/Cleaning/BP_WetMop` |
| HeldTransform + towel preview | `/Game/Bathhouse/Blueprints/Towel/BP_TowelBasket` |
| stain authoring | `/Game/Bathhouse/Blueprints/Cleaning/BP_WaterStain` |
| towel preview | `/Game/Bathhouse/Blueprints/Towel/BP_CleanTowelStack` |
| towel preview | `/Game/Bathhouse/Blueprints/Towel/BP_UsedTowelBin` |
| towel preview | `/Game/Bathhouse/Blueprints/Towel/BP_Washer` |
| towel preview | `/Game/Bathhouse/Blueprints/Towel/BP_Dryer` |
| towel mesh profile | `/Game/Bathhouse/Data/Towel/DA_TowelVisual_Washer` |
| towel mesh profile | `/Game/Bathhouse/Data/Towel/DA_TowelVisual_Dryer` |

## Carry HeldTransform

| Blueprint | Location | Rotation `(Pitch,Yaw,Roll)` | Scale |
|---|---:|---:|---:|
| `BP_BathhouseKey` | `(0,0,0)` | `(0,0,0)` | `(1,1,1)` |
| `BP_WetMop` | `(0,15,-10)` | `(0,0,90)` | `(1,1,1)` |
| `BP_TowelBasket` | `(0,10,-15)` | `(0,0,0)` | `(1,1,1)` |

- `HeldKeyAnchor`, world mesh scale, collision, throw 값과 key placement/recovery 계약은 수정하지 않았다.
- 실제 PIE에서 key hook take와 customer assignment를 실행했고 key Identity가 기존 anchor snap과 일치했다.
- mop pickup에서 held actor가 `HeldKeyAnchor`에 부착되고 원래 visual scale이 유지되는 것을 확인했다.
- 최종 재시작 후 세 Blueprint CDO에서 위 값이 그대로 재로드됐다.
- DefaultMap 외부 액터 패키지에 세 타입 모두 `HeldTransform` 인스턴스 override가 직렬화돼 있지 않아 placed actor도 새 class default를 상속한다.
- `BP_FirstPersonCharacter`의 `DropCarryAction`은 `IA_DropCarry`에, `IMC_FirstPerson`의 해당 action은 G에 연결된 기존 설정을 유지한다.
- `BathhouseSim.Interaction.PhysicalCarryDropSweepAndTransaction`이 성공해 mop/basket G intent, drop bounds, physics release, wall sweep, 실패 rollback 및 재획득 계약을 확인했다.

## Water Stain Variation

- `StainVisual` 이름을 유지하고 inherited `StainVisualRoot` 아래로 재부착했다.
- `InteractionCollision`의 root/relative transform/collision은 변경하지 않았다.
- `MaterialVariants`: `/Engine/EngineMaterials/WaterMaterial.DefaultWaterMaterial` 1개.
- `MinXYScale=(0.70,0.80)`, `MaxXYScale=(1.30,1.20)`.
- `MinYawDegrees=-180`, `MaxYawDegrees=180`.
- `Event Apply Stain Material Variant(SelectedMaterial)`을 `StainVisual.SetMaterial`, element index 0에 연결했다.
- Event Graph에 random 계산, Tick/Delay, cleaning/registry mutation은 추가하지 않았다.

PIE에서 기존 8개와 추가 spawn 5개, 총 13개 stain을 확인했다.

- 모든 X/Y scale이 authored 범위 안이고 Z scale은 `1`이었다.
- 모든 yaw가 authored 범위 안이고 material slot 0은 지정 material이었다.
- runtime tag로 signature를 기록한 뒤 3초 후 13개 모두 material/yaw/scale이 동일해 lifetime reroll이 없었다.
- 전체 자동화의 0/1/multiple material 후보, min/max normalization, Z=1, interaction collision 불변, hold 취소/완료 및 spacing 검증도 성공했다.

## Towel Editor Preview Authoring

| Blueprint | Type | Profile | State/Count | Seed | Relative transform | Layout |
|---|---|---|---:|---:|---|---|
| `BP_CleanTowelStack` | Stack | `DA_TowelVisual_Shelf` | Clean / 8 | 101 | L `(0,0,51)`, S `(0.3,0.2,0.02)` | Z spacing `105` |
| `BP_UsedTowelBin` | Stack | `DA_TowelVisual_UsedBin` | Used / 6 | 202 | L `(0,0,51)`, S `(0.22,0.22,0.022)` | Z spacing `100` |
| `BP_TowelBasket` | Stack | `DA_TowelVisual_Basket` | Used / 4 | 303 | L `(0,0,55)`, S `(0.65,0.6,0.1)` | Z spacing `110` |
| `BP_Washer` | Pile | `DA_TowelVisual_Washer` | Used / 6 | 404 | L `(31,0,45)`, S `(0.16,0.12,0.025)` | half extent `(60,150,480)` |
| `BP_Dryer` | Pile | `DA_TowelVisual_Dryer` | Clean / 6 | 505 | L `(31,0,45)`, S `(0.16,0.12,0.025)` | half extent `(60,150,480)` |

- 모든 relative rotation은 `(0,0,0)`이다.
- Washer/Dryer pile은 `ItemsPerLayer=4`, `LayerSpacing=120`, `MaxZJitter=15`, rotation min `(-4,-25,-6)`, max `(4,25,6)`을 유지했다.
- Washer profile: Used → `SM_Towel_used_common`, Wet → `SM_Towel_wet_common`.
- Dryer profile: Wet → `SM_Towel_wet_common`, Clean → `SM_Towel_clean_common`.
- 위 두 profile에 남아 있던 Engine Cube placeholder만 실제 towel mesh로 교체했다. Shelf/UsedBin/Basket profile과 gameplay 설정은 보존했다.

다섯 placed preview 대상 모두 같은 seed/input의 clear/rebuild에서 mesh 및 수치 transform이 동일했다.

- `ClearPreview` 후 visible instance와 ISM instance가 모두 0이었다.
- rebuild count는 각각 8/6/4/6/6이었다.
- preview 호출 전후 inventory initial state/count/capacity가 변하지 않았다.
- 별도 임시 test Actor의 `TowelSlotVisual`에 same-owner slot을 `C,A,B` 순으로 연결했다. 요청 4는 유효 slot capacity 3으로 clamp됐고 clear 0/rebuild 3을 확인했다.
- 임시 actor와 `/Game/__CodexTransient/BP_SlotPreviewTest`는 즉시 삭제했으며 `BP_DryingSpot`과 gameplay actor는 건드리지 않았다.

## Towel PIE

- PIE 2회를 실행했다.
- 다섯 actor 모두 authoritative inventory binding을 가졌다.
- Clean stack은 `Clean / 20 / revision 0`과 populated ISM bucket 1개를 표시했다.
- Used bin, basket, washer, dryer는 현재 authoritative `None / 0 / revision 0`을 표시하고 populated bucket은 0이었다.
- PIE에서 preview rebuild/clear를 호출해도 authoritative state/count/revision이 변하지 않았다.
- 두 번째 PIE 결과가 첫 번째와 같아 preview 잔존, 중복 ISM, 중복 timer/delegate 반응이 없었다.
- 별도 자동화에서 transfer/state swap, count animation, basket carry/drop, washer/dryer process와 recovery가 성공했다.

## Bath Snap 및 StateTree

- `BP_Bath`, `BP_BathhouseCustomer`, `ST_CustomerRoutine` graph는 수정하지 않았다.
- 두 Bath actor의 모든 slot capsule 위치를 덮도록 임시 `BlockAll` blocker 2개를 배치했다.
  - `(1750,-638,91)`
  - `(1750,0,190)`
- PIE에서 플레이어가 hook의 key를 직접 취득하고 queue-front customer에게 전달했다.
- 고객은 `Bath2/FacilitySlotB`를 선택해 `(1750,-638,91)`의 cached ActionPoint에 정확히 snap했다. 위치 오차는 `0.0`이었다.
- blocker 중심과 capsule 중심 거리도 `0.0`인 상태에서 snap이 거부되지 않았다.
- bath dwell 중 movement는 `MOVE_None`, capsule collision은 `QueryAndPhysics`, WorldStatic/Pawn response는 `Block`으로 유지됐다.
- navigation retry나 technical abort 없이 dwell을 끝내고 Drying → Dress → checkout까지 진행했다.
- 종료 후 movement는 `MOVE_Walking`으로 복구됐고 slot이 해제됐으며 key는 `OnCounter` 상태가 됐다.
- `BathhouseSim.Customer.BathSnapCleanup` 자동화도 성공해 interruption/technical-abort cleanup과 cached approach return 계약을 확인했다.
- 임시 blocker 2개는 PIE 후 삭제했고 map/external actor는 저장하지 않았다.

## Compile, Data Validation, Reload

- 수정 Blueprint 8개를 warnings-as-errors로 개별 compile: 8/8 성공.
- 수정 Blueprint 8개와 Data Asset 2개를 Data Validation: 10/10 `VALID`, asset error/warning 0.
- 의도한 위 10개 에셋만 저장했다.
- UE 5.8.1 새 commandlet 프로세스에서 디스크 재로드 후 다음을 다시 확인했다.
  - 세 HeldTransform 값과 unit scale
  - stain hierarchy, material/range 및 event-to-SetMaterial 연결
  - 다섯 preview state/count/seed/profile
  - Washer/Dryer profile의 실제 mesh와 Cube placeholder 부재
  - Blueprint compile 8/8, Data Validation 10/10
- 최종 commandlet 결과: `Success - 0 error(s), 1 warning(s)`. warning 1개는 ModelContextProtocol UE EULA 안내이며 asset/compiler warning이 아니다.
- 현재 저장 상태에서 `BathhouseSim` 전체 자동화: 17 success / 0 failure, `GIsCriticalError=0`.

## 최종 범위 확인

- 생성한 영구 Content asset은 없다.
- 저장한 Content는 위 10개 에셋뿐이다.
- 임시 slot test asset과 blocker는 제거했다.
- StateTree, map, external actor, Source, Config에는 이번 Editor 단계의 저장 변경이 없다.
- 별도 `USER_UNREAL.md` 후속 작업은 필요하지 않다.
