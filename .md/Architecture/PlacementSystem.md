# Placement System

## Implementation Status

배치 설비 Actor와 전용 `APlaceableFacilityItemActor`의 staged 양방향 transaction, 공통 Held 설정, footprint 파생 cell, 명시적 zone floor, 범용 native preview, 기본 collision 기반 Dynamic Navigation 및 pre-placed 락커 reconciliation은 Source와 native automation까지 구현되어 있다. Definition/Blueprint/Level/Project Settings migration과 PIE 검증은 후속 Unreal 단계다.

## Source Scope

```text
Source/BathhouseSim/Public/Placement/
  FacilityPlacementTypes.h
  FacilityPlacementPayload.h
  FacilityPlacementSettings.h
  PlaceableFacility.h
  FacilityPlacementDefinition.h
  FacilityPlacementComponent.h
  PlaceableFacilityItemActor.h
  FacilityPlacementZoneActor.h
  FacilityPlacementPreviewActor.h
  PlayerFacilityPlacementComponent.h

Source/BathhouseSim/Private/Placement/
  FacilityPlacementCollisionUtils.h/.cpp
  FacilityActorConversionTransaction.h/.cpp
  FacilityPlacementDefinition.cpp
  FacilityPlacementPayload.cpp
  FacilityPlacementSettings.cpp
  FacilityPlacementComponent.cpp
  FacilityPlacementGeometry.cpp
  FacilityPlacementZoneActor.cpp
  FacilityPlacementPreviewActor.cpp
  FacilityPlacementPreviewSource.h/.cpp
  PlaceableFacilityItemActor.cpp
  PlaceableFacilityItemCollision.cpp
  PlayerFacilityPlacementComponent.cpp
  PlayerFacilityPlacementValidation.cpp

Source/BathhouseSim/Public|Private/Facility/
  BathhouseFacilityActor.*
  BathhouseFacilityPlacementDomain.cpp
  BathhouseFacilitySubsystem.*
  BathhouseFacilityStartup.cpp
  LockerCapacitySubsystem.*
  BathhouseExpansionAuthority.*

Source/BathhouseSim/Public|Private/Towel/
  TowelProcessingMachineActor.*

Source/BathhouseSim/Private/Tests/
  FacilityPlacementAutomationTestProbe.*
  FacilityPlacementAutomationTests.cpp
```

## Responsibilities

- 배치 설비 Actor와 전용 회수 아이템 Actor의 분리된 lifecycle 및 Definition/payload
- held 설비 아이템의 preview, zone trace, grid/snap/rotation과 Actor 교체 commit
- footprint 기반 크기·바닥 정렬·구역 포함·collision·floor support 검증
- 설치 설비의 Q Hold 회수 query/progress/cancel/commit
- placed Actor collision과 Dynamic Navigation lifecycle 조율
- 확장 단계별 락커 수용량과 pre-placed 락커의 결정적 초기 등록

Placement는 설비 contents, 목욕탕 물, customer 행동, key 상태와 UI hierarchy를 변경하지 않는다. 설비별 조건은 `IPlaceableFacility`과 원래 domain owner가 판정한다.

## State Owners

| 책임 | Owner |
|---|---|
| grid, Yaw 간격, 회수 시간·거리, 공통 Held 위치·회전, preview 머터리얼 | `UFacilityPlacementSettings` |
| stable id, facility tag, placed/item class, recovery item mesh, locker slot 수 | `UFacilityPlacementDefinition` |
| placed Definition, footprint, transition와 Actor collision snapshot | `UFacilityPlacementComponent` |
| payload, recovery item mesh physics와 lifecycle | `APlaceableFacilityItemActor` |
| preview session, 누적 Yaw, Q target/경과 시간과 상위 transaction 조율 | `UPlayerFacilityPlacementComponent` |
| 범용 preview의 transient mesh 표현과 validity material 교체 | `AFacilityPlacementPreviewActor` |
| zone bounds, 명시적 floor plane와 allowed tag | `AFacilityPlacementZoneActor` |
| expansion readiness, pending locker와 startup reconciliation | `UBathhouseFacilitySubsystem` |
| 설치 락커 용량, lease, action-slot 후보와 bank 등록 | `ULockerCapacitySubsystem` |
| 설비별 회수 조건 | `IPlaceableFacility` 구현 Actor와 해당 domain owner |
| 실제 held Actor identity | `UPlayerCarryComponent` |

