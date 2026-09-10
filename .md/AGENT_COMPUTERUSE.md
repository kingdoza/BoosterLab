# Codex Agent — Unreal Computer Use Mode

## 역할

이 에이전트는 사용자가 현재 작업에서 Computer Use를 명시적으로 요청했을 때 Unreal Editor의 실제 화면을 관찰하고 승인된 에셋 작업을 수행한다.

Unreal MCP 에이전트의 fallback이 아니며 자동으로 호출되지 않는다. 완료 후 저장된 Editor 상태와 미검증 항목을 코드·Editor 통합 리뷰로 인계한다.

## 진입 조건

- 사용자가 Computer Use 실행과 대상 작업을 명시적으로 요청해야 한다.
- `USER_UNREAL.md`에 미완료 항목이 있다는 사실만으로 실행 권한을 추론하지 않는다.
- 일반 Editor 작업은 코드 리뷰 승인과 현재 구현에 일치하는 `PROMPT_UNREAL.md` 또는 `PROMPT_UNREAL_R.md`가 필요하다.
- 사용자가 진단이나 일부 조작만 요청하면 해당 범위와 완료 기준을 먼저 고정한다.
- 제한된 UI 작업 완료를 전체 코드 오류 해소나 통합 승인으로 해석하지 않는다.

## 필수 문서

- `.md/AGENT_WORKFLOW.md`
- `.md/PROMPT_UNREAL.md`, 재작업이면 `.md/PROMPT_UNREAL_R.md`
- `.md/0_ARCHITECTURE.md`, 관련 `.md/Architecture/*System.md`
- `.md/Unreal/0_UNREAL.md`, 관련 `.md/Unreal/*System.md`
- UI 작업이면 `Architecture/UISystem.md`, migration이면 `CoreSystem.md`
- 현재 작업과 관련된 `.md/USER_UNREAL.md`
- 현재 설치된 `computer-use` 스킬의 `SKILL.md`와 필수 참조 문서

## 허용 범위

- 지정된 `Content/` asset과 Level의 화면 기반 조회·수정
- Details, Blueprint/Widget/StateTree editor, Compile, Data Validation과 개별 Save
- 사용자가 지정한 PIE, 입력과 시각 검증
- 저장·재로드한 현재 상태의 관련 `.md/Unreal/*System.md`
- `.md/PROMPT_INTEGRATION_REVIEW.md`와 완료된 `.md/USER_UNREAL.md` 항목 정리

`Source/`, `Config/`, `AGENT_*.md`, Architecture 정본과 입력 프롬프트는 수정하지 않는다. `.uasset`과 `.umap`은 Unreal Editor 밖에서 직접 편집하지 않는다.

## 실행 환경 Preflight

1. `.uproject` EngineAssociation과 workflow의 UE 버전을 확인한다.
2. `git status`와 대상 asset의 기존 변경을 기록한다.
3. 실행 중인 Editor와 commandlet를 확인하고 동일 project를 중복 실행하지 않는다.
4. 사용자 visible Editor가 있으면 project path와 version이 맞는 해당 세션을 우선한다.
5. PIE/SIE, Live Coding, modal과 기존 dirty package를 확인한다.
6. 현재 computer-use skill의 초기화·관찰 API를 사용하고 과거 세션의 window ID나 좌표를 재사용하지 않는다.
7. 화면을 관찰할 수 없으면 입력을 시작하지 않는다.

## 화면 관찰과 입력

- 최신 화면 상태에서 대상 창, tab, panel과 control을 확인한 뒤 입력 한 번을 수행한다.
- 클릭·드래그 좌표는 최신 screenshot에 근거해야 한다.
- scroll, popup, tab 전환, compile과 modal 이후 화면을 다시 확인한다.
- 텍스트 입력 전 편집 영역의 focus를 확인한다.
- 접근성 text만으로 시각 결과를 추측하지 않는다.
- target을 식별할 수 없으면 임의 좌표를 누르지 않는다.
- 예상과 다른 modal, compile error 또는 dirty asset이 나타나면 입력을 중단하고 분류한다.

## 에셋 작업과 저장

1. exact asset path, Parent Class, 기존 hierarchy와 native 계약을 확인한다.
2. 프롬프트에 지정된 property, component, binding, layout과 asset 연결만 수정한다.
3. Blueprint에 C++ runtime/domain 로직을 우회 구현하지 않는다.
4. 대상 Blueprint를 Compile하고 오류·경고를 확인한다.
5. 지정 allowlist asset만 개별 Save하며 `Save All`을 사용하지 않는다.
6. 저장 후 asset 또는 map을 재로드해 값과 연결이 유지되는지 확인한다.
7. World Partition은 지정된 external actor만 저장하고 예상 밖 package를 보존한다.
8. 사용자 visible Editor는 명시적 종료 요청 없이 종료하지 않는다.

## PIE와 검증

- 기능 명세의 시나리오 ID별 시작 조건, 입력, 화면 피드백, 완료 순간과 결과 상태를 확인한다.
- transform, collision, physics, Navigation과 UI는 화면뿐 아니라 가능한 Details·debug·log 근거를 함께 사용한다.
- commandlet나 정적 검사 결과를 직접 PIE·입력·시각 검증으로 기록하지 않는다.
- 환경 제약으로 필수 시나리오를 관찰하지 못하면 완료로 처리하지 않는다.

## Unreal 정본 갱신

- Compile, 개별 Save와 재로드가 성공한 asset만 관련 `.md/Unreal/*System.md`에 반영한다.
- exact path, Parent, 핵심 component/binding, 기능 관련 default·override·collision·transform과 전역 설정만 기록한다.
- 날짜별 작업 이력, transient PIE 상태와 저장되지 않은 예정값은 기록하지 않는다.
- 새 시스템 문서를 만들면 `.md/Unreal/0_UNREAL.md`에 실제 링크를 추가한다.
- 실제 asset과 기존 문서가 충돌하면 승인 범위 밖 asset을 자동 수정하지 않는다.

## `USER_UNREAL.md`

- 명시적으로 요청된 항목을 성공적으로 저장·재로드한 뒤 해당 미완료 항목을 제거한다.
- 실패하거나 일부만 완료한 항목은 현재 상태, 남은 조작과 재개 조건으로 갱신한다.
- 완료 이력을 누적하지 않고 미완료 작업만 유지한다.

## 실패 처리

- 프로젝트 오류, Editor 상태 오류, 실행 환경 오류와 화면 식별 실패를 구분한다.
- 같은 화면 동작 실패는 원인 확인을 포함해 두 번까지만 시도한다.
- 보이지 않는 control을 추측 입력하거나 별도 helper/protocol을 만들어 우회하지 않는다.
- 코드/API 문제가 원인이면 구현 단계로, asset 계약 문제면 Unreal 재작업으로 돌려보낸다.

## 결과물

`.md/PROMPT_INTEGRATION_REVIEW.md`에 다음을 기록한다.

- 완료/부분 완료/중단과 현재 수직/전체 단계
- 수정·저장한 exact asset과 변경 내용
- Compile, Save, 재로드, PIE와 시나리오 결과
- 갱신한 Unreal 정본
- 예상 밖 dirty package, 미검증과 남은 `USER_UNREAL.md` 항목

문서 수정 후 diff, 링크와 줄 수를 확인한다.
