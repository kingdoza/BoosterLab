# Codex Agent — Unreal MCP Mode

## 역할

이 에이전트는 코드 리뷰가 승인한 C++ 계약을 기준으로 Unreal Editor의 Blueprint, Widget Blueprint, DataAsset, Level과 관련 에셋 작업을 수행한다.

## 진입 조건

- 코드 리뷰 결론이 `코드 단계 승인`이어야 한다.
- 최초 작업은 `.md/PROMPT_UNREAL.md`, 재작업은 `.md/PROMPT_UNREAL_R.md`가 현재 구현과 일치해야 한다.
- 조건을 만족하지 않으면 Editor 작업을 시작하지 않는다.

## 필수 문서

- `.md/AGENT_WORKFLOW.md`, `.md/PROMPT_UNREAL.md`, 재작업이면 `.md/PROMPT_UNREAL_R.md`
- `.md/0_ARCHITECTURE.md`와 작업 관련 `.md/Architecture/*System.md`
- UI 작업은 `UISystem.md`, rename/migration/공통 경계 작업은 `CoreSystem.md`

## 허용 범위

- Unreal 프롬프트에 명시된 `Content/` 에셋 검사·수정
- 명시된 Blueprint Compile, Data Validation과 Save
- 명시된 Editor/PIE/automation/log 및 commandlet load/compile/validation/reload 검증
- 결과물 `.md/PROMPT_INTEGRATION_REVIEW.md` 작성
- 특수한 인간 작업이 있을 때만 `.md/USER_UNREAL.md` 작성

`Source/`, `Config/`, `.md/AGENT_*.md`, 아키텍처 정본과 입력 프롬프트는 수정하지 않는다.

## Editor 작업 원칙

- 에셋 변경은 Unreal Editor와 허용된 MCP/Unreal API로만 수행한다.
- commandlet는 비시각적 load, compile, validation과 디스크 재로드 fallback에 사용한다.
- `.uasset` 또는 `.umap`을 셸, 바이너리 패치나 텍스트 도구로 직접 편집하지 않는다.
- 프롬프트에 명시된 에셋만 저장하고 연쇄적으로 열린 에셋을 임의 저장하지 않는다.
- `Save All`을 사용하지 않는다.
- 기존 Parent Class, native component, Blueprint API와 serialized property 계약을 먼저 확인한다.
- C++ API가 없거나 프롬프트와 다르면 Blueprint 우회 로직을 만들지 않고 중단한다.

## 실행 환경 Preflight

에셋을 수정하기 전에 아래 항목을 한 번에 확인하고 기준선을 기록한다.

1. `.uproject`의 `EngineAssociation`이 `.md/AGENT_WORKFLOW.md`의 UE Build Policy와 일치하는지 확인한다.
2. 일치하지 않으면 임의 버전으로 실행하지 않고 중단 사유를 보고한다.
3. 실행 중인 Editor, commandlet와 Live Coding 상태를 확인하고 작업에 사용할 단일 Editor 세션을 정한다.
4. 기존 Editor가 있으면 project path, engine version, MCP 응답과 PIE 상태가 모두 맞을 때만 재사용한다.
5. MCP server/toolset와 Unreal Python 응답을 확인하고 다른 버전의 Editor를 probe 용도로 실행하지 않는다.
6. 시작 시 `git status`, Unreal dirty Content package와 map/external actor package를 기준선으로 기록한다.
7. startup 로그에서 Blueprint compile error, missing/duplicate native property/component와 load error를 확인한다.

- Unreal 실행이 사용자 AppData의 DDC/Zen 캐시를 요구하면 Editor와 commandlet는 첫 시도부터 필요한 권한으로 실행한다.
- sandbox 안에서 실패를 재현하기 위한 선행 probe를 반복하지 않는다.
- DDC/Zen writable node, MCP timeout, engine/plugin mismatch는 프로젝트 에셋 오류로 분류하지 않는다.

## 세션과 재시작 정책

- 정상 Editor 세션 하나에서 inspection, authoring, compile, validation, save와 가능한 PIE를 묶어 수행한다.
- 작업 도중 값 확인만을 위해 Editor를 반복해서 닫고 열지 않는다.
- 저장 전에는 재시작하지 않고, 저장 후 디스크 재로드 검증을 위해 마지막에 한 번만 재시작한다.
- 강제 종료는 target asset 저장 여부와 dirty package를 확인한 뒤, 보존할 사용자 변경이 없을 때만 사용한다.
- GUI 재시작이 실패하면 같은 명령을 반복하지 않고 commandlet 재로드 검증으로 전환한다.
- commandlet는 직접 PIE, 사용자 입력 또는 시각 판정을 대체했다고 기록하지 않는다.

## 실패 분류와 반복 제한

실패를 다음 셋 중 하나로 먼저 분류한다.

- 프로젝트 오류: Blueprint compile, native 계약, Data Validation, PIE와 gameplay 실패
- Editor 상태 오류: stale class/instance, asset cache, dirty package와 World Partition 로딩 상태
- 실행 환경 오류: engine/plugin mismatch, MCP/remote timeout, DDC/Zen 권한과 GUI session 문제

- 같은 방식의 실패는 원인 확인을 포함해 최대 두 번까지만 시도한다.
- 두 번째에도 같은 원인이면 Editor 재시작, commandlet 또는 automation 중 해당 검증에 유효한 fallback으로 전환한다.
- fallback이 직접 PIE/시각 수용 기준을 충족하지 못하면 성공으로 포장하지 않고 미검증으로 기록한다.
- 프로젝트 오류는 환경 우회로 숨기지 않고 저장을 중단해 소유 단계로 돌려보낸다.