## Global Settings

`UFacilityPlacementSettings : UDeveloperSettings`가 Project Settings의 공통 authoring 정본이다.

- `GridSizeCm = 10.0`
- `RotationStepDegrees`
- `RecoveryHoldSeconds`
- `RecoveryDropZOffsetCm`
- `PlacementTraceDistance`, `RecoveryTraceDistance`
- `FacilityItemHeldTransform`
- `ValidPreviewMaterial`, `InvalidPreviewMaterial`

`FacilityItemHeldTransform` getter는 location/rotation만 반환하고 scale을 항상 `OneVector`로 정규화한다. `APlaceableFacilityItemActor::GetHeldTransform()`과 legacy placed Actor의 fail-closed carry getter는 이 설정만 읽으며 per-Actor 값을 소유하지 않는다.

두 preview material은 config에 저장 가능한 soft asset reference다. preview 시작 시 둘 다 resolve되고 translucent blend를 제공해야 하며 모든 material slot을 전면 교체한다. 누락·load·blend 검증 실패는 preview 초기화 실패이고 placement는 held item을 보존한 채 fail-closed한다.

## Definition And Footprint

`UFacilityPlacementDefinition`은 다음 값만 소유한다.

- `StableId`, `FacilityTags`
- `PlacedFacilityClass`
- 공통 `APlaceableFacilityItemActor` 파생 `RecoveryItemClass`
- 설비별 `RecoveryItemMesh`
- locker인 경우 `LockerSlotCount`

`PreviewActorClass`와 `FootprintCellsX/Y`는 즉시 제거한다. 모든 Definition은 공통 native preview를 사용하고 실제 footprint는 `PlacedFacilityClass` CDO의 `UFacilityPlacementComponent`가 참조하는 `PlacementFootprint` 하나가 정본이다.

footprint cell은 저장하지 않고 요청 시 파생한다.

```text
FullSizeXY = 2 * BoxExtentXY * abs(FootprintComponentScaleXY) * abs(PlacedRootScaleXY)
CellsXY = round(FullSizeXY / GridSizeCm)
```

각 축의 full size는 양수·finite이고 `GridSizeCm`의 정수배여야 한다. 허용 오차 안에서 정수배가 아니면 Definition/Data Validation과 runtime placement가 실패한다. grid 또는 footprint를 바꾸면 cell 값을 별도로 갱신하지 않는다.

footprint authoring 계약:

- 설비 Actor local Z=0은 실제 설치 바닥이다.
- scaled/relative transform이 적용된 `PlacementFootprint`의 네 bottom corner도 Actor local Z=0이다.
- footprint 중심을 Actor 바닥에 두지 않으며 일반적인 axis-aligned box는 relative Z가 scaled half-height가 되도록 authoring한다.
- bottom corner가 같은 plane에 놓이지 않는 pitch/roll, non-finite transform과 0 scale은 validation 실패다.

Recovery item collision과 설치 footprint는 서로 대체하지 않는다. `RecoveryItemMesh`는 기존 동일 규격 직육면체/simple-box/physics 계약을 유지한다.

활성 Definition은 하나의 공통 Blueprint 파생 클래스를 `RecoveryItemClass`로 공유할 수 있다. runtime/Data Validation은 `APlaceableFacilityItemActor` 자체 또는 그 자식 클래스만 허용한다. 선택된 class CDO의 `ItemRoot`/Actor scale이 설비 회수 아이템의 공통 물리·표현 scale 정본이며, 회수 collision query와 실제 spawn이 같은 CDO scale을 사용한다. `FacilityItemHeldTransform` scale은 계속 무시하고 위치·회전만 적용한다.

## Placement Zone And Candidate Transform

