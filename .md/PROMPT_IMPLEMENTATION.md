# 구현 프롬프트 — 설비 배치 Authoring·높이·Preview·Navigation·락커 초기화

## 입력과 범위

- 승인 기능 계약: `.md/PROMPT_ARCHITECTURE.md`
- 정본: `.md/0_ARCHITECTURE.md`, `.md/Architecture/PlacementSystem.md`, `.md/Architecture/FacilitySystem.md`, `.md/Architecture/CoreSystem.md`
- 사용자 결과: `.md/QNA_FEATURE_SPEC.md` Q1~Q3, 기술 결정: `.md/QNA_ARCHITECTURE.md` Q1~Q11
- reflected property/default subobject 즉시 제거 때문에 모든 영향 설비를 같은 migration 단위로 처리한다.
- 이 단계는 Source와 native automation만 수정한다. Content/Level/Project Settings asset 값은 후속 Unreal 단계로 인계한다.

## 목표

1. 설비 아이템 Held transform과 preview material을 Developer Settings로 공통화한다.
2. footprint X/Y 크기에서 grid cell을 파생하고 Definition cell authoring을 제거한다.
3. explicit zone floor와 footprint bottom으로 설치 Z를 한 번만 계산한다.
4. placed class-default Static Mesh를 복제하는 공통 native preview를 사용한다.
5. NavModifier를 제거하고 실제 body mesh collision을 Dynamic Recast에 맡긴다.
6. pre-placed locker를 stable ID 기반 startup reconciliation으로 등록한다.

## 변경 금지

- E/LMB/LCtrl/Mouse Wheel/G/Q 입력, LMB 우선순위와 UI
- zone tag, X/Y snap/Yaw와 footprint collision·containment·floor-support 의미
- facility↔item payload/class 분리, recovery gate와 Actor 교체 원자성
- locker lease/key-pool/customer locker 사용과 contents/water/processing/StateTree
- Clean Towel Stack과 Used Towel Bin placement opt-out

## Global Settings와 Definition

`UFacilityPlacementSettings`에 config `FacilityItemHeldTransform`, `TSoftObjectPtr<UMaterialInterface> ValidPreviewMaterial/InvalidPreviewMaterial`을 추가한다. Held getter는 location/rotation만 반환하고 scale은 `OneVector`로 만든다. item과 legacy fail-closed placed carry getter는 이 값만 읽는다. material 누락/load 실패나 비반투명 설정은 preview 실패다.

즉시 삭제:

- `APlaceableFacilityItemActor::HeldTransform`
- `UFacilityPlacementComponent::HeldTransform`과 getter
- `UFacilityPlacementDefinition::PreviewActorClass`
- `UFacilityPlacementDefinition::FootprintCellsX/Y`

Definition validation은 `PlacedFacilityClass` CDO의 placement component/footprint를 resolve한다. scaled full X/Y와 `GridSizeCm` 비율이 허용 오차 안의 양의 정수인지 검사하고 필요 시 non-reflected `FIntPoint`로 파생한다. 기존 stable id, tag, placed/item class, recovery mesh와 locker slot count 검증은 유지한다.

파생 계산은 다음 계약을 따른다.

```text
FullSizeX = 2 * PlacementFootprint.BoxExtent.X * abs(PlacementFootprint world-relative scale X)
FullSizeY = 2 * PlacementFootprint.BoxExtent.Y * abs(PlacementFootprint world-relative scale Y)
CellsX = RoundToInt(FullSizeX / GridSizeCm)
CellsY = RoundToInt(FullSizeY / GridSizeCm)
```

각 비율은 양수·finite이며 반올림한 정수와 허용 오차 안에서 같아야 한다. `FootprintCellsX/Y`를 대체하는 새 reflected cache를 만들지 않는다. 전역 grid나 footprint 크기가 바뀌면 다음 조회부터 파생값이 자동으로 바뀐다.

## Zone Floor와 Candidate Transform

`AFacilityPlacementZoneActor`는 기존 `ZoneBounds` root 아래 stable native `USceneComponent PlacementFloor`를 추가한다. trace point를 floor local XY에 투영하고 LCtrl quantization/누적 Yaw를 적용한다. candidate Z에 Bounds extent와 trace impact Z를 사용하지 않는다.

