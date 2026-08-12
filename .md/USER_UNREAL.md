# USER Unreal Work — Cleaning And Towel Circulation

## 현재 상태

- 기준 작업서: `.md/PROMPT_UNREAL.md`
- UE 버전: 5.8.1
- 상태: 부분 완료. 아래 세 가지 Editor UI 작업이 끝나기 전에는 통합 완료가 아니다.
  1. 현재 메모리에 배치된 World Partition 액터 저장
  2. `WBP_InteractionPrompt`의 필수 `BindWidget` 세 개 추가
  3. `ST_CustomerRoutine`에 towel 획득·사용·반납 흐름 추가
- Blueprint에서 towel count, token, machine state, stain progress, satisfaction을 변경하지 않는다. 이 값들은 모두 네이티브 시스템이 소유한다.
- CheckIn, Bath, Montage, Checkout, `ExitTechnicalAbort`와 기존 이동 재시도 흐름은 유지한다.

## 0. 가장 먼저 현재 레벨 저장

MCP로 신규 World Partition 액터를 배치했지만, MCP의 `save_actor`가 아직 생성되지 않은 external actor package를 먼저 조회하는 문제로 저장에 실패했다. **에디터를 닫거나 레벨을 다시 열기 전에** 다음 순서로 저장한다.

1. PIE 시작 실패 메시지나 Message Log가 열려 있으면 닫는다.
2. PIE가 실제 실행 중이면 Stop한다. 실행되지 않았으면 그대로 진행한다.
3. 현재 레벨이 `/Game/Maps/DefaultMap`인지 확인한다.
4. `Ctrl+S`를 한 번 눌러 현재 레벨과 신규 external actors를 저장한다.
5. Save 대화상자가 뜨면 `DefaultMap`과 그 external actors만 저장한다. unrelated dirty asset을 일괄 저장하지 않는다.

저장 전 World Outliner에서 아래 구성을 확인한다. 액터 라벨에는 `Bathhouse_` 접두사를 붙이지 않는다.

| 액터 라벨 | Blueprint | World 위치 | 추가 값 |
|---|---|---:|---|
| `CleaningDirector` | `BP_CleaningDirector` | `(0,700,0)` | Director 1개 |
| `DressingCleaningZone` | `BP_StainSpawnZone` | `(1000,-700,0)` | `DressingFloor`, Bounds `(400,200,100)` |
| `BathCleaningZone` | `BP_StainSpawnZone` | `(2050,-300,0)` | `BathFloor`, Bounds `(150,250,100)` |
| `WetMop` | `BP_WetMop` | `(750,750,43)` | Scale `(0.08,0.08,0.8)` |
| `TowelBasketCart` | `BP_TowelBasket` | `(1150,750,10)` | Scale `(0.4,0.3,0.2)` |
| `CleanTowelStack` | `BP_CleanTowelStack` | `(1250,120,0)` | `FacilityType=TowelShelf` |
| `UsedTowelBin` | `BP_UsedTowelBin` | `(1150,420,0)` | `FacilityType=TowelBasket` |
| `Washer` | `BP_Washer` | `(1500,750,0)` | `MachineKind=Washer` |
| `Dryer` | `BP_Dryer` | `(1850,750,0)` | `MachineKind=Dryer` |

기존 `/Game/Bathhouse/Blueprints/Facility/BP_TowelBasket` 인스턴스였던 라벨 `TowelBasket`은 새 `UsedTowelBin`으로 교체되어 Outliner에 없어야 한다. 기존 Blueprint 에셋 자체는 삭제하지 않는다.

## 1. WBP_InteractionPrompt 필수 위젯 추가

대상 에셋은 `/Game/Bathhouse/UI/WBP_InteractionPrompt`이고 native parent는 `UInteractionPromptWidget`이다. 현재 PIE가 중단된 직접 원인은 이 WBP에 아래 세 `BindWidget`이 없기 때문이다.

기존 이름은 변경하지 않는다.

- `PromptRoot`
- `TargetNameText`
- `ActionNameText`
- `FailureReasonText`

Designer에서 `PromptRoot` 아래에 다음 child를 추가한다. 세 위젯 모두 **Is Variable**을 켜고 이름과 타입을 정확히 맞춘다.

| 이름 | 타입 | 역할 |
|---|---|---|
| `SecondaryActionNameText` | `TextBlock` | F 보조 행동 이름 |
| `SecondaryFailureReasonText` | `TextBlock` | F 보조 행동 실패 이유 |
| `InteractionProgressBar` | `ProgressBar` | E hold 진행률 |

권장 배치는 crosshair 부근에서 위에서 아래 순서로 다음과 같다.

