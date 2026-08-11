# USER Unreal Work — Bath Montage Slot And StateTree Wiring

## 현재 상태

- 기준 작업서: `.md/PROMPT_UNREAL.md`
- 상태: 부분 완료. 아래 네 에디터 UI 작업이 끝나기 전에는 통합 완료가 아니다.
- UE 버전: 5.8.1
- Source, Config, Input, UI, Interaction, Economy 에셋은 수정하지 않는다.
- 기존 Queue, Key, Cash, Facility, Navigation, Bath 반복 구조와 `Timed Customer Activity`는 아래에서 지정한 부분 외에는 변경하지 않는다.
- 기존 Bath 흐름의 조기 완료 방지를 위해 추가해 둔 상위 `Delay`의 `Run Forever` 설정은 유지한다.

## 에이전트가 완료한 작업

### 네이티브 및 Customer 검증

- 에디터를 닫고 `BathhouseSimEditor Win64 Development -WaitMutex -NoHotReloadFromIDE` 빌드에 성공했다.
- 새 에디터에서 다음 네이티브 타입이 모두 로드됐다.
  - `UCustomerMontagePlaybackComponent`
  - `FCustomerFacilitySnapTask`
  - `FCustomerBeginActivityTask`
  - `FCustomerFinishActivityTask`
  - `FPlayCustomerMontageOnceTask`
  - `FPlaySelectedMontageLoopForDurationTask`
- `BP_BathhouseCustomer`에는 상속 컴포넌트 `CustomerSession`, `CustomerMontagePlayback`이 있다.
- `CustomerMontagePlayback`은 tick이 없고 Blueprint 로직을 추가하지 않았다.
- 실제 고객 AnimBP는 `/Game/Characters/Mannequins/Anims/Unarmed/ABP_Unarmed`다.
- 고객 Skeletal Mesh는 `/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple`, Skeleton은 `/Game/Characters/Mannequins/Meshes/SK_Mannequin`이다.

### Bath Action/Approach 재설정 필요

- 네이티브 코드는 이제 `FacilitySlot`의 Action/Approach transform을 캡슐 중심이 아니라 **캐릭터 발바닥 위치**로 해석한다.
- 이 코드 변경에서는 기존 dirty `BP_Bath`/`DefaultMap` 에셋을 수정하거나 저장하지 않았다. 새 DLL로 에디터를 완전히 재시작한 뒤 아래 값을 Blueprint 기본값과 `Bathhouse_Bath` 배치 인스턴스에 다시 적용해야 한다.
- `FacilityVisual` 상대 스케일은 `(1.5, 1.8, 0.45)`다.
- 고객 Capsule은 Radius 25, Half Height 88이다.
- Bath blockout 상단은 World Z 72.5이며 Action 발바닥을 그보다 2cm 위인 World Z 74.5에 둔다.
- 적용할 슬롯 값은 다음과 같다. `162.5`는 런타임 캡슐 중심 높이이므로 슬롯 위치에 입력하지 않는다.

| Slot | Action 발바닥 상대 위치 | ApproachOffset | FacingRotation |
|---|---:|---:|---:|
| A | `(0, -60, 74.5)` | `(150, 0, -72.5)` | `(0, 180, 0)` |
| B | `(0, 0, 74.5)` | `(150, 0, -72.5)` | `(0, 180, 0)` |
| C | `(0, 60, 74.5)` | `(150, 0, -72.5)` | `(0, 180, 0)` |

- 배치 액터가 `(1750, 0, 0)`이므로 발바닥 기준 Action World 위치는 `(1750, -60/0/60, 74.5)`, Approach World 위치는 `(1900, -60/0/60, 2)`다.
- 네이티브 코드가 scaled capsule half height 88을 한 번 더해 실제 ActorLocation을 Action Z 162.5, Approach Z 90으로 만든다.
- Action은 Bath blockout footprint 위이며 NavMesh 목표가 아니다. Approach는 Bath 바깥 지면의 발바닥/NavMesh 위치다.
- 세 Action 사이 거리는 60cm로, 지름 50cm인 고객 Capsule끼리 겹치지 않는다.

### 생성한 Montage

- `/Game/Bathhouse/Animations/Customer/AM_Customer_Action_Once`
  - Skeleton: `SK_Mannequin`
  - AnimSequence: `/Game/Characters/Mannequins/Anims/Unarmed/Jump/MM_Land`
  - 길이: 0.666667초
  - Root Motion 없음, AnimNotify 없음, Asset Loop 꺼짐, Auto Blend Out 켜짐, Slot 필드 `CustomerAction`
