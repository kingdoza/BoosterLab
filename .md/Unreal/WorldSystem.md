# World Editor Authoring

## DefaultMap

- Level: `/Game/Maps/DefaultMap`
- World Partition Level이다.
- Placement Zone은 한 개다.
  - Actor: `/Game/Maps/DefaultMap.DefaultMap:PersistentLevel.BP_FacilityPlacementZone_C_UAID_F02F7433CA36D1FF02_1155169559`
  - Transform: Location `(600,-100,0)`, Rotation `(0,0,0)`, Scale `(1,1,1)`
  - `ZoneBounds` Extent: `(1400,900,10)`
  - `AllowedFacilityTags`: `Facility.Placeable`
  - `PlacementFloor` world plane: `Z=0`
- 선배치 Clothes Locker 두 개는 [FacilitySystem.md](FacilitySystem.md)의 고유 `RegistrationId`를 가진다.

## Navigation

- Recast actor: `/Game/Maps/DefaultMap.DefaultMap:PersistentLevel.RecastNavMesh_UAID_F02F7433CA3615F402-Default`
- 현재 디스크에 저장된 `RuntimeGeneration`은 `DynamicModifiersOnly`다.
- 설비 Blueprint의 body Static Mesh Navigation 활성과 helper Navigation 비활성은 저장돼 기존 Level instance에도 반영된다.
- 삭제된 `PlacementNavModifier`는 조사한 Bath 2개, Clothes Locker 2개, Washer 1개, Dryer 1개 external actor instance에 존재하지 않는다.

현재 MCP의 World Partition actor 저장 경로가 external actor package를 저장하지 못하므로, Recast를 `Dynamic`으로 바꾸는 작업은 [USER_UNREAL.md](../USER_UNREAL.md)에 남아 있다.
