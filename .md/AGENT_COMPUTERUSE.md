# Codex Agent — Unreal Computer Use Mode

## 역할

이 에이전트는 Unreal Editor의 실제 화면을 관찰하고 마우스·키보드로 승인된 에셋 작업을 수행한다.
기술 워크플로의 Editor 단계이며, 완료 후 코드·Editor 통합 리뷰로 인계한다.

## 필수 문서

- [AGENT_WORKFLOW.md](AGENT_WORKFLOW.md): 순서, 소유권과 문서 크기
- [AGENT_UNREAL_MCP.md](AGENT_UNREAL_MCP.md): 공통 Editor 계약, 저장과 검증 규칙
- [0_ARCHITECTURE.md](0_ARCHITECTURE.md)와 관련 `Architecture/*System.md`
- UI 작업은 [UISystem.md](Architecture/UISystem.md), 이관 작업은 [CoreSystem.md](Architecture/CoreSystem.md)
- [PROMPT_UNREAL.md](PROMPT_UNREAL.md), 재작업이면 `PROMPT_UNREAL_R.md`
- 사용자가 지정한 경우 [USER_UNREAL.md](USER_UNREAL.md)의 대상 조작과 재개 조건
- 현재 설치된 `computer-use` 스킬의 `SKILL.md`, `docs/guidance.md`, `docs/api.md`, `docs/confirmations.md`

## 진입 조건과 허용 범위

- 기본 진입 조건은 코드 단계 승인과 현재 구현에 일치하는 Unreal 프롬프트다.
- 사용자가 일부 단계만 명시적으로 지시하면 해당 범위와 완료 기준을 먼저 고정한다.
- 제한된 UI 작업의 완료를 전체 코드 오류 해소 또는 통합 승인으로 해석하지 않는다.
- 대상 `Content/` 에셋의 Designer, Details, Compile, Data Validation과 개별 Save를 수행한다.
- `Source/`, `Config/`, 아키텍처와 입력 프롬프트는 별도 지시 없이 수정하지 않는다.
- 문서 편집·파일 조회는 파일 도구를 사용하고, Windows 앱 조작은 Computer Use JS API로 수행한다.
- `.uasset`과 `.umap`은 Unreal Editor 밖에서 직접 편집하지 않는다.

## 실행 환경 Preflight

1. `.uproject`의 EngineAssociation과 설치 엔진의 `Engine/Build/Build.version`을 확인한다.
2. 문서와 실제 patch version이 다르면 차이를 기록하고 로드·native 계약 검증을 생략하지 않는다.
3. `git status --short`와 대상 파일의 기존 변경을 기록한다.
4. 실행 중인 Editor와 commandlet를 확인하고 동일 프로젝트를 중복 실행하지 않는다.
5. 기존 사용자 Editor가 있으면 해당 세션을 우선 사용한다.
6. 프로젝트 경로, PIE/SIE 종료, Live Coding 상태와 기존 dirty package를 확인한다.
7. UI 화면과 모달을 관찰할 수 있어야 에셋 편집을 시작한다.
8. Computer Use는 활성 Windows 데스크톱의 visible Editor를 사용한다. offscreen 실행을 기본값으로 삼지 않는다.

## 확인된 초기화와 창 선택 설정

기준 환경: Windows 10 Home 22H2 `19045.6466`, Computer Use 스킬 번들 `26.901.51231`, 설치 UE `5.8.2`.
아래는 실제 성공한 연결 단계다. 화면 캡처와 에셋 편집까지 성공했다는 의미는 아니다.

```javascript
if (!globalThis.sky) {
  const { sky } = await import("@oai/sky");
  globalThis.sky = sky;
}
```

- 실행 도구는 `mcp__node_repl__js`이며 새 JavaScript 세션마다 위 초기화를 수행한다.
- `sky.list_apps()`와 `sky.list_windows()`의 앱·창 열거가 성공했다.
- 실행 중인 Editor가 없을 때 다음 기존 설치 경로로 `sky.launch_app({ app: ... })` 실행이 성공했다.
- Editor 실행 경로: `C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe`.
- 이 실행은 exe 실행만 확인한다. BathhouseSim 프로젝트 로드 성공을 별도로 확인해야 한다.
- 반환된 Unreal 창의 `id`와 `app`을 `sky.get_window({ id, app })`에 전달한다.
- `sky.activate_window({ window })`로 대상 창 활성화가 성공했다.
- 후보 창이 정확히 하나가 아니면 제목·프로젝트 문맥으로 다시 선택한다. 핸들을 추측하지 않는다.
- 창 ID, PID와 화면 좌표는 세션마다 달라지므로 지침서의 고정 설정으로 저장하지 않는다.

## 화면 관찰과 입력 규칙

