# Project Rename Prompt

## User Input

아래 한 줄의 값만 원하는 새 프로젝트 이름으로 바꾼 뒤 이 프롬프트를 실행한다.

```text
PROJECT_NAME = __CHANGE_ME__
```

## Role

너는 Unreal Engine C++ 템플릿 프로젝트를 새 프로젝트명으로 이식하는 구현 에이전트다.

현재 작업 디렉터리를 이미 복사된 템플릿 프로젝트 루트로 간주한다. 위 입력값만 새 이름으로 사용하고, 나머지 기존 이름은 현재 파일 구조에서 자동 추론한다.

## Hard Exclusions

다음 항목은 이번 작업에서 변경하지 않는다.

- `Content/` 아래 asset, Blueprint, `.uasset`, `.umap`
- `/Game/...` Content 경로
- `GlobalDefaultGameMode` key와 그 값
- `/Game/...` 형태의 Blueprint/Content asset 연결 경로
- Content 폴더명
- 프로젝트 루트 디렉터리명
- gameplay C++ type 이름
- `AFirstPersonCharacter`, `AFirstPersonController`, `UFirstPersonMovementComponent`, `UFirstPersonCameraShakeComponent`, `AFirstPersonCameraManager` 같은 native gameplay class 이름
- input action, input mapping context, camera shake Blueprint asset 이름
- `Intermediate/`, `Binaries/`, `Saved/`, `DerivedDataCache/`, `.vs/`, `.idea/`

Build.cs class, Target.cs class, primary module 이름, export macro는 프로젝트/모듈명에 속하므로 변경 대상이다.

## Validation Before Editing

1. 새 이름이 Unreal/C# module identifier와 export macro에 안전한지 확인한다.
   - 영문으로 시작
   - 이후 문자는 영문, 숫자, `_`만 허용
   - 공백, 하이픈, 한글, 특수문자 금지
   - C# 예약어 금지
   - C++ 예약어 금지
   - 기존 프로젝트명과 동일한 이름 금지
   - Windows에서 대소문자만 다른 이름 금지
   - `<새 프로젝트명 대문자>_API` 형태의 export macro가 영문, 숫자, `_`만 포함하는지 확인
2. 루트에서 `.uproject` 파일을 정확히 하나 찾고, 파일 stem을 기존 프로젝트명으로 사용한다.
3. `Source/<기존 프로젝트명>/` 모듈 디렉터리와 다음 파일들이 있는지 확인한다.
   - `Source/<기존 프로젝트명>.Target.cs`
   - `Source/<기존 프로젝트명>Editor.Target.cs`
   - `Source/<기존 프로젝트명>/<기존 프로젝트명>.Build.cs`
   - `Source/<기존 프로젝트명>/<기존 프로젝트명>.cpp`
   - `Source/<기존 프로젝트명>/<기존 프로젝트명>.h`
4. 다음 destination path가 이미 있으면 편집하지 말고 충돌 목록을 보고한다.
   - `<새 프로젝트명>.uproject`
   - `<새 프로젝트명>.sln`
   - `Source/<새 프로젝트명>/`
   - `Source/<새 프로젝트명>.Target.cs`
   - `Source/<새 프로젝트명>Editor.Target.cs`
   - `Source/<새 프로젝트명>/<새 프로젝트명>.Build.cs`
   - `Source/<새 프로젝트명>/<새 프로젝트명>.cpp`
   - `Source/<새 프로젝트명>/<새 프로젝트명>.h`
5. 위 조건이 맞지 않으면 편집하지 말고, 발견한 구조와 필요한 사용자 확인 사항을 보고한다.

## Required Changes

### Project Root

- 루트 `.uproject` 파일명을 새 이름으로 변경한다.
- `.uproject` 내부 module name을 새 이름으로 변경한다.
- 루트 `.sln` 파일이 있고 파일 stem이 기존 프로젝트명과 같으면 새 이름으로 파일명만 변경한다. 솔루션 내부 내용은 Unreal이 재생성할 수 있으므로 수동 편집을 최소화한다.
- 프로젝트 루트 디렉터리명은 변경하지 않는다.

### Source Module

- `Source/<기존 프로젝트명>/` 디렉터리를 새 이름으로 변경한다.
- `Source/<기존 프로젝트명>.Target.cs` 파일명을 새 이름 기준으로 변경한다.
- `Source/<기존 프로젝트명>Editor.Target.cs` 파일명을 새 이름 기준으로 변경한다.
- Target.cs 내부 class 이름, constructor 이름, `ExtraModuleNames.Add(...)` 값을 새 이름 기준으로 변경한다.
- module Build.cs 파일명, class 이름, constructor 이름을 새 이름 기준으로 변경한다.
- primary module `.cpp`/`.h` 파일명을 새 이름 기준으로 변경한다.
- primary module `.cpp`의 include와 `IMPLEMENT_PRIMARY_GAME_MODULE` module symbol/string을 새 이름 기준으로 변경한다.
- Source 전체에서 기존 export macro를 새 이름 기준 export macro로 변경한다.
  - 규칙: `<기존 프로젝트명 대문자>_API` -> `<새 프로젝트명 대문자>_API`
  - 예: `FIRSTPERSONTEMPLATE_API` -> `<NEWPROJECTNAME_UPPER>_API`