- `/Game/Bathhouse/Animations/Customer/AM_Customer_Bath_Loop`
  - Skeleton: `SK_Mannequin`
  - AnimSequence: `/Game/Characters/Mannequins/Anims/Unarmed/MM_Idle`
  - 기술 검증 구간 길이: 0.666667초
  - Root Motion 없음, AnimNotify 없음, Asset Loop 꺼짐, Slot 필드 `CustomerAction`
- 두 에셋은 호환 가능한 기존 `MM_Pistol_Fire_Montage`를 복제한 뒤 위 Sequence와 Slot으로 교체했다.
- 아직 Skeleton에 Slot이 등록되지 않았고 Bath Montage section 이름은 `Default`다.

## 1. Skeleton Slot 등록

1. `/Game/Characters/Mannequins/Meshes/SK_Mannequin`을 연다.
2. `Anim Slot Manager`를 연다.
3. 기존 `DefaultGroup` 아래에 새 Slot `CustomerAction`을 추가한다.
4. 최종 표시가 `DefaultGroup.CustomerAction`인지 확인한다.
5. `DefaultSlot`은 삭제하거나 이름을 바꾸지 않는다.
6. Skeleton을 저장한다. Retarget, Compatible Skeleton, Bone, Preview Mesh는 변경하지 않는다.

예상 결과:

- `DefaultGroup`에 `DefaultSlot`과 `CustomerAction`이 함께 존재한다.

## 2. AnimBP와 Montage Slot/Section 확정

### ABP_Unarmed

1. `/Game/Characters/Mannequins/Anims/Unarmed/ABP_Unarmed`을 연다.
2. AnimGraph에서 이미 `Main States -> Slot(DefaultSlot) -> Control Rig -> Output Pose`로 연결된 Slot 노드를 찾는다.
3. 새 Slot 노드를 추가하지 말고 기존 Slot 노드의 Slot Name만 `DefaultGroup.CustomerAction`으로 바꾼다.
4. 연결을 유지한 채 Compile Error 0, Warning 0을 확인하고 저장한다.

### AM_Customer_Action_Once

1. `/Game/Bathhouse/Animations/Customer/AM_Customer_Action_Once`을 연다.
2. Slot Track이 `DefaultGroup.CustomerAction`인지 확인한다. 아니면 해당 Slot으로 바꾼다.
3. `MM_Land` 한 구간, Loop Count 1, Asset Loop 꺼짐, Auto Blend Out 켜짐을 유지한다.
4. Section은 `Default`여도 된다. Section 간 순환 링크를 만들지 않는다.
5. 저장한다.

### AM_Customer_Bath_Loop

1. `/Game/Bathhouse/Animations/Customer/AM_Customer_Bath_Loop`을 연다.
2. Slot Track이 `DefaultGroup.CustomerAction`인지 확인한다. 아니면 해당 Slot으로 바꾼다.
3. 현재 `Default` section의 이름을 정확히 `BathLoop`로 바꾼다.
4. `BathLoop`의 Next Section은 `None`으로 둔다. 에셋 자체를 무한 루프로 만들지 않는다.
5. `MM_Idle` 한 구간, Loop Count 1, Asset Loop 꺼짐을 유지한다.
6. 저장한다.

예상 결과:

- 두 Montage는 동일 Skeleton과 동일 Slot을 사용한다.
- Bath Montage에 정확히 `BathLoop` section이 있고 자체 순환은 없다.

## 3. ST_CustomerRoutine Bath 경로 교체

`/Game/Bathhouse/AI/ST_CustomerRoutine`에서 기존 Bath 이동 경로 뒤에 Action Point 정렬, Bath 체류 시작, 반복 Montage 재생, 공통 정리를 추가한다. 아래 작업은 **Bath 분기만 수정**한다. CheckIn, PreShower, MainShower, CheckOut 및 기존 이동 실패 복구 경로는 건드리지 않는다.

### 3.1 작업 전 상태 확인

StateTree를 연 뒤 먼저 다음 항목을 확인한다.

- Schema: `StateTreeAIComponentSchema`
- Context Actor: `BathhouseCustomerCharacter` 또는 해당 BP 자식 타입
- AI Controller: `BathhouseCustomerAIController`
- Compile 결과: 오류 없음
- 기존 `BathCycle` 상위의 `Delay` Task: `Run Forever=true`

