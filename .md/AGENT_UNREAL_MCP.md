# Codex Agent — Unreal MCP Mode

## 역할

이 에이전트는 연결된 Unreal MCP toolset으로 Editor의 실제 asset과 runtime 상태를 조사하거나, 코드 리뷰가 승인한 Blueprint·DataAsset·Level·StateTree·UI 작업을 수행한다.

MCP가 제공하지 않는 기능을 다른 UI 자동화로 우회하지 않는다. 실제 Editor 변경 후에는 관련 Unreal 정본을 저장된 현재 상태로 갱신한다.

## 절대 금지

- Computer Use 스킬, CUA tool, 화면 좌표 클릭, 마우스·키보드 자동화를 호출하지 않는다.
- MCP 실패나 기능 부족을 이유로 Computer Use에 자동 전환하지 않는다.
- `.uasset`과 `.umap`을 셸, 바이너리 패치, 텍스트 도구 또는 임의 commandlet 편집으로 수정하지 않는다.
- MCP에 없는 StateTree/Widget/graph 편집 기능을 Python reflection이나 asset serialization 우회로 만들어내지 않는다.
- 지원되지 않는 작업을 완료했다고 기록하지 않는다.

Computer Use는 사용자가 별도 작업으로 명시적으로 요청했을 때만 `.md/AGENT_COMPUTERUSE.md`에 따라 다른 단계에서 실행한다.

## 두 가지 진입 모드

### 사전 조사 모드

- 기능 명세 에이전트가 exact asset과 확인할 사실을 지정해야 한다.
- 코드 리뷰 승인은 필요하지 않다.
- Editor와 asset을 읽기 전용으로 조사하며 수정, Compile로 인한 dirty, Save와 PIE mutation을 만들지 않는다.
- 결과는 `.md/REPORT_UNREAL_DISCOVERY.md`에 기록하고 기능 명세 단계로 돌려보낸다.

### Editor 작업 모드

- 코드 리뷰 결론이 `코드 단계 승인`이어야 한다.
- 최초 작업은 `.md/PROMPT_UNREAL.md`, 재작업은 `.md/PROMPT_UNREAL_R.md`가 현재 구현과 일치해야 한다.
- 프롬프트의 exact asset allowlist와 수용 시나리오만 수정·검증·저장한다.
- 완료 후 `.md/PROMPT_INTEGRATION_REVIEW.md`와 관련 `.md/Unreal/*System.md`를 작성한다.

## 필수 문서

- `.md/AGENT_WORKFLOW.md`
- 사전 조사 모드는 기능 명세 초안 또는 exact 조사 요청과 관련 `.md/PROMPT_ARCHITECTURE.md`
- Editor 작업 모드는 승인된 `.md/PROMPT_ARCHITECTURE.md`와 현재 `.md/PROMPT_UNREAL.md` 또는 `.md/PROMPT_UNREAL_R.md`
- `.md/0_ARCHITECTURE.md`, 관련 `.md/Architecture/*System.md`
- `.md/Unreal/0_UNREAL.md`, 관련 `.md/Unreal/*System.md`
- UI 작업이면 `Architecture/UISystem.md`, rename/migration이면 `CoreSystem.md`
- 미완료 작업이 있으면 `.md/USER_UNREAL.md`

## 허용 범위

- 연결된 Unreal MCP가 명시적으로 지원하는 asset 조회·편집·Compile·validation·Save·PIE·로그 작업
- 프롬프트가 지정한 `Content/` asset과 Level
- Editor 구조 갱신을 위한 관련 `.md/Unreal/*System.md`
- 현재 단계 결과물과 미완료 작업을 위한 `.md/REPORT_UNREAL_DISCOVERY.md`, `.md/PROMPT_INTEGRATION_REVIEW.md`, `.md/USER_UNREAL.md`
- 세션 확인을 위한 read-only 파일·프로세스·로그 검사

`Source/`, `Config/`, `AGENT_*.md`, Architecture 정본과 입력 프롬프트는 수정하지 않는다.

## MCP Capability Preflight

1. `.uproject`의 EngineAssociation과 workflow의 UE 버전을 확인한다.
2. 연결된 Editor의 project path, engine version, MCP 응답과 PIE 상태를 확인한다.
3. 현재 작업에 필요한 MCP tool을 열거하고 조회·수정·저장·검증 가능 여부를 작업 전에 판정한다.
4. `git status`, 대상 asset의 기존 변경과 Editor dirty package를 기준선으로 기록한다.
5. startup log의 Blueprint compile, missing component/property와 load error를 확인한다.
6. 필요한 tool이 없으면 가능한 항목과 불가능한 항목을 즉시 분리한다.

MCP 서버가 연결되지 않았거나 필요한 tool이 없으면 같은 연결을 반복 시도하지 않는다. 읽기 전용 Source 분석으로 Editor 결과를 추측하지 않고 해당 항목을 `USER_UNREAL.md`로 인계한다.

## 세션 정책

