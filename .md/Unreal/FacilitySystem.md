# Facility Editor Authoring

## 설비 Blueprint

| Asset | Parent Class | 역할 |
|---|---|---|
| `/Game/Bathhouse/Blueprints/Facility/BP_Bath` | `/Script/BathhouseSim.BathhouseFacilityActor` | Bath 시설과 3개 Facility Slot |
| `/Game/Bathhouse/Blueprints/Facility/BP_Shower` | `/Script/BathhouseSim.BathhouseFacilityActor` | Shower 시설과 2개 Facility Slot |
| `/Game/Bathhouse/Blueprints/Facility/BP_ClothesLocker` | `/Script/BathhouseSim.BathhouseFacilityActor` | 1칸 Clothes Locker |
| `/Game/Bathhouse/Blueprints/Facility/BP_ClothesLocker_4` | `/Script/BathhouseSim.BathhouseFacilityActor` | 4칸 Clothes Locker |
| `/Game/Bathhouse/Blueprints/Facility/BP_ClothesLocker_8` | `/Script/BathhouseSim.BathhouseFacilityActor` | 8칸 Clothes Locker |
| `/Game/Bathhouse/Blueprints/Towel/BP_Washer` | `/Script/BathhouseSim.TowelProcessingMachineActor` | Washer |
| `/Game/Bathhouse/Blueprints/Towel/BP_Dryer` | `/Script/BathhouseSim.TowelProcessingMachineActor` | Dryer |
| `/Game/Bathhouse/Blueprints/Towel/BP_CleanTowelStack` | `/Script/BathhouseSim.CleanTowelStackActor` | placement opt-out 수건 공급대 |
| `/Game/Bathhouse/Blueprints/Towel/BP_UsedTowelBin` | `/Script/BathhouseSim.UsedTowelBinActor` | placement opt-out 사용 수건함 |

설비 Blueprint 9개는 삭제된 inherited `PlacementNavModifier`를 보유하지 않는다. 배치 대상 7개의 body Static Mesh만 collision과 Navigation을 담당하고, `PlacementFootprint`, `PackagePhysicalRoot`, slot, water/contents/presentation helper는 Navigation 비활성이다. 상세 extent와 body 설정은 [PlacementSystem.md](PlacementSystem.md)에 있다.

## Clothes Locker authoring

- `BP_ClothesLocker`, `BP_ClothesLocker_4`, `BP_ClothesLocker_8`의 Definition `LockerSlotCount`는 각각 1, 4, 8이다.
- 4칸 Blueprint는 `LockerSlot01~04`, 8칸 Blueprint는 `LockerSlot01~08`을 가진다.
- Level에 선배치된 1칸 Locker 두 개의 저장 ID는 유효하고 서로 다르다.
  - `BP_ClothesLocker_C_UAID_F02F7433CA3615F402_1440894859`: `837E39DD-4CE7-A610-79EA-BB8B2642FB0D`
  - `BP_ClothesLocker_C_UAID_F02F7433CA3615F402_1441212860`: `657F1FCD-40CC-C44F-A1C1-DCAC6D2BB8CC`
- 새로 배치하거나 Editor에서 복제한 Locker는 native `RegistrationId` 생성 계약을 따른다. PIE duplicate는 저장된 ID를 바꾸지 않는다.

현재 저장 상태를 불러온 깨끗한 PIE 2회에서 duplicate ID, expansion limit, capacity 중복 합산 또는 startup locker 오류가 발생하지 않았다. 개별 Level actor Data Validation과 World Partition 저장이 필요한 항목은 [USER_UNREAL.md](../USER_UNREAL.md)에 남아 있다.
