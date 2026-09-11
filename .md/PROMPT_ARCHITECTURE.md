# 기능 계약 — 설비 배치 Authoring 단순화와 초기화 안정화

## 승인 상태

- 사용자 요구사항과 `.md/QNA_FEATURE_SPEC.md`, `.md/QNA_ARCHITECTURE.md` 답변을 2026-09-10(KST) 승인 입력으로 사용한다.
- 미답변과 사용자 동작 미확정 사항은 없다.
- 현재 구현의 48/50/60cm 부양, 대형 `NavArea_Null` 영역과 일시적 락커 등록 오류는 목표 동작이 아니라 수정 대상이다.

## 목적과 범위

- 설비 아이템의 공통 Held 위치·회전을 Project Settings 한 곳에서 조정한다.
- 설치 footprint 크기와 전역 grid로 필요한 셀 수를 자동 계산한다.
- 모든 설비의 footprint 바닥을 PlacementZone의 명시적 바닥 plane에 맞춘다.
- 설비 class-default 외형을 사용하는 범용 반투명 프리뷰를 제공한다.
- 실제 설비 메시 collision만 Unreal의 동적 NavMesh 생성에 반영한다.
- 월드 시작 락커 등록을 확장 Authority와 Actor의 `BeginPlay` 순서에서 분리한다.

## 사용자·Authoring 계약

- 설비 아이템 Held 설정은 모든 설비가 공유하며 위치와 회전만 적용한다. 입력된 Scale은 무시한다.
- `PlacementFootprint`의 scaled 전체 X/Y 크기는 전역 grid 크기의 정수배여야 한다.
- Definition에 별도 X/Y cell 수를 입력하지 않는다.
- PlacementZone은 Bounds 두께와 독립적인 바닥 plane을 가진다.
- 설비 Actor의 local 설치 바닥과 `PlacementFootprint` 바닥면은 모두 local Z=0에 맞춘다.
- 설치 가능 프리뷰 머터리얼과 설치 불가 프리뷰 머터리얼은 Project Settings에서 각각 지정한다.
- 프리뷰는 새로 설치될 설비의 class-default 빈 상태·대기 상태 외관을 보여준다.
- 프리뷰 외형은 설비의 표시 가능한 class-default Static Mesh를 사용하고 원래 머터리얼 전체를 선택된 프리뷰 머터리얼로 교체한다.
- 실제 몸체 메시의 기존 collision과 Navigation relevancy가 설치 장애물의 정본이다.
- Placement helper, 상호작용 영역, 슬롯과 Action/Approach Point는 Navigation에 영향을 주지 않는다.
- RecastNavMesh Runtime Generation은 `Dynamic`을 사용한다.

## 성공·실패·복구 계약

- 프리뷰, staged placement와 recovery unregistration 동안 설비 Actor collision은 Navigation에 반영되지 않는다.
- placement commit 뒤 새 설비의 domain 등록이 확정된 경우에만 authored Actor collision을 복원한다.
- recovery rollback은 원본 설비의 이전 Actor collision과 domain 등록을 정확히 복원한다.
- 프리뷰 mesh나 유효/무효 머터리얼을 만들 수 없으면 배치를 fail-closed하고 held 설비 아이템을 유지한다.
- Authority 미준비 락커는 오류 용량 0으로 거부하지 않고 pending 상태로 유지한다.
- 월드 시작 락커는 고정된 persistent registration ID 순서로 한 번 reconciliation한다.
- 현재 tier 한도 안에 들어가는 락커만 활성화하며 한도를 넘는 락커만 fail-closed한다.
- 초과 락커는 설정 오류를 한 번만 기록하고 facility, capacity와 Navigation domain을 활성화하지 않는다.
- reconciliation은 facility 변경 한 번과 capacity 변경 한 번만 최종 공개한다.
- Authority 미준비 상태는 실제 확장 한도 초과 오류와 구분한다.

## 수용 시나리오

### FP-AS-01 공통 Held 설정

- Given 서로 다른 설비 아이템이 있고 Project Settings에 공통 Held Transform이 설정됨
- When 플레이어가 각 아이템을 듦
- Then 모든 아이템이 같은 위치·회전을 사용하고 각 아이템의 물리 Scale은 보존됨

### FP-AS-02 footprint 파생 셀

