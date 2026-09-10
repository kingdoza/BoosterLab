# Unreal Architecture Prompt — Facility Placement Authoring Simplification

## 목적

현재 설비 배치 시스템의 중복 authoring과 공통 설정 위치를 정리하는 설계를 작성한다. 아래 명시된 항목만 설계 범위로 삼고 구현은 진행하지 않는다.

## 필수 설계 범위

1. 설비 공통 `HeldTransform`을 `UFacilityPlacementSettings`의 Developer Settings 항목으로 노출한다.
   - 모든 `APlaceableFacilityItemActor`가 동일한 설정값을 사용하게 한다.
   - 런타임에는 기존 계약대로 위치와 회전만 적용하고 Scale은 무시한다.
   - 기존 reflected `HeldTransform`의 호환·폐기 방법만 필요한 범위에서 정한다.

2. 설비 설치 시 `PlacementFootprint`의 바닥면이 `PlacementZone`의 실제 바닥면에 맞도록 한다.
   - Zone Bounds의 두께 때문에 설비가 그 두께만큼 위로 뜨면 안 된다.
   - `PlacementFootprint`의 상대 위치와 높이를 이용한 바닥 정렬 책임을 명확히 한다.
   - 기존 X/Y grid snap과 회전 동작은 변경하지 않는다.

3. 설비 셀 크기의 이중 authoring을 폐기하고 `PlacementFootprint.BoxExtent`를 단일 정본으로 사용한다.
   - `FootprintCellsX/Y`를 사용자가 별도로 맞추는 계약을 제거한다.
   - 필요한 셀 수는 `PlacementFootprint`의 scaled 전체 X/Y 크기와 전역 `GridSizeCm`으로 계산한다.
   - 검증은 전체 크기가 전역 grid 단위로 나누어떨어지는지만 확인한다.
   - 기존 serialized `FootprintCellsX/Y`의 호환·폐기 방법만 필요한 범위에서 정한다.

## 수용 기준

- 공통 Held 위치·회전은 Project Settings 한 곳에서 조정할 수 있다.
- PlacementZone Bounds 두께와 무관하게 설치 설비의 footprint 바닥면이 zone 바닥면에 정렬된다.
- 전역 grid 또는 footprint 크기를 변경할 때 Definition의 셀 수를 별도로 수정하지 않는다.
- 기존 배치 충돌, 구역 포함, 바닥 지지, preview, 회수 및 Actor 변환 계약은 이번 변경에 필요한 부분 외에는 유지한다.

## 설계 제한

- 새 gameplay 기능, UI, 입력, 설비별 예외를 추가하지 않는다.
- 요구사항 밖의 구조 개편이나 책임 재분배를 제안하지 않는다.
- 필요한 실제 Source와 관련 Placement 아키텍처 문서만 확인한다.
- 설계 결과는 현재 workflow에 따라 관련 아키텍처 정본과 `.md/PROMPT_IMPLEMENTATION.md`에 반영한다.

## 추가 문제 및 필수 설계 범위

4. PIE 시작 시 설비 주변 NavMesh가 실제 설비보다 과도하게 사라지는 문제를 해결한다.
   - 현재 `Placed` 전환에서 `PlacementNavModifier`의 Navigation relevancy를 켜고, RecastNavMesh가 `Dynamic Modifiers Only`로 동작하면서 에디터의 정적 NavMesh 위에 런타임 `NavArea_Null` 영역이 추가되는 것이 직접 원인이다.
   - `UNavModifierComponent`가 적절한 navigation primitive를 찾지 못하면 설비별 `FailsafeExtent`를 사용하고, 일부 설비에서는 상호작용용 Box까지 형상 후보가 되어 실제 몸체보다 큰 영역이 제거된다.
   - 공통 `PlacementNavModifier`와 설비별 `FailsafeExtent`를 설치 장애물의 정본으로 사용하지 않는다.
   - 설치 설비의 기존 실제 몸체 충돌 primitive만 navigation geometry의 정본으로 지정하고, 상호작용 Box, 슬롯, `PlacementFootprint`, Action/Approach Point는 항상 Navigation에 영향을 주지 않게 한다.
   - preview, staged placement와 recovery unregistration 중에는 몸체의 Navigation 영향을 끄고, placement commit으로 domain 등록이 확정된 뒤에만 켠다. rollback은 이전 Navigation 상태를 정확히 복원한다.
   - 일반 collision geometry의 런타임 생성·제거를 사용할 경우 RecastNavMesh Runtime Generation을 `Dynamic`으로 맞춘다. raw geometry 전환과 호환되지 않는 `Dynamic Modifiers Only`를 그대로 유지하지 않는다.
   - Recast agent radius에 따른 정상적인 통행 여유 외에는 실제 몸체보다 큰 `NavArea_Null` 박스가 생기지 않아야 하며, 모든 Facility/Queue Approach Point는 생성된 NavMesh 위에 남아야 한다.