`UFacilityPlacementComponent`는 placed CDO root scale, footprint relative transform과 local bottom point `(0,0,-Extent.Z)`로 bottom world offset을 계산해 desired floor point에서 한 번 뺀 final Actor transform을 반환한다. 네 bottom corner가 Actor local Z=0에 놓이고 transform/scale이 finite인지 검증한다.

공통 계산식은 다음 의미를 유지한다. `P`는 `PlacementFloor` 위 desired bottom point, `Q`는 최종 Actor rotation, `S`는 placed CDO root scale, `R`은 footprint relative transform, `Bf`는 footprint local bottom center다.

```text
BottomOffsetWorld = Q.RotateVector(S * R.TransformPosition(Bf))
ActorLocation = P - BottomOffsetWorld
```

scale 곱의 실제 구현은 UE transform 규칙과 non-uniform scale을 보존해야 하며, 위 식을 이유로 extent나 relative Z를 다른 단계에서 다시 더하면 안 된다. 기존 Blueprint CDO는 Actor local install floor Z=0과 footprint 네 bottom corner Z=0이 일치하도록 Unreal 단계에서 migration한다.

`ValidateCurrentPlacement()`은 final candidate에 height/relative offset을 더하지 않는다. preview root, deferred spawn, footprint world transform, containment와 네 corner support trace는 같은 candidate와 `PlacementFloor` normal을 사용한다. geometry 구현은 별도 cpp로 나눌 수 있지만 owner는 기존 Component/Zone에 둔다.

## 범용 Native Preview

Player placement는 Definition을 조회하지 않고 native `AFacilityPlacementPreviewActor`를 직접 spawn한다. preview 초기화는 `PlacedFacilityClass` CDO에서 다음을 수행한다.

1. 유효 mesh가 있고 class-default에서 표시되는 non-instanced `UStaticMeshComponent`를 찾는다.
2. helper, hidden, editor-only, ISM과 runtime contents/pile/water 표현을 제외한다.
3. actor root 기준 transform과 mesh/render 기본값으로 transient mesh component를 만든다.
4. collision/overlap/physics/Tick/Navigation을 끄고 source material을 복사하지 않는다.
5. 모든 material slot을 current valid/invalid settings material로 교체한다.
6. source footprint relative transform/extent snapshot과 authoritative CDO geometry 정합을 검사한다.

eligible mesh 0개, material/component 생성 실패와 geometry mismatch는 live preview 실패이며 item을 유지하고 confirm을 막는다. 기존 failure text/event는 유지할 수 있으나 Blueprint가 mesh/material/validity를 결정하지 않는다. per-facility preview asset 정리는 Unreal 단계로 넘긴다.

preview mesh 수집 순서와 identity는 CDO component의 stable object name 기준으로 고정해 반복 생성 결과를 결정적으로 만든다. source component의 world transform을 복사하지 말고 placed Actor root 기준 상대 transform을 사용한다. source의 mesh asset, visibility/render flags와 cast-shadow 같은 비-domain 표현값만 복사하고 material override, collision profile, overlap delegate, physics body와 navigation state는 복사하지 않는다. preview validity 변경은 component를 재생성하지 않고 모든 slot material만 교체한다.

## Collision과 Navigation

다음을 즉시 삭제한다.

- facility/towel machine의 `PlacementNavModifier` property/default subobject
- `UFacilityPlacementComponent::Configure()` NavModifier 인자·포인터
- NavModifier relevancy 전환과 관련 include

primitive 배열, navigation tag, `FailsafeExtent`와 `NavArea_Null`을 추가하지 않는다. body Static Mesh의 Simple Collision, response와 `CanEverAffectNavigation`이 정본이며 Recast mode는 Editor에서 `Dynamic`으로 설정한다.

`UFacilityPlacementComponent`는 transition별 Actor collision bool 하나를 snapshot한다. stage/recovery unregister에서 `SetActorEnableCollision(false)`, placement 최종 commit과 recovery rollback에서 원값을 복원한다. 개별 component collision은 변경하지 않고 snapshot을 중복 캡처·소비하지 않는다.

