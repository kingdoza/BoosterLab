# Cleaning System

## Implementation Status

물걸레를 이용한 물 얼룩 제거와 bath/dressing floor의 무작위 얼룩 생성 Source는 구현되었다. Blueprint class, mesh/decal, level director/zone 배치는 Unreal 후속 단계다.

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
- 유효 바닥, 기존 얼룩 간격과 Pawn overlap 검증
- 물걸레를 요구하는 hold-primary 청소 transaction
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
| 물걸레 world/held presentation | `AWetMopActor` |
| E hold lifecycle과 focus 재검증 | `UPlayerInteractionComponent` |
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
7. 전부 통과하면 floor hit transform에 stain을 생성·등록한다.
8. bounded attempt 안에 후보가 없으면 이번 interval만 건너뛴다.

Actor/component 이름이나 전체 world scan으로 floor를 추측하지 않는다. Level designer가 zone과 floor collision/tag를 명시한다.

## Wet Mop Carry

`AWetMopActor`는 generic physical carry 계약을 구현한다.

- 빈손 player가 E로 획득
- 다른 key/mop/basket 소지 중이면 정확한 실패 이유 반환
- held 중 world collision/physics 비활성화와 공용 held anchor 부착
- G로 camera forward 방향의 authorable impulse를 받아 world에 복귀
- key와 달리 hook/holder domain state를 만들지 않음
- falling out of world 또는 carrier EndPlay 시 last safe transform, 없으면 initial transform으로 복구

## Water Stain Interaction

`AWaterStainActor`는 primary hold interaction만 제공한다. secondary action은 노출하지 않는다.

Query 조건:

- held object가 `AWetMopActor`인지 확인
- 다른 cleaner가 active인지 확인
- stain이 `Idle` 또는 현재 interactor 소유 `Cleaning`인지 확인
- 실패 시 `물걸레가 필요합니다.` 같은 `FText`를 반환

Runtime flow:

1. E Started가 같은 target의 hold session을 시작한다.
2. stain이 interactor identity를 잠그고 `Cleaning`으로 전이한다.
3. Interaction Tick은 E hold, 동일 focus, mop 소지와 actor 유효성을 재검증한다.
4. stain이 authorable `RemovalDurationSeconds` 기준 authoritative progress를 갱신한다.
5. E release, focus 이탈, mop drop, target/player EndPlay 시 취소하고 progress를 0으로 복구한다.
6. progress 1.0에서 `Removed`를 한 번 commit하고 collision을 끈다.
7. 완료 event 뒤 actor가 자신을 제거하며 subsystem count를 정리한다.

다른 interactor가 청소 중이면 takeover하지 않는다. future multiplayer에서도 한 stain의 cleaner는 하나다.

## Blueprint/API Contracts

Editor authoring:

- director interval, 전체 제한, spawn attempt와 stain class
- zone bounds, kind, weight, 구역 제한, floor filter와 clearance
- stain 제거 시간, decal/mesh/collision
- mop mesh/collision, held presentation과 throw impulse

Blueprint 표현 event:

- `AWaterStainActor::OnCleaningStarted`
- `AWaterStainActor::OnCleaningProgressChanged`
- `AWaterStainActor::OnCleaningCancelled`
- `AWaterStainActor::OnCleaningCompleted`
- `AWetMopActor::OnHeldPresentationChanged`

Blueprint는 material, decal, particle, sound와 animation만 담당한다. progress, completion, spawn count와 carry state를 변경하지 않는다.

## Dependencies

- Cleaning -> Interaction의 query/hold/carry public 계약
- Cleaning -> Engine collision/timer/world subsystem
- Character -> Interaction 입력 routing
- UI -> Interaction 표시 데이터
- Interaction은 Cleaning concrete class를 판별하지 않는다.

## Failure And Cleanup

- spawn candidate 실패: state 변경 없이 다음 interval 대기
- mop 없음/다른 소지품: hold 시작 전 실패
- cleaning 중 G drop: hold cancel 후 mop world drop
- stain EndPlay: active hold와 subsystem reference 정리
- mop EndPlay: carry reference 정리, active cleaning cancel
- player EndPlay: stain cleaner lock 해제, mop 안전 복구
- 중복 completion/EndPlay: terminal guard로 idempotent 처리

## Manual Review Points

- 얼룩이 zone 밖, invalid floor, 기존 stain/Pawn overlap 위치에 생성되지 않는지 확인한다.
- E를 누르는 동안만 진행하고 release/focus 이탈/mop drop에서 초기화되는지 확인한다.
- 물걸레 없이 prompt 실패 이유가 지속·실행 결과 양쪽에서 정확한지 확인한다.
- Blueprint 표현을 중단해도 C++ cleaning state와 active stain count가 일치하는지 확인한다.
- 다른 stain/tool type이 이번 구현에 추가되지 않았는지 확인한다.
