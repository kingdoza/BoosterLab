# FirstPersonTemplate

Unreal Engine C++ 기반 범용 1인칭 템플릿 프로젝트입니다.

## Scope

- 1인칭 이동/시점 조작
- Jump
- Sprint
- 이동/착지 기반 camera shake
- Blueprint CameraManager를 통한 pitch limit 조정

도메인 gameplay, UI, inventory, interaction 기능은 포함하지 않습니다.

## Project Rename

새 프로젝트로 이식할 때는 루트의 `PROMPT_PROJECT_RENAME.md`를 사용합니다.

1. `PROMPT_PROJECT_RENAME.md`의 `PROJECT_NAME = __CHANGE_ME__` 한 곳만 새 프로젝트명으로 바꿉니다.
2. 해당 프롬프트를 구현 에이전트에게 실행시킵니다.
3. 프롬프트는 프로젝트/모듈명, Target/Build.cs, export macro, 문서 경로를 새 이름으로 갱신합니다.

이 rename 프롬프트는 다음 항목을 변경하지 않습니다.

- `Content/` asset, Blueprint, `.uasset`, `.umap`
- `/Game/...` Content 경로
- `GlobalDefaultGameMode`
- Content 폴더명
- gameplay C++ class 이름
- input action, mapping context, camera shake Blueprint asset 이름

## Documents

- `AGENTS.md`: agent entry point
- `.md/0_ARCHITECTURE.md`: architecture map
- `.md/Architecture/*.md`: system-specific architecture
- `.md/AGENT_*.md`: role-specific agent rules

## Generated Directories

다음 디렉터리는 설계 기준으로 보지 않고 필요 시 재생성합니다.

- `Binaries/`
- `Intermediate/`
- `Saved/`
- `DerivedDataCache/`