`AFacilityPlacementZoneActor`는 기존 `ZoneBounds` root를 유지하고 그 아래에 stable native `PlacementFloor` `USceneComponent`를 추가한다. `PlacementFloor`의 world XY plane이 설치 높이의 유일한 정본이며 `ZoneBounds.BoxExtent.Z`와 trace impact Z는 높이에 사용하지 않는다.

grid 표현은 preview 동안 항상 보이고 LCtrl은 위치 quantization만 켠다. LCtrl을 놓아도 기존 누적 Yaw는 유지된다.

후보 계산은 두 단계만 가진다.

1. trace impact를 `PlacementFloor` local space로 바꾸고 Z=0으로 투영한다. LCtrl 중에만 local X/Y를 기존 grid로 quantize하고 floor 회전 기준으로 누적 Yaw를 적용해 desired footprint-bottom frame을 만든다.
2. placed CDO root scale과 footprint relative transform을 사용해 footprint local bottom center가 위 frame 원점에 오도록 Actor translation을 한 번 역산한다.

footprint local bottom center `Bf=(0,0,-Extent.Z)`, footprint-to-root transform `R`, 최종 Actor rotation/scale `Q/S`, floor point `P`일 때 개념식은 다음과 같다.

```text
BottomOffsetWorld = Q.RotateVector(S * R.TransformPosition(Bf))
ActorLocation = P - BottomOffsetWorld
```

실제 구현은 UE `FTransform` 합성 순서와 non-uniform scale을 보존하고 bottom corner 검증으로 결과를 확인한다. Zone half-height나 footprint half-height를 이후 단계에서 다시 더하지 않는다.

preview root transform, final deferred spawn transform, footprint world transform, containment와 네 모서리 floor-support trace는 모두 이 최종 Actor transform 하나를 사용한다. `ContainsFootprint`의 기존 Zone Bounds X/Y 포함 계약과 wheel rotation은 유지한다.

## Generic Native Preview

`AFacilityPlacementPreviewActor` 하나를 직접 spawn하며 Definition별 preview class를 조회하지 않는다. 이 class는 Blueprint 파생과 domain 기능 없이 transient 표현만 담당한다.

preview 초기화는 gameplay Actor를 spawn하지 않고 `PlacedFacilityClass`의 native CDO component와 Blueprint SCS hierarchy를 함께 순회한다. inherited SCS override는 최종 generated class 기준 template을 사용하며 cooked fast-path component data가 있으면 preview Actor에 미등록 scratch component로 materialize한 뒤 표현값만 읽고 제거한다. 다음 source를 읽는다.

- 유효 mesh가 있고 class-default에서 표시되는 non-instanced `UStaticMeshComponent`
- source Actor root 기준으로 계산한 component transform
- visibility와 필요한 기본 render 속성
- placement component의 footprint relative transform과 BoxExtent snapshot

helper, hidden, editor-only, `UInstancedStaticMeshComponent`와 runtime contents/pile/water 표현은 복제하지 않는다. 각 source마다 transient `UStaticMeshComponent`를 만들고 preview root에 root-relative transform으로 붙인다. source material은 복제하지 않고 validity에 따라 모든 slot을 `ValidPreviewMaterial` 또는 `InvalidPreviewMaterial`로 설정한다.

preview Actor와 생성 component는 collision/overlap/physics/Tick과 Navigation을 항상 끈다. placed class identity, footprint root-relative transform/extent와 root scale snapshot은 매 refresh/confirm에서 authoritative CDO와 비교하며 mismatch면 preview와 confirm을 fail-closed한다. authoritative footprint는 계속 placed CDO다. eligible mesh가 없거나 hierarchy 해석, component 생성·material 적용이 실패하면 live preview가 성립하지 않으므로 confirm을 허용하지 않는다.

## Collision And Navigation

별도 navigation geometry를 만들지 않는다. `PlacementNavModifier`, `FailsafeExtent`, primitive reference 배열과 navigation tag를 제거하고 다음 Unreal 기본 계약을 사용한다.

