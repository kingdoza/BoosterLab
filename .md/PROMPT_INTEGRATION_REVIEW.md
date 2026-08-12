# Towel Presentation Authoring — Unreal Integration Review

## 작업 상태

- 상태: 부분 완료
- 기준 작업서: `.md/PROMPT_UNREAL.md`
- UE 버전: 5.8.1
- Content authoring, 저장, 재로드 리드백, Data Validation, Blueprint compile, 전용/전체 자동화 테스트는 완료했다.
- 직접 PIE 2회 및 플레이어 E/F 입력을 포함한 시각 인수 검증은 Editor 재시작 후 main frame/MCP 응답 정지 때문에 실행하지 못했다.

## Native Preflight

- 실행 중인 Editor와 Live Coding을 종료했다.
- UE 5.8 `Build.bat`으로 `BathhouseSimEditor Win64 Development -WaitMutex -NoHotReloadFromIDE`를 빌드했다.
- 결과: `Succeeded`; target up to date.
- 새 Editor에서 다섯 Blueprint의 parent, inherited `TowelPresentationVisual` 이름과 concrete type을 확인했다.
  - Clean stack, used bin, basket: `TowelStackVisualComponent`
  - Washer, dryer: `TowelPileVisualComponent`
- 저장 전에 `LogBlueprint: Error`, internal compiler error, duplicate property, missing native property/default subobject, 관련 load error가 없음을 확인했다.

## 생성한 Mesh Profile

모든 profile은 `/Game/Bathhouse/Data/Towel/`에 생성하고 저장했다.

| Asset | State → Mesh |
|---|---|
| `DA_TowelVisual_Shelf` | Clean → `/Engine/BasicShapes/Cube.Cube` |
| `DA_TowelVisual_UsedBin` | Used → `/Engine/BasicShapes/Cube.Cube` |
| `DA_TowelVisual_Basket` | Used, Wet, Clean → `/Engine/BasicShapes/Cube.Cube` |
| `DA_TowelVisual_Washer` | Used, Wet → `/Engine/BasicShapes/Cube.Cube` |
| `DA_TowelVisual_Dryer` | Wet, Clean → `/Engine/BasicShapes/Cube.Cube` |

- 프로젝트 Content에 towel 전용 Static Mesh가 없어서 Engine Cube를 얇게 스케일한 placeholder로 사용했다.
- `None`, duplicate state, null mesh는 없다.
- 같은 profile 안에 같은 state 또는 같은 state의 중복 mesh 후보를 넣지 않았다.
- 모든 상태가 같은 placeholder mesh이므로 현재는 상태별 외형 차이는 없고 수량/배치 표현만 확인 가능하다.

## 수정한 Blueprint와 최종 Layout

공통 설정은 `CountStepInterval=0.06`, `bAnimateInventoryChanges=true`, `BaseLocalOffset=(0,0,0)`이다.

| Blueprint | Profile | Seed | Relative Transform | Layout |
|---|---|---:|---|---|
| `/Game/Bathhouse/Blueprints/Towel/BP_CleanTowelStack` | Shelf | 101 | L `(0,0,51)`, R `(0,0,0)`, S `(0.3,0.2,0.02)` | `ZSpacing=105` |
| `/Game/Bathhouse/Blueprints/Towel/BP_UsedTowelBin` | UsedBin | 202 | L `(0,0,51)`, R `(0,0,0)`, S `(0.22,0.22,0.022)` | `ZSpacing=100` |
| `/Game/Bathhouse/Blueprints/Towel/BP_TowelBasket` | Basket | 303 | L `(0,0,55)`, R `(0,0,0)`, S `(0.65,0.6,0.1)` | `ZSpacing=110` |
| `/Game/Bathhouse/Blueprints/Towel/BP_Washer` | Washer | 404 | L `(31,0,45)`, R `(0,0,0)`, S `(0.16,0.12,0.025)` | Pile 설정 아래 참조 |
| `/Game/Bathhouse/Blueprints/Towel/BP_Dryer` | Dryer | 505 | L `(31,0,45)`, R `(0,0,0)`, S `(0.16,0.12,0.025)` | Pile 설정 아래 참조 |

- Basket attachment는 native `WorldMesh`를 유지한다. 나머지는 native actor root에 붙어 있다.
- Washer/Dryer 공통 pile:
  - `PileHalfExtent=(60,150,480)`
  - `ItemsPerLayer=4`
  - `LayerSpacing=120`
  - `MaxZJitter=15`
  - `MinRandomRotation=(-4,-25,-6)`
  - `MaxRandomRotation=(4,25,6)`
- Machine kind, processing timer, transfer port, control, delegate와 Event Graph는 수정하지 않았다.
- `UTowelSlotVisualComponent`, drying-rack 기능, `BP_DryingSpot`은 수정하지 않았다.

## BP_CleanTowelStack.StackVisual 결정

