# 코드 리뷰 프롬프트 — 회수 프롬프트와 낙하 위치

## 요구사항

1. 회수 row가 일반 포커스 interaction query 경로로 전달되어, 회수 불가능 상태에서도 action과 failure reason이 표시되어야 한다.
2. 회수 package의 시작 위치는 `PlacementFootprint X/Y`, `world bounds bottom Z + RecoveryDropZOffsetCm`이어야 한다.

## 구현 요약

- `ABathhouseFacilityActor`, `ATowelProcessingMachineActor`가 focused supplemental source를 구현하고 자신의 side-effect-free `QueryFacilityRecovery()` 결과를 combined query에 합친다.
- `UPlayerInteractionComponent`는 refresh, primary/secondary 조회와 primary hold 재조회 모두에서 포커스 Actor supplemental query를 같은 방식으로 합친다.
- `UPlayerFacilityPlacementComponent`의 global supplemental query는 passive recovery trace를 제거하고 활성 hold progress만 덮어쓴다.
- `UFacilityPlacementSettings::RecoveryDropZOffsetCm` 기본값 `100 cm`를 추가했다.
- `UFacilityPlacementComponent::GetRecoveryDropTransform()`이 footprint 기준 transform을 계산하고 `CanEnablePackagedCollision()`이 해당 예정 위치를 검사한다.
- bathhouse facility와 towel machine의 recovery commit은 예정 위치로 teleport한 뒤 physics를 켜며, 실패 시 이전 transform/mode/domain을 복구한다.

## 호환성

- 기존 reflected type/property/function은 rename/delete하지 않았다.
- 새 `UPROPERTY(Config)` 하나가 추가되어 Core Redirect는 필요 없다.
- Widget Blueprint hierarchy와 BindWidget 계약은 변경하지 않았다.
- Content asset 수정은 없다.

## 클래스 성장

- Interaction에는 focused query 합성 helper 하나만 추가했다.
- 낙하 위치 계산과 충돌 검사는 기존 placement state owner에 유지했다.
- 설비 Actor에는 자신의 recovery query를 presentation row로 변환하는 작은 adapter와 기존 recovery transaction 보강만 추가했다.

## 검증 결과

- `git diff --check`: 오류 없음(기존 line-ending 경고만 존재)
- UE 5.8 `BathhouseSimEditor Win64 Development`: 성공
- `BathhouseSim.Placement.LockerRecoveryAndExpansionKeyPool`: 성공
- `BathhouseSim.Placement.SettingsZoneLeaseAndCompatibility`: 성공

## 리뷰 중점

- focused Actor가 component hit에서도 recovery row를 잃지 않는지
- active hold progress가 target row 위에만 합성되는지
- 회수 예정 위치 collision과 실제 commit 위치가 동일한지
- bathhouse facility rollback이 domain registration과 transform을 함께 복원하는지
- towel machine mode/transform rollback이 대칭인지

## 미검증

- 실제 프로젝트 Blueprint와 레벨에서 bath/washer/dryer PIE 표현