`Delay(Run Forever)`는 이전 StateTree 조기 완료 문제를 막기 위해 이미 넣어 둔 workaround다. 이 Task를 삭제하거나 새로 만들지 않는다. 새 `BathUse`나 `BathCleanup` 안에도 `Run Forever` Delay를 추가하지 않는다.

Task 선택 목록에 아래 이름이 전혀 없다면 StateTree를 먼저 고치지 말고 Editor를 종료한 뒤 최신 C++로 `BathhouseSimEditor`를 빌드하고 다시 연다.

- `Snap Customer Facility Point`
- `Begin Customer Activity`
- `Finish Customer Activity`
- `Play Selected Montage Loop For Duration`

### 3.2 수정 범위 찾기

Bath 구간에서 `Hold Customer Facility` Task의 `Facility` 값이 `Bath`인 상태를 찾는다. 상태 이름이 문서와 조금 달라도 Task와 `Facility=Bath` 값으로 식별한다.

이 부모 상태 안의 다음 기존 흐름은 그대로 유지한다.

1. `Get Customer Facility Target`
   - `bUseApproachPoint=true`
2. `Move To`
   - `Destination`은 기존 Facility Target 결과에 바인딩된 상태 유지
3. 이동 성공 시 `Record Customer Navigation Result`
4. 이동 실패 시 기존 재시도 또는 `ExitTechnicalAbort` 전이

새 상태는 **이동 성공 기록 다음**, 그리고 기존 Bath 체류/반복 판정 이전에 배치한다. 최종 계층은 다음 형태가 되어야 한다.

```text
BathCycle
  Task: 기존 Delay (Run Forever=true)                 <- 유지
  └─ HoldBathFacility                                 <- Hold Customer Facility, Facility=Bath
      ├─ 기존 Bath Target 조회 / Move To / 결과 기록
      ├─ SnapBathAction
      ├─ BathUse
      │   Task 0: Begin Customer Activity (BathDwell)
      │           Toggle Completion=꺼짐
      │   Task 1: Play Selected Montage Loop For Duration
      │           Toggle Completion=켜짐
      └─ BathCleanup
          Task 0: Finish Customer Activity (BathDwell)
          Task 1: Snap Customer Facility Point (ApproachPoint)

기존 BathRepeat / 남은 Bath 시간 판정 / MainShower 분기
```

중요한 경계는 다음과 같다.

- `SnapBathAction`, `BathUse`, `BathCleanup`은 모두 `Hold Customer Facility(Bath)`가 활성 상태인 동안 실행되어야 한다.
- 따라서 세 상태를 `Hold Customer Facility(Bath)`의 자식 영역에 둔다.
- `BathCleanup` 전에 Facility 부모 밖으로 나가면 Reservation을 해제하는 시점과 고객 위치 복구 순서가 뒤섞일 수 있다.
- `BathRepeat`와 `MainShower`는 기존 위치와 전이를 유지하고, `BathCleanup` 완료 후에만 진입한다.

### 3.3 Binding 원칙

새 Task를 선택하고 Details의 각 입력값 오른쪽 Binding 버튼을 눌러 아래 소스를 선택한다. Unreal 버전에 따라 목록 표시는 `Context Actor` 또는 `Actor`로 보일 수 있다.

| 대상 입력 | Binding 소스 | 주의사항 |
|---|---|---|
| 모든 새 Customer Task의 `Session` | `Context Actor > CustomerSession` | `Global > Parameter.Session`, `Root > Parameter.Session` 사용 금지 |
| Montage Task의 `Customer` | `Context Actor` 자체 | `CustomerSession`을 연결하는 값이 아님 |
| Montage Task의 `Duration` | 같은 `BathUse`의 `Begin Customer Activity > ResolvedDuration` | 숫자를 직접 입력하지 않음 |

`Session` Binding 목록에 `Global/Parameter.Session`과 `Root/Parameter.Session`만 보이고 `Context Actor`가 없다면 임시로 Parameter를 연결하지 않는다. 다음을 순서대로 확인한다.

1. StateTree Schema가 `StateTreeAIComponentSchema`인지 확인한다.
2. Schema의 Context Actor 타입이 고객 Character인지 확인한다.
3. StateTree를 Compile하고 닫았다가 다시 연다.
4. 그래도 없으면 Editor를 재시작하고 C++ 클래스 로딩 상태를 확인한다.