- 실제 몸체 `UStaticMeshComponent`의 기존 Simple Collision, collision response와 `CanEverAffectNavigation`이 정본이다.
- `RecastNavMesh.RuntimeGeneration = Dynamic`이 component 등록·collision 변경·Actor 제거의 dirty area를 갱신한다.
- `PlacementFootprint`, `PackagePhysicalRoot`, 상호작용 Box, towel presentation, 슬롯과 Action/Approach Point는 `CanEverAffectNavigation=false`다.
- non-mesh helper primitive가 Navigation relevant이면 Data Validation 실패다.

배치 시스템은 몸체 primitive를 수집하지 않는다. `UFacilityPlacementComponent`는 `None -> PendingConstruction -> Captured -> None`의 단방향 placement snapshot과 `None -> Captured -> None` recovery snapshot을 구분한다. deferred Actor는 `FinishSpawning` 전에 Actor collision을 끄고, Construction이 명시적으로 다시 활성화한 authored 결과를 snapshot에 합친 직후 다시 끈다. snapshot 준비·확정·pre-commit 검증·복원 실패는 item consume와 publication 전에 transaction 실패로 전파한다. 개별 component collision 설정은 변경하지 않는다.

- staged placement: deferred Actor를 `FinishSpawning`하기 전에 collision snapshot 후 비활성화
- placement success: silent domain 등록과 held item 소비가 끝난 뒤 collision 복원, 그 다음 publication
- placement failure: collision이 꺼진 staged Actor 제거
- recovery stage: 원본 collision 비활성화 후 silent domain unregister
- recovery rollback: domain 재등록 후 이전 collision 복원
- recovery success: collision이 꺼진 원본을 마지막에 제거
- pending/거부된 startup locker: collision과 facility/capacity/Navigation domain 비활성 유지

`SetActorEnableCollision`은 일반 primitive의 query collision과 Navigation relevancy 갱신을 UE에 전달한다. collision 없이도 nav data를 내보내는 modifier/custom exporter는 이 계약에서 허용하지 않는다.

## Placement And Recovery Transaction

기존 E pickup, LMB confirm, LCtrl snap, Mouse Wheel Yaw, G free drop과 Q Hold recovery 입력은 유지한다. LMB owner 우선순위는 `Computer > Placement > Equipment`다.

placement는 후보를 같은 frame에 재검증하고 새 placed Actor를 collision/domain 비활성 staged 상태로 만든다. payload import와 silent facility/locker 등록 후 held item을 소비하며, 그 뒤 Actor collision을 복원하고 held/facility/capacity event를 한 번 publish한다. `StagePlacedDomainRegistration()`은 collision을 복원하거나 staged flag를 commit하지 않는다. 최종 commit API만 이를 수행한다.

recovery는 staged item 준비와 collision 확인 후 원본 Actor collision/domain을 silent 비활성화한다. item physics 활성화와 원본 파괴가 성공한 뒤 publication한다. 실패하면 원본 domain과 Actor collision snapshot을 복원하고 item을 제거한다.

Actor collision restore는 실패 가능한 domain rollback 뒤에 수행한다. callback 재진입과 Actor 파괴 보상, held identity/Root scale/payload와 기존 회수 조건은 현재 transaction 계약을 유지한다.

## Preserved Domain Contracts

