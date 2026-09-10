# Codex Agent — Common Workflow

## 목적

이 문서는 BathhouseSim 기술 작업의 공통 순서, 사용자 승인 관문, 단계별 결과물, 문서 소유권과 크기 제한을 정의한다.

워크플로는 기술적으로 일관된 구현뿐 아니라 사용자가 승인한 실제 게임 동작과 Editor 결과까지 일치시키는 것을 목표로 한다.

## 기본 순서

다음 단계를 순서대로 수행한다.

1. 기능 명세 초안
2. 필요한 경우 Unreal MCP 읽기 전용 사전 조사
3. 기능 명세 확정과 사용자 승인
4. 아키텍처 설계
5. C++ 구현
6. 코드 리뷰
7. Unreal MCP Editor 작업
8. 코드·Editor 통합 리뷰

- 기능 명세 승인 전에는 아키텍처와 구현을 시작하지 않는다.
- 앞 단계가 승인 또는 완료되기 전에는 다음 단계로 진행하지 않는다.
- 코드 리뷰 실패는 구현으로, 설계 문제는 아키텍처 또는 기능 명세로 돌아간다.
- Unreal 작업 이후에는 반드시 통합 리뷰를 거친다.
- MCP가 수행할 수 없는 Editor 작업이 남으면 통합 승인하지 않고 `.md/USER_UNREAL.md`로 인계한다.

## 수직 구현 경로

입력·UI·Content·Actor lifecycle·물리·collision·Navigation 또는 여러 대상의 공통화가 함께 바뀌는 작업은 대표 시나리오 하나를 먼저 완성한다.

```text
기능 명세 승인
→ 수직 구현 아키텍처
→ 구현
→ 코드 리뷰
→ Unreal MCP 작업
→ 통합 리뷰
→ 사용자 수직 구현 승인
→ 전체 확장 아키텍처부터 동일 순서 반복
```

- 수직 구현은 일부 계층만 만드는 것이 아니라 대표 대상 하나의 사용자 흐름 전체를 완성한다.
- 사용자 승인 전에는 나머지 설비·아이템·상태로 일반화하지 않는다.
- 단순 내부 버그, 계산, 로그와 Content 비의존 변경은 기능 명세에서 근거를 남기고 직접 전체 구현 경로를 사용할 수 있다.
- 사용자 수직 구현 승인도 구현·Editor를 직접 수정하는 권한이 아니며, 전체 확장 단계의 새 아키텍처 입력으로 사용한다.

## Unreal MCP 사용 경계

Unreal MCP 에이전트에는 두 가지 진입 모드가 있다.

- 사전 조사 모드: 기능 명세 중 필요한 Blueprint/Level/StateTree/UI/설정 사실을 읽기 전용으로 조사한다.
- Editor 작업 모드: 코드 리뷰가 승인한 `PROMPT_UNREAL.md` 범위만 수정·검증·저장한다.

MCP 에이전트는 기본적으로 Computer Use를 실행하지 않는다. Unreal MCP toolset으로 수행할 수 있는 작업만 처리하고, 지원되지 않는 클릭·시각 조작·asset 편집은 우회하지 않고 `.md/USER_UNREAL.md`에 남긴다. Computer Use는 사용자가 별도로 명시적으로 요청한 작업에서만 `.md/AGENT_COMPUTERUSE.md`에 따라 실행한다.

## 정본 정책

| 정책 | 정본 |
|---|---|
| 승인된 사용자 동작과 수용 시나리오 | `.md/PROMPT_ARCHITECTURE.md` |
| 전체 시스템 지도와 의존 방향 | `.md/0_ARCHITECTURE.md` |
| 클래스 성장과 책임 분리 | `.md/Architecture/CoreSystem.md` |
| 시스템별 책임과 API | 관련 `.md/Architecture/*System.md` |
| 전체 Editor authoring 지도 | `.md/Unreal/0_UNREAL.md` |
| 시스템별 asset 구조·연결·설정 | 관련 `.md/Unreal/*System.md` |
| 실제 serialized Editor 데이터 | `Content/`의 `.uasset`, `.umap` |

Unreal 문서는 asset 전체를 복제하지 않고 C++ 계약과 기능 결과에 영향을 주는 현재 구조만 기록한다. 날짜별 진행 기록은 Git에 맡긴다.

## 정기 결과물

| 생산 단계 | 결과물 |
|---|---|
| 기능 명세 | `.md/PROMPT_ARCHITECTURE.md` |
| MCP 사전 조사 | `.md/REPORT_UNREAL_DISCOVERY.md` |
| 아키텍처 설계 | `.md/PROMPT_IMPLEMENTATION.md` |
| C++ 구현 | `.md/PROMPT_REVIEW.md`, `.md/PROMPT_UNREAL.md` |
| 코드 리뷰 실패 | `.md/PROMPT_IMPLEMENTATION_R.md` |
| Unreal MCP/Computer Use 작업 | `.md/PROMPT_INTEGRATION_REVIEW.md`, 관련 `.md/Unreal/*System.md` |
| 통합 리뷰 코드 재작업 | `.md/PROMPT_IMPLEMENTATION_R.md` |
| 통합 리뷰 Unreal 재작업 | `.md/PROMPT_UNREAL_R.md` |

