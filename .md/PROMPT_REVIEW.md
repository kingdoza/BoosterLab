# Code Review Prompt — Unified Physical Carry Drop And Wall Sweep

## Review Objective

UE 5.8 C++에서 wet mop/towel basket의 중복 물리 드랍 실행을 `UPlayerCarryComponent`로 통합하고, 실제 물체 크기 기반 wall sweep과 실패 트랜잭션 보존을 추가한 변경을 Editor 검증 전에 리뷰한다.

이번 제출은 첫 코드 리뷰에서 확인된 target-side start-penetration MTD와 Interaction 정본의 G world-drop 범위 모순을 수정하고, delegate 재진입 회귀를 추가한 재검토본이다.

기준 문서:

- `.md/0_ARCHITECTURE.md`
- `.md/Architecture/InteractionSystem.md`
- `.md/Architecture/CleaningSystem.md`
- `.md/Architecture/TowelSystem.md`
- 사용자 제공 `Unreal Prompt — Unified Physical Carry Drop And Wall Sweep`

이 리뷰에서는 Source와 문서를 검사하되 Content, Config와 `.uproject`를 수정하지 않는다.

## Acceptance Criteria

- G 입력과 `UPlayerInteractionComponent`의 intent/result 흐름은 유지된다.
- `UPlayerCarryComponent`만 detach, 위치 적용, collision/physics 활성화와 impulse를 실행한다.
- `IPhysicalCarryable`은 root primitive, 기존 drop distance/impulse와 commit 완료 알림을 제공한다.
- mop/basket 완료 알림은 carrier, last safe transform과 presentation만 정리한다.
- primitive 월드 AABB, identity rotation, 기본 `ECC_Visibility`로 held bounds 중심부터 기존 목표점까지 sweep한다.
- actor origin/bounds center 차이를 보정하고 blocking hit의 `Hit.Location`에서 clearance를 확보한다.
- start penetration MTD가 투척 목표 쪽으로 진행하면 반대편 벽 통과 후보로 간주해 실패하고, 나머지 후보만 같은 shape/channel overlap으로 재검증한다.
- 안전 공간 없음, 위치 적용 실패 또는 physics 활성화 실패가 반쪽 release를 남기지 않는다.
- key의 G free drop 거부와 held identity가 유지된다.
- 기존 `ThrowSpawnDistance`와 `ThrowImpulseStrength` 값은 바뀌지 않는다.

## Changed Files

- `Source/BathhouseSim/Public/Interaction/PhysicalCarryable.h`
- `Source/BathhouseSim/Public/Interaction/PlayerCarryComponent.h`
- `Source/BathhouseSim/Private/Interaction/PlayerCarryComponent.cpp`
- `Source/BathhouseSim/Public/Interaction/BathhouseKeyActor.h`
- `Source/BathhouseSim/Private/Interaction/BathhouseKeyActor.cpp`
- `Source/BathhouseSim/Public/Cleaning/WetMopActor.h`
- `Source/BathhouseSim/Private/Cleaning/WetMopActor.cpp`
- `Source/BathhouseSim/Public/Towel/TowelBasketActor.h`
- `Source/BathhouseSim/Private/Towel/TowelBasketActor.cpp`
- `Source/BathhouseSim/Private/Tests/BathhouseCleaningTowelTestProbe.h`
- `Source/BathhouseSim/Private/Tests/BathhouseCleaningTowelTestProbe.cpp`
- `Source/BathhouseSim/Private/Tests/CleaningTowelAutomationTests.cpp`
- `.md/Architecture/InteractionSystem.md`

정기 인계 결과물은 이 파일과 `.md/PROMPT_UNREAL.md`다. `.md/0_ARCHITECTURE.md`의 시스템 경계는 현재 구조와 충돌하지 않아 이번 작업으로 수정하지 않았다.

## Contract And Implementation Summary

`IPhysicalCarryable::HandleReleasedBy`를 제거하고 다음 계약으로 분리했다.

- `GetPhysicalCarryPrimitive()`
- `GetThrowSpawnDistance()`
- `GetThrowImpulseStrength()`
- `NotifyPhysicalDropCommitted()`

non-droppable key는 default primitive/parameter 계약을 사용하고 `CanFreeDrop`에서 기존처럼 거부된다. free-droppable mop/basket은 `WorldMesh` root와 기존 parameter를 반환한다.

공통 호출 흐름:

`G → Character → PlayerInteractionComponent → PlayerCarryComponent → validate → box sweep/overlap → detach/place/physics/impulse → carryable notify → ClearHeldObject`

commit 전에는 held reference를 지우지 않는다. start penetration의 depenetration delta가 throw direction으로 전진하면 안전한 player-side 후보가 아니므로 mutation 없이 실패한다. commit 중 presentation delegate가 재진입해도 중복 release하지 않도록 guard를 두었고, 위치/physics 실패 시 이전 actor transform, collision, relative transform과 held anchor attachment를 복원한다.