5. 월드 시작 시 락커와 확장 Authority의 `BeginPlay` 순서에 의존하는 등록 실패를 제거한다.
   - 현재 락커가 Authority보다 먼저 시작하면 최대 설치 칸 수를 `0`으로 조회해 최초 등록이 실패하고 오류를 출력한다. 락커는 `OnExpansionAuthorityChanged`를 먼저 구독하므로 Authority가 이후 정상 등록되면 `HandleExpansionAuthorityChanged()`에서 재등록을 시도하며, 실제 tier 제한에 여유가 있으면 뒤늦게 등록된다.
   - 위 일시적 미준비 상태를 실제 `ExpansionLimit` 초과와 같은 영구 실패로 취급하지 않는다.
   - 확장 Authority 준비 전의 pre-placed 락커는 pending 상태로 수집하고, Authority 등록 완료 시 한 번의 명시적인 reconciliation 단계에서 결정적으로 등록한다.
   - reconciliation은 중복 등록, 용량 이중 합산과 중복 publication을 만들지 않아야 하며 성공한 락커의 facility/capacity/Navigation domain을 함께 활성화한다.
   - Authority가 준비된 뒤에도 전체 authored 락커 칸이 현재 tier의 `MaxInstalledLockerSlots`를 실제로 초과하면 이는 재시도로 해결되지 않는 설정 오류다. 초과 락커는 fail-closed로 유지하고 transient 초기화 메시지와 구분되는 명확한 오류를 한 번만 기록한다.
   - Actor 간 `BeginPlay` 우연한 순서나 지연 Tick에 의존하지 않고, readiness와 pending registration의 owner를 기존 Facility/Locker subsystem 경계 안에서 정한다.

6. 설비별로 48cm, 50cm, 60cm 떠서 설치되는 높이 계산을 바로잡는다.
   - 현재 `MakeCandidateTransform()`이 후보 Z를 `ZoneBounds` 상단인 `ZoneHalfHeight`로 올린 뒤, `ValidateCurrentPlacement()`가 `PlacementFootprint` 반높이를 다시 더한다. 현재 footprint 상대 Z가 0이므로 결과적으로 `ZoneHalfHeight + FootprintHalfHeight`가 Actor 높이에 그대로 합산된다.
   - 현재 authoring 값에서는 목욕탕 `10 + 38 = 48cm`, 세탁기·건조기 `10 + 40 = 50cm`, 옷장 `10 + 50 = 60cm`로 실제 증상과 일치한다. 이를 설비별 보정값으로 해결하지 않는다.
   - 설치 높이의 정본을 `ZoneBounds` 상단이나 카메라 trace 충돌점이 아닌, Bounds 두께와 독립적인 PlacementZone의 명시적 바닥 plane으로 정한다.
   - `PlacementFootprint` 바닥면과 설비 실제 설치 바닥면의 관계를 하나의 authoring 계약으로 고정한다. 현재 Blueprint CDO처럼 footprint 중심이 Actor 바닥 원점에 놓인 자산은 footprint 상대 Z 또는 공통 pivot 계약을 교정하여 footprint 바닥과 실제 설비 바닥이 일치하게 migration한다.
   - 공통 transform 계산은 zone 바닥 plane과 회전·scale이 적용된 footprint 상대 transform으로 Actor 위치를 한 번만 역산해야 한다. Zone 반높이와 footprint 반높이를 별도 단계에서 중복 가산하지 않는다.
   - preview transform, 최종 spawned Actor, footprint 포함 검사와 네 모서리 floor-support trace가 모두 같은 후보 transform을 사용해야 한다.

## 추가 수용 기준

- PIE 전후에 설비의 실제 몸체와 무관한 대형 NavMesh 구멍이 새로 생기지 않는다.
- 설비 회수 후 해당 몸체가 차지하던 Navigation 영역이 복구되고, 재배치 후 새 위치만 다시 반영된다.
- Authority와 락커의 `BeginPlay` 순서가 어느 쪽이 먼저여도 허용 범위 내 pre-placed 락커의 최종 등록 결과와 publication 횟수가 같다.
- 실제 tier 한도를 초과한 락커만 등록 거부되며, Authority 미준비는 영구적인 용량 초과 오류로 기록되지 않는다.
- 목욕탕, 세탁기, 건조기와 옷장의 설치 바닥 높이가 동일한 zone floor plane에 맞고, `ZoneBounds.BoxExtent.Z`나 설비별 `PlacementFootprint.BoxExtent.Z`를 변경해도 공중 부양 오차가 생기지 않는다.
- 위 변경은 기존 zone tag 호환성, X/Y grid snap, 회전, 충돌 검사와 회수 transaction 계약을 변경하지 않는다.
