# Cleaning And Towel Circulation — Unreal Integration Review

## 작업 상태

- 상태: 부분 완료 — 사용자 Editor 작업 대기
- 기준 작업서: `.md/PROMPT_UNREAL.md`
- 후속 사용자 작업: `.md/USER_UNREAL.md`
- UE 버전: 5.8.1
- 완료로 보고하지 않는 이유:
  - `WBP_InteractionPrompt`에 필수 `BindWidget` 세 개를 추가해야 한다.
  - `ST_CustomerRoutine` towel flow는 등록된 Unreal MCP 도구로 편집할 수 없다.
  - 신규 World Partition 액터는 메모리에 배치됐지만 MCP `save_actor` 결함 때문에 사용자 `Ctrl+S`가 필요하다.
  - WBP 컴파일 실패로 PIE 진입이 중단돼 인수 시나리오를 아직 실행하지 못했다.

## Native Preflight와 생성자 크래시 수정

- 실행 중이던 Editor를 정상 종료한 뒤 UE 5.8 공식 빌드 경로로 `BathhouseSimEditor Win64 Development -WaitMutex -NoHotReloadFromIDE`를 전체 링크 빌드했다.
- 첫 새 Editor 시작에서 `UTowelTransferPortComponent` 생성 중 `NewObject with empty name can't be used to create default subobjects` fatal을 확인했다.
- 원인은 UObject 생성자에서 `SetBoxExtent`/`SetSphereRadius`를 호출해 BodySetup 갱신과 `NewObject`가 발생한 것이었다.
- 생성자 전용 초기화 API로 다음 네 파일을 수정했다.
  - `TowelTransferPortComponent.cpp`: `SetBoxExtent` → `InitBoxExtent`
  - `TowelMachineControlComponent.cpp`: `SetBoxExtent` → `InitBoxExtent`
  - `StainSpawnZoneActor.cpp`: `SetBoxExtent` → `InitBoxExtent`
  - `WaterStainActor.cpp`: `SetSphereRadius` → `InitSphereRadius`
- CrashReportClient가 DLL/PDB를 잠근 첫 재빌드 시도를 정상 종료 후 다시 빌드했다.
- 최종 전체 링크 빌드는 `Succeeded`, 5.05초였고 새 Editor에서 Unreal MCP가 재연결됐다. Hot Reload는 사용하지 않았다.

## E/F/G Input

- 생성:
  - `/Game/Input/Actions/IA_SecondaryInteract`
  - `/Game/Input/Actions/IA_DropCarry`
- 두 InputAction 모두 Boolean/Digital이며 asset Trigger/Modifier는 비어 있다.
- `/Game/Input/IMC_FirstPerson` 최종 mapping:
  - E → `IA_Interact`
  - F → `IA_SecondaryInteract`
  - G → `IA_DropCarry`
- `/Game/FirstPersonCharacter/BP_FirstPersonCharacter` 기본값:
  - `InteractAction=IA_Interact`
  - `SecondaryInteractAction=IA_SecondaryInteract`
  - `DropCarryAction=IA_DropCarry`
- `/Game/FirstPerson/Blueprints/BP_FirstPersonController`의 `DefaultMappingContext`는 기존 `IMC_FirstPerson`을 유지한다.
- Player Blueprint는 warnings-as-errors 컴파일에 성공했고 관련 Input assets와 함께 저장됐다.

## Interaction Prompt Widget

- HUD: `/Game/Bathhouse/Blueprints/Game/BP_BathhouseHUD`
- 실제 Prompt WBP: `/Game/Bathhouse/UI/WBP_InteractionPrompt`
- native parent: `/Script/BathhouseSim.InteractionPromptWidget`
- 기존 필수 이름 `PromptRoot`, `TargetNameText`, `ActionNameText`, `FailureReasonText`는 존재한다.
- 현재 누락:
  - `SecondaryActionNameText` (`TextBlock`)
  - `SecondaryFailureReasonText` (`TextBlock`)
  - `InteractionProgressBar` (`ProgressBar`)
- 등록된 MCP에 Widget hierarchy 편집 도구가 없어 `.md/USER_UNREAL.md`에 정확한 UI 절차를 기록했다.
- PIE 시작 시 `Blueprint failed to compile: WBP_InteractionPrompt`가 발생했고 PostPIEStarted가 오지 않아 MCP StartPIE 대기가 반환되지 않았다.

## Cleaning Blueprints

생성 경로는 `/Game/Bathhouse/Blueprints/Cleaning/`이다.

- `BP_CleaningDirector` → `ACleaningDirectorActor`
  - Spawn Interval 15초, total limit 8, attempts 12
  - `StainClass=BP_WaterStain`, spacing 100, Pawn clearance 80
- `BP_StainSpawnZone` → `AStainSpawnZoneActor`
  - 기본 BathFloor, weight 1, per-zone limit 4
  - Visibility floor trace, distance 300, max slope 10도
  - required component tag 없음, zone 밖 fallback 없음
- `BP_WaterStain` → `AWaterStainActor`
  - `StainVisual` Plane, NoCollision, presentation-only
  - native `InteractionCollision` 유지, removal duration 2초
- `BP_WetMop` → `AWetMopActor`
  - `WorldMesh` Cube placeholder와 `MopHeadVisual` presentation component
  - throw impulse 450, throw distance 70
  - 배치 actor scale과 head absolute scale을 이용해 지면에 맞춘 mop 형태 유지

네 Blueprint는 warnings-as-errors 컴파일과 저장에 성공했다. Blueprint에 cleaning count/progress mutation은 추가하지 않았다.