- 사전 조사가 불필요하면 `REPORT_UNREAL_DISCOVERY.md`를 만들지 않고 생략 근거를 기능 명세에 남긴다.
- 코드 리뷰 승인과 최종 통합 승인은 정기 결과물을 만들지 않고 보고로 종료한다.
- 하나의 결과물은 현재 작업 또는 현재 수직 구현 단계 하나만 기록한다.

## 결과물 소유권

- 기능 명세 단계만 `PROMPT_ARCHITECTURE.md`를 작성한다.
- 아키텍처 단계만 `PROMPT_IMPLEMENTATION.md`와 Architecture 정본을 작성한다.
- 구현 단계만 Source/승인된 Config, `PROMPT_REVIEW.md`, `PROMPT_UNREAL.md`를 작성한다.
- Unreal MCP 사전 조사만 `REPORT_UNREAL_DISCOVERY.md`를 작성한다.
- 실제 Editor 작업을 수행한 Unreal MCP 또는 명시적 Computer Use 단계만 관련 Unreal 정본과 `PROMPT_INTEGRATION_REVIEW.md`를 작성한다.
- 리뷰 단계는 입력을 직접 고치지 않고 해당 소유 단계로 돌려보낸다.
- `AGENT_*.md`에는 작업별 클래스·에셋·수행 기록을 누적하지 않는다.

## `USER_UNREAL.md` 미완료 작업 큐

Unreal MCP toolset으로 완료할 수 없는 실제 Editor 작업만 `.md/USER_UNREAL.md`에 기록한다.

- 한국어로 작성한다.
- exact asset path, 현재 상태, 필요한 조작, 예상 결과와 검증·재개 조건을 포함한다.
- 각 항목은 미완료 상태만 유지하고 완료 이력은 누적하지 않는다.
- 일반적인 MCP 가능 작업이나 단순 검증을 사용자에게 넘기지 않는다.
- MCP 에이전트는 Computer Use로 우회하지 않는다.
- 사용자가 직접 완료하거나 Computer Use를 명시적으로 요청할 수 있다.
- 완료 후 실제 asset 상태를 검증하고 관련 항목을 제거하기 전에는 의존하는 통합 리뷰를 승인하지 않는다.

## 질문과 복귀

- 사용자 동작 선택은 기능 명세 단계에서 `.md/QNA_FEATURE_SPEC.md`로 질문한다.
- 기술 설계 선택은 `QNA_ARCHITECTURE.md`를 사용한다.
- 구현 선택은 `QNA_IMPLEMENTATION.md`를 사용한다.
- 리뷰 정보 부족은 `QNA_REVIEW.md`를 사용한다.
- 구현 중 사용자 동작을 새로 결정하지 않고 기능 명세로 복귀한다.
- Editor 단계에서 코드·설계 문제가 발견되면 Blueprint 우회를 만들지 않고 소유 단계로 복귀한다.

## 문서 크기 정책

- `AGENT_*.md`: 목표 80~120줄, 최대 150줄
- 공통 정책 문서: 최대 200줄
- 작업 프롬프트와 결과물: 작업 하나당 최대 200줄
- 시스템 문서: 300줄부터 분리 검토, 400줄 이상 성장 동결

상한을 넘으면 현재 상태만 남기고 안정적인 책임 경계로 분리한다. 같은 목록·규칙·asset 값을 여러 문서에 복제하지 않는다.

## 공통 완료 조건

- 승인된 기능 시나리오와 현재 단계 범위를 벗어나지 않았다.
- 사용자 소유 변경과 무관한 파일·asset을 수정하지 않았다.
- 빌드, Compile, Save, 재로드, PIE 중 수행한 검증과 미검증을 구분했다.
- Editor 변경은 관련 `.md/Unreal/*System.md`의 현재 상태와 일치한다.
- 예상 밖 dirty package와 `USER_UNREAL.md` 미완료 항목이 통합 승인 전에 해소됐다.
- 문서 변경 후 diff, 링크, 소유권과 줄 수를 확인했다.

## UE 5.8 Build Policy

BathhouseSim의 C++ build는 항상 다음 진입점을 사용한다.

```powershell
& 'C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat' `
  BathhouseSimEditor Win64 Development `
  -Project='C:\UnrealProjects\BathhouseSim\BathhouseSim.uproject' `
  -WaitMutex `
  -NoHotReloadFromIDE
```

Codex shell에서는 UBT 자식 프로세스와 Engine/Uba 접근을 위해 첫 시도부터 필요한 권한으로 실행한다. 승인 실행이 불가능하면 시도하지 않고 차단 사유를 보고한다. system `dotnet`, MSBuild 직접 실행, `UnrealBuildTool.exe` 또는 `.dll` 직접 실행은 사용하지 않는다.
