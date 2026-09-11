# 코드 리뷰 프롬프트 — 설비 설치 Transform 이중 보정 수정

## 기능 계약

`FP-AS-03 동일 바닥 설치`에 따라 `PlacementFloor`에서 한 번 계산한 최종 Actor Transform을 프리뷰와 실제 deferred spawn이 그대로 공유해야 한다. 실제 설치 단계에서 footprint 반높이 또는 상대 오프셋을 다시 적용하지 않는다.

## 변경 파일

- `Source/BathhouseSim/Private/Placement/FacilityActorConversionTransaction.h/.cpp`
- `Source/BathhouseSim/Private/Placement/FacilityPlacementGeometry.cpp`
- `Source/BathhouseSim/Private/Tests/FacilityPlacementAutomationTests.cpp`

## 구현 내용

- `PlaceItemAsFacility()` 입력 이름을 `FinalActorTransform`으로 명확히 했다.
- transaction 내부의 두 번째 `BuildPlacedActorTransform()` 호출을 제거했다.
- Blueprint CDO에서 갱신되지 않은 `ComponentToWorld`를 사용하지 않고, `PlacementFootprint`부터 Actor Root까지 authored relative transform 계층을 직접 합성한다.
- 분리되거나 순환된 Footprint hierarchy는 명시적으로 실패한다.
- 전달된 최종 Transform의 NaN/회전 정규화 검사는 유지하고, deferred spawn은 해당 Transform을 그대로 사용한다.
- non-unit Root scale fixture에서 한 번 계산한 최종 candidate를 transaction에 전달한다.
- Spawn된 Actor Transform이 최종 candidate와 같고, 실제 footprint bottom이 요청한 floor 높이를 유지하는 회귀 검사를 추가했다.

## 리뷰 중점

- `ValidateCurrentPlacement()`만 raw `PlacementFloor` frame을 최종 Actor Transform으로 변환하는지 확인한다.
- CDO와 runtime instance 모두 동일한 Footprint-to-Root 상대 Transform을 계산하는지 확인한다.
- `PlaceItemAsFacility()`가 최종 Transform에 footprint/Bounds 높이를 다시 더하지 않는지 확인한다.
- staged placement, payload, collision snapshot, domain 등록과 rollback 순서가 변경되지 않았는지 확인한다.
- Blueprint, reflected API, Config와 Content 변경이 없는지 확인한다.

## 검증 상태

- 변경 파일 대상 `git diff --check`는 통과했다.
- UE 5.8 `BathhouseSimEditor Win64 Development` 전체 빌드가 성공했다.
- `BathhouseSim.Placement` 자동화 4개가 모두 성공했다.
- 전체 `BathhouseSim` 자동화가 exit code 0으로 성공했고 failure/ensure marker는 없었다.
- 실제 플레이어 입력 기반 PIE 시각 확인은 아직 수행하지 않았다.

## 후속 검증

새 DLL로 Editor를 재시작한 PIE에서 목욕탕, 옷장, 세탁기와 건조기의 프리뷰/설치 footprint bottom이 같은 `PlacementFloor`에 붙는지 확인한다.