## Towel Blueprints

생성 경로는 `/Game/Bathhouse/Blueprints/Towel/`이다.

- `BP_TowelBasket` → `ATowelBasketActor`
  - Cube placeholder, native physics/interaction collision 유지
  - throw impulse 400, throw distance 70
- `BP_CleanTowelStack` → `ACleanTowelStackActor`
  - `StackVisual`, `FacilitySlot`
  - `FacilityType=TowelShelf`, Clean 20/30
  - slot location `(-75,0,0)`, approach offset `(-100,0,0)`
- `BP_UsedTowelBin` → `AUsedTowelBinActor`
  - `BinVisual`, `FacilitySlot`, `FacilityType=TowelBasket`
  - overflow radius 80..180, floor trace 250, attempts 8, spacing 50, Pawn clearance 60
  - `WorldUsedTowelClass=BP_WorldUsedTowel`
- `BP_WorldUsedTowel` → `AWorldUsedTowelActor`
  - Engine Plane placeholder로 한 장의 얇은 world presentation 유지
- `BP_Washer`, `BP_Dryer` → `ATowelProcessingMachineActor`
  - MachineKind 각각 Washer/Dryer, processing duration 10초
  - inherited TransferPort `(35,0,60)`, extent 30
  - inherited MachineControl `(35,0,125)`, extent 20
  - 두 visibility trace volume은 공간적으로 분리됨

여섯 Blueprint는 warnings-as-errors 컴파일과 저장에 성공했다. Blueprint에 count/state/token mutation은 추가하지 않았다.

## Customer Data Asset

- 대상: `/Game/Bathhouse/Data/DA_CustomerRoutine_Default`
- `TowelAvailabilityWaitSeconds=10`
- `TowelUnavailableSatisfactionPenalty=10`
- 값 설정과 저장을 확인했다.

## DefaultMap 배치

메모리상 현재 배치는 다음과 같다.

| 라벨 | 위치 | 비고 |
|---|---:|---|
| CleaningDirector | `(0,700,0)` | 1개 |
| DressingCleaningZone | `(1000,-700,0)` | DressingFloor, extent `(400,200,100)` |
| BathCleaningZone | `(2050,-300,0)` | BathFloor, extent `(150,250,100)` |
| WetMop | `(750,750,43)` | scale `(0.08,0.08,0.8)` |
| TowelBasketCart | `(1150,750,10)` | scale `(0.4,0.3,0.2)` |
| CleanTowelStack | `(1250,120,0)` | customer approach가 NavMesh 내부 |
| UsedTowelBin | `(1150,420,0)` | 기존 generic TowelBasket 인스턴스 대체 |
| Washer | `(1500,750,0)` | customer facility 아님 |
| Dryer | `(1850,750,0)` | customer facility 아님 |

- 모든 후보 위치에서 World Visibility trace가 바닥 Z=0에 적중했다.
- zone XY는 시설물 아래를 피해 빈 바닥으로 제한했다.
- 기존 `/Game/Bathhouse/Blueprints/Facility/BP_TowelBasket`의 배치 인스턴스만 제거했고 에셋은 보존했다.
- 액터 라벨에 `Bathhouse_` 접두사를 사용하지 않았다.
- Map package 저장만으로 신규 World Partition external actor package가 생성되지 않았고, `SceneTools.save_actor`는 `Asset does not exist: /Game/__ExternalActors__/...`로 실패했다.
- 따라서 에디터 종료/재로드 전에 사용자가 현재 레벨에서 `Ctrl+S`를 해야 한다.

## StateTree

- native Task 로드 대상:
  - `Acquire Or Wait For Clean Towel`
  - `Mark Customer Towel Used`
  - `Return Customer Towel`
- native Condition에 `HasTowel`, `TowelWaitExpired`가 존재한다.
- 등록된 Unreal MCP에는 StateTree graph 편집 도구가 없어 asset을 변경하지 않았다.
- `.md/USER_UNREAL.md`에 기존 facility 예약/이동 패턴을 복제하는 정확한 구조와 Binding을 기록했다.

## Compile, Save, Restart와 PIE

- 새 Input/Blueprint/Data assets는 개별 compile/save했다.
- 최종 수정한 Cleaning/Towel Blueprint도 다시 warnings-as-errors compile 후 저장했다.
- `WBP_InteractionPrompt`의 필수 BindWidget 누락 때문에 PIE 시작 전 Blueprint compile 단계에서 중단됐다.
- 현재 신규 map actors가 아직 외부 패키지로 저장되지 않았으므로 Editor 재시작은 수행하지 않았다.
- 따라서 E/F/G, stain hold, machine transfer, customer towel 정상/품절/overflow/interruption PIE 시나리오는 아직 통과로 판정하지 않는다.

## 사용자 작업 후 재개할 검증

1. WBP compile 및 E/F row/hold bar 동작
2. DefaultMap external actors 재로드 유지
3. ST_CustomerRoutine compile, towel facility reservation/release
4. mop 없이 stain 실패, mop hold progress/cancel/complete
5. G drop과 key 거부, 자동 swap 없음
6. stain spawn floor/spacing/limit
7. basket/stack/bin/machine E one/F max와 direction/full/processing gate
8. washer Used→Wet, dryer Wet→Clean, output drain 뒤 Waiting/None 복귀
9. customer towel 정상 획득·사용·반납
10. 품절 timeout과 session당 단일 satisfaction penalty
11. full bin overflow/PendingSpill
12. return 전 interruption과 technical abort token terminalization
