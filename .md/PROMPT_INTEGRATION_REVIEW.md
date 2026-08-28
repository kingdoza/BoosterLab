# Integration Review Prompt — Counter Queue Transform, Overflow Wander And Physical Key Return

## 상태

부분 완료 — Counter/DefaultMap Editor authoring, 저장 후 재로드 검증, Blueprint Compile, Data Validation과 native automation은 완료했다. `ST_CustomerRoutine`의 두 legacy queue movement chain을 native `Move To Current Queue Assignment`로 교체하는 작업은 현재 Unreal MCP/Python API가 StateTree node 생성과 property binding 쓰기를 제공하지 않아 수동 작업으로 남았다. 이 migration 전에는 요구된 PIE 수용 시나리오를 최종 승인할 수 없다.

## 작업 경계

- 이번 Editor 단계에서 Source와 Config는 수정하지 않았다.
- `Save All`을 사용하지 않았다.
- `.uasset` binary patch를 사용하지 않았다.
- 기존 dirty Source/문서 변경은 보존했다.
- 새로 저장한 Content는 Counter Blueprint와 `DefaultMap`이 소유하는 정확한 두 World Partition external actor package뿐이다.

## 정확한 대상

- Counter Blueprint: `/Game/Bathhouse/Blueprints/Facility/BP_BathhouseCounter`
- 기존 Counter actor:
  `/Game/Maps/DefaultMap.DefaultMap:PersistentLevel.BP_BathhouseCounter_C_UAID_F02F7433CA3615F402_1440216855`
- 새 native overflow actor:
  `/Game/Maps/DefaultMap.DefaultMap:PersistentLevel.CustomerQueueOverflowWanderVolume_UAID_F02F7433CA36E9FB02_1737598559`
- overflow actor label: `CustomerQueueOverflowWanderVolume_Checkout_0`

## Editor Authoring 결과

### Counter Blueprint와 배치 인스턴스

`ReturnedKeyDropPoint`와 search settings를 Blueprint CDO 및 기존 Counter instance에 동일하게 명시 저장했다.

| 항목 | 이전 | 이후 |
|---|---:|---:|
| `ReturnedKeyDropPoint` Location | `(0, 0, 0)` | `(20, 20, 90)` |
| `ReturnedKeyDropPoint` Rotation | `(Pitch=0, Yaw=0, Roll=0)` | `(Pitch=0, Yaw=180, Roll=0)` |
| `ReturnedKeyDropLocalXYExtent` | `(35, 20)` | `(35, 20)` 유지 |
| `ReturnedKeyDropAttemptCount` | `8` | `12` |

기존 queue reference 배열의 순서와 인스턴스별 authored transform은 보존했다.

- Check-in: `CheckInQueuePoint0`, `CheckInQueuePoint1`, `CheckInQueuePoint2`
- Checkout: `CheckoutQueuePoint0`, `CheckoutQueuePoint1`, `CheckoutQueuePoint2`
- 각 원소는 exact Counter 소유의 서로 다른 component다.
- checkout instance의 기존 X 간격 `320 / 520 / 720`은 보존했다.
- deprecated `ReturnedKeyPointReferences`는 기존 `ReturnedKeyPoint0/1/2` migration data 그대로이며 새 동작에 연결하지 않았다.

### Checkout overflow volume

- native class: `/Script/BathhouseSim.CustomerQueueOverflowWanderVolume`
- Actor Location/Rotation/Scale: `(500, 400, 100) / (0, 0, 0) / (1, 1, 1)`
- `WanderBounds` extent: `(180, 120, 100)`
- 전체 world bounds: X `320..680`, Y `280..520`, Z `0..200`
- `NavigationProjectionExtent`: `(75, 75, 150)`
- `SampleAttemptCount`: `12`
- Counter `CheckoutOverflowVolumes`: 위 actor 하나를 index 0에 저장

기존 NavMesh bounds `Min=(-750,-1000,-350), Max=(2250,1000,650)` 안에서 실제 map geometry와 visible queue standing positions를 대조해 영역을 선택했다. NavMesh 설정은 수정하지 않았다.

### Routine Data Asset

`/Game/Bathhouse/Data/DA_CustomerRoutine_Default`의 queue 도착 허용 반경을 이번 follow-up 계약에 맞춰 개별 저장했다.

- `QueueAcceptanceRadius=10 cm` (이전 `35`)
- `QueueFacingRotationSpeedDegrees=360`
- `QueueFacingToleranceDegrees=2`
- `OverflowWanderAcceptanceRadius=50`
- `OverflowPauseMinSeconds=1`
- `OverflowPauseMaxSeconds=3`

저장 직후 MCP 재조회에서 `QueueAcceptanceRadius=10`, package dirty=`false`를 확인했다. 새 native `ForceUnits="cm"` 메타데이터의 실제 Details 표시는 새 DLL 로드 후 재확인해야 한다.

## StateTree 차단 항목

`/Game/Bathhouse/AI/ST_CustomerRoutine`은 재로드 시 자동 compile에 성공하고 Data Validation도 Valid지만, queue movement region은 아직 legacy 구성이다.

