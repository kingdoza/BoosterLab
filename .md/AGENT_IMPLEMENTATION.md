# Codex Agent — Implementation Mode

## 역할

이 에이전트는 승인된 아키텍처와 `.md/PROMPT_IMPLEMENTATION.md`를 기준으로 C++를 구현하고 코드 리뷰·Unreal 작업 프롬프트를 생산한다.

임의의 구조 변경, Public API 삭제, Blueprint 계약 변경과 Content 수정은 하지 않는다.

## 필수 문서

- `.md/AGENT_WORKFLOW.md`
- `.md/PROMPT_IMPLEMENTATION.md`
- 재작업이면 `.md/PROMPT_IMPLEMENTATION_R.md`
- `.md/0_ARCHITECTURE.md`
- 작업과 관련된 `.md/Architecture/*System.md`
- `.md/Architecture/CoreSystem.md`
- UI 작업이면 `.md/Architecture/UISystem.md`
- `.md/QNA_IMPLEMENTATION.md`

## 허용 범위

- 승인된 `Source/BathhouseSim/Public`, `Source/BathhouseSim/Private`
- 명시적으로 승인된 `Config/`
- 구현으로 현재 구조가 바뀐 경우 관련 아키텍처 정본
- 정기 결과물 `.md/PROMPT_REVIEW.md`, `.md/PROMPT_UNREAL.md`

`Content/` 에셋은 검사할 수 있지만 수정하거나 저장하지 않는다.

## 구현 절차

1. 구현 프롬프트와 정본 문서가 일치하는지 확인한다.
2. 대상 파일, 기존 책임, Blueprint/API/Core Redirect 영향을 보고한다.
3. 대상 클래스의 변경 전 크기와 신규 책임을 확인한다.
4. 불명확한 선택이 있으면 `QNA_IMPLEMENTATION.md`를 작성하고 중단한다.
5. 승인된 책임 경계 안에서 구현한다.
6. 변경 후 클래스 성장과 Widget 책임 경계를 다시 확인한다.
7. diff 검사, focused search와 가능한 UBT 빌드를 수행한다.
8. 실제 구조가 바뀌었다면 관련 정본을 현재 상태로 갱신한다.
9. `PROMPT_REVIEW.md`와 `PROMPT_UNREAL.md`를 작성한다.

## 클래스 성장 검사

`.md/Architecture/CoreSystem.md`의 Class Growth Policy를 적용한다.

구현 전후에 다음을 확인한다.

- header/cpp 물리적 줄 수 변화
- 추가된 `UPROPERTY`, `UFUNCTION`, delegate와 lifecycle 함수
- 새 상태와 실행 흐름이 기존 책임에 속하는지
- Component, UObject, Subsystem, USTRUCT 또는 private helper 분리 가능성

경고선을 넘은 클래스에 새 독립 책임을 추가해야 한다면 임의로 진행하지 않는다. 설계에 분리가 없으면 QnA 또는 아키텍처 단계로 돌려보낸다.

크기를 줄이기 위한 무의미한 wrapper 분리는 피하고 상태와 lifecycle이 함께 움직이는 응집된 기능을 분리한다.

## C++ Widget 구현

`.md/Architecture/UISystem.md`의 Native Widget Policy를 적용한다.

다음이 있는 Widget은 Unreal 작업 전에 native C++ base를 구현한다.

- 런타임 상태와 context
- delegate bind/unbind
- `NativeConstruct`/`NativeDestruct` cleanup
- 입력, drag/drop 판단과 validation
- 표시 데이터 변환
- Inventory/Focus/Interaction API 호출

Widget Blueprint에는 hierarchy, layout, style, animation, asset 연결과 표현 이벤트만 남긴다.

- root/slot/row/drag operation/domain component를 책임별로 나눈다.
- 하나의 C++ root widget에 모든 UI 로직을 몰지 않는다.
- C++에서 구현해야 할 로직을 `PROMPT_UNREAL.md`의 Blueprint 작업으로 미루지 않는다.

## Blueprint와 Core Redirect

- Blueprint native parent 또는 reflected type rename은 사용자 승인 없이 진행하지 않는다.
- 참조된 `UFUNCTION`/`UPROPERTY`는 migration 전까지 삭제하지 않는다.
- reflected rename은 Core Redirect, Editor 재시작, Blueprint compile/save와 post-migration scan을 함께 계획한다.
- Content 작업이 필요하다는 사실만으로 구현을 중단하지 않고 정확한 Unreal 프롬프트로 인계한다.

## 검증

- 변경 범위에 대해 `git diff --check`와 focused `rg` 검사를 수행한다.
- 가능한 경우 UE 5.8 `BathhouseSimEditor Win64 Development` UBT 빌드를 수행한다.
- 설정된 엔진 경로가 없으면 다른 버전을 추측하지 않고 미검증으로 보고한다.
- Blueprint Compile/Save와 PIE는 `PROMPT_UNREAL.md`의 검증 범위로 명시한다.

## 아키텍처 문서

- 구조·책임·API가 바뀌었을 때만 관련 정본을 갱신한다.
- 단순 버그 수정이나 내부 구현 변경은 갱신하지 않을 수 있으며 이유를 보고한다.
- 날짜별 Update나 구현 일지를 정본 문서에 추가하지 않는다.

## 정기 결과물

### `.md/PROMPT_REVIEW.md`

- 요구사항과 수용 기준
- 변경 파일과 구현 요약
- 클래스 크기·책임 변화
- Blueprint/API/Core Redirect 영향
- 빌드와 정적 검증 결과
- 코드 리뷰 중점과 미검증 항목

### `.md/PROMPT_UNREAL.md`

- 작업 필요 / 변경 불필요 상태
- 정확한 에셋 경로와 Parent Class
- property, component, `BindWidget`, event와 asset 연결 계약
- layout/style/animation 등 Blueprint 허용 작업
- Compile/Save/재로드/PIE 절차와 수용 기준
- Blueprint에서 구현하면 안 되는 C++/domain 로직

Editor 변경이 없어도 `변경 불필요`와 필요한 검증 범위를 명시한다.

`USER_UNREAL.md`는 정기 결과물이 아니다. 실제 인간 작업이 필요한 특수 상황에만 `.md/AGENT_WORKFLOW.md` 규칙에 따라 사용할 수 있다.

## 완료 보고

```text
[수행 내용]
[영향 파일]
[클래스 성장 검사]
[검증]
[아키텍처 문서]
[PROMPT_REVIEW.md] 작성
[PROMPT_UNREAL.md] 작성
```
