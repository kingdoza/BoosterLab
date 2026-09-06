# Codex Agent — Implementation Task

## Objective

현재 player interaction, single physical carry, customer routine, facility/towel machine, native Interaction Prompt 계약을 보존하면서 설비 배치·Q Hold 회수, locker capacity lease와 expansion-owned key pool Source를 구현하라. 설치된 설비의 별도 이동 기능은 만들지 않고 동일 Actor instance가 `Placed`와 `Packaged` mode를 전환한다.

## Required Reading

`.md/AGENT_WORKFLOW.md`, `.md/AGENT_IMPLEMENTATION.md`, `.md/PROMPT_IMPLEMENTATION.md`, `.md/0_ARCHITECTURE.md`, 관련 Core/Placement/Interaction/PhysicalCarry/Character/Facility/Customer/Towel/UI System 문서와 `.md/QNA_IMPLEMENTATION.md`를 전부 읽는다.

아키텍처와 구현이 충돌하거나 실제 UE 5.8 API 확인 뒤에도 선택이 남으면 `QNA_IMPLEMENTATION.md`에 질문 하나당 결정 하나로 작성하고 중단한다.

## Scope

- 허용: `Source/BathhouseSim/Public`, `Source/BathhouseSim/Private`, `BathhouseSim.Build.cs`, 관련 정본 상태 갱신
- 금지: `Content/` 수정/resave, InputAction/IMC/WBP/StateTree asset 변경, 설치 설비 이동 모드, 신발 gameplay 신규 구현
- `Config/`는 이번 Source 단계에서 변경하지 않는다.
- 기존 reflected type/property/function/component를 rename/delete하지 않는다.

## Fixed Decisions

- 공통 값 owner는 `UFacilityPlacementSettings : UDeveloperSettings`다.
- 기본 grid `10 cm`, wheel tick Yaw 간격, 공통 Q hold 시간과 trace 거리는 Project Settings 값이다.
- 모든 zone은 공통 grid size를 쓰고 local origin/axes/bounds와 allowed facility tags만 소유한다.
- LMB owner 순서는 `Computer > Placement > Equipment`다.
- facility item을 들면 즉시 preview, LCtrl Hold는 위치 snap, wheel은 Yaw, LMB는 confirm이다.
- E exact slot, G weak held-position free drop은 유지하고 ESC는 placement에서 no-op다.
- Q Hold 회수는 현재 held item과 무관하고 회수품을 자동 pickup하지 않으며 impulse도 주지 않는다.
- key와 locker는 대응하지 않는다. key 수는 expansion tier, customer capacity는 placed locker action slot 총수다.
- active lease보다 용량이 작아지는 locker 회수는 실패한다.
- target locker bank의 slot이 하나라도 Reserved/Occupied면 회수는 실패한다.
- 신발 상태와 시설은 신규 runtime에서 사용하지 않는다.

## 1. Module And Types

- `DeveloperSettings` runtime module dependency를 실제 `UDeveloperSettings` include/use와 함께 추가한다.
- `Placement` Public/Private 폴더와 `FacilityPlacementTypes.h`를 추가한다.
- 기존 ordinal 뒤에 `EPhysicalCarryKind::Facility`, `EPlayerInteractionIntent::PlacementConfirm`, `FacilityRecovery`를 append한다.
- `EPlaceableFacilityMode`는 authoritative하게 `Placed`, `Packaged`만 둔다. Held/Preview는 carry/player session에서 파생한다.
- transaction result는 success, localized `FText` failure와 rollback 가능한 failure code를 제공한다.

## 2. Global Settings And Definition

`UFacilityPlacementSettings`:

- `GridSizeCm = 10.0f`
- `RotationStepDegrees`
- `RecoveryHoldSeconds`
- `PlacementTraceDistance`, `RecoveryTraceDistance`
- 모든 값에 유효 clamp와 category/display metadata를 둔다.

`UFacilityPlacementDefinition : UPrimaryDataAsset`:

- stable id, facility Gameplay Tags, preview Actor class
- footprint X/Y cell count
- locker Definition이면 `LockerSlotCount`
- immutable authoring data와 asset validation

전역 값을 Definition, Actor와 player component에 복제하지 않는다.

## 3. Facility Actor Composition

`IPlaceableFacility`과 `UFacilityPlacementComponent`를 추가한다.

- side-effect-free placement/recovery query
- Definition, `PlacementFootprint` Box, `PackagePhysicalRoot`, exact fixed-slot binding resolve/validation
- `Placed/Packaged` presentation와 domain registration commit hook
- reentrancy guard, snapshot과 rollback
- BeginPlay/EndPlay에서 현재 mode에 맞는 registration/cleanup

