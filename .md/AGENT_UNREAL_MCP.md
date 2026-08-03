# Codex Agent — Unreal MCP Mode

## 역할

이 에이전트는 코드 리뷰가 승인한 C++ 계약을 기준으로 Unreal Editor의 Blueprint, Widget Blueprint, DataAsset, Level과 관련 에셋 작업을 수행한다.

## 진입 조건

- 코드 리뷰 결론이 `코드 단계 승인`이어야 한다.
- 최초 작업은 `.md/PROMPT_UNREAL.md`, 재작업은 `.md/PROMPT_UNREAL_R.md`가 현재 구현과 일치해야 한다.
- 조건을 만족하지 않으면 Editor 작업을 시작하지 않는다.

## 필수 문서

- `.md/AGENT_WORKFLOW.md`
- `.md/PROMPT_UNREAL.md`
- 재작업이면 `.md/PROMPT_UNREAL_R.md`
- `.md/0_ARCHITECTURE.md`
- 작업과 관련된 `.md/Architecture/*System.md`
- UI 작업이면 `.md/Architecture/UISystem.md`
- rename, migration 또는 공통 경계 작업이면 `.md/Architecture/CoreSystem.md`

## 허용 범위

- `PROMPT_UNREAL.md`에 명시된 `Content/` 에셋 검사·수정
- 명시된 Blueprint Compile/Save
- 명시된 PIE와 Editor 로그 검증
- 결과물 `.md/PROMPT_INTEGRATION_REVIEW.md` 작성
- 특수한 인간 작업이 있을 때만 `.md/USER_UNREAL.md` 작성

다음은 수정하지 않는다.

- `Source/`
- `Config/`
- `.md/AGENT_*.md`
- 아키텍처 정본
- 입력 프롬프트

## Editor 작업 원칙

- 에셋은 Unreal Editor와 허용된 MCP 기능으로만 수정한다.
- `.uasset` 또는 `.umap`을 셸이나 텍스트 도구로 직접 편집하지 않는다.
- 프롬프트에 명시된 에셋만 저장하고 연쇄적으로 열린 에셋을 임의 저장하지 않는다.
- 기존 Parent Class, native component, Blueprint API와 serialized property 계약을 먼저 확인한다.
- C++ API가 없거나 프롬프트와 다르면 Blueprint 우회 로직을 만들지 않고 중단한다.

## Widget Blueprint 원칙

`.md/Architecture/UISystem.md`의 Native Widget Policy를 따른다.

- Blueprint는 hierarchy, layout, style, animation, asset 연결과 표현 반응을 담당한다.
- 런타임 상태, delegate lifecycle, 입력 판단, drag/drop 규칙, 데이터 변환과 domain mutation을 새 Blueprint 그래프로 구현하지 않는다.
- 반복 로직이나 큰 Event Graph가 필요하면 C++ 구현 단계로 돌려보낸다.
- `BindWidget`, Blueprint event와 Parent Class 계약을 임의로 바꾸지 않는다.

## 수행 절차

1. 현재 단계의 Unreal 프롬프트 상태, 대상 에셋과 수용 기준을 확인한다.
2. 에셋을 열고 native class/API가 실제 Editor에 노출되는지 확인한다.
3. 명시된 에셋 작업만 수행한다.
4. 대상 Blueprint/DataAsset/Level을 Compile하고 명시된 에셋만 Save한다.
5. 에셋 재로드, PIE와 로그 검증을 가능한 범위에서 수행한다.
6. 예상 밖 변경과 미검증 항목을 분리해 기록한다.
7. `PROMPT_INTEGRATION_REVIEW.md`를 작성한다.

현재 Unreal 프롬프트가 Editor 변경 불필요 상태라면 에셋을 수정하지 않고 필요한 로드·계약 검증만 수행한 뒤 결과물을 작성한다.

## 중단과 특수 사용자 작업

- C++/Blueprint 계약 불일치가 있으면 저장 가능한 우회책을 만들지 않는다.
- MCP가 지원하지 않는 인간 조작이 실제 완료 조건이면 `USER_UNREAL.md`를 특수하게 작성하고 상태를 `중단` 또는 `부분 완료`로 둔다.
- 사용자 작업이 필요한 상태에서는 통합 리뷰 완료로 진행하지 않는다.

## 정기 결과물

`.md/PROMPT_INTEGRATION_REVIEW.md`에는 다음을 포함한다.

- 작업 상태: 완료 / 부분 완료 / 중단
- 기준 `PROMPT_UNREAL.md` 또는 `PROMPT_UNREAL_R.md`와 native API
- 생성·수정·저장한 에셋
- Parent Class, hierarchy, property, event와 asset 연결 변경
- Compile/Save/재로드/PIE/로그 결과
- 미검증 항목과 예상 밖 변경
- 통합 리뷰가 확인할 코드·에셋 계약

이 파일에는 현재 작업 하나만 기록하고 200줄을 넘기지 않는다.

## 보고 형식

```text
[상태] 완료 / 부분 완료 / 중단
[변경 에셋]
[검증]
[미검증]
[USER_UNREAL.md] 작성 / 미작성
[PROMPT_INTEGRATION_REVIEW.md] 작성
```
