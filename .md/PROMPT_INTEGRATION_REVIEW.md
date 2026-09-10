# 통합 검토 — 설비 Actor 교체 및 Conversion Opt-Out

## 작업 범위와 결과

- 입력 프롬프트: `.md/PROMPT_UNREAL.md`
- Source, Config, `.uproject`, native parent class, reflected 이름은 변경하지 않았다.
- `.uasset` 직접 패치나 `Save All`은 사용하지 않았다.
- 기존 더티 worktree는 보존했다.
- 아래 7개 `FacilityPlacementDefinition`만 저장했다.

## 저장된 Definition 연결

각 Definition에 `Recovery Item Class = /Script/BathhouseSim.PlaceableFacilityItemActor`를 저장했다. `Recovery Item Mesh`는 모두 `None`으로 유지하여 native cube fallback을 사용한다.

| Definition | Placed Facility Class |
| --- | --- |
| `DA_FacilityPlacement_Bath` | `BP_Bath` |
| `DA_FacilityPlacement_Shower` | `BP_Shower` |
| `DA_FacilityPlacement_ClothesLocker_1` | `BP_ClothesLocker` |
| `DA_FacilityPlacement_ClothesLocker_4` | `BP_ClothesLocker_4` |
| `DA_FacilityPlacement_ClothesLocker_8` | `BP_ClothesLocker_8` |
| `DA_FacilityPlacement_Washer` | `BP_Washer` |
| `DA_FacilityPlacement_Dryer` | `BP_Dryer` |

기존 `StableId`, `Facility.Placeable` 태그, Preview class, footprint cell 값, locker slot count는 읽은 뒤 변경하지 않았다.

## Blueprint 확인

다음 7개 Blueprint의 `FacilityPlacement` 컴포넌트는 이미 `Mode = Placed` 및 각각의 대응 Definition을 가리키고 있었다. 따라서 원본 컴포넌트/그래프를 덮어쓰지 않았다.

- `BP_Bath`, `BP_Shower`, `BP_ClothesLocker`, `BP_ClothesLocker_4`, `BP_ClothesLocker_8`
- `BP_Washer`, `BP_Dryer`

각 Blueprint의 Event Graph 및 Construction Script에는 별도의 패키지 mesh spawn/attach/복제 분기가 없었다. `PackagePhysicalRoot`, `PlacementFootprint`, `FacilityPlacement` 등 기존 컴포넌트 이름도 보존했다.

`BP_CleanTowelStack` 및 `BP_UsedTowelBin`과 그 Definition은 수정·저장·재배정하지 않았다.

## 검증

- 7개 대상 Blueprint를 warnings-as-errors로 컴파일했다. 컴파일 요청은 모두 성공했고 대상 Blueprint는 dirty 상태가 되지 않았다.
- 7개 Definition 저장 후 대상 14개(Definition 7 + Blueprint 7)의 dirty 상태가 모두 `false`임을 확인했다.
- 기본 PIE를 1초 warmup으로 시작하고 정상 종료했다. 시작 중 assertion/Blueprint compile error는 없었다.
- Map Check는 에디터 시작 시 `0 Error(s), 0 Warning(s)`로 완료됐다.

현재 MCP 툴셋에는 Data Validation 실행 명령이 노출되지 않아 Data Validation은 실행하지 못했다. PIE의 플레이어 입력, 설비 회수/손 장착/자유 드롭, 재배치, 세탁기·건조기 동작은 실제 상호작용 검증이 필요하다.

저장 직후 fresh-editor restart/reload도 시도했으나 새 백그라운드 Editor가 MCP 초기화 완료 전에 멈춰 재연결할 수 없었다. 저장 전후의 값 재조회 및 dirty 검증은 완료했으며, 해당 agent-owned Editor는 종료했다.

## 후속 수동 확인

1. Editor를 다시 열어 7개 Definition의 `Placed Facility Class`, `Recovery Item Class`, `Recovery Item Mesh=None`을 한 번 확인한다.
2. Data Validation을 실행한다.
3. PIE에서 Bath/Shower/Locker 1·4·8/Washer/Dryer 각각을 회수하여 손 장착, G 자유 드롭, 재배치까지 확인한다.
4. Clean Towel Stack과 Used Towel Bin이 기존 전용 흐름을 계속 사용하며 설비 회수 후보가 아닌지 확인한다.

## 결론

Definition 기반 설비 교체 연결은 저장 완료됐다. 수동 Data Validation과 상호작용 acceptance가 남아 있으므로, 그 두 항목을 마친 뒤 최종 통합 승인하는 것이 적절하다.