collision snapshot 상태는 `없음 → 캡처됨 → 복원/소비됨`으로 단방향 전이한다. placement에서 새 Actor의 authored collision 값은 CDO/Construction 결과가 확정된 직후 한 번 캡처하고, staged 시작 전에 끈다. recovery는 원본 Actor의 현재 authored enable 값을 한 번 캡처한다. rollback 도중 domain 복구가 실패하면 snapshot을 소비하거나 collision을 먼저 복원하지 않는다. 정상 commit 뒤에는 stale snapshot을 남기지 않는다.

native helper는 `CanEverAffectNavigation=false`다. CDO validation은 footprint/package root, non-mesh helper, 알려진 presentation/interaction helper가 nav relevant이거나 collision 없이 nav data를 export하면 실패한다. 배치 시스템은 body primitive를 수집하지 않는다.

## Transaction 순서

Placement:

1. deferred placed Actor에 collision-off stage를 `FinishSpawning` 전에 적용한다.
2. payload import, final candidate와 domain 조건을 검증한다.
3. facility/locker를 silent 등록하되 staged/collision-off를 유지한다.
4. held item consume/source 제거 뒤 collision snapshot과 staged flag를 commit한다.
5. held/facility/capacity event를 발행한다.

Recovery:

1. staged item과 drop collision을 검증한다.
2. source collision을 끄고 facility/locker를 silent unregister한다.
3. item physics 활성화 후 source를 마지막에 제거하고 publish한다.
4. 실패하면 domain 재등록 성공 뒤 collision snapshot을 복원하고 item을 제거한다.

`StagePlacedDomainRegistration()`은 collision/staged flag를 commit하지 않는다. 기존 fault injection, callback 재진입 보상, held identity/Root scale/payload rollback을 유지한다. domain rollback 실패 시 collision을 먼저 켜지 않고 fail-closed/invariant 오류로 남긴다.

## Locker Startup Reconciliation

`ABathhouseFacilityActor`에 cooked runtime 직렬화 instance `FGuid RegistrationId`를 추가한다. Editor load와 duplicate/import에서 고유 ID를 자동 생성하며 Data Validation은 invalid/duplicate를 거부한다. CDO/runtime-spawned non-startup actor는 startup 정렬에 사용하지 않는다.

`RegistrationId`는 Editor migration에서 생성·저장하고 duplicate/import 시 원본과 다른 값을 부여한다. cooked runtime의 `BeginPlay`에서 임의 GUID를 생성해 순서를 바꾸는 fallback은 금지한다. cooked map에 invalid/duplicate ID가 남아 있으면 해당 locker만 fail-closed하고 한 번 오류를 기록한다.

pre-placed locker `BeginPlay`는 slot delegate 준비, collision-off stage와 pending 제출만 수행한다. actor별 expansion delegate retry를 제거하고 subsystem 호출용 idempotent silent register/commit/fail-closed API를 제공한다.

`UBathhouseFacilitySubsystem`은 Authority readiness, pending weak set과 permanent-failure/logged set을 소유한다. Initialize에서 world post-Actor-BeginPlay callback을 연결하고 Deinitialize에서 해제한다. Authority가 먼저 준비되면 post-begin에, 늦게 등록되면 등록 직후 한 번 reconcile한다. delayed Tick은 금지한다.

post-Actor-BeginPlay 경계는 `UWorld::OnWorldBeginPlay`처럼 모든 pre-placed Actor의 `BeginPlay` 제출이 끝난 뒤 호출되는 기존 world lifecycle event를 사용한다. Authority registration과 world callback이 같은 프레임에 겹쳐도 reconciliation guard가 중첩 실행을 막고, 새 pending 또는 readiness revision이 없으면 no-op한다.

Reconciliation:

1. invalid/already-registered/duplicate 제출을 정리하고 `RegistrationId`로 정렬한다.
2. Authority가 없으면 pending을 유지하고 transient readiness 진단만 한 번 기록한다.
3. remaining tier slots에 들어가는 bank를 facility+capacity silent 등록한다. 큰 bank가 안 들어가도 뒤의 작은 bank 검사를 계속한다.
4. bank 내부 실패는 해당 bank만 rollback/fail-closed하고, 성공 bank만 collision/Nav를 복원한다.
5. batch 뒤 accepted가 있으면 ClothesLocker facility event와 capacity event를 각각 한 번 발행한다.

