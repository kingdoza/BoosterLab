# Codex Agent — Implementation Mode

## 역할

이 에이전트는 FirstPersonTemplate의 **구현 담당**이다.

구현은 현재 정본 아키텍처 문서와 사용자 지시를 기준으로 수행한다. 임의의 구조 변경, Public API 삭제, Blueprint 계약 변경은 금지한다.

## 정본 문서

작업 시작 시 반드시 읽는다.

- `.md/0_ARCHITECTURE.md`
- 작업과 직접 관련된 `.md/Architecture/*System.md`
- `.md/QNA_IMPLEMENTATION.md`

필요 시 함께 참고한다.

- `.md/Architecture/CoreSystem.md`: Core Redirect와 공통 규칙
- `.md/USER_UNREAL.md`: Editor 수동 검증이 필요한 경우

## 주요 작업 경로

| 용도 | 경로 |
|---|---|
| 전체 아키텍처 지도 | `.md/0_ARCHITECTURE.md` |
| 시스템별 아키텍처 | `.md/Architecture/*.md` |
| 구현 QnA | `.md/QNA_IMPLEMENTATION.md` |
| 리뷰 프롬프트 | `.md/PROMPT_REVIEW.md` |
| Unreal Editor 사용자 작업 | `.md/USER_UNREAL.md` |

## Source 구조 기준

- 현재 Source 시스템 폴더와 파일 분포는 `.md/0_ARCHITECTURE.md`의 `Source 구조` 섹션을 단일 출처로 삼는다.
- 시스템별 책임과 의존 방향은 `.md/0_ARCHITECTURE.md`의 `시스템 문서`, `시스템 간 책임 흐름`, `주요 의존 방향` 섹션을 따른다.
- `Core`는 문서상 경계이며 소스 폴더가 아니다.
- 시스템 추가/삭제가 있으면 이 파일이 아니라 `.md/0_ARCHITECTURE.md`와 관련 `.md/Architecture/*System.md`를 갱신한다.

## 구현 절차

1. `.md/0_ARCHITECTURE.md`를 읽고 작업과 직접 관련된 `.md/Architecture/*System.md`를 선택해 읽는다.
2. 작업 범위와 영향 파일을 보고한다.
3. Blueprint/API/Core Redirect 영향이 있는지 확인한다.
4. 애매한 점이 있으면 `.md/QNA_IMPLEMENTATION.md`에 질문을 작성하고 중단한다.
5. 구현한다.
6. 빌드 또는 가능한 검증을 수행한다.
7. 구조 변경이 있으면 `.md/0_ARCHITECTURE.md`와 관련 `.md/Architecture/*System.md`를 갱신한다.
8. 리뷰용 프롬프트가 필요하면 `.md/PROMPT_REVIEW.md`를 작성한다.
9. Editor 수동 검증이 필요하면 `.md/USER_UNREAL.md` 또는 최종 보고에 사용자 작업을 명시한다.

## QnA 규칙

다음 상황에서는 구현하지 말고 `.md/QNA_IMPLEMENTATION.md`에 질문한다.

- 구현 방식이 여러 가지로 나뉘는 경우
- 기존 아키텍처 문서와 충돌하는 경우
- Public Blueprint API 삭제/rename 가능성이 있는 경우
- UCLASS/USTRUCT/UENUM rename 또는 file rename이 필요한 경우
- Core Redirect 필요 여부가 불명확한 경우
- Content 에셋 compile/save가 필요한 경우
- 성능과 구조 사이의 트레이드오프가 큰 경우

질문 형식:

```text
### [질문 항목]

1. 질문 제목
- 질문 내용
- 필요한 이유
- 선택지
  - 옵션 A: ...
  - 옵션 B: ...
  - 옵션 C: ...
- 권장 옵션:
```

## 코드 작성 원칙

- 기존 시스템 경계를 따른다.
- 상태 owner와 입력/표시 router를 분리한다.
- Actor는 composition root에 가깝게 유지하고, 상태와 반복 가능한 기능은 Component 또는 명확한 owner로 분리한다.
- 새 기능을 추가할 때는 먼저 상태 owner, 실행 주체, 입력/표시 router, Blueprint/API 계약을 정의한다.
- 사용자 지시 없이 현재 아키텍처 범위 밖의 도메인 기능을 추가하지 않는다.
- `Private` helper는 public header로 노출하지 않는다.
- `#include "Public/..."` 형태를 만들지 않는다.
- 삭제/rename은 참조 검색과 Blueprint 영향 확인 후 진행한다.

## Blueprint/Core Redirect 원칙

- Blueprint native parent로 쓰이는 class rename은 사용자 승인 없이 진행하지 않는다.
- Blueprint 참조가 확인된 UFUNCTION/UPROPERTY는 대체 노드 migration 전까지 삭제하지 않는다.
- UCLASS/USTRUCT/UENUM rename 시 `Config/DefaultEngine.ini` `[CoreRedirects]` 검토가 필요하다.
- Core Redirect 작업은 Editor 재시작, Blueprint compile/save, post-migration scan까지 검증 범위에 포함한다.

## 문서 업데이트 규칙

구조 변경 시 다음을 갱신한다.

- 전체 시스템 목록, Blueprint 계약, 변경 기록: `.md/0_ARCHITECTURE.md`
- 시스템 책임/flow/API 변경: 관련 `.md/Architecture/*System.md`
- Core Redirect 또는 공통 규칙 변경: `.md/Architecture/CoreSystem.md`

단순 버그 수정이나 내부 구현만 바뀐 경우에는 문서 갱신이 필요 없을 수 있다. 이때 최종 보고에 "아키텍처 문서 반영 불필요" 사유를 적는다.

## Unreal 검증

가능한 경우 UBT 빌드를 수행한다.

```powershell
& "C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\DotNET\AutomationTool\UnrealBuildTool.exe" FirstPersonTemplateEditor Win64 Development -Project="C:\UnrealProjects\FirstPersonTemplate\FirstPersonTemplate.uproject" -WaitMutex -NoHotReloadFromIDE
```

경로가 없으면 임의의 엔진 버전을 사용하지 말고 사용자에게 확인한다.

## 인간 검토 필요 영역

구현 완료 후 아래 항목을 보고한다.

- Blueprint compile/save가 필요한 항목
- Core Redirect 검증이 필요한 항목
- 게임플레이 밸런스에 영향을 주는 계산이 추가된 경우
- UObject lifetime, GC, transient 참조
- local player 전용 로직
- Tick 비용이 늘어난 부분

## 보고 형식

```text
[상태] 작업 중 / 질문 필요 / 완료

[참조 문서]
- 읽은 아키텍처 문서

[수행 내용]
- 구현 요약

[영향 파일]
- 변경 파일

[검증]
- 빌드/검색/수동 검증 결과

[아키텍처 문서 반영]
- 반영 / 불필요
- 대상 문서

[검토 필요]
- 사용자 또는 리뷰 에이전트가 확인할 항목

[PROMPT_REVIEW.md]
- 작성 / 미작성
```
