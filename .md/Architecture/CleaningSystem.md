# Cleaning System

## Implementation Status

물걸레의 범용 LMB equipment Hold 청소, target 유무와 독립된 mopping state/motion, bath/dressing floor의 무작위 얼룩 생성과 spawn별 seeded material/local yaw/XY scale variation이 Source에 구현되어 있다. Water Stain은 E hold를 노출하지 않는다.

이번 범위는 물 얼룩과 물걸레 하나만 구현한다. 다른 얼룩·청소 도구, 물통, 물걸레 세척, 내구도와 소모품은 제외한다.

## Source Scope

```text
Source/BathhouseSim/Public/Cleaning/
  CleaningTypes.h
  CleaningWorldSubsystem.h
  CleaningDirectorActor.h
  StainSpawnZoneActor.h
  WaterStainActor.h
  WetMopActor.h

Source/BathhouseSim/Private/Cleaning/
  CleaningWorldSubsystem.cpp
  CleaningDirectorActor.cpp
  StainSpawnZoneActor.cpp
  WaterStainActor.cpp
  WetMopActor.cpp

Source/BathhouseSim/Private/Tests/
  CleaningTowelAutomationTests.cpp
```

## Responsibilities

- 물 얼룩과 spawn zone의 runtime 등록
- Editor authoring interval, 전체/구역별 제한에 따른 random spawn
- 얼룩별 seeded material/yaw/XY scale variation 선택
- 유효 바닥, 기존 얼룩 간격과 Pawn overlap 검증
- 물걸레를 요구하는 LMB equipment Hold 청소 transaction
- target 유무와 분리된 mopping state와 held Actor loop motion
- 청소 진행·취소·완료 상태와 Blueprint 표현 event
- stain/tool/player EndPlay의 대칭 cleanup

Cleaning은 player 입력 mapping, carry slot, UI 상태와 고객 routine을 소유하지 않는다.

## State And Execution Owners

| 책임 | Owner |
|---|---|
| 활성 zone/stain 등록부 | `UCleaningWorldSubsystem` |
| spawn timer와 전체 제한 | `ACleaningDirectorActor` |
| 구역 범위와 구역별 제한 | `AStainSpawnZoneActor` |
| 청소자, 진행률과 terminal commit | `AWaterStainActor` |
| 선택된 material/yaw/XY scale과 visual root | `AWaterStainActor` |
| 물걸레 world/held presentation·mopping state | `AWetMopActor` |
| LMB Hold lifecycle과 camera context | `UPlayerEquipmentUseComponent` |
| 물걸레 loop transform 표현 | `UHeldEquipmentMotionComponent` |
| 단일 소지 reference와 G drop | `UPlayerCarryComponent` |

## Types

`ECleaningStainType`은 확장 경계이며 이번 구현 값은 `Water` 하나다.

`EStainCleaningState`:

- `Idle`
- `Cleaning`
- `Removed`

`Removed`는 terminal이다. 완료 commit, subsystem 등록 해제와 Actor 제거가 반복 호출되어도 수량이 두 번 감소하지 않아야 한다.

## Spawn Architecture

`UCleaningWorldSubsystem`은 zone/stain의 등록과 compact만 담당한다. 매 spawn마다 world actor scan을 수행하지 않는다.

`ACleaningDirectorActor`는 level에 하나를 배치하는 authoring actor다.

- `SpawnIntervalSeconds`
- `MaxActiveStains`
- `MaxPlacementAttemptsPerInterval`
- `StainClass`
- default stain spacing/Pawn clearance

`AStainSpawnZoneActor`는 bath floor 또는 dressing floor의 유효 범위를 정의한다.

- zone kind와 selection weight
- `MaxActiveStainsInZone`
- Box 기반 spawn bounds
- floor trace channel/distance
- optional required floor component tag
- maximum floor slope
- stain spacing과 Pawn clearance override

한 interval의 spawn 절차:

1. 전체 active 제한과 등록된 zone을 확인한다.
2. 구역 제한이 남은 zone을 weight random 선택한다.
3. bounds 안 random XY에서 아래로 floor trace한다.
4. blocking floor, component tag와 slope를 검증한다.
5. 등록된 stain과 최소 거리를 확인한다.
6. player/customer `Pawn` overlap을 확인한다.
7. 전부 통과하면 floor hit transform에 stain을 deferred spawn하고 visual variation seed를 주입한다.
8. spawn을 완료한 뒤 stain BeginPlay가 material/yaw/XY scale을 한 번 선택·적용하고 등록한다.
9. bounded attempt 안에 후보가 없으면 이번 interval만 건너뛴다.

Actor/component 이름이나 전체 world scan으로 floor를 추측하지 않는다. Level designer가 zone과 floor collision/tag를 명시한다.

## Water Stain Visual Variation

`AWaterStainActor`는 native `StainVisualRoot` `USceneComponent`를 `InteractionCollision` 아래에 추가한다. 기존 `BP_WaterStain`의 Blueprint-owned `StainVisual` component 이름은 보존하고 Editor 단계에서 `StainVisualRoot` 아래로 재부착한다. random transform은 visual root에만 적용하여 interaction sphere, spawn spacing, cleaning state와 floor transform을 변경하지 않는다.

Editor authoring 값:

- `MaterialVariants`: null을 제외한 `UMaterialInterface` 후보
- `MinXYScale`, `MaxXYScale`: X/Y별 양수 범위
- `MinYawDegrees`, `MaxYawDegrees`: floor-local Z축 회전 범위

`ACleaningDirectorActor`는 placement에 사용하는 random stream에서 variation seed를 만들고 `SpawnActorDeferred`와 `FinishSpawningActor` 사이에 `ConfigureVisualVariationSeed(Seed)`를 정확히 한 번 호출한다. 이 호출은 seed만 저장하고 Blueprint SCS component가 구성된 뒤의 BeginPlay가 variation을 선택·적용한다. manually placed stain 또는 직접 spawn 경로는 configured seed가 없을 때만 BeginPlay에서 fallback seed를 만든다.

Stain은 private transient 선택 결과와 initialization guard를 소유한다. X/Y는 각 범위에서 독립 추첨하고 Z scale은 `1`로 유지한다. material 유효 후보가 0개면 Blueprint 기본 material을 유지하고, 1개면 random state를 추가 소비하지 않으며, 여러 개면 entry 기준 균등 선택한다. 결과는 stain lifetime 동안 재추첨하지 않는다.

Native는 `StainVisualRoot`에 local yaw/scale을 적용하고 유효 material을 선택한 경우에만 `ApplyStainMaterialVariant(SelectedMaterial)` BlueprintImplementableEvent를 호출한다. Blueprint는 기존 `StainVisual` material slot과 선택적 효과만 갱신하며 random 선택, cleaning state와 spawn 수를 변경하지 않는다.

## Wet Mop Carry

`AWetMopActor`는 generic physical carry 계약을 구현한다.

- 빈손 player가 E로 획득
- 다른 key/mop/basket/monkey wrench 소지 중이면 정확한 실패 이유 반환
- held 중 world collision/physics 비활성화와 공용 held anchor 부착
- G로 camera forward 방향의 authorable impulse를 받아 world에 복귀
- key와 달리 hook/holder domain state를 만들지 않음
- falling out of world 또는 carrier EndPlay 시 last safe transform, 없으면 initial transform으로 복구
- `IHeldEquipmentUsable`의 Hold mode를 구현하고 LMB중 `bIsMopping=true`를 유지
- active use 시 target이 없어도 `UHeldEquipmentMotionComponent`의 loop curve를 계속 재생
- LMB release/cancel/drop/EndPlay에서 mopping state, active stain lock과 motion baseline을 한 번 정리

## Water Stain Interaction

`AWaterStainActor`는 E primary hold를 제공하지 않고 LMB equipment-use target query와 native cleaning transaction API를 제공한다. secondary action은 노출하지 않는다. 기존 generic E hold interface는 다른 target 호환을 위해 Interaction에 남지만 stain이 사용하지 않는다.

Query 조건:

- held object가 `AWetMopActor`인지 확인하고 LMB action/failure row로 표시
- 다른 cleaner가 active인지 확인
- stain이 `Idle` 또는 현재 interactor 소유 `Cleaning`인지 확인
- 실패 시 `물걸레가 필요합니다.` 같은 `FText`를 반환

