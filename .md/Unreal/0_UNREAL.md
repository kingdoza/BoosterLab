# Unreal Editor System Map

## 목적

이 문서는 BathhouseSim의 현재 Unreal Editor authoring 구조를 찾기 위한 진입점이다. `.uasset`과 `.umap`이 실제 serialized 데이터이며, 이 디렉터리의 문서는 C++ 계약에 영향을 주는 Editor 구조·연결·설정의 현재 상태를 설명한다.

작업 이력이나 완료 일지를 누적하지 않는다. 변경 기록은 Git에 맡기고 저장·재로드로 확인된 현재 상태만 유지한다.

## 문서 생성과 라우팅

시스템별 Editor 작업이 처음 발생할 때 Unreal MCP 또는 명시적으로 호출된 Computer Use 에이전트가 관련 문서를 생성하고 이 표에 링크를 추가한다.

| 영역 | 권장 문서 | 포함 범위 |
|---|---|---|
| Placement | [PlacementSystem.md](PlacementSystem.md) | Zone, footprint, preview, placed/item Definition, grid와 Navigation authoring |
| Facility | [FacilitySystem.md](FacilitySystem.md) | 설비 Blueprint, slot, counter, locker와 expansion authoring |
| Customer AI | `CustomerAISystem.md` | Character/AIController Blueprint, StateTree schema/state/task/binding |
| Interaction/UI | `InteractionUISystem.md` | Input Mapping, Widget hierarchy, BindWidget와 표시 asset |
| Towel | `TowelSystem.md` | 수건 설비·표현 Blueprint, mesh/material과 presentation 설정 |
| World | [WorldSystem.md](WorldSystem.md) | map actor, World Settings, RecastNavMesh와 유일 Authority |

아직 실제 작업으로 검증되지 않은 시스템 문서는 미리 추측해 만들지 않는다. 문서가 없으면 해당 영역은 Editor 기준선 미확정 상태이며, 기능 명세에서 필요할 때 읽기 전용 Unreal MCP 조사를 수행한다.

## 시스템 문서 필수 내용

- exact asset path와 native Parent Class
- C++ 계약에 필요한 component 이름·타입·attachment hierarchy
- DataAsset, class, mesh, material, montage, widget와 StateTree 연결
- 기능에 영향을 주는 Class Default와 허용된 Level instance override
- collision, physics, Navigation과 trace 역할
- transform, pivot, bounds, 단위와 local/world 좌표 기준
- StateTree schema, evaluator/task/condition과 binding source
- Widget의 필수 hierarchy, `BindWidget`, 입력과 표시 책임
- map/world/project 단위의 전역 설정과 유일 Actor 계약
- MCP로 수정할 수 없는 남은 사용자 작업의 `USER_UNREAL.md` 참조

## 기록하지 않는 내용

- 장식용 속성 전체 덤프
- PIE에서만 존재하는 transient Actor와 런타임 수치
- 날짜별 작업 일지와 완료 목록
- 저장되지 않았거나 재로드로 확인되지 않은 예정 상태
- Source 코드에 이미 충분히 정의된 내부 구현
- 근거 없는 asset 값과 경로

## 갱신 규칙

1. 작업 전 관련 시스템 문서와 실제 대상 asset을 대조한다.
2. MCP 또는 Computer Use로 실제 저장한 allowlist asset만 반영한다.
3. Compile, 개별 Save와 디스크 재로드가 성공한 뒤 문서를 갱신한다.
4. Class Default와 Level override를 구분하고 override가 계약인지 우발적 값인지 기록한다.
5. 기존 설명을 현재 상태로 대체하며 변경 이력을 뒤에 누적하지 않는다.
6. 새 시스템 문서를 만들면 이 지도의 실제 링크를 함께 추가한다.
7. 통합 리뷰는 변경 asset과 관련 Unreal 문서가 일치하는지 확인한다.

## 정본 경계

- 시스템 책임, 상태 owner와 C++ API: `.md/0_ARCHITECTURE.md`, `.md/Architecture/*System.md`
- Editor asset 구조, authoring과 연결 계약: `.md/Unreal/*System.md`
- 현재 작업의 수행·검증 결과: `.md/PROMPT_INTEGRATION_REVIEW.md`
- MCP로 수행할 수 없는 미완료 Editor 작업: `.md/USER_UNREAL.md`

동일 사실을 Architecture와 Unreal 문서에 중복하지 않는다. C++ 책임과 Editor authoring이 만나는 지점만 양쪽에서 서로 링크한다.