`ULockerCapacitySubsystem`은 Authority 미준비, actual expansion limit와 invalid topology를 FText 비교 없이 typed result로 구분한다. 기존 atomic bank registration/rollback과 `bPublish=false`를 유지한다. actual limit/invalid·duplicate ID/topology는 actor별 한 번만 오류를 기록하고 startup retry에서 제거한다. runtime tier 상승, streaming 재정렬과 자동 재활성화는 범위 밖이다.

typed result는 최소한 `Success`, `AuthorityNotReady`, `ExpansionLimitExceeded`, `InvalidTopology`, `AlreadyRegistered`를 구분한다. `AuthorityNotReady`만 pending을 유지하며, `AlreadyRegistered`는 용량을 다시 더하지 않는 idempotent 성공/no-op으로 처리한다. batch publication observer가 호출될 때는 accepted locker의 facility registry, capacity 합계와 collision/Navigation 상태가 모두 최종값이어야 한다.

## Compatibility와 Editor 인계

- 삭제는 rename이 아니므로 Core Redirect를 추가하지 않는다.
- `EPlaceableFacilityMode` ordinal, `Mode`, release 값, `PackagePhysicalRoot` 이름과 fail-closed legacy carry API를 유지한다.
- `PlacementFloor`, `RegistrationId`는 새 stable reflected 이름이다.
- old/new Content 혼용 fallback을 만들지 않는다.

즉시 삭제되는 property/default subobject는 deprecated shadow property로 한 cycle 유지하지 않는다. 따라서 Source 변경과 Content migration 사이의 중간 상태는 지원 대상이 아니며, 구현 단계에서는 삭제 symbol을 참조하는 Content 목록을 Unreal 인계 문서에 빠짐없이 남긴다. 기존 enum ordinal과 유지 대상으로 명시된 reflected 이름 외에는 요구 범위 밖 rename을 하지 않는다.

구현 후 `.md/PROMPT_UNREAL.md`에 다음을 정확히 인계한다.

- 모든 영향 Definition/Blueprint compile·resave와 stale preview/cell/NavModifier 제거 확인
- footprint bottom local Z=0, PlacementFloor actual floor와 Project Settings Held/material 지정
- body mesh Simple Collision/Nav relevance, helper Nav 비관련과 Recast `Dynamic`
- locker instance ID 생성·고유성, unused preview Blueprint reference audit
- PIE에서 모든 facility 높이/preview와 NavMesh 회수·복구·재배치 검증

## 대상 Source

- `Public|Private/Placement/FacilityPlacementSettings.*`, `FacilityPlacementDefinition.*`, `FacilityPlacementComponent.*`
- `PlaceableFacilityItemActor.*`, `FacilityPlacementZoneActor.*`, `FacilityPlacementPreviewActor.*`
- `PlayerFacilityPlacementComponent*`, `FacilityActorConversionTransaction.*`
- `Public|Private/Facility/BathhouseFacilityActor*`, `BathhouseFacilitySubsystem.*`, `LockerCapacitySubsystem.*`, `BathhouseExpansionAuthority.*`
- `Public|Private/Towel/TowelProcessingMachineActor.*`
- `Private/Tests/FacilityPlacementAutomationTestProbe.*`, `FacilityPlacementAutomationTests.cpp`

새 runtime module/plugin dependency는 추가하지 않는다.

## Native Automation과 완료 조건

- global Held location/rotation, authored scale 무시와 item Root scale 보존
- scaled footprint→cell, grid 변경/non-multiple 실패와 38/40/50cm half-height 부양 회귀
- Bounds Z와 footprint height에 독립적인 common floor transform/support trace
- multi-mesh preview root-relative 복제, all-slot valid/invalid material과 실패 경로
- stage/recovery/commit/rollback collision snapshot 및 기존 fault injection 무손실
- helper nav validation, NavModifier 부재와 기존 zone/input/overlap/payload/recovery/key 회귀
- Authority/locker order permutation, ID 정렬·중복, 1/4/8 bank fitting, log/publication once
- Authority missing/late, pending duplicate와 capacity 이중 합산 없음
- `git diff --check`, UE 5.8 `Build.bat BathhouseSimEditor Win64 Development`, 관련 Placement/Facility automation
- 완료 후 현재 작업만 담은 `.md/PROMPT_REVIEW.md`, `.md/PROMPT_UNREAL.md`를 작성한다.
- Content/PIE 미수행은 성공으로 보고하지 않고 Editor 검증으로 명시한다.