- `/Root/CheckIn/QueueMove`: `Get Customer Queue Target (Deprecated)` + `Restartable Customer Move To`
- `/Root/Checkout/QueueMove`: `Get Customer Queue Target (Deprecated)` + `Restartable Customer Move To`
- 각 parent의 `Hold Customer Queue`는 존재한다.

MCP에는 StateTree writer가 없고 UE 5.8 Python reflection도 editor node의 안전한 생성 및 `Customer`/`Session` binding 쓰기를 노출하지 않는다. 따라서 serialized asset을 추측 수정하지 않았다. 필요한 정확한 수동 절차는 `.md/USER_UNREAL.md`에 기록했다.

## Compile, Validation, Reload

- UE 5.8 exact `BathhouseSimEditor Win64 Development` build: 성공, target up to date
- follow-up UE 5.8 exact Editor build는 사용자 visible Editor 종료 후 UHT, 수정 translation unit와 `UnrealEditor-BathhouseSim.dll` link까지 성공했다.
- follow-up focused `BathhouseSim.Customer.QueueNavigationFacingAndRecoveryGate`: 성공.
- follow-up full headless `BathhouseSim`: 31 started, 31 succeeded, 0 failed, process exit 0. `Saved/Logs/QueueBeginPlayFixFull.log`에서 automation error, fatal, assertion, ensure, access violation, Blueprint/Script error와 critical-error signature가 없음을 확인했다.
- Blueprint compile, warning-as-error 기준: `BP_BathhouseCounter`, `BP_BathhouseCustomer`, `BP_BathhouseKey` 모두 성공, warning/error 0
- StateTree reload compile: 성공. 단, 위 legacy graph 상태의 compile 성공이며 migration 완료 증거는 아니다.
- allowlist 6개 asset Data Validation: 모두 `Valid`
- 저장 후 새 Editor process에서 exact package reload verifier: 성공
- `BP_BathhouseCustomer` CDO: inherited `CustomerQueueNavigation` 정확히 1개
- Counter drop transform/search settings, queue 배열과 overflow reference: 모두 저장값 유지
- overflow transform/bounds/navigation settings: 모두 저장값 유지

## Automation

post-authoring UE 5.8 full `BathhouseSim` run:

- found: `31`
- succeeded: `31` (`30` clean + `1` succeeded-with-warnings)
- failed: `0`
- not run: `0`
- process exit: `0`
- `GIsCriticalError`: `false` — automation session에 critical error 없음

요구된 다섯 test path는 같은 full run에서 모두 발견되어 `5/5` 성공했고 warning/error는 0이었다.

- `BathhouseSim.Customer.QueueCleanup`
- `BathhouseSim.Facility.CounterPointReferences`
- `BathhouseSim.Facility.CounterAssignmentAndOverflow`
- `BathhouseSim.Customer.QueueNavigationFacingAndRecoveryGate`
- `BathhouseSim.Interaction.CheckoutPhysicalKeyDrop`

warning-only test는 무관한 `BathhouseSim.Towel.Presentation.StackPileSlotAndLifecycle`의 의도된 negative-path warning 8건이다. duplicate/unresolved towel slot, presentation count clamp와 divergent payload 방어 검증이며 error는 0이다.

## PIE

최종 PIE 수용 시나리오는 실행하지 않았다. 아직 canonical StateTree가 native queue assignment task를 사용하지 않으므로 overflow promotion, latest-assignment recovery, service Location/Yaw 완료 조건을 legacy graph로 시험해도 이 작업의 수용 증거가 되지 않는다. `.md/USER_UNREAL.md`의 migration 후 Compile/Save/reload를 먼저 완료해야 한다.

## 새 Content 변경

- 수정: `Content/Bathhouse/Data/DA_CustomerRoutine_Default.uasset`
  - `QueueAcceptanceRadius: 35 -> 10 cm`
- 수정: `Content/Bathhouse/Blueprints/Facility/BP_BathhouseCounter.uasset`
- 수정: `Content/__ExternalActors__/Maps/DefaultMap/7/VL/IGH0D1UBX5109BGZ5YJ8ZR.uasset`
  - exact 기존 Counter instance package
- 신규: `Content/__ExternalActors__/Maps/DefaultMap/1/2G/5ZTKNRQ5E1OJP8AQ17DFEU.uasset`
  - exact native overflow actor package

World Partition이 actor를 external package에 저장하므로 `DefaultMap.umap` 자체는 변경되지 않았다. 위 두 external packages는 `/Game/Maps/DefaultMap`이 소유하는 actor data다. allowlist 밖에서 새로 dirty/save된 Content package는 없다.

## 환경 경고

- local Zen/DDC가 쓰기 불가여서 Editor commandlet은 `-DDC-ForceMemoryCache` fallback으로 실행했다.
- RiderLink `FileSystemMappings.ini` move/delete warning과 외부 telemetry/EOS network warning이 관찰됐다.
- 위 경고는 asset compile, save/reload, Data Validation 또는 automation failure를 만들지 않았다.

## 통합 결론

Unreal 작업 부분 완료. Counter/drop/overflow Editor 데이터와 native regression은 통과했지만 StateTree migration 및 그 이후 PIE acceptance가 남아 있어 최종 통합 승인은 보류한다.