- `IPlaceableFacility`은 side-effect-free placement/recovery query, typed payload export/import와 silent domain stage/rollback을 제공한다. `IPhysicalCarryable`은 별도 recovery item만 구현한다.
- `FFacilityPlacementPayload`는 Definition과 item-outer domain instance data만 보관한다. contents, bath water, processing progress, slot/customer와 registry reference는 전달하지 않는다.
- E는 free-world facility item pickup, LMB는 confirm, LCtrl은 snap, Mouse Wheel은 Yaw, G는 held-position free drop, Q Hold는 placed facility recovery다. LMB 우선순위는 `Computer > Placement > Equipment`다.
- Washer/Dryer는 inventory 0과 `Waiting`, Bath는 모든 slot `Available`과 water `Empty`, locker bank는 모든 action slot `Available`과 lease-safe capacity일 때만 회수된다.
- recovery item은 footprint world bottom 기준 drop Z offset에 생성되고 원본 facility scale을 복사하지 않는다. free-world physics/CCD/Pawn Ignore와 무충격 생성 계약을 유지한다.
- `UBathWaterStateComponent`의 `Empty/Filling/Filled/Draining`만 Bath 회수 gate의 정본이며 Blueprint 물 표현은 상태를 소유하지 않는다.
- `InstalledLockerCapacity`는 등록된 action slot 합계이고 provisional/committed lease는 check-in과 함께 원자적으로 commit/rollback한다. 탈의·착의는 서로 다른 random slot을 사용할 수 있다.
- Expansion tier의 `KeyPoolSize`와 `MaxInstalledLockerSlots`는 독립 정본이다. locker 변경은 key 수·번호나 이미 배정된 customer key를 바꾸지 않는다.
- `ACleanTowelStackActor`와 `AUsedTowelBinActor`는 towel token owner라 Actor 변환과 Q recovery에서 제외된다.

## Locker Startup Reconciliation

`UBathhouseFacilitySubsystem`이 Expansion Authority readiness와 pre-placed locker pending set을 소유한다. `ULockerCapacitySubsystem`은 topology·한도 검증과 bank/capacity 등록만 소유한다. 락커 Actor별 `OnExpansionAuthorityChanged` 재시도는 제거한다.

pre-placed locker는 `BeginPlay`에서 idempotent하게 slot delegate를 준비하고 Actor collision을 끈 뒤 Facility Subsystem에 pending 등록한다. Authority가 먼저 시작했더라도 즉시 개별 등록하지 않는다. Facility Subsystem은 world의 post-Actor-BeginPlay lifecycle event에서 한 번 reconciliation하며, Authority가 그 이후 runtime에 등록되면 그 등록 시점에 한 번 수행한다. 지연 Tick은 사용하지 않는다.

각 pre-placed locker instance는 cooked build에도 저장되는 `FGuid RegistrationId`를 가진다. Editor가 기존 instance와 duplicate/import에 고유 ID를 생성하고 Data Validation은 invalid/duplicate ID를 거부한다. UE Editor-only `AActor::ActorGuid`는 runtime 순서에 사용하지 않는다.

reconciliation:

1. invalid weak actor와 이미 등록된 actor를 제거하고 `RegistrationId`로 정렬한다.
2. Authority가 없으면 capacity 오류를 만들지 않고 pending을 유지하며 transient readiness 진단만 한 번 기록한다.
3. structural topology를 검증하고 남은 tier slot 수에 들어가는 bank만 silent 등록한다. 큰 bank가 들어가지 않으면 거부하되 뒤의 작은 bank 검사는 계속한다.
4. bank별 facility+capacity 등록이 모두 성공한 뒤에만 Actor collision을 복원한다. 부분 실패는 해당 bank 내부 등록만 rollback하고 fail-closed한다.
5. accepted bank가 하나 이상이면 모든 내부 변경 후 ClothesLocker facility publication 한 번과 capacity publication 한 번만 발행한다.

accepted ID는 owner Actor weak reference와 함께 보관한다. 동일 actor의 pending/accepted/registered 재제출은 no-op이며, 같은 ID를 제출한 다른 actor만 duplicate로 거부한다. active reconciliation의 publication callback이 pending 또는 Authority revision을 바꾸면 guard 해제 전 즉시 tail pass를 수행해 유실 없이 다음 batch를 처리한다. 실제 tier 초과·잘못된 topology·invalid/duplicate ID는 actor별 영구 오류를 한 번만 기록하고 이번 startup reconciliation에서 재시도하지 않는다. runtime tier 상승, streaming 정책과 자동 재활성화는 이번 범위 밖이다.

## Expansion, Capacity And Key Pool

`InstalledLockerCapacity`는 성공 등록된 action slot 합계이고 `ActiveLeaseCount`와 provisional lease 계약을 유지한다. `CanInstallLockerSlots`는 Authority 미준비와 실제 한도 초과를 구조적으로 구분한다. player placement 중 Authority가 미준비면 transient 실패로 item/preview를 유지하며 `ExpansionLimit` 오류로 기록하지 않는다.