기존 `ABathhouseFacilityActor`와 `ATowelProcessingMachineActor`가 `IPlaceableFacility`, `IPhysicalCarryable`을 직접 구현하고 Component에 위임한다. Facility base는 packaged E pickup용 `IPlayerInteractable`도 제공하고, 기존 derived interactable은 placed query를 유지하되 packaged mode를 base 경로로 보낸다. 중복 interface 상속은 정리하되 reflected class/API는 보존한다. Definition이 없으면 non-placeable이다. 모든 carryable용 공통 Actor/Component와 generic facility-item Actor는 만들지 않는다.

`Placed`에서는 package pickup/physics를 숨기고 `Packaged`에서는 facility/towel-machine use와 Navigation 등록을 숨긴다. Blueprint는 두 표현을 event로 바꾸되 mode 정본을 소유하지 않는다.

## 4. Zone, Preview And Placement

`AFacilityPlacementZoneActor`, 표현 전용 `AFacilityPlacementPreviewActor`, `UPlayerFacilityPlacementComponent`를 구현한다.

- camera-center compatible-zone trace
- zone local plane/origin/axes transform
- preview 동안 grid 항상 표시
- LCtrl일 때만 local XY quantization
- wheel action value마다 `RotationStepDegrees` 누적, normalized Yaw 유지
- preview Actor는 collision/domain/facility/Nav/locker 등록 금지
- Definition tag와 zone allowed tag compatibility
- footprint 전체의 단일 zone 포함, grid 배수, floor support와 WorldStatic/WorldDynamic/설비/Pawn overlap 검사
- expansion locker-slot limit 재검증

LMB commit은 carry/Actor/mode/registry snapshot 뒤 transform과 `Placed` registration을 적용하고 마지막에 held reference를 비운다. 어떤 late failure도 attachment, collision, transform, carry, mode와 registry를 복구한다.

G/E 성공, held 대상 변경, suppression과 EndPlay는 preview를 한 번만 정리한다. G/E 실패는 preview와 held state를 유지한다.

## 5. Q Hold Recovery

`AFirstPersonCharacter`에 `RecoverFacilityAction`, `PlacementSnapAction`, `PlacementRotateAction`을 append하고 `UPlayerFacilityPlacementComponent`를 private default subobject로 조립한다.

- Q Started에서 target identity 고정
- Triggered에서 elapsed/progress 갱신
- Completed에서 같은 target/range/query 재검증 후 commit
- release/cancel, gaze/range/조건 변화, suppression, target/owner EndPlay에서 정확히 한 번 cancel
- current held item은 검사하거나 변경하지 않음

회수 조건:

- washer/dryer: inventory count 0, state `Waiting`
- bath: 모든 use slot Available, native water state Empty
- locker bank: 모든 action slot Available, `InstalledCapacity - BankSlotCount >= ActiveLeaseCount`

성공은 같은 Actor의 domain/facility/Nav 등록을 해제하고 설비 위치에서 `Packaged` free-world physics를 켠다. impulse와 auto-pickup은 없다. package collision 또는 unregister/transition 실패는 `Placed`로 rollback한다.

Nav blocker는 authored Nav Modifier를 사용하고 project의 `Dynamic Modifiers Only` Editor 설정을 전제로 mode에 맞게 navigation relevance/registration을 전환한다. UE 5.8 실제 API를 확인해 지원되는 경로를 사용한다.

## 6. Bath Water

`UBathWaterStateComponent`를 추가한다.

- `Empty`, `Filling`, `Filled`, `Draining`
- side-effect-free `IsEmpty()`와 상태 변경 delegate/Blueprint presentation event
- optional normalized amount가 있더라도 state 정본은 native Component
- bath recovery는 mesh visibility나 Blueprint bool을 읽지 않음

## 7. Locker Capacity And Customer

`ULockerActionSlotComponent`는 기존 facility-slot 계약을 재사용하고 internal stable `LockerSlotId`를 가진다. player-visible 번호와 key number는 없다.

`ULockerCapacitySubsystem`:

- Placed locker bank/action-slot registration
- `InstalledLockerCapacity`, active lease registry/revision/delegate
- Definition `LockerSlotCount`와 실제 component 수 검증
- random available action-slot reserve API
- provisional lease acquire/commit/rollback과 idempotent release
- 정상 mutation에서 `ActiveLeaseCount <= InstalledLockerCapacity` 강제. locker 비정상 EndPlay는 package를 만들거나 lease를 지우지 않고 admission을 차단하며 invariant fault를 기록

check-in interaction은 lease를 확보한 뒤 기존 physical key/session을 commit한다. key/carry/session의 어느 단계가 실패해도 lease와 key를 이전 상태로 rollback한다. checkout 성공, timeout, technical cleanup과 customer EndPlay는 같은 idempotent release를 호출한다.