- 기본 관찰은 `sky.get_window_state({ window })`이며 결과 스크린샷을 직접 확인한다.
- 접근성 확인이 필요할 때만 `include_text: true`를 추가한다.
- `include_screenshot: false, include_text: true`는 접근성 진단용이며 화면 검증을 대신하지 않는다.
- 관찰 결과를 읽고 다음 호출에서 입력 한 번만 수행한 뒤 즉시 상태를 다시 조회한다.
- 클릭·드래그 좌표는 최신 스크린샷과 그 `screenshotId`에 근거해야 한다.
- 스크롤·팝업·탭 전환·리셋 이후 이전 좌표, screenshotId와 element index를 재사용하지 않는다.
- 텍스트 입력 전 실제 편집 영역을 선택하고 포커스를 확인한다. 제어 키는 `press_key`로 보낸다.
- 모달이 보이지 않으면 `list_windows()`로 별도 창을 찾고 그 창을 관찰한다.
- 자동 표시된 스크린샷을 다시 디코드·저장·재출력하지 않는다.
- 화면 또는 접근성으로 편집 대상을 확인할 수 없으면 추측 입력을 하지 않는다.

## 확인된 캡처 차단 조건과 복구

- 위 기준 환경에서 스크린샷 요청은 `SetIsBorderRequired failed: 해당 인터페이스를 지원하지 않습니다. (0x80004002)`로 실패했다.
- 최초 캡처, 창 재검색·재바인딩, `js_reset` 후 재초기화·재선택에서도 동일 오류를 확인했다.
- 스크린샷을 끈 접근성 조회는 성공했지만 Unreal 최상위 창 한 개만 반환했고 내부 편집 컨트롤은 없었다.
- 따라서 현재 설정은 연결 일부 성공, 시각 제어 차단 상태다. 완전 성공한 편집 설정은 아직 확보되지 않았다.
- Microsoft 문서의 `IsBorderRequired` 도입 빌드는 `20348`이며 현재 OS 빌드 `19045`보다 높다.
- 오류와 OS/API 요구사항의 일치에 근거해 캡처 런타임의 OS 호환성 문제로 판단한다. Unreal 에셋 오류로 분류하지 않는다.
- 제공된 `sky` API에는 border 설정 또는 대체 캡처 backend 선택 옵션이 없다. 존재하지 않는 옵션을 만들지 않는다.
- helper exe 직접 실행·검색이나 별도 helper 프로토콜 클라이언트 구현으로 우회하지 않는다.
- 복구에는 해당 API를 지원하는 환경 또는 구형 OS를 처리하는 수정된 캡처 런타임이 필요하다. 실제 성공 여부는 재검증한다.
- OS 업그레이드, 보안·개인정보 설정 변경을 일반적인 Editor 세팅으로 수행하지 않는다.
- 환경이 달라지면 초기화 → 창 선택 → 화면 캡처 → 비파괴 UI 입력 → 재관찰 순서로 성공을 확인한다.
- 참고: [Microsoft IsBorderRequired 요구사항](https://learn.microsoft.com/en-us/uwp/api/windows.graphics.capture.graphicscapturesession.isborderrequired?view=winrt-26100).
- 참고: [공식 Computer Use 설정 및 Windows foreground 안내](https://learn.chatgpt.com/docs/computer-use).

## 에셋 작업과 저장

1. 대상 에셋의 경로, Parent Class, 기존 hierarchy와 native 계약을 확인한다.
2. 지정된 widget 이름·타입·Is Variable, component 또는 property만 수정한다.
3. UI는 hierarchy·layout·style을 담당하며 runtime visibility, timer와 progress 계산을 그래프로 추가하지 않는다.
4. Compile 결과와 오류·경고를 확인하고 계약 불일치는 구현 단계로 돌려보낸다.
5. 지정된 에셋만 개별 저장한다. `Save All`을 사용하지 않는다.
6. 작업 지시의 재오픈·재컴파일 조건을 확인한다. 에셋 탭 재오픈과 디스크 재로드 검증을 구분한다.
7. World Partition은 지정된 external actor/package만 저장하고 맵 재오픈 후 존속 여부를 검증한다.
8. 사용자 visible Editor를 임의 종료하지 않는다. 세션 정리는 공통 Editor 규칙을 따른다.

## 완료 조건과 결과물

- 환경 설정은 초기화 성공, 창 선택 성공, 캡처 성공, 입력·재관찰 성공을 각각 구분해 보고한다.
- 실제 에셋 변경·저장·컴파일·재로드 결과가 확인되어야 해당 작업을 완료 처리한다.
- 제한된 단계만 지시받았으면 StateTree, 맵, PIE 등 후속 단계를 자동으로 시작하지 않는다.
- Editor 작업 결과는 `PROMPT_INTEGRATION_REVIEW.md`에 현재 작업 상태와 미검증 항목을 기록한다.
- 환경 진단만 수행했으면 기존 에셋 검증 결과를 덮어쓰지 않고 진단 범위와 차단 원인을 보고한다.
- 이 지침서는 현재 설정과 재사용 규칙만 유지하며 에셋별 수행 이력을 누적하지 않는다.
- 문서 수정 후 diff, 링크와 줄 수를 확인한다. 목표 80~120줄, 최대 150줄이다.
