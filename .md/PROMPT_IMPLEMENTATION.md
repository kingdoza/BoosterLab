# Implementation Prompt — Carry/Stain/Towel Preview/Bath Snap Extension

## 목적

현재 player interaction, 단일 physical carry, Cleaning, Towel Presentation과 Customer StateTree 계약을 보존하면서 다음 네 확정 설계를 native C++로 구현한다.

1. `IPhysicalCarryable` 기반 아이템별 `HeldTransform`
2. water stain의 spawn별 material/yaw/XY scale variation
3. Stack/Pile/Slot Towel Visual의 PIE 없는 Editor preview
4. blocking collision 여부와 무관한 Customer Bath snap

기준 문서:

- `.md/AGENT_WORKFLOW.md`
- `.md/AGENT_IMPLEMENTATION.md`
- `.md/0_ARCHITECTURE.md`
- `.md/Architecture/InteractionSystem.md`
- `.md/Architecture/CleaningSystem.md`
- `.md/Architecture/TowelPresentationSystem.md`
- `.md/Architecture/CustomerSystem.md`
- `.md/Architecture/CoreSystem.md`

이 파일은 이전 Towel Presentation 구현 프롬프트를 전부 대체한다.

## 공통 금지와 보존 계약

- 공통 carry Actor, `UPhysicalCarryableComponent`, inventory, hotbar와 item swap을 만들지 않는다.
- key/wet mop/towel basket Actor가 `IPhysicalCarryable`을 직접 구현하는 현재 경계를 유지한다.
- `HeldKeyAnchor`, `TowelPresentationVisual`, 기존 Slot preview property/function과 Blueprint component 이름을 rename/delete하지 않는다.
- towel inventory count/state/revision, cleaning state와 customer facility state owner를 presentation/input 계층으로 옮기지 않는다.
- StateTree/Blueprint graph에 domain mutation을 추가하지 않는다.
- 새 module dependency, Core Redirect와 Config 변경을 추가하지 않는다.
- 구현 단계는 `Content/` asset을 수정하거나 resave하지 않는다.
- 사용자 소유 dirty Content와 `Reference/`를 건드리지 않는다.

## 1. `IPhysicalCarryable` HeldTransform

대상:

- `Interaction/PhysicalCarryable.h`
- `Interaction/BathhouseKeyActor.h/.cpp`
- `Cleaning/WetMopActor.h/.cpp`
- `Towel/TowelBasketActor.h/.cpp`
- focused carry automation tests

`IPhysicalCarryable`에 기본 Identity를 반환하는 native `GetHeldTransform() const` 계약을 추가한다. 다음 세 Actor는 각각 같은 의미의 property를 직접 소유하고 override한다.

```cpp
UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "...|Carry|Presentation")
FTransform HeldTransform = FTransform::Identity;
```

- `ABathhouseKeyActor`
- `AWetMopActor`
- `ATowelBasketActor`

기존 `HeldKeyAnchor`는 공용 기준점이다. player-held 부착 시 anchor에 snap한 뒤 Actor relative location/rotation을 `GetHeldTransform()` 값으로 적용한다. scale은 물리 크기/drop bounds를 바꾸지 않도록 항상 `(1,1,1)`을 사용하고 non-unit authored scale은 runtime에서 무시하며 Editor validation warning을 제공한다.

Key는 `TryTakeFromHook`와 `TryTakeFromCounter` 성공 경로에서만 HeldTransform을 적용한다. hook 부착, customer assignment, counter return slot과 recovery에는 적용하지 않는다. Mop/Basket은 기존 `HandleTakenBy` 경로에서 적용한다. Identity 값은 현재 anchor snap 결과와 같아야 한다.

기존 acquire rollback, Carrier, presentation delegate, key state transaction과 common G drop commit을 보존한다. HeldTransform이 world drop transform, last-safe transform과 primitive scale을 오염시키지 않아야 한다.

## 2. Water Stain Visual Variation

대상:

- `Cleaning/CleaningDirectorActor.h/.cpp`
- `Cleaning/WaterStainActor.h/.cpp`
- focused cleaning automation tests

`AWaterStainActor`에 `InteractionCollision` 아래 native default subobject `StainVisualRoot` `USceneComponent`를 추가한다. 기존 Blueprint-owned `StainVisual`과 이름이 충돌하지 않아야 한다. random transform은 visual root에만 적용하고 root sphere, interaction trace, floor transform과 registry 위치는 변경하지 않는다.

Editor authoring property:

- `TArray<TObjectPtr<UMaterialInterface>> MaterialVariants`
- `FVector2D MinXYScale`, `MaxXYScale`
- `float MinYawDegrees`, `MaxYawDegrees`

`ACleaningDirectorActor`의 placement random stream에서 variation seed를 만들고 deferred spawn 중 `ConfigureVisualVariationSeed(int32)`로 seed만 주입한다. `SelectZone`을 포함한 동일 spawn attempt의 선택에서 global random과 local stream을 불필요하게 섞지 않는다.

`AWaterStainActor::BeginPlay`는 Blueprint construction 완료 뒤 정확히 한 번 variation을 resolve/apply한다. configured seed가 없는 manually/directly spawned stain만 fallback seed를 사용한다. private transient initialization guard와 선택 결과를 유지한다.

- null material 제외
- 후보 0개: 기존 Blueprint material 유지, event 호출 안 함
- 후보 1개: 추가 random draw 없이 선택
- 후보 여러 개: array entry 균등 선택
- X/Y 독립 random, Z scale `1`
- inverted min/max는 정규화
- non-positive scale은 validation warning과 안전한 positive clamp
- floor-local Z축 yaw 적용
- lifetime 중 재추첨 금지

유효 material 선택 시에만 다음 event를 한 번 호출한다.