- Given footprint 전체 X/Y 크기와 전역 grid가 정수배 관계임
- When Definition을 검증하거나 배치 후보를 계산함
- Then 별도 cell 입력 없이 필요한 X/Y 셀 수가 계산됨
- And 정수배가 아니면 명확한 authoring 오류로 배치가 거부됨

### FP-AS-03 동일 바닥 설치

- Given 서로 높이가 다른 목욕탕·세탁기·건조기·락커와 같은 PlacementZone 바닥 plane이 있음
- When 같은 바닥 위치에 각각 preview하고 설치함
- Then footprint 바닥이 모두 같은 plane에 맞고 Bounds/footprint 두께를 더하지 않음

### FP-AS-04 범용 프리뷰

- Given 설비 class에 하나 이상의 표시 가능한 class-default Static Mesh가 있음
- When 설비 아이템을 들고 preview를 시작함
- Then 범용 preview가 동일 mesh와 root 기준 상대 transform으로 외형을 구성함
- And 설치 가능 시 초록 반투명, 불가능 시 빨강 반투명 머터리얼로 모든 material slot을 교체함

### FP-AS-05 동적 Navigation

- Given 실제 몸체 mesh collision이 Navigation relevant이고 helper는 Navigation 비관련임
- When 설비를 회수하고 다른 위치에 다시 설치함
- Then 이전 몸체 영역의 NavMesh가 복구되고 새 몸체 위치만 agent radius만큼 통행 여유를 가짐
- And 별도 대형 `NavArea_Null` 박스가 생기지 않음

### FP-AS-06 락커 초기 등록 순서

- Given 같은 락커와 Authority가 서로 다른 `BeginPlay` 순서로 시작함
- When 월드 시작 reconciliation이 완료됨
- Then 등록 락커, 설치 용량과 publication 횟수가 모두 같음

### FP-AS-07 락커 확장 한도 초과

- Given persistent ID가 고정된 pre-placed 락커의 총 칸 수가 현재 tier 한도를 초과함
- When reconciliation함
- Then ID 순서로 수용 가능한 락커만 활성화되고 나머지는 비활성 상태와 단일 설정 오류를 유지함

### FP-AS-08 transaction rollback

- Given placement 또는 recovery가 domain 확정 전에 실패함
- When rollback함
- Then 원본 Actor·held identity·collision·Navigation·facility와 capacity 상태가 시작 전과 같음

## 유지 계약

- zone tag 호환성, X/Y grid snap, LCtrl 동작과 휠 Yaw 회전은 변경하지 않는다.
- 배치 충돌, zone 포함, 네 모서리 바닥 지지, Q 회수 조건과 Actor 교체 transaction을 유지한다.
- 열쇠 수와 번호는 확장 단계에만 종속되고 락커와 일대일 대응하지 않는다.
- 세탁기·건조기 내용물, 목욕탕 물·손님과 락커 사용 상태에 따른 회수 제한을 유지한다.

## Migration 승인 사항

- 기존 per-item/legacy `HeldTransform`, Definition의 `FootprintCellsX/Y`, `PreviewActorClass`를 즉시 제거한다.
- 기존 공통 `PlacementNavModifier` native component를 즉시 제거한다.
- 영향받는 Blueprint와 Definition은 같은 Editor migration에서 compile·resave한다.
- 락커 instance에는 cooked build에서도 유지되는 자동 생성 persistent registration ID를 저장한다. UE Editor-only Actor GUID는 사용하지 않는다.

## 비목표

- 새 입력, UI, gameplay 기능과 설비별 예외를 추가하지 않는다.
- runtime contents나 물·작동 상태를 preview에 복제하지 않는다.
- 별도 Navigation modifier, 몸체 primitive 배열과 component tag 기반 Navigation 형상을 추가하지 않는다.
- 구체적인 시설 확장 밸런스 수치와 기존 회수 조건을 변경하지 않는다.

## 구현 경로 판단

- 공통 reflected property와 native default subobject를 즉시 제거하므로 일부 설비만 먼저 migration하면 나머지 asset이 깨진다.
- 따라서 이 작업은 대표 설비만 분리하는 수직 구현 대신 모든 영향 Definition·설비 Blueprint를 같은 migration 단위로 처리한다.
- 실제 asset 이름과 component 상태는 코드 리뷰 후 Unreal Editor 단계에서 조회·수정·PIE 검증한다.
