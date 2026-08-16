# Code Review Prompt — Carry/Stain/Towel Preview/Bath Snap Extension

## Review Objective

UE 5.8의 기존 single physical carry, Cleaning, Towel Presentation과 Customer StateTree 경계를 유지하면서 구현한 다음 네 native 확장을 Editor authoring 전에 리뷰한다.

1. key/wet mop/towel basket별 `HeldTransform`
2. water stain spawn별 seeded material/yaw/XY-scale variation
3. Stack/Pile/Slot의 PIE 없는 transient Editor preview
4. blocking collision과 무관한 Customer Bath action-point snap

기준 문서:

- `.md/AGENT_WORKFLOW.md`
- `.md/AGENT_REVIEW.md`
- `.md/0_ARCHITECTURE.md`
- `.md/Architecture/InteractionSystem.md`
- `.md/Architecture/CleaningSystem.md`
- `.md/Architecture/TowelPresentationSystem.md`
- `.md/Architecture/CustomerSystem.md`
- `.md/Architecture/CoreSystem.md`
- `.md/PROMPT_IMPLEMENTATION.md`

## Acceptance Contract

- 공통 carry Actor/component, inventory/hotbar, drying-rack gameplay를 만들지 않는다.
- `HeldKeyAnchor`, `TowelPresentationVisual`, 기존 Slot preview symbol과 Blueprint component 이름을 보존한다.
- carry transform은 player-held 위치/회전에만 적용하고 authored scale은 경고 후 무시한다.
- stain variation은 native visual root에만 한 번 적용하고 interaction/floor/registry transform을 바꾸지 않는다.
- towel preview는 inventory를 bind/mutate하지 않고 Editor/EditorPreview에서만 transient presentation을 만든다.
- Bath snap은 cached transform과 기존 unswept teleport를 유지하며 collision state를 바꾸지 않는다.
- Content/Config/module/Core Redirect를 변경하지 않는다.

## Implemented Changes

### Physical carry

- `IPhysicalCarryable::GetHeldTransform()` 기본값은 Identity다.
- `ABathhouseKeyActor`, `AWetMopActor`, `ATowelBasketActor`가 각자 `EditDefaultsOnly`, `BlueprintReadOnly` `HeldTransform`을 소유한다.
- anchor snap 뒤 RootComponent relative location/rotation만 적용하고 scale은 unit으로 sanitize한다.
- key는 성공한 hook/counter take에만 적용한다. hook return, counter slot, customer assignment와 recovery는 기존 snap/recovery를 사용한다.
- 세 Actor는 non-unit authored scale에 Editor validation warning을 낸다.

### Water stain

- `InteractionCollision` 아래 native `StainVisualRoot`를 추가했다.
- director의 placement `FRandomStream`이 zone 선택, placement와 variation seed를 연속 처리한다.
- deferred spawn은 `ConfigureVisualVariationSeed()`로 seed만 주입하고 BeginPlay가 정확히 한 번 resolve/apply한다.
- null filtering, 0/1/multiple material 규칙, independent XY, unit Z, inverted range normalization, positive clamp와 local yaw를 구현했다.
- 유효 material이 있을 때만 `ApplyStainMaterialVariant()`를 한 번 호출한다.

### Towel preview

- base에는 protected `RebuildEditorPreview()`/`ClearEditorPreview()`만 preview 공통 계약으로 추가했다.
- Stack/Pile이 각자 `PreviewState`, `PreviewCount`, `RebuildPreview`, `ClearPreview`와 monotonic revision을 소유한다.
- Slot의 기존 owner/name을 보존하고 `ClearPreview`만 추가했다.
- rebuild는 timer/bucket/record 정리, seed reset, layout/slot resolve, immediate sync와 render-state 갱신을 수행한다.
- Game/PIE guard는 visual과 revision을 바꾸지 않는다. BeginPlay/bind/unregister/EndPlay는 preview를 제거하고 authority를 복원한다.

### Customer Bath snap

- `IsActionTransformClear()` declaration/definition/call을 삭제했다.
- cached feet transform, capsule-height 변환, AI/movement stop, saved mode와 `MOVE_None`, unswept `TeleportPhysics` 경로는 유지했다.
- capsule/Actor collision enabled와 response를 변경하지 않고 release/abort 시 cached approach로 복귀한다.