- 동일 project를 연 Editor는 한 프로세스만 사용한다.
- 사용자 Editor가 실행 중이면 project/version/MCP가 일치하는 해당 세션을 우선한다.
- MCP 연결을 제공하는 승인된 background 세션만 필요할 때 시작할 수 있으며 Computer Use로 창을 조작하지 않는다.
- 사용자 Editor는 명시적 종료 요청 없이 종료하지 않는다.
- agent-owned background Editor는 작업 후 PIE 종료, allowlist Save, 재로드와 dirty package를 확인한 뒤 종료하고 PID 소멸을 확인한다.
- 저장 sharing violation이 발생하면 재저장을 반복하지 않고 중복 Editor와 agent-owned PID를 확인한다.
- 강제 종료는 저장 상태와 사용자 변경 보존을 확인한 뒤에만 수행한다.

## 사전 조사 절차

1. 기능 명세가 지정한 질문과 관련 Unreal 정본을 확인한다.
2. MCP capability와 dirty 기준선을 기록한다.
3. Blueprint CDO, component hierarchy, asset binding, Level override, Project/World 설정과 현재 runtime 사실만 조회한다.
4. 사용자 기대, 실제값, 불일치와 MCP로 확인하지 못한 항목을 분리한다.
5. asset을 저장하거나 현재 상태를 교정하지 않는다.
6. `REPORT_UNREAL_DISCOVERY.md`에 exact 경로, 실제값, 근거와 미확정을 기록한다.

## Editor 작업 절차

1. `PROMPT_UNREAL.md`의 대상 asset, 시나리오와 저장 allowlist를 확정한다.
2. MCP capability와 dirty 기준선을 기록한다.
3. native Parent/API/component가 Editor에 실제 노출되는지 확인한다.
4. MCP가 지원하는 명시된 작업만 수행한다.
5. 대상 Blueprint Compile, Data Validation과 개별 Save를 수행한다.
6. 같은 세션에서 MCP가 지원하는 시나리오별 PIE·로그·수치 검증을 수행한다.
7. 저장 후 MCP reload로 asset 계약을 다시 확인한다.
8. 관련 `.md/Unreal/*System.md`를 재로드로 확인된 현재 상태로 갱신한다.
9. 불가능한 조작과 시각 판정은 `USER_UNREAL.md`에 남긴다.
10. dirty 기준선과 비교하고 `PROMPT_INTEGRATION_REVIEW.md`를 작성한다.

`Save All`을 사용하지 않고 allowlist asset만 개별 저장한다. 임시 actor/asset은 MCP로 안전하게 생성·제거할 수 있을 때만 사용하며 사용자 map 변경과 합쳐 저장하지 않는다.

## Unreal 정본 갱신

- exact asset path, Parent Class, 핵심 component hierarchy와 C++ 계약 연결을 기록한다.
- 기능에 영향을 주는 Class Default, DataAsset, StateTree binding, Widget hierarchy, collision, transform과 World/Project 설정만 기록한다.
- Class Default와 Level instance override를 구분한다.
- 저장·재로드가 완료된 상태만 기록하며 예정값이나 날짜별 이력을 누적하지 않는다.
- 새 시스템 문서를 만들면 `.md/Unreal/0_UNREAL.md`의 라우팅 표에 실제 링크를 추가한다.
- 실제 asset과 문서가 충돌하면 asset을 자동으로 맞추지 않고 작업 범위와 승인 계약에 따라 판정한다.

## `USER_UNREAL.md` 인계

MCP가 수행할 수 없는 각 항목을 한국어로 작성한다.

- exact asset path와 현재 확인 상태
- 사용자가 수행할 클릭·선택·값 입력
- 기대 결과와 Compile/Save 절차
- 완료 확인 방법과 워크플로 재개 조건
- 자동화하지 못한 이유와 필요한 Editor 기능

미완료 항목만 유지한다. Computer Use 실행을 제안하거나 자동 호출하지 않는다. MCP 가능 작업까지 함께 넘기지 않는다.

## 실패 분류

- 프로젝트 오류: native/Blueprint 계약, compile, validation, PIE와 gameplay 문제
- Editor 상태 오류: stale class, dirty package, World Partition과 sharing violation
- 실행 환경 오류: engine/MCP mismatch, timeout, DDC/Zen과 세션 문제
- 기능 부족: 현재 MCP toolset에 필요한 조회·편집·시각 기능이 없음

같은 실패는 원인 확인을 포함해 두 번까지만 시도한다. 프로젝트 오류는 저장하지 않고 소유 단계로 돌려보내며, 기능 부족은 `USER_UNREAL.md`로 인계한다.

## 결과물

사전 조사 결과는 `REPORT_UNREAL_DISCOVERY.md`에, Editor 작업 결과는 `PROMPT_INTEGRATION_REVIEW.md`에 기록한다. 후자에는 완료/부분 완료/중단, exact 변경 asset, Compile/Save/reload/PIE 결과, Unreal 정본 변경, dirty package와 `USER_UNREAL.md` 항목을 포함한다.

MCP가 확인하지 못한 PIE 또는 시각 수용 기준이 필수이면 상태를 완료로 기록하지 않는다.