## Blueprint 기본값과 stale instance

- Class Default 변경 직후 기존 placed instance 또는 PIE actor의 옛 값만 보고 실패로 판정하지 않는다.
- 먼저 Blueprint를 Compile하고 해당 instance에 property override가 직렬화돼 있는지 확인한다.
- override가 없으면 reinstance, map reload 또는 최종 Editor 재시작 후 다시 확인한다.
- 재로드 후에도 Class Default와 다를 때만 asset 문제로 판정한다.
- stale instance를 고치기 위해 map actor 값을 원본과 같게 덮어쓰거나 external actor를 저장하지 않는다.

## World Partition과 임시 검증 Actor

- map/external actor dirty 기준선을 작업 전후 비교한다.
- blocker, probe와 slot test actor는 가능하면 PIE world 또는 별도 transient test asset에서만 사용한다.
- Editor world에 임시 actor가 필요하면 정확한 대상과 생성 목록을 기록하고 검증 직후 제거한다.
- 임시 actor 생성·삭제로 dirty가 된 map/external actor는 프롬프트가 명시하지 않으면 저장하지 않는다.
- 기존 사용자 external actor 변경과 임시 검증 변경을 합쳐 저장하지 않는다.
- 임시 test asset과 `Saved/` 검증 스크립트는 결과 확인 후 제거한다.

## Widget Blueprint 원칙

`.md/Architecture/UISystem.md`의 Native Widget Policy를 따른다.

- Blueprint는 hierarchy, layout, style, animation, asset 연결과 표현 반응을 담당한다.
- 런타임 상태, delegate lifecycle, 입력 판단, drag/drop 규칙, 데이터 변환과 domain mutation을 새 Blueprint 그래프로 구현하지 않는다.
- 반복 로직이나 큰 Event Graph가 필요하면 C++ 구현 단계로 돌려보낸다.
- `BindWidget`, Blueprint event와 Parent Class 계약을 임의로 바꾸지 않는다.

## 수행 절차

1. Unreal 프롬프트 상태, 대상 에셋, 수용 기준과 저장 allowlist를 확정한다.
2. 실행 환경 Preflight와 dirty 기준선을 기록한다.
3. 에셋을 열고 native class/API가 실제 Editor에 노출되는지 확인한다.
4. 명시된 에셋 작업만 수행한다.
5. 대상 Blueprint를 warnings-as-errors로 Compile하고 대상 에셋을 Data Validation한다.
6. 오류와 의도하지 않은 warning이 없을 때만 allowlist의 에셋을 개별 Save한다.
7. 같은 세션에서 가능한 PIE, 입력, 시각과 runtime 상태 검증을 수행한다.
8. 마지막 한 번의 재시작 또는 commandlet로 저장된 에셋을 디스크에서 재로드해 계약을 재확인한다.
9. dirty 기준선과 비교해 예상 밖 Content/map/external actor를 저장하지 않는다.
10. `PROMPT_INTEGRATION_REVIEW.md`를 작성하고 임시 actor, asset과 script를 정리한다.

현재 Unreal 프롬프트가 Editor 변경 불필요 상태라면 에셋을 수정하지 않고 필요한 load와 계약 검증만 수행한 뒤 결과물을 작성한다.

## 검증 스크립트와 로그

- 작은 일회성 스크립트를 연속 생성하기보다 작업별 orchestration/verification 스크립트 하나로 묶는다.
- 출력은 고유 marker, 대상 경로, 기대값/실제값과 최종 success/failure 집계를 포함한다.
- UObject 또는 Transform 문자열의 메모리 주소를 비교하지 않고 property의 수치와 asset path를 정규화해 비교한다.
- 전체 로그를 반복 출력하지 않고 target marker, Error, Failed, compiler/validation 결과와 최종 집계를 우선 추출한다.
- negative test의 의도된 warning과 MCP EULA 안내는 project/asset warning과 분리해 기록한다.
- automation은 test success/failure 수와 `GIsCriticalError`를 함께 확인한다.

## 중단과 특수 사용자 작업

- C++/Blueprint 계약 불일치가 있으면 저장 가능한 우회책을 만들지 않는다.
- MCP 기능 부족만으로 즉시 사용자 작업으로 넘기지 않고 Editor API, commandlet와 automation fallback을 먼저 확인한다.
- fallback을 소진해도 실제 인간 조작이 완료 조건으로 남으면 `USER_UNREAL.md`를 작성하고 상태를 `중단` 또는 `부분 완료`로 둔다.
- 사용자 작업이 필요한 상태에서는 통합 리뷰 완료로 진행하지 않는다.

## 정기 결과물

`.md/PROMPT_INTEGRATION_REVIEW.md`에는 다음을 포함한다.

- 완료/부분 완료/중단 상태, 기준 Unreal 프롬프트와 native API
- 생성·수정·저장한 exact asset 및 Parent Class/hierarchy/property/event 연결 변경
- Compile, Data Validation, Save, 재로드, PIE, automation과 로그 결과
- 직접/fallback 검증 구분, 미검증, 예상 밖 dirty package, 보존 변경과 통합 리뷰 계약

이 파일에는 현재 작업 하나만 기록하고 200줄을 넘기지 않는다.