## Changed Source

- `Interaction/PhysicalCarryable.h`, `BathhouseKeyActor.h/.cpp`, `BathhouseKeyHookActor.h`
- `Cleaning/CleaningDirectorActor.h/.cpp`, `WaterStainActor.h/.cpp`, `WetMopActor.h/.cpp`
- `Towel/TowelBasketActor.h/.cpp`
- `Towel/Presentation/TowelQuantityVisualComponent.h/.cpp`, Stack/Pile/Slot `.h/.cpp`
- `Customer/CustomerSessionComponent.h/.cpp`
- `Tests/BathhouseDomainTests.cpp`, `CleaningTowelAutomationTests.cpp`, `TowelPresentationAutomationTests.cpp`

관련 정본과 `.md/PROMPT_UNREAL.md`도 현재 native 계약으로 갱신했다.

## Class Growth Check

| Owner | Header lines | CPP lines | 판단 |
|---|---:|---:|---|
| Key | 101→111 | 300→337 | Actor-local authoring/validation만 추가 |
| Wet Mop | 60→71 | 151→187 | 같은 carry 책임 유지 |
| Towel Basket | 72→83 | 166→202 | 같은 carry 책임 유지 |
| Director / Water Stain | 46→48 / 75→124 | 95→98 / 185→278 | spawn seed owner와 stain variation owner로 응집 |
| Quantity Visual | 122→128 | 438→521 | 공통 preview lifecycle만 추가 |
| Stack / Pile / Slot | 23→38 / 38→53 / 47→50 | 8→26 / 31→49 / 93→104 | layout별 authoring API 유지 |
| Customer Session | 211→208 | 1030→1002 | overlap rejection 제거로 축소 |

Water stain과 quantity visual 증가는 각각 one-time variation lifecycle과 공통 preview lifecycle 안에서 함께 움직이는 상태라 별도 Actor/module로 분리하지 않았다.

## Blueprint/API/Core Redirect Impact

새 reflected 계약:

- 세 carry Actor의 `HeldTransform`
- `AWaterStainActor::StainVisualRoot`, variation properties, `ApplyStainMaterialVariant`
- Stack/Pile preview properties/functions와 세 layout의 `ClearPreview`

기존 reflected symbol rename/delete가 없어 Core Redirect는 없다. `Config/`, `.uproject`, `BathhouseSim.Build.cs`를 변경하지 않았다. 구현 단계는 사용자 소유 dirty Content/Reference를 수정하거나 resave하지 않았다.

## Verification Evidence

- `git diff --check`: pass
- UE 5.8 `Build.bat` `BathhouseSimEditor Win64 Development`: UHT/compile/link success
- commandlet `Automation RunTests BathhouseSim`: 17 success, 0 fail, exit code 0
- focused coverage: Identity/nonidentity/unit scale, key hook/counter/recovery, drop rollback; stain 0/1/multiple/null/seed/range/one-time/root isolation; three preview layouts/same-seed/Game guard/runtime override; blocked Bath snap/collision preservation/approach return

## Review Focus

1. HeldTransform이 successful player-held 경로 밖의 hook/counter/recovery/drop transform을 오염시키지 않는가?
2. director attempt에서 global random과 local stream이 섞이지 않고 stain BeginPlay 이전에는 seed만 전달하는가?
3. material 후보 하나가 random draw를 추가 소비하지 않고 initialization guard가 event/transform 재적용을 막는가?
4. preview helper가 inventory authority, runtime revision ordering과 animation semantics를 바꾸지 않는가?
5. Game/PIE no-op, reconstruction cleanup과 BeginPlay/bind authoritative override가 대칭인가?
6. Bath snap이 overlap만 무시하고 cache/component/transform 실패와 movement-mode cleanup은 유지하는가?
7. `.md/PROMPT_UNREAL.md`가 C++ domain logic 없이 asset authoring/compile/PIE 검증만 요구하는가?

## Review Output

- finding은 severity와 exact file/line 근거로 작성한다.
- 문제가 없으면 코드 단계 승인 후 `.md/PROMPT_UNREAL.md`를 Editor 단계로 인계한다.
- Source 문제가 있으면 `.md/PROMPT_IMPLEMENTATION_R.md`에 최소 재작업 범위와 회귀 검증을 작성한다.