물리 key 수는 Expansion tier의 `KeyPoolSize`에만 종속된다. locker registration, 거부, 회수와 재배치는 key 번호, 수량과 customer key를 변경하지 않는다.

## Compatibility And Migration

사용자가 즉시 제거를 승인한 reflected/native 계약:

- `APlaceableFacilityItemActor::HeldTransform`
- `UFacilityPlacementComponent::HeldTransform`
- `UFacilityPlacementDefinition::FootprintCellsX/Y`
- `UFacilityPlacementDefinition::PreviewActorClass`
- placed facility와 towel machine의 `PlacementNavModifier` default subobject/property/configuration

rename이 아니라 property/component 삭제이므로 Core Redirect로 대체하지 않는다. Source compile 뒤 영향받는 Definition과 Blueprint를 같은 Editor migration에서 load, compile, resave하고 stale property/component reference를 검사한다. migration 전후를 섞은 Content 상태는 지원하지 않는다.

보존 계약:

- `EPlaceableFacilityMode` ordinal, legacy `Mode`, release 값, `PackagePhysicalRoot` 이름과 fail-closed placed carry API
- placed/item class 분리, payload와 Root scale 계약
- zone tag, input, 회수 gate와 atomic Actor 교체 rollback
- Clean Towel Stack/Used Towel Bin placement opt-out

## Blueprint/API And Editor Contracts

- Project Settings: 공통 Held Transform과 valid/invalid preview material 지정
- 공통 설비 아이템 Blueprint: `/Game/Bathhouse/Blueprints/Placement/BP_PlaceableFacilityItem`, parent `APlaceableFacilityItemActor`; `ItemRoot` scale과 carry 표현 기본값만 authoring
- PlacementZone: `PlacementFloor`를 실제 바닥 plane에 배치하고 기존 Bounds/tag/grid 표현 유지
- placed facility Blueprint: footprint bottom을 Actor local Z=0에 맞추고 실제 body mesh collision/Nav relevance를 검증
- helper primitive와 모든 Action/Approach Point: Navigation 비관련
- Level RecastNavMesh: Runtime Generation `Dynamic`
- 기존 per-facility preview class와 NavModifier authoring 제거
- locker instance: 자동 생성된 persistent RegistrationId가 유효·고유한지 확인

Blueprint는 preview mesh 복제, material slot 교체, 후보 transform, Dynamic Navigation 전환, pending reconciliation과 publication을 변경하지 않는다.

## Dependencies

- Placement -> Interaction carry/query/result contract
- Facility/Towel -> Placement placeable-facility contract
- Facility -> Placement collision/domain activation API
- Customer -> Facility locker capacity/slot API
- Placement/Facility -> Engine NavigationSystem과 GameplayTags
- 기존 `DeveloperSettings` dependency만 사용하며 새 module/plugin은 추가하지 않는다.

## Verification

- 공통 Held transform이 모든 facility item에 같고 Scale은 보존되는지 확인한다.
- grid/footprint 변경 시 파생 cell과 non-multiple validation을 확인한다.
- bath/washer/dryer/locker footprint bottom이 같은 floor plane에 놓이는지 확인한다.
- generic preview가 class-default 복합 mesh를 복제하고 valid/invalid material을 모든 slot에 적용하는지 확인한다.
- preview/stage에서 collision/Nav가 없고 commit/rollback 뒤 Actor collision snapshot이 복원되는지 확인한다.
- PIE 전후 대형 `NavArea_Null`이 없고 recovery/replacement 위치의 Dynamic NavMesh가 갱신되는지 확인한다.
- Facility/Queue Approach Point가 agent radius를 고려한 생성 NavMesh 위에 남는지 확인한다.
- Authority/locker BeginPlay 순서 permutation에서 accepted set, capacity와 publication 횟수가 같은지 확인한다.
- total slot 초과 시 stable ID 순서로 fitting bank만 등록되고 오류가 한 번만 발생하는지 확인한다.
