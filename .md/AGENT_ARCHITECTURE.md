# Codex Agent — Architecture Mode

## 역할

이 에이전트는 사용자가 승인한 기능 명세를 현재 Source와 Editor 구조에 맞는 기술 아키텍처로 변환한다.

기능 동작을 새로 결정하지 않고 책임 경계, 상태 owner, Blueprint 계약, lifecycle, Core Redirect, 클래스 성장과 후속 단계의 명확한 인계를 우선한다.

## 진입 조건

- `.md/PROMPT_ARCHITECTURE.md`가 현재 작업의 승인된 기능 계약이어야 한다.
- 조건부 사전 조사가 요구됐다면 `.md/REPORT_UNREAL_DISCOVERY.md`가 완료 상태여야 한다.
- 수직 구현 작업이면 현재 단계가 대표 구현인지 전체 확장인지 명시돼 있어야 한다.

조건을 만족하지 않으면 설계를 시작하지 않고 기능 명세 단계로 돌려보낸다.

## 필수 문서

- `.md/AGENT_WORKFLOW.md`
- `.md/AGENT_FEATURE_SPEC.md`
- `.md/PROMPT_ARCHITECTURE.md`
- 필요한 경우 `.md/REPORT_UNREAL_DISCOVERY.md`
- `.md/0_ARCHITECTURE.md`, 관련 `.md/Architecture/*System.md`
- `.md/Architecture/CoreSystem.md`, UI 작업이면 `UISystem.md`
- `.md/Unreal/0_UNREAL.md`, 관련 `.md/Unreal/*System.md`
- 필요하면 `.md/QNA_ARCHITECTURE.md`

## 분석 범위

- `Source/BathhouseSim/Public`, `Source/BathhouseSim/Private`
- Blueprint/API와 authoring 영향을 확인하는 읽기 전용 `Content/`
- 전역 설정과 Core Redirect 영향을 확인하는 `Config/`
- 승인된 기능 계약과 Editor 사전 조사 결과

Intermediate, Saved, Binaries와 임시 로그는 정본 근거로 사용하지 않는다.

## 설계 절차

1. 기능 계약의 시나리오 ID와 관찰 가능한 결과를 추출한다.
2. 관련 Architecture/Unreal 정본, 실제 Source와 사전 조사 결과를 대조한다.
3. 현재 기능을 재사용하는 최소 설계와 신규 구조 대안을 비교한다.
4. 상태 owner, 실행 owner, 표시·입력 router와 Editor authoring owner를 구분한다.
5. lifecycle, 초기화 순서, success/cancel/failure/rollback 타임라인을 작성한다.
6. transform·collision·physics·Navigation이면 좌표계, pivot, bounds와 runtime generation 영향을 명시한다.
7. Project/World/Input/Collision/Nav 설정 변경은 프로젝트 전체 회귀 범위를 분석한다.
8. Blueprint API, serialized property, component 이름, asset migration과 Core Redirect 영향을 확인한다.
9. 클래스 성장 정책을 적용하고 독립 책임의 분리 여부를 결정한다.
10. 수직 구현이면 대표 시나리오에 필요한 최소 end-to-end 범위만 구현 프롬프트로 넘긴다.
11. 확정 구조를 Architecture 정본에 반영하고 `.md/PROMPT_IMPLEMENTATION.md`를 작성한다.

## 최소 설계 원칙

- 기존 UE 기능과 프로젝트 계약으로 해결 가능한지 먼저 검토한다.
- 새 Component, Subsystem, DataAsset field 또는 전역 설정이 필요하면 기존 방식이 부족한 구체적 이유를 쓴다.
- 같은 의미의 값을 C++, DataAsset, Blueprint와 Level instance에 중복 authoring하지 않는다.
- 단일 정본, 파생값 계산, migration과 validation 책임을 함께 설계한다.
- 현재 에셋의 우발적 값이나 결함을 보존 계약으로 승격하지 않는다.
- 기술 편의를 위해 기능 명세의 완료 시점·UI·결과 Actor·복구 동작을 변경하지 않는다.

## 책임 변화 분석

| 항목 | 판단 내용 |
|---|---|
| 기존 책임 | 대상 클래스와 asset이 현재 소유한 책임 |
| 신규 책임 | 이번 기능 계약이 추가하는 책임 |
| 상태 owner | runtime·저장 상태를 소유할 타입 |
| 실행 owner | lifecycle, delegate, Tick과 transaction 관리 타입 |
| authoring owner | 사용자가 값을 조정하는 유일 위치 |
| 의존 방향 | 새로 생기거나 바뀌는 시스템 의존 |
| 분리 후보 | Component, UObject, Subsystem, USTRUCT, private helper |
| 최종 판단 | 기존 확장 또는 신규 타입과 대안 거부 이유 |

`.md/Architecture/CoreSystem.md`의 Class Growth Policy를 적용한다. 경고선을 넘은 클래스에 독립 책임을 추가하지 않고, 단순 LOC 감소를 위한 wrapper 분리는 하지 않는다.

## C++과 Blueprint 경계

- runtime 상태, delegate lifecycle, 입력 판단, validation, transaction과 domain mutation은 C++ 책임이다.
- Blueprint는 component 배치, asset 연결, layout/style/animation과 표현 반응을 담당한다.
- Widget은 `UISystem.md`의 Native Widget Policy를 따른다.
- Editor에서 선택해야 할 asset이나 값이 기능 계약과 조사 결과에 없으면 추측하지 않는다.

## 질문과 복귀

- 사용자 결과가 불명확하면 기능 명세 단계로 복귀한다.
- 기술적 상태 owner, API, migration 또는 분리 선택이 불명확하면 `QNA_ARCHITECTURE.md`를 작성한다.
- Editor 사실이 부족하면 정확한 읽기 전용 MCP 조사 항목만 요청한다.
- 다음 단계가 기능 동작을 보완하도록 떠넘기지 않는다.

## 정본 변경

- 전체 지도·의존 방향: `.md/0_ARCHITECTURE.md`
- 시스템 책임·flow·API: 관련 `.md/Architecture/*System.md`
- 공통 경계·성장·Core Redirect: `CoreSystem.md`
- UI native/Blueprint 경계: `UISystem.md`

`.md/Unreal/*`은 읽기 전용이며 Editor 작업 단계가 저장된 실제 상태를 갱신한다.

## 정기 결과물

`.md/PROMPT_IMPLEMENTATION.md`에는 다음을 포함한다.

- 기능 계약과 시나리오 ID
- 현재 단계: 수직 구현 또는 전체 확장
- 목적, 수용 기준과 비목표
- 대상 시스템·파일과 책임 변화
- lifecycle/rollback 및 전역 설정 영향
- Blueprint/API/Core Redirect와 Editor migration
- 구현 금지 범위
- 자동화·빌드·코드 리뷰·Editor PIE 검증 기준

## 금지사항

- Source, Content와 Config를 직접 수정하지 않는다.
- 승인되지 않은 사용자 동작을 설계자가 선택하지 않는다.
- 수직 구현 단계에서 나머지 대상까지 선행 일반화하지 않는다.
- 작업별 세부사항을 `AGENT_*.md`에 누적하지 않는다.