## Sweep Review Focus

- `Primitive.Bounds.BoxExtent`가 유효하고 세 축 모두 0보다 큰지 확인한다.
- `SweepStart=Primitive.Bounds.Origin`, `SweepEnd=DesiredActorLocation+BoundsOffset`인지 확인한다.
- 월드 AABB에 `FQuat::Identity`를 사용하고 owner/object만 ignore하는지 확인한다.
- 일반 blocking hit 후보가 `Hit.Location - direction * clearance`이고 segment 밖으로 나가지 않는지 확인한다.
- start penetration의 `Dot(DepenetrationDelta, ThrowDirection) > 0` 경로가 overlap-free인 얇은 벽 반대편 후보도 승인하지 않는지 확인한다.
- 후방·측면 start-penetration 후보가 segment clamp와 동일 shape/channel overlap 검사를 우회하지 않는지 확인한다.
- 실패 전에 attachment, carrier, physics와 presentation mutation이 없는지 확인한다.
- notify callback과 held delegate 재진입 중 object/carrier 불일치가 생기지 않는지 확인한다.

## Compatibility And Scope Review

- `TryReleaseHeldEquipment`, key commit/query/delegate와 G result intent는 유지된다.
- reflected rename/delete, Core Redirect와 Build.cs 변경은 없다.
- 새 reflected authoring 값은 `DropSweepChannel`, `DropSweepClearance`뿐이며 Blueprint 연결은 요구하지 않는다.
- pickup, recovery, fell-out-of-world, input mapping, towel/cleaning domain 규칙은 변경하지 않았다.
- 선형/각속도 초기화, character velocity inheritance, pawn collision grace와 물리 재질은 의도적으로 기존 동작을 유지한다.
- 이번 작업에서 Content/.uasset을 수정하거나 저장하지 않았다. 작업 트리의 기존 Content 변경은 별도 Editor 작업 소유다.

## Class Growth

- `UPlayerCarryComponent`: header 62→83줄, cpp 160→369줄. generic held ownership에 공통 physical drop transaction과 private sweep helper가 응집되었다.
- `AWetMopActor`: header 61→60줄, cpp 156→151줄.
- `ATowelBasketActor`: header 69→68줄, cpp 166→161줄.

구체 장비는 감소했고 공통 owner가 성장했다. `.md/Architecture/CoreSystem.md`에는 agent 문서가 참조하는 구체 Class Growth Policy가 아직 없으므로 임의 threshold는 적용하지 않았다. 리뷰에서는 sweep 계산을 별도 component로 분리할 정도의 독립 lifecycle/state가 실제로 필요한지보다 현재 carry transaction 응집도가 적절한지 우선 판단한다.

재작업은 production class/property를 추가하지 않았다. `UBathhousePhysicalDropReentryProbe`는 automation 전용 delegate observer이며 production lifecycle/state를 소유하지 않는다. `CleaningTowelAutomationTests.cpp`는 958줄로 크지만 승인된 기존 domain 통합 테스트 파일이며 이번 변경은 동일 physical-carry fixture의 회귀 범위만 확장한다.

## Verification Results — 2026-08-12 KST

- UE 5.8 `Build.bat BathhouseSimEditor Win64 Development -WaitMutex -NoHotReloadFromIDE`: 성공.
- `Automation RunTests BathhouseSim`: 16 success, 0 fail, exit code 0.
- focused `BathhouseSim.Interaction.PhysicalCarryDropSweepAndTransaction`: 1 success, 0 fail, exit code 0.
- 신규 coverage는 얇은 벽 fixture가 `bStartPenetrating`과 target-side MTD를 실제로 생성함을 먼저 단언하고, 반대편 commit 거부와 held object/attachment/physics/transform/concrete carrier 보존을 확인한다.
- 기존 큰 start blocker, 정상 벽 직전 배치, physics/forward impulse, 빈 공간 재시도와 mop/basket 공통 경로가 계속 성공한다.
- release presentation delegate의 중첩 drop attempt는 실패 이유와 함께 거부되고, outer commit은 presentation 방송 한 번, held clear와 physics commit 한 번으로 끝난다.
- 기존 `BathhouseSim.Cleaning.CarryHoldZoneAndRegistry`: 성공. key G 거부와 held identity 보존 포함.
- `git diff --check`: 오류 없음. 기존 working-copy LF→CRLF 경고만 존재.
- focused search: `HandleReleasedBy` 0건, concrete mop/basket의 `AddImpulse`와 drop `SetActorLocation` 0건, 공통 실행은 `PlayerCarryComponent` 한 곳.

## Not Yet Verified

- 실제 authored mesh/collision을 사용하는 PIE 12개 시나리오
- 비스듬한 벽과 벽 모서리에서의 시각적 관통/physics 안정성
- player capsule ignore와 실제 camera/held-anchor offset 조합

이 항목은 `.md/PROMPT_UNREAL.md`의 검증 전용 절차로 인계한다.
