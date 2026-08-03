# Bath Approach Snap And Customer Montage — Unreal Integration Review

## 작업 상태

- 상태: 부분 완료 — 사용자 에디터 작업 대기
- 기준 작업서: `.md/PROMPT_UNREAL.md`
- 후속 사용자 작업: `.md/USER_UNREAL.md`
- UE 버전: 5.8.1
- 완료로 보고하지 않는 이유: Skeleton Slot 등록, Bath Montage section 이름, AnimBP Slot 선택, StateTree Task/Binding/Transition은 현재 Unreal MCP의 등록 도구로 편집할 수 없다.

## Native Preflight

- 실행 중이던 Editor를 종료하고 UE 5.8 공식 `Build.bat`로 `BathhouseSimEditor Win64 Development -WaitMutex -NoHotReloadFromIDE`를 빌드했다.
- 번들 .NET SDK 10.0을 사용했고 결과는 `Succeeded`, 타깃은 최신 상태였다.
- 새 Editor 프로세스로 프로젝트를 열었다. Hot Reload와 stale DLL은 사용하지 않았다.
- 다음 네이티브 타입을 새 프로세스에서 확인했다.
  - `/Script/BathhouseSim.CustomerMontagePlaybackComponent`
  - `/Script/BathhouseSim.CustomerFacilitySnapTask`
  - `/Script/BathhouseSim.CustomerBeginActivityTask`
  - `/Script/BathhouseSim.CustomerFinishActivityTask`
  - `/Script/BathhouseSim.PlayCustomerMontageOnceTask`
  - `/Script/BathhouseSim.PlaySelectedMontageLoopForDurationTask`
- 누락된 native type/property 오류는 없었다.

## Customer Blueprint와 실제 Animation 자산

- `/Game/Bathhouse/Blueprints/Customer/BP_BathhouseCustomer`
  - native parent 계약을 유지했다.
  - 상속 컴포넌트 `CustomerSession`, `CustomerMontagePlayback`을 확인했다.
  - `CustomerMontagePlayback.PrimaryComponentTick.bCanEverTick=false`다.
  - Blueprint Event Graph, montage delegate, timer 로직을 추가하지 않았다.
  - warnings-as-errors 컴파일에 성공했다.
- 실제 고객 Anim Class는 `/Game/Characters/Mannequins/Anims/Unarmed/ABP_Unarmed.ABP_Unarmed_C`다.
- 고객 Skeletal Mesh는 `/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple`이다.
- Skeleton은 `/Game/Characters/Mannequins/Meshes/SK_Mannequin`이다.
- `ABP_Unarmed` AnimGraph의 현재 경로는 `Main States -> Slot(DefaultSlot) -> Control Rig -> Output Pose`다.
- Slot 배치는 이미 올바르지만 Skeleton에 `CustomerAction`이 없어서 Slot 이름은 아직 `DefaultSlot`이다.
- 현재 상태의 `ABP_Unarmed`은 warnings-as-errors 컴파일에 성공했다.

## Bath Slot Authoring

- 수정 에셋:
  - `/Game/Bathhouse/Blueprints/Facility/BP_Bath`
  - `/Game/Maps/DefaultMap`
- 배치 Bath 인스턴스: `Bathhouse_Bath`, World `(1750, 0, 0)`
- 고객 Capsule: Radius 25, Half Height 88
- Bath placeholder mesh: `/Engine/BasicShapes/Cube`, 상대 위치 `(0,0,50)`, 상대 스케일 `(1.5,1.8,0.45)`, BlockAllDynamic
- Bath top World Z는 72.5다.
- Action Capsule 바닥은 Bath top에서 2cm 위이고, Approach Capsule 바닥은 지면에서 2cm 위다.

| Slot | Action Local | Action World | ApproachOffset | Approach World | Facing |
|---|---:|---:|---:|---:|---:|
| A | `(0,-60,162.5)` | `(1750,-60,162.5)` | `(150,0,-72.5)` | `(1900,-60,90)` | Yaw 180 |
| B | `(0,0,162.5)` | `(1750,0,162.5)` | `(150,0,-72.5)` | `(1900,0,90)` | Yaw 180 |
| C | `(0,60,162.5)` | `(1750,60,162.5)` | `(150,0,-72.5)` | `(1900,60,90)` | Yaw 180 |

- Action은 Bath footprint 위의 비-Nav 목표다.
- Approach는 Bath 바깥 지면의 고객 중심 높이다.
- 세 Action 간격 60cm는 고객 Capsule 지름 50cm보다 크다.
- `BP_Bath`는 warnings-as-errors 컴파일에 성공했고 `DefaultMap`과 함께 저장했다.
- Editor 재시작 후 Blueprint 기본값과 배치 인스턴스 값이 모두 유지됨을 다시 읽어 확인했다.

## 생성한 Montage

### AM_Customer_Action_Once

- 경로: `/Game/Bathhouse/Animations/Customer/AM_Customer_Action_Once`
- 생성 원본: `/Game/Characters/Mannequins/Anims/Pistol/MM_Pistol_Fire_Montage`
- Skeleton: `/Game/Characters/Mannequins/Meshes/SK_Mannequin`
- AnimSequence: `/Game/Characters/Mannequins/Anims/Unarmed/Jump/MM_Land`
- 기술 검증 구간: 0.666667초
- Root Motion 없음, AnimNotify 없음
- Slot track field: `CustomerAction`
- Asset loop 꺼짐, Auto Blend Out 켜짐, 자연 종료 가능