`Duration`의 `ResolvedDuration`은 `Begin Customer Activity`의 출력값이다. 이를 안정적으로 참조하도록 `Begin Customer Activity`와 Montage Task를 **같은 `BathUse` 상태**에 Task 0, Task 1 순서로 둔다. 두 Task를 서로 다른 sibling 상태로 나누면 이전 상태의 출력이 Binding 후보에서 사라진다.

### 3.4 SnapBathAction 작성

기존 Bath 이동 성공 상태의 다음 상태로 `SnapBathAction`을 만든다.

Task를 하나 추가하고 다음과 같이 설정한다.

| 속성 | 값 |
|---|---|
| Task | `Snap Customer Facility Point` |
| `Session` | `Context Actor.CustomerSession` Binding |
| `Target` | `ActionPoint` |

이 Task는 NavMesh 이동으로 Approach Point까지 도착한 고객을 실제 Bath 동작 위치와 회전에 정확히 맞춘다. `Move To` 목적지를 Action Point로 바꾸는 것이 아니라, 기존처럼 Approach Point까지 이동한 뒤 여기서 Action Point로 Snap한다.

전이는 다음과 같이 만든다.

- `On State Succeeded` → `BathUse`
- `On State Failed` → 기존 `ExitTechnicalAbort`

UE 5.8의 `On State Completed`는 성공과 실패를 모두 포함한다. 실패를 `ExitTechnicalAbort`로 분리해야 하므로 여기서는 `On State Completed`가 아니라 반드시 `On State Succeeded`를 사용한다.

### 3.5 BathUse 작성

`SnapBathAction`의 다음 sibling으로 상태 `BathUse`를 만든다. State Details에서 `Tasks Completion`은 `All`로 설정한다.

먼저 Task 0으로 다음 Task를 추가한다.

| 속성 | 값 |
|---|---|
| Task | `Begin Customer Activity` |
| `Session` | `Context Actor.CustomerSession` Binding |
| `Activity` | `BathDwell` |

`ResolvedDuration`은 직접 입력하는 속성이 아니라 이 Task가 Session 상태를 기준으로 계산해 내보내는 출력이다.

Task 0의 행 또는 Details에 표시되는 `Toggle Completion` 아이콘을 눌러 **꺼진 상태**로 만든다. 툴팁에 `The task doesn't affect the state completion`이 표시되는 상태가 맞다. 아이콘 색 기준으로는 완료에 포함되는 켜진 상태가 청록색이고, 여기서는 꺼진 일반색이어야 한다.

이 설정이 필요한 이유는 `Begin Customer Activity`가 `EnterState`에서 체류 시간을 계산한 뒤 즉시 `Succeeded`를 반환하기 때문이다. 완료 영향이 켜져 있으면 Montage가 아직 `Running`이어도 `BathUse`가 즉시 완료되어 목욕 애니메이션과 체류 시간이 건너뛰어질 수 있다.

그다음 같은 `BathUse` 상태의 Task 1로 다음 Task를 추가한다.

| 속성 | 값 |
|---|---|
| Task | `Play Selected Montage Loop For Duration` |
| `Customer` | `Context Actor` Binding |
| `MontageCandidates[0]` | `/Game/Bathhouse/Animations/Customer/AM_Customer_Bath_Loop` |
| `LoopSection` | `BathLoop` |
| `Duration` | `BathUse.Begin Customer Activity.ResolvedDuration` Binding |
| `PlayRate` | `1.0` |
| `BlendInTime` | `0.2` |
| `BlendOutTime` | `0.2` |

Task 1의 `Toggle Completion`은 **켜진 상태**로 유지한다. 따라서 `BathUse`의 성공/실패와 완료 시점은 실제로 시간 동안 실행되는 Montage Task가 결정한다.

`MontageCandidates`는 배열이므로 `+`를 눌러 원소를 하나 만든 다음 `AM_Customer_Bath_Loop` 에셋을 지정한다. 이것은 Component 참조 배열이 아니라 Montage 에셋 배열이므로 인스턴스 오브젝트를 찾을 필요가 없다.

`Duration` Binding을 열었을 때 `ResolvedDuration`이 보이지 않으면 다음 세 가지를 확인한다.

- `Begin Customer Activity`와 Montage Task가 같은 `BathUse` 상태에 있는지
- `Begin Customer Activity`가 Task 0, Montage Task가 Task 1인지
- `Begin Customer Activity`를 선택했을 때 Output에 `ResolvedDuration`이 보이는지