Runtime flow:

1. LMB Started가 mop의 mopping state/motion을 시작한다.
2. Equipment Use Tick이 camera focus hit를 다시 resolve한다.
3. 유효한 water stain이 새로 들어오면 기존 stain을 cancel하고 새 stain이 interactor identity를 잠그며 `Cleaning`으로 전이한다.
4. 같은 stain을 유지하면 authorable `RemovalDurationSeconds` 기준 authoritative progress를 갱신한다.
5. target을 벗어나면 stain progress/lock만 0으로 취소하고 `bIsMopping`과 loop motion은 유지한다.
6. LMB release, mop drop, player/tool EndPlay에서 active stain을 cancel하고 mopping/motion을 종료한다.
7. progress 1.0에서 `Removed`를 한 번 commit하고 collision, subsystem count와 actor lifetime을 정리한다.

다른 interactor가 청소 중이면 takeover하지 않는다. future multiplayer에서도 한 stain의 cleaner는 하나다.

## Blueprint/API Contracts

Editor authoring:

- director interval, 전체 제한, spawn attempt와 stain class
- zone bounds, kind, weight, 구역 제한, floor filter와 clearance
- stain 제거 시간, decal/mesh/collision
- stain material 후보, yaw 범위와 X/Y scale 범위
- mop mesh/collision, held presentation과 throw impulse
- mop loop position/rotation curve, motion period와 optional blend/reset value

Blueprint 표현 event:

- `AWaterStainActor::OnCleaningStarted`
- `AWaterStainActor::OnCleaningProgressChanged`
- `AWaterStainActor::OnCleaningCancelled`
- `AWaterStainActor::OnCleaningCompleted`
- `AWaterStainActor::ApplyStainMaterialVariant`
- `AWetMopActor::OnHeldPresentationChanged`
- `AWetMopActor::OnMoppingStateChanged`

Blueprint는 material, decal, particle, sound와 animation만 담당한다. progress, completion, spawn count와 carry state를 변경하지 않는다.

## Dependencies

- Cleaning -> Interaction의 query/equipment-use/motion/carry public 계약
- Cleaning -> Engine collision/timer/world subsystem
- Character -> Interaction 입력 routing
- UI -> Interaction 표시 데이터
- Interaction은 Cleaning concrete class를 판별하지 않는다.

## Failure And Cleanup

- spawn candidate 실패: state 변경 없이 다음 interval 대기
- invalid/null material 후보: 제외하고 유효 후보가 없으면 기존 Blueprint 기본 material 유지
- 뒤집힌 scale/yaw 범위: 작은 값과 큰 값을 정규화하되 non-positive scale은 validation 경고와 안전값 clamp
- mop 없음/다른 소지품: hold 시작 전 실패
- cleaning 중 G drop: equipment use/stain progress/motion cancel 후 mop world drop
- stain EndPlay: active hold와 subsystem reference 정리
- mop EndPlay: carry reference 정리, active cleaning cancel
- player EndPlay: stain cleaner lock 해제, mop 안전 복구
- 중복 completion/EndPlay: terminal guard로 idempotent 처리

## Manual Review Points

- 얼룩이 zone 밖, invalid floor, 기존 stain/Pawn overlap 위치에 생성되지 않는지 확인한다.
- 같은 seed는 같은 material/yaw/XY scale을 만들고 다른 spawn은 lifetime 중 결과를 재추첨하지 않는지 확인한다.
- visual root scale/rotation이 interaction sphere, floor alignment와 spawn registry 위치를 바꾸지 않는지 확인한다.
- LMB를 누르는 동안 target 유무와 관계없이 mopping state/motion이 유지되는지 확인한다.
- 유효한 stain에만 progress가 올라가고 focus 이탈은 stain만 취소하며 LMB release/drop은 전체 use를 종료하는지 확인한다.
- 물걸레 없이 prompt 실패 이유가 지속·실행 결과 양쪽에서 정확한지 확인한다.
- Blueprint 표현을 중단해도 C++ cleaning state와 active stain count가 일치하는지 확인한다.
- 다른 stain/tool type이 이번 구현에 추가되지 않았는지 확인한다.
