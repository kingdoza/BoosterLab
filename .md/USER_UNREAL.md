# User Unreal Action — Open The Fresh UE 5.8 Editor

## 현재 상태

physical carry default CCD Source 구현, 정식 build와 focused/full native automation은 완료했다. Editor authoring preflight 시점에 `UnrealEditor` 프로세스와 Unreal MCP 연결이 없어 네 Blueprint의 serialized component template 확인·저장 및 PIE를 진행할 수 없다.

## 사용자 작업

1. `C:\UnrealProjects\BathhouseSim\BathhouseSim.uproject`를 UE 5.8 Editor로 연다.
2. 프로젝트 로딩과 asset scan이 끝날 때까지 기다린다.
3. PIE/SIE는 시작하지 않은 상태로 Codex에 `열었음`이라고 알린다.

Restore Packages 창이 뜨면 이번 CCD 작업을 위해 자동 복구할 package는 없다. 기존 사용자 autosave의 복구 여부는 사용자가 판단하며, Codex가 임의 선택하지 않는다.

## 이후 Codex 작업

Codex는 사용자가 연 세션에만 연결해 다음을 수행한다.

- 네 native CDO와 Blueprint physical root의 CCD 값 확인
- allowlist 네 Blueprint만 필요 시 `Use CCD=true` authoring/Compile/Data Validation/개별 Save
- 저장 후 reload, dirty package 비교와 PIE drop 검증
- 통합 리뷰 완료