`Reactivation` 항목은 이 구성에 필요하지 않다. 현재 엔진 UI에 해당 옵션이 없어도 정상이다.

### 3.6 BathCleanup 작성

`BathUse`와 같은 레벨, 즉 `Hold Customer Facility(Bath)` 아래에 sibling 상태 `BathCleanup`을 만든다. Task 목록 순서는 반드시 다음처럼 위에서 아래로 둔다.

#### Task 0 — Bath 활동 종료

| 속성 | 값 |
|---|---|
| Task | `Finish Customer Activity` |
| `Session` | `Context Actor.CustomerSession` Binding |
| `Activity` | `BathDwell` |

#### Task 1 — Approach Point로 복귀

| 속성 | 값 |
|---|---|
| Task | `Snap Customer Facility Point` |
| `Session` | `Context Actor.CustomerSession` Binding |
| `Target` | `ApproachPoint` |

이 순서는 Bath 활동 상태를 먼저 닫고, 고객을 이동 가능한 접근 위치로 되돌리는 의미다. 같은 Cleanup을 자연 완료와 `BathStayExpired` Event가 공유해야 하므로 경로별로 Finish/Snap Task를 복제하지 않는다.

`BathCleanup`의 `Tasks Completion`도 `All`로 설정하고 두 Task의 `Toggle Completion`은 모두 켜 둔다. 실패 전이는 기존 `ExitTechnicalAbort`로 연결한다. 두 Task가 모두 성공한 뒤에만 기존 `BathRepeat` 또는 남은 Bath 시간 판정으로 간다.

### 3.7 전이 설정

전이는 아래 표대로 정리한다.

| 출발 | Trigger/조건 | 목적지 |
|---|---|---|
| 기존 Bath 이동 성공 기록 상태 | On State Succeeded | `SnapBathAction` |
| `SnapBathAction` | On State Succeeded | `BathUse` |
| `SnapBathAction` | On State Failed | `ExitTechnicalAbort` |
| `BathUse` | On State Succeeded | `BathCleanup` |
| `BathUse` | Event `Customer.Event.BathStayExpired` | `BathCleanup` |
| `BathUse` | On State Failed | `ExitTechnicalAbort` |
| `BathCleanup` | On State Succeeded | 기존 `BathRepeat`/남은 시간 판정 |
| `BathCleanup` | On State Failed | `ExitTechnicalAbort` |

`Customer.Event.BathStayExpired` 전이는 `BathUse`에 둔다. Event를 받으면 Montage Task의 native `ExitState`가 자신이 시작한 재생 토큰만 중단한 뒤 동일한 `BathCleanup`으로 진입한다. Event 전이에 Priority 항목이 있으면 일반 완료 전이보다 높게 두고 `Consume Event`를 켠다.

다음 전이는 만들지 않는다.

- `BathStayExpired` → `MainShower`
- `BathStayExpired` → `BathRepeat`
- `BathStayExpired` → `Hold Customer Facility(Bath)` 바깥 상태
- `BathUse` 완료 → Facility 부모 바깥 상태

자연 완료든 Event 만료든 반드시 `BathCleanup`을 한 번 통과해야 한다. Cleanup 완료 후 기존 `Customer Has Bath Time Remaining` Condition이 참이면 `BathRepeat`, 거짓이면 `MainShower`로 보내는 기존 판정을 사용한다.

### 3.8 StateTree Task 실행 방식에서 주의할 점

- 한 상태에 여러 Task를 넣으면 Task들은 상태 진입 시 목록 순서대로 시작되지만, 서로를 자동으로 기다리는 직렬 단계 목록은 아니다.
- `BathUse`에서는 Task 0이 Duration을 계산하고 즉시 성공하며, Task 1이 그 출력을 받아 시간 동안 `Running`한다.
- Task 0의 `Toggle Completion`을 끄고 Task 1만 완료에 포함해야 Task 0의 즉시 성공이 `BathUse`를 조기 완료시키지 않는다.
- `BathCleanup`의 두 Task는 즉시 실행되는 정리 Task들이므로 한 상태 안에서 `Finish`를 Task 0, `Snap`을 Task 1로 두고 둘 다 완료에 포함한다.
- `BathUse`의 자연 완료 전이는 Montage Task가 완료한 뒤 `BathCleanup`으로 간다.
- 기존 `BathCycle.Delay(Run Forever)`는 상위 StateTree가 자식 진행 중 완료로 판정되는 것을 막는 장치일 뿐, Montage 시간을 대신하는 Delay가 아니다.

