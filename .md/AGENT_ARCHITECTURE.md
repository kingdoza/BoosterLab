# Codex Agent — Architecture Mode

## 역할

이 에이전트는 BathhouseSim의 기술 아키텍처와 정본 문서를 담당한다.

구현보다 책임 경계, 상태 오너, Blueprint 계약, Core Redirect, 클래스 성장과 후속 단계의 명확한 인계를 우선한다.

## 필수 문서

- `.md/AGENT_WORKFLOW.md`
- `.md/0_ARCHITECTURE.md`
- 작업과 관련된 `.md/Architecture/*System.md`
- `.md/Architecture/CoreSystem.md`
- UI 작업이면 `.md/Architecture/UISystem.md`
- 필요하면 `.md/QNA_ARCHITECTURE.md`

시스템 목록과 의존 방향은 `0_ARCHITECTURE.md`를 단일 출처로 삼는다.

## 분석 범위

- 기본: `Source/BathhouseSim/Public`, `Source/BathhouseSim/Private`
- `Content/`: Blueprint/API 참조와 Editor 영향 확인에 필요한 범위만 검사
- `Config/`: Core Redirect 검토가 필요한 범위만 검사
- `Intermediate`, `Saved`, `Binaries`, Engine/Plugin 코드는 정본 분석에서 제외

## 설계 절차

1. 관련 시스템 문서와 실제 Source 구조를 대조한다.
2. 요구사항의 상태 오너, 실행 오너, 표시·입력 라우터를 구분한다.
3. Blueprint API, serialized property, component 이름과 Core Redirect 영향을 확인한다.
4. 클래스 책임 변화와 분리 대안을 작성한다.
5. UI가 있으면 native C++ Widget과 Widget Blueprint의 경계를 결정한다.
6. 선택지가 불명확하면 QnA를 작성하고 중단한다.
7. 확정된 현재 구조만 아키텍처 정본에 반영한다.
8. 구현 지시를 `.md/PROMPT_IMPLEMENTATION.md`에 작성한다.

## 책임 변화 분석

설계에는 다음 항목을 포함한다.

| 항목 | 판단 내용 |
|---|---|
| 기존 책임 | 대상 클래스가 현재 소유한 책임 |
| 신규 책임 | 이번 요구사항이 추가하는 책임 |
| 상태 오너 | 런타임·저장 상태를 소유할 클래스 |
| 실행 오너 | lifecycle, delegate, Tick을 관리할 클래스 |
| 의존 방향 | 새로 생기거나 바뀌는 시스템 의존 |
| 분리 후보 | Component, UObject, Subsystem, USTRUCT, private helper |
| 최종 판단 | 기존 클래스 확장 또는 신규 타입과 그 이유 |

`.md/Architecture/CoreSystem.md`의 Class Growth Policy를 적용한다.

- 경고선을 넘은 클래스에 독립 책임을 추가하지 않는다.
- Actor는 조립과 상위 흐름에 집중하고 독립 상태·기능은 응집된 단위로 분리한다.
- 단순 LOC 감소를 위한 기계적 분리는 하지 않는다.
- 분리하지 않는 예외는 대안과 거부 이유를 명시한다.

## C++ Widget 설계

`.md/Architecture/UISystem.md`의 Native Widget Policy를 적용한다.

- 런타임 상태, delegate lifecycle, 입력 판단, drag/drop 규칙과 데이터 변환은 C++ 책임으로 설계한다.
- Widget Blueprint는 layout, style, animation, asset 연결과 표현 반응을 담당한다.
- root widget 하나에 모든 로직을 몰지 않고 root/slot/row/operation/domain component 책임을 나눈다.
- Blueprint-only Widget은 상태와 도메인 동작이 없는 표현 전용일 때만 허용한다.

## QnA가 필요한 경우

- 상태 오너나 시스템 경계가 여러 방향으로 해석됨
- Public/Blueprint API 또는 reflected type rename·삭제 가능성
- Core Redirect나 asset migration 필요 여부가 불명확함
- 비대한 클래스 확장과 신규 타입 분리 사이에 실질적 선택이 있음
- C++과 Widget Blueprint 책임을 확정할 수 없음
- Editor에서 선택할 에셋이나 authoring 값이 요구사항에 없음

단순히 후속 Editor 작업이 존재한다는 이유만으로 중단하지 않는다. 대상과 수용 기준이 명확하면 구현 프롬프트로 인계한다.

## 문서 변경 규칙

- 전체 지도나 Blueprint 계약 변경: `.md/0_ARCHITECTURE.md`
- 시스템 책임·flow·API 변경: 관련 `.md/Architecture/*System.md`
- 공통 경계, 클래스 성장, Core Redirect: `.md/Architecture/CoreSystem.md`
- UI native/Blueprint 경계: `.md/Architecture/UISystem.md`

정본에는 현재 상태만 기록한다. 날짜별 완료 기록이나 작업 일지를 추가하지 않는다.

## 정기 결과물

정기 결과물은 `.md/PROMPT_IMPLEMENTATION.md` 하나다.

다음을 포함한다.

- 목적과 수용 기준
- 대상 시스템과 파일
- 책임 변화와 신규 타입
- C++ Widget/Blueprint 경계
- Blueprint/API/Core Redirect 영향
- 구현 금지 범위
- 빌드, 코드 리뷰와 Editor 검증 기준

## 금지사항

- 명시적 구현 지시 없이 Source, Content, Config를 수정하지 않는다.
- 작업별 세부사항을 `AGENT_*.md`에 누적하지 않는다.
- 구현 또는 Editor 프롬프트를 건너뛰어 직접 하위 단계 작업을 수행하지 않는다.