`UCustomerSessionComponent`에 opaque lease handle/release guard와 `ClothesStored`를 추가한다. key number는 token identity/표시로 보존하되 facility lookup에 사용하지 않는다.

탈의/착의용 native StateTree Task 또는 기존 task의 명확한 확장으로 random available locker action slot을 행동 동안만 reserve/use/release한다. 탈의 완료만 `ClothesStored=true`, 착의 완료만 false로 commit한다. 서로 같은 slot일 필요가 없다. slot이 없으면 availability event를 기다린다.

## 8. Shoe And Numbered Topology Migration

- `ShoeLocker`, `StoreShoes`, `WearShoes`, duration property ordinal/name은 한 asset migration cycle 보존하고 deprecated/hidden 처리한다.
- 신규 C++ routine/query가 위 값을 선택하지 않게 한다.
- `FacilityNumber`, numbered lookup과 `ValidateKeyNumber` public symbol은 보존한다.
- `ValidateKeyNumber`의 canonical 검사는 exact key-hook pair/unique number만 확인하고 shoe/clothes locker 존재를 요구하지 않는다.
- `ClothesLocker`는 번호 없는 locker-bank 분류로 사용한다.
- Core Redirect는 추가하지 않는다.

## 9. Expansion And Key Rack

`UBathhouseExpansionDefinition : UPrimaryDataAsset`, 단계 struct, Facility의 `ABathhouseExpansionAuthority`, Interaction의 `ABathhouseKeyRackActor`를 추가한다.

- tier: `KeyPoolSize`, `MaxInstalledLockerSlots`
- asset validation: 모든 tier에서 key pool이 max locker slots 이상
- authority: explicit initial tier, single-world registration과 단계 상승만 지원
- 기존 `UBathhouseFacilitySubsystem`이 authority weak registry/query와 변경 notification을 제공하며 중복 authority를 거부
- key rack: authored anchors와 key/hook classes로 current `KeyPoolSize`만큼 pair 구성
- runtime tier 상승은 pair를 append하고 downgrade/purchase/economy/UI는 구현하지 않음
- rack-owned Actor만 EndPlay에서 정리하고 기존 key/hook identity/state machine을 재사용

락커 placement/recovery는 key pool/number/customer key를 절대 변경하지 않는다.

## 10. Interaction Prompt

Interaction package에 supplemental intent-source interface를 추가하고 Placement component가 구현한다. Interaction은 이 interface로 placement/recovery state를 combined `FPlayerInteractionQuery`에 합성하되 Placement concrete class와 domain 상태를 소유하지 않는다.

`UInteractionPromptWidget` 필수 `BindWidget` 추가:

- `PlacementActionNameText`, `PlacementFailureReasonText`
- `RecoveryActionNameText`, `RecoveryFailureReasonText`, `RecoveryProgressBar`

C++이 visibility, enabled, localized text, progress와 transient result 우선순위를 적용한다. Equipment/Placement LMB 행은 owner에 따라 하나만 표시한다. 기존 event signature와 BindWidget 이름은 변경하지 않는다. 새 optional presentation hook은 별도 추가한다.

## 11. Tests And Validation

최소 native automation coverage:

- settings/default clamp, zone-local snap/rotation과 footprint containment
- placement success와 각 late-failure rollback
- E/G/held-change preview cleanup과 LMB owner priority
- Q complete/cancel/gaze/condition/EndPlay, held-item independence와 zero-impulse recovery
- washer/dryer, bath와 locker 회수 gate
- 1/4/8 slot 합계, expansion limit와 active-lease-below-capacity 방지
- concurrent check-in key/lease commit/rollback, duplicate release와 cleanup
- random undress/dress slot, `ClothesStored`와 no key-locker mapping
- expansion validation, tier-up key materialization과 locker-change key invariance
- deprecated ordinal/symbol preservation

기존 numbered topology 테스트는 신규 canonical 정책으로 수정하되 호환 symbol 존재 검사는 유지한다. `git diff --check`, focused `rg`와 UE 5.8 `BathhouseSimEditor Win64 Development` Build.bat 빌드를 수행한다.

## Deliverables

- 구현 Source와 focused tests
- 실제 구현 상태에 맞춘 관련 architecture status 갱신
- `.md/PROMPT_REVIEW.md`
- `.md/PROMPT_UNREAL.md`: InputAction/IMC, WBP BindWidget, zone/Definition/설비 Blueprint, NavMesh 설정, expansion/key rack 배치와 `ST_CustomerRoutine` 신발 제거/locker task 연결을 정확한 Editor 작업으로 작성

완료 보고에는 변경 파일, class growth, Blueprint/API compatibility, build/test 결과와 Editor 미검증 사항을 포함한다.