### 3.9 Compile 후 확인 체크리스트

StateTree를 Compile한 뒤 아래 항목을 하나씩 확인한다.

- Schema 관련 오류가 없다.
- 새 Task 네 종류가 모두 정상 클래스 이름으로 표시된다.
- 모든 `Session`이 `Context Actor.CustomerSession`에 연결되어 있다.
- Montage의 `Customer`가 `Context Actor`에 연결되어 있다.
- Montage의 `Duration`에 숫자가 아니라 `ResolvedDuration` Binding이 표시된다.
- `BathUse.Tasks Completion=All`이며 Begin Task의 `Toggle Completion`은 꺼져 있다.
- Montage Task의 `Toggle Completion`은 켜져 있다.
- Bath Montage 에셋과 `BathLoop` Section이 지정되어 있다.
- 자연 완료와 `BathStayExpired`가 모두 동일한 `BathCleanup`으로 들어간다.
- `BathCleanup`에서 `Finish BathDwell` 후 `ApproachPoint` Snap 순서가 유지된다.
- Cleanup 이전에 `MainShower`, `BathRepeat`, Facility 부모 밖으로 나가는 전이가 없다.
- 기존 이동 실패/재시도/`ExitTechnicalAbort` 전이가 남아 있다.
- 기존 `Delay(Run Forever)`가 그대로 남아 있다.

PIE에서는 한 고객으로 먼저 다음 순서를 확인한다.

1. Bath Approach Point까지 NavMesh 이동
2. Bath Action Point 위치와 회전으로 Snap
3. `BathLoop` Montage 반복
4. 자연 완료 또는 `BathStayExpired` 발생
5. Montage 종료 및 `BathDwell` Finish
6. Bath Approach Point로 복귀
7. 남은 Bath 시간이 있으면 반복, 없으면 MainShower 진행

그 후 여러 고객을 생성해 Facility 점유 해제, BathRepeat, 경로 충돌 여부를 확인한다.

## 4. one-shot 기술 검증을 Drying에 연결

기존 `DryingSpot / Drying` Facility 부모에서 기존 이동 성공 뒤의 `Timed Customer Activity(Drying)`만 다음 세 상태로 교체한다.

1. `BeginDrying`
   - Task: `Begin Customer Activity`
   - Session: `Context Actor.CustomerSession`
   - Activity: `Drying`
2. `PlayDryingOnce`
   - Task: `Play Customer Montage Once`
   - Customer: `Context Actor`
   - MontageCandidates[0]: `AM_Customer_Action_Once`
   - PlayRate: `1.0`
   - StartSection: 비움
   - BlendInTime / BlendOutTime: 우선 `0.2`
   - 별도 Delay나 duration wait를 추가하지 않는다.
3. `FinishDrying`
   - Task: `Finish Customer Activity`
   - Session: `Context Actor.CustomerSession`
   - Activity: `Drying`
4. `FinishDrying` 성공 뒤 기존 `ReturnTowel` 단계로 간다.

나머지 animation-free 활동의 기존 `Timed Customer Activity`는 유지한다.

## 5. Compile, Save, Restart

1. `SK_Mannequin`, `ABP_Unarmed`, 두 Montage, `ST_CustomerRoutine`을 저장한다.
2. `ABP_Unarmed`을 Compile한다.
3. `ST_CustomerRoutine`을 Compile하고 Error 0, Warning 0을 확인한다.
4. 에디터를 정상 종료하고 UE 5.8로 다시 연다.
5. 위 네 종류 에셋을 다시 열어 Slot, `BathLoop`, Candidate, Binding이 유지되는지 확인한다.
6. `ST_CustomerRoutine`을 다시 Compile한다.
7. 완료 후 에이전트에게 통합 검증 재개를 요청한다.

재개 시 에이전트가 수행할 작업:

- `.md/PROMPT_UNREAL.md`의 PIE 10개 인수 시나리오
- Action blocked, natural duration, `BathStayExpired`, technical abort 순서 확인
- montage candidate 0/1/many와 stale token 확인
- 두 고객의 서로 다른 Bath Slot 점유와 Approach 복귀 확인
- `.md/PROMPT_INTEGRATION_REVIEW.md` 최종 갱신