```text
PromptRoot
  TargetNameText
  E row: ActionNameText / FailureReasonText
  F row: SecondaryActionNameText / SecondaryFailureReasonText
  InteractionProgressBar
```

주의사항:

- Visibility, Enabled, Text, Percent는 native `UInteractionPromptWidget`이 갱신한다.
- Designer Binding과 Event Graph에서 query, failure timer, hold percent를 계산하지 않는다.
- `OnInteractionPromptChanged`와 `OnInteractionPromptDetailsChanged`를 구현한다면 색·페이드 같은 표현만 수행한다.
- legacy `OnInteractionPromptChanged.bCanInteract`는 E primary 가능 여부다. F만 가능한 상태에서 root나 F row를 이 값으로 다시 disable하지 않는다.

완료 후 WBP를 Compile한다. `Required widget binding ... was not found` 오류가 0개인지 확인하고 저장한다.

## 2. ST_CustomerRoutine towel 흐름 추가

대상은 `/Game/Bathhouse/AI/ST_CustomerRoutine`이다. 아래 towel 단계만 추가하고 현재 CheckIn, StoreShoes, Undress, PreShower, Bath, MainShower, Drying montage, Dress, Checkout 및 failure cleanup을 보존한다.

### 2.1 공통 사전 확인

1. Schema가 `StateTreeAIComponentSchema`인지 확인한다.
2. Context Actor가 고객 Character 타입인지 확인한다.
3. 다음 native Task와 Condition이 검색되는지 확인한다.
   - `Acquire Or Wait For Clean Towel`
   - `Mark Customer Towel Used`
   - `Return Customer Towel`
   - `Customer Session Condition`
4. 모든 새 Task/Condition의 `Session`은 `Context Actor > CustomerSession`에 bind한다.
5. `Global/Parameter.Session` 또는 `Root/Parameter.Session`은 사용하지 않는다.

`Context Actor`가 Binding 목록에 없다면 임시 Parameter를 연결하지 말고 Schema와 Context Actor 타입을 다시 지정한 뒤 Compile하고 에셋을 닫았다가 연다.

### 2.2 Undress 뒤 TowelShelf 획득 단계

기존 Shower/Bath 시설 예약 구성을 복제해 `HoldTowelShelf` 부모 상태를 만든다. Task 설정은 다음과 같다.

| Task | 값 |
|---|---|
| `Hold Customer Facility` | `Session=Context Actor.CustomerSession`, `FacilityType=TowelShelf`, `bExcludeLastBath=false` |

이 부모 안의 자식 흐름은 기존 facility 이동 패턴과 같은 순서로 둔다.

```text
HoldTowelShelf
  Task: Hold Customer Facility (TowelShelf)
  ├─ GetTowelShelfTarget
  │    Get Customer Facility Target
  │    bUseApproachPoint=true
  ├─ MoveTowelShelf
  │    기존 Move To 설정 복제
  │    Destination = GetTowelShelfTarget.Destination
  ├─ RecordTowelShelfNavigation
  │    기존 Record Customer Navigation Result 설정 복제
  └─ AcquireTowel
       Acquire Or Wait For Clean Towel
       Session = Context Actor.CustomerSession
```

전이는 기존 facility 패턴을 그대로 따른다.

- Target 조회 성공 → Move
- Move 성공 → 성공 결과 기록 → `AcquireTowel`
- Move 실패 → 실패 결과 기록 → 기존 재시도 또는 `ExitTechnicalAbort`
- `AcquireTowel` 성공 → `HoldTowelShelf` 바깥의 기존 다음 단계로 이동
- `AcquireTowel` 실패 → `ExitTechnicalAbort`

`AcquireTowel`은 clean towel이 있으면 한 장 획득하고 즉시 성공한다. 없으면 Data Asset의 시간 동안 자체적으로 기다리며, timeout 뒤 `bProceedingWithoutTowel=true`로 성공한다. 따라서 다음 로직을 추가하지 않는다.

- StateTree에서 stack count 감소
- 별도 Delay
- timeout 만족도 감소
- Event를 받을 때 Task cancel/restart
- 같은 state reselect를 위한 별도 재진입 action

`bProceedingWithoutTowel` 출력은 디버그/명시적 분기에만 선택적으로 쓸 수 있다. towel 유무는 뒤의 `HasTowel` Condition으로 다시 판정할 수 있으므로 필수 Binding이 아니다.

### 2.3 Drying 종료 뒤 used 표시

기존 Drying 활동과 one-shot montage cleanup이 모두 끝나는 지점, 즉 고객이 Drying 시설을 정상 종료하기 직전에 상태 `MarkTowelUsed`를 추가한다.

| 속성 | 값 |
|---|---|
| Task | `Mark Customer Towel Used` |
| `Session` | `Context Actor.CustomerSession` |

전이:

- 성공 → 아래 `HasTowel` 분기
- 실패 → `ExitTechnicalAbort`

towel 없이 진행한 고객은 handle이 없으므로 이 Task도 성공한다. `bHadTowel` 출력으로 count를 수정하거나 별도 towel actor를 생성하지 않는다.

### 2.4 HasTowel 분기

`MarkTowelUsed` 성공 뒤 두 전이를 둔다. 조건은 둘 다 `Customer Session Condition`이며 `Session` Binding은 동일하다.

| 우선순위 | Condition | 목적지 |
|---:|---|---|
| 높음 | `Condition=HasTowel`, `bInvert=false` | `HoldUsedTowelBin` |
| 낮음 | `Condition=HasTowel`, `bInvert=true` | 기존 towel 반납 다음 단계 |

두 번째 목적지는 현재 트리에서 반납이 끝난 뒤 진행하던 Dress/Checkout 쪽 기존 상태다. towel이 없는 고객은 bin 예약과 이동 자체를 건너뛴다.

### 2.5 TowelBasket 반납 단계

기존 facility 예약 구성을 다시 복제해 `HoldUsedTowelBin` 부모 상태를 만든다.

| Task | 값 |
|---|---|
| `Hold Customer Facility` | `Session=Context Actor.CustomerSession`, `FacilityType=TowelBasket`, `bExcludeLastBath=false` |

자식 흐름:

```text
HoldUsedTowelBin
  Task: Hold Customer Facility (TowelBasket)
  ├─ GetUsedTowelBinTarget
  │    Get Customer Facility Target
  │    bUseApproachPoint=true
  ├─ MoveUsedTowelBin
  │    Destination = GetUsedTowelBinTarget.Destination
  ├─ RecordUsedTowelBinNavigation
  │    기존 navigation result 처리 복제
  └─ ReturnTowel
       Return Customer Towel
       Session = Context Actor.CustomerSession
```

전이:

- 정상 이동 및 `ReturnTowel` 성공 → `HoldUsedTowelBin` 바깥의 기존 다음 단계
- Target/Move/Return 실패 → 기존 retry 규칙 또는 `ExitTechnicalAbort`

Used bin이 가득 찬 경우도 `Return Customer Towel`은 성공한다. floor towel 생성 또는 `PendingSpill` 보존은 네이티브 시스템이 처리한다. 다음 Blueprint/StateTree 보정은 만들지 않는다.

- bin count 직접 증가
- customer handle 직접 삭제
- floor towel 직접 spawn
- interruption/abort 시 token 보정 action

부모 상태를 빠져나가면 `Hold Customer Facility`가 slot을 release한다. `ReturnTowel` 성공 전에 부모 밖으로 직접 나가는 정상 전이는 만들지 않는다.

### 2.6 최종 형태

현재 트리의 이름이 조금 달라도 기능적 순서는 다음과 같아야 한다.

```text
... Undress 완료
→ HoldTowelShelf
   → Approach 이동
   → Acquire Or Wait For Clean Towel
→ 기존 PreShower / Bath / MainShower
→ 기존 Drying activity + montage cleanup
→ Mark Customer Towel Used
→ [HasTowel]
   ├─ true  → HoldUsedTowelBin → Approach 이동 → Return Customer Towel
   └─ false → bin 단계 건너뜀
→ 기존 Dress / Checkout
```

## 3. Compile, Save, Restart

다음 순서로 진행한다.

1. `WBP_InteractionPrompt` Compile/Save
2. `ST_CustomerRoutine` Compile/Save
3. `DefaultMap`에서 `Ctrl+S`
4. Error 0, Warning 0 확인
5. 에디터 정상 종료
6. UE 5.8 Editor를 새로 열기
7. WBP와 StateTree를 다시 열어 이름, Binding, Transition이 유지되는지 확인
8. 두 에셋을 다시 Compile

StateTree Compile 체크리스트:

- 세 towel Task가 기본 base task가 아니라 정확한 이름으로 표시된다.
- 모든 `Session`이 `Context Actor.CustomerSession`이다.
- `TowelShelf`와 `TowelBasket` 시설 타입이 뒤바뀌지 않았다.
- towel 없는 분기는 Used bin 예약 전에 갈라진다.
- `Return Customer Towel` 성공 전 정상 경로가 facility 부모 밖으로 나가지 않는다.
- 기존 이동 실패/재시도/`ExitTechnicalAbort`가 유지된다.
- count, token, timeout penalty를 변경하는 Blueprint Task가 없다.

완료 후 에이전트에게 통합 검증 재개를 요청한다. 재개 시 PIE에서 E/F/G, stain hold/cancel, washer/dryer direction gate, customer towel 정상/품절/가득 찬 bin/interruption 시나리오를 검증한다.