```cpp
UFUNCTION(BlueprintImplementableEvent, Category = "Cleaning|Presentation")
void ApplyStainMaterialVariant(UMaterialInterface* SelectedMaterial);
```

기존 cleaning started/progress/cancel/completed event와 제거 transaction은 변경하지 않는다.

## 3. Towel Visual Editor Preview

대상:

- `Towel/Presentation/TowelQuantityVisualComponent.h/.cpp`
- `Towel/Presentation/TowelStackVisualComponent.h/.cpp`
- `Towel/Presentation/TowelPileVisualComponent.h/.cpp`
- `Towel/Presentation/TowelSlotVisualComponent.h/.cpp`
- `Tests/TowelPresentationAutomationTests.cpp`

Base에 reflected preview property를 옮기지 않는다. 기존 Slot의 `PreviewState`, `PreviewCount`, `RebuildPreview()` owner/name을 유지한다. Base에는 protected common helper만 추가한다.

- `RebuildEditorPreview(State, Count, Revision)`
- `ClearEditorPreview()`

Stack과 Pile에 각자 다음 동일 authoring API를 신규 추가하고 Slot에도 `ClearPreview()`만 호환 추가한다.

- `PreviewState`
- `PreviewCount >= 0`
- `RebuildPreview()` `CallInEditor`
- `ClearPreview()` `CallInEditor`
- private monotonic preview revision

Preview rebuild:

1. Editor/EditorPreview world인지 확인하고 Game/PIE에서는 mutation하지 않는다.
2. inventory를 bind/mutate하지 않는다.
3. timer와 이전 bucket/record를 정리한다.
4. `RandomStream`을 `RandomSeed`로 재초기화한다.
5. layout prepare/slot resolve 후 animation 없이 즉시 동기화한다.
6. viewport render state를 갱신한다.

같은 seed와 입력은 같은 mesh/Pile transform을 재현해야 한다. preview ISM은 기존처럼 transient, NoCollision, overlap off, navigation off, tick off다. `BindInventorySource()`와 BeginPlay는 preview를 제거한 뒤 최신 authoritative snapshot을 적용한다. unregister/reconstruction/EndPlay도 timer, records와 buckets를 정리한다.

Stack/Pile/Slot runtime count animation, state swap, revision ordering, mesh bucket과 inventory explicit binding 계약은 바꾸지 않는다. drying-rack Actor/inventory/interaction은 만들지 않는다.

## 4. Customer Bath Snap Collision Ignore

대상:

- `Customer/CustomerSessionComponent.h/.cpp`
- `Tests/BathhouseDomainTests.cpp`

`SnapToCurrentFacilityActionPoint()`에서 `IsActionTransformClear()` 호출과 capsule overlap rejection을 제거하고 private declaration/definition도 삭제한다.

다음 검증은 유지한다.

- current reservation/slot
- reservation-time cached transform
- Character, capsule와 CharacterMovement
- 유효한 계산 transform

AI/movement 정지, movement mode 저장과 `MOVE_None` 전환 뒤 기존 `SetActorLocationAndRotation(..., false, ..., ETeleportType::TeleportPhysics)` unswept 경로를 사용한다. blocking volume/facility mesh/다른 collision overlap만으로 snap을 실패시키지 않는다.

Capsule/Actor collision enabled와 response는 변경하지 않는다. Return은 cached ApproachPoint로 같은 unswept teleport 후 저장한 movement mode를 복원한다. release, interruption, technical abort와 EndPlay cleanup의 idempotency를 유지한다. blocked action point는 navigation retry 또는 technical abort 원인이 아니다.

## Blueprint/API와 Unreal 인계

새 reflected 계약:

- 세 carry Actor의 `HeldTransform`
- `AWaterStainActor::StainVisualRoot`, variation authoring property와 material event
- Stack/Pile preview property/function과 세 layout의 `ClearPreview`

기존 reflected symbol의 rename/delete가 없으므로 Core Redirect는 추가하지 않는다. 구현 후 `.md/PROMPT_UNREAL.md`에는 다음 Editor 작업을 정확한 asset 경로와 함께 기록한다.

- `BP_BathhouseKey`, `BP_WetMop`, towel `BP_TowelBasket`의 HeldTransform authoring
- `BP_WaterStain`의 기존 `StainVisual`을 inherited `StainVisualRoot` 아래로 재부착하고 material event에서 slot 0 적용
- stain material/yaw/XY scale 후보 설정
- clean stack/used bin/basket/washer/dryer에서 PIE 없이 preview rebuild/clear 검증

구현 Agent는 Content를 직접 수정하지 않는다.

## Native Tests와 검증

- Identity와 서로 다른 HeldTransform 위치/회전, unit scale, key의 hook/counter 미적용과 drop/recovery 보존
- material 후보 0/1/여러 개/null, seed 재현성, yaw/XY 범위와 one-time initialization
- stain visual root transform이 interaction collision transform/scale을 바꾸지 않음
- Stack/Pile/Slot preview rebuild/clear, same-seed 재현, Game/PIE guard와 runtime bind override
- 기존 Towel revision/layout/ISM lifecycle 회귀 없음
- blocked Bath ActionPoint snap 성공, 정확한 feet transform, `MOVE_None`, collision state 보존과 approach 복귀
- missing cache/component failure, repeated release와 technical abort 회귀 없음
- 기존 carry/cleaning/towel/customer automation tests 유지

`git diff --check` 후 UE 5.8 `Build.bat`을 `.md/AGENT_WORKFLOW.md` 정책대로 첫 시도부터 승인된 sandbox 밖에서 실행한다. 구현 완료 후 `.md/PROMPT_REVIEW.md`와 `.md/PROMPT_UNREAL.md`만 정기 결과물로 작성한다.