### AM_Customer_Bath_Loop

- 경로: `/Game/Bathhouse/Animations/Customer/AM_Customer_Bath_Loop`
- 생성 원본: `/Game/Characters/Mannequins/Anims/Pistol/MM_Pistol_Fire_Montage`
- Skeleton: `/Game/Characters/Mannequins/Meshes/SK_Mannequin`
- AnimSequence: `/Game/Characters/Mannequins/Anims/Unarmed/MM_Idle`
- 기술 검증 구간: 0.666667초
- Root Motion 없음, AnimNotify 없음
- Slot track field: `CustomerAction`
- Asset loop 꺼짐

두 Montage는 저장 후 Editor를 재시작해 AnimSequence, Skeleton, Slot, 길이와 비루프 설정이 유지됨을 확인했다.

## 현재 미완료 Editor 연결

- `/Game/Characters/Mannequins/Meshes/SK_Mannequin`
  - `DefaultGroup.CustomerAction` Slot 등록 필요
- `/Game/Characters/Mannequins/Anims/Unarmed/ABP_Unarmed`
  - 기존 `DefaultSlot` 노드를 `DefaultGroup.CustomerAction`으로 선택 필요
- `/Game/Bathhouse/Animations/Customer/AM_Customer_Action_Once`
  - Skeleton Slot 등록 뒤 `DefaultGroup.CustomerAction` 표시 확인 필요
- `/Game/Bathhouse/Animations/Customer/AM_Customer_Bath_Loop`
  - Skeleton Slot 등록 뒤 `DefaultGroup.CustomerAction` 표시 확인 필요
  - 현재 `Default` section을 `BathLoop`로 이름 변경 필요
- `/Game/Bathhouse/AI/ST_CustomerRoutine`
  - 기존 Schema, Context Actor, AI Controller는 정상이며 재시작 뒤 컴파일 성공했다.
  - 현재 asset 문자열/등록 task 검사에서 새 Snap/Begin/Finish/one-shot/duration-loop Task가 존재하지 않는다.
  - Bath shared cleanup과 one-shot Drying 기술 검증 경로가 필요하다.
  - 기존 상위 `Delay(Run Forever)` workaround는 보존해야 한다.

정확한 UI 절차와 값은 `.md/USER_UNREAL.md`에 기록했다.

## Compile, Save, Restart 검증

- 변경 에셋은 모두 명시 경로로만 저장했다. 전체 dirty asset 일괄 저장은 사용하지 않았다.
- 첫 세션에서 `BP_BathhouseCustomer`, `BP_Bath`, `ABP_Unarmed` 컴파일에 성공했다.
- `ST_CustomerRoutine`은 현재 그래프로 컴파일 성공했다.
- `BP_Bath`, `DefaultMap`, 두 Montage를 저장했다.
- Editor를 종료하고 새 UE 5.8 프로세스로 다시 열었다.
- 새 프로세스에서 일곱 대상 에셋을 모두 load했고 dirty asset은 없었다.
- `BP_BathhouseCustomer`, `BP_Bath`, `ABP_Unarmed`을 warnings-as-errors로 다시 컴파일했다.
- `ST_CustomerRoutine` 재로드 로그: `Compile StateTree ... succeeded`.
- Map Check: 0 Error, 0 Warning.

## PIE 스모크

- 재시작 후 PIE를 25초 이상 실행했다.
- `BP_BathhouseCustomer` 4개와 AI Controller 4개가 생성됐다.
- 세 queue 고객 위치는 대략 `(242.6,-74.3,90.15)`, `(341.6,-74.0,90.15)`, `(451.7,-76.0,90.15)`로 확인돼 기존 생성/AI 루틴이 실행 중이었다.
- 프로젝트 StateTree 초기화 오류, Blueprint 오류, Bath snap 오류, montage 오류는 관찰되지 않았다.
- 새 StateTree Task가 아직 연결되지 않았으므로 이 스모크는 Bath snap/montage 기능 성공을 의미하지 않는다.

## 로그 분류

- 프로젝트 compile/map 오류: 없음
- StateTree 현 그래프 compile 오류: 없음
- 비차단 엔진 환경 로그:
  - aqProf, VTune, WinPix, Wintab 선택 DLL 미탑재
  - NVIDIA 610 미만 드라이버에서 TSR 16-bit VALU 비활성화
  - 엔진 `UnifiedErrorTest` 자체 테스트 로그
  - MCP session reinitialize 및 JSON schema 미지원 delegate/property 경고
  - StateTree editor menu 중복 등록 경고
- Montage `sequenceLength` 직접 설정 시도 경고는 최종 asset 값을 0.666667초로 일치시킨 뒤 저장했고 재시작 검증을 통과했다.

## 아직 수행하지 않은 PIE 인수 시나리오

- Approach 이동 후 정확한 Action snap과 이동 모드 disable/restore
- blocked Action 충돌 실패와 비이동 진단
- 자연 duration 종료와 Finish -> Approach -> Release 순서
- `BathStayExpired`의 Montage Exit -> Finish -> Approach -> Release 순서
- technical abort cleanup
- MontageCandidates null/zero/many와 Enter당 단일 선택
- one-shot 자연 종료, 외부 중단, stale token
- 기존 `Timed Customer Activity` 회귀
- 두 고객의 서로 다른 Bath Slot 동시 사용과 exclusivity

위 항목은 `.md/USER_UNREAL.md` 완료 후에만 검증할 수 있다.