- Source 전체에서 기존 모듈 파일 include만 새 이름 기준으로 변경한다.
- gameplay class 이름과 gameplay include 경로는 변경하지 않는다.

### Config

- `Config/*.ini`에서 `/Script/<기존 프로젝트명>` module reference를 새 이름 기준으로 변경한다.
- 단, `GlobalDefaultGameMode` key는 값이 `/Script/...`이더라도 변경하지 않는다.
- `GlobalDefaultGameMode`에 기존 module reference가 남으면 최종 보고에서 제외 조건 때문에 남긴 항목으로 보고한다.
- `/Game/...` Content 경로 또는 `/Game/...` 형태의 Blueprint/Content asset 연결 경로는 변경하지 않는다.
- `Config/DefaultEngine.ini`의 `[/Script/Engine.Engine]` 섹션에서 `ActiveGameNameRedirects`를 검토한다.
- 기존 module script package를 참조하는 Blueprint/asset을 보호하기 위해 `ActiveGameNameRedirects`에 기존 script package에서 새 script package로 가는 redirect가 있어야 한다.
- 최소한 다음 두 형태를 보장한다.
  - `+ActiveGameNameRedirects=(OldGameName="<기존 프로젝트명>",NewGameName="/Script/<새 프로젝트명>")`
  - `+ActiveGameNameRedirects=(OldGameName="/Script/<기존 프로젝트명>",NewGameName="/Script/<새 프로젝트명>")`
- `ActiveGameNameRedirects`의 `OldGameName` 값은 migration source이므로 변경하지 않는다. `NewGameName`만 `/Script/<새 프로젝트명>`으로 갱신한다.
- 기존 템플릿 또는 이식 출발점의 redirect가 이미 있으면 `NewGameName`을 `/Script/<새 프로젝트명>`으로 갱신한다.
- 이미 같은 의미의 redirect가 있으면 중복 추가하지 않는다.
- `/Game/...` Content 경로는 변경하지 않는다.

### Documents

다음 문서의 프로젝트명, Source 경로, UBT target/project 경로를 새 이름 기준으로 변경한다.

프로젝트 루트 디렉터리명은 변경하지 않으므로, 절대경로를 갱신할 때는 현재 작업 디렉터리 경로를 유지하고 파일명/target/module 부분만 새 이름으로 재구성한다. 예를 들어 현재 작업 디렉터리가 `C:\UnrealProjects\FirstPersonTemplate`이면 project path는 `C:\UnrealProjects\FirstPersonTemplate\<새 프로젝트명>.uproject` 형태여야 한다.

- `AGENTS.md`
- `.md/0_ARCHITECTURE.md`
- `.md/AGENT_ARCHITECTURE.md`
- `.md/AGENT_IMPLEMENTATION.md`
- `.md/AGENT_REVIEW.md`
- `.md/Architecture/*.md`
- `README.md`에 기존 프로젝트명이나 Source 경로가 있으면 함께 변경한다.

문서에서 gameplay class 이름은 실제 코드 이름을 유지한다.

## Verification

1. 다음 범위에서 기존 프로젝트명을 검색한다.
   - `AGENTS.md`
   - `.md/`
   - `Config/`
   - `Source/`
   - 루트 `.uproject`
   - `Source/*.Target.cs`
2. Source 전체에서 `<기존 프로젝트명 대문자>_API` export macro가 남아 있는지 검색한다.
3. 기존 프로젝트명은 다음 항목에만 허용한다.
   - `[/Script/Engine.Engine]`의 `ActiveGameNameRedirects` `OldGameName`
   - `GlobalDefaultGameMode`처럼 hard exclusion key/value에 남은 reference
   - 프로젝트 루트 디렉터리명이 포함된 절대경로
4. hard exclusion 때문에 남긴 기존 프로젝트명 reference는 최종 보고에 위치와 이유를 명시한다.
5. `Content/`, `Intermediate/`, `Binaries/`, `Saved/`, `DerivedDataCache/`, `.vs/`, `.idea/`는 검색/수정 기준에서 제외한다.
6. UnrealBuildTool로 새 Editor target을 빌드한다. 명령 형태는 다음과 같다.
   ```powershell
   & "<UE 경로>\Engine\Binaries\DotNET\AutomationTool\UnrealBuildTool.exe" <새 프로젝트명>Editor Win64 Development -Project="<현재 작업 디렉터리>\<새 프로젝트명>.uproject" -WaitMutex -NoHotReloadFromIDE
   ```
7. 빌드 실패 시 코드/프로젝트명 rename 누락을 먼저 수정하고 다시 빌드한다.
8. 가능하면 Editor를 새 `.uproject`로 열어 Blueprint native parent, GameMode, input/camera reference 로드 오류가 없는지 확인한다.
9. Editor 검증 중에도 `Content/` asset을 수정하거나 저장하지 않는다.
10. Editor 검증을 수행하지 못하면 최종 보고에 `ActiveGameNameRedirects`/Blueprint 로드 검증 미수행을 명시한다.

## Final Report

최종 보고에는 다음만 포함한다.

- 변경한 파일/디렉터리 목록
- 변경하지 않은 exclusion 항목 확인
- 기존 프로젝트명이 남은 위치와 남긴 이유
- UBT 빌드 결과
- Editor Blueprint 로드 검증 결과 또는 미수행 사유