- 결정: 보존
- 삭제 전 확인한 값:
  - Mesh: `/Engine/BasicShapes/Cube.Cube`
  - Material override: 없음
  - Relative location `(0,0,25)`, scale `(0.5,0.35,0.5)`
  - Collision: Query Only; Visibility block, Camera ignore
- 이 컴포넌트는 현재 shelf/base placeholder이자 clean stack의 interaction trace 표면이다.
- 삭제하면 native presentation instance가 interaction에 참여하지 않는 계약 때문에 E/F trace 표면도 함께 사라진다.
- native towel은 얇은 scale과 Z=51에서 base 위로 쌓이므로 base와 같은 위치의 중복 towel 표현으로 판단하지 않았다.
- `BinVisual`, `MachineVisual`도 보존했다.

## Compile, Save, Reload

- profile 5개 Data Validation: 5개 모두 `contains valid data`, profile error/warning 0. Commandlet 전역에는 무관한 MCP plugin EULA 안내 warning 1개만 있었다.
- target Blueprint 5개: warnings-as-errors 개별 compile 성공.
- 저장 직전 dirty asset은 의도한 profile 5개와 Blueprint 5개뿐이었다.
- 위 10개만 저장했고 저장 직후 `/Game` dirty asset은 0이었다.
- Editor 프로세스를 종료한 뒤 별도 UE commandlet 프로세스에서 디스크 asset을 다시 로드했다.
- 재로드 리드백 결과:
  - profile의 모든 state/mesh 유지
  - 각 Blueprint의 `TowelPresentationVisual` 정확히 1개
  - profile reference, seed, animation, relative transform과 layout 값 모두 유지
  - Python/Blueprint/compiler 오류 0

## 자동화 검증

- Focused: `BathhouseSim.Towel.Presentation.StackPileSlotAndLifecycle`
  - 결과: 1 success / 0 fail
- 전체: `BathhouseSim`
  - 결과: 17 success / 0 fail, `GIsCriticalError=0`
- 전용 테스트가 확인한 범위:
  - stack top-first 증감과 unrelated revision의 기존 mesh/transform 안정성
  - state conversion 시 count/transform 보존과 mesh만 교체
  - pile bounds, lower-layer-first, 동일 seed 재현
  - ISM collision 없음, overlap 없음, navigation 영향 없음, tick 없음
  - inventory commit 수렴, 빠른 target reversal, unregister/reregister 재동기화
  - actor BeginPlay 즉시 binding 및 EndPlay unbind
- 전체 테스트가 carry/drop sweep, towel atomic transfer/machine recovery, customer towel 흐름 회귀를 함께 통과했다.

## PIE Acceptance 판정

| 항목 | 판정 | 근거 |
|---|---|---|
| 초기 inventory 즉시 표시 | 자동화 통과 / 직접 PIE 미실행 | actor BeginPlay 즉시 binding과 clean stack initial snapshot 검증 |
| E/F 후 visible count 수렴 | 자동화 통과 / 직접 입력 미실행 | commit, revision, target reversal 수렴 검증 |
| unrelated revision 안정성 | 통과 | focused test |
| Basket/Washer/Dryer state swap | native 계약 통과 / authored BP 직접 PIE 미실행 | stack 및 machine state conversion 검증 |
| Pile bounds/lower-first/seed | 통과 | focused test |
| collision/overlap/nav 비참여 | 통과 | focused test |
| carry/drop/process/overflow 회귀 | 자동화 통과 / 직접 PIE 미실행 | 전체 17개 테스트 |
| clean stack 중복 표시 없음 | 미검증 | base 보존 결정은 완료했으나 런타임 시각 확인 필요 |
| PIE 종료/재시작 중복 반응 없음 | lifecycle 자동화 통과 / PIE 2회 미실행 | reregister 및 EndPlay 정리 검증 |

## 직접 PIE 미실행 사유와 재현

1. 저장 후 Editor를 종료하고 UE 5.8 Editor를 새로 실행한다.
2. 로그는 MCP port 8000 listener와 toolset 등록, `Engine is initialized`까지 기록된다.
3. 이후 main frame handle이 생성되지 않고 `/mcp` 요청과 MCP `list_toolsets`가 timeout된다.
4. `LogBlueprint: Error`, fatal, duplicate/missing native contract 오류는 없다.
5. 멈춘 Editor PID만 종료한 뒤 commandlet 재로드와 자동화 검증으로 대체했다.

## 최종 변경 범위

- 생성: profile Data Asset 5개
- 수정: target Blueprint 5개
- 수정: `.md/PROMPT_INTEGRATION_REVIEW.md`
- 임시 검증 스크립트는 삭제했다.
- Source, Config, StateTree, map, machine gameplay, transfer gameplay는 이 Editor 단계에서 수정하지 않았다.
- Content 기준 의도하지 않은 dirty/untracked asset은 없다.
