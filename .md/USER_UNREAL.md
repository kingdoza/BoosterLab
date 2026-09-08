# 사용자 Unreal 작업 — 시설 배치, 사물함 수용량 및 확장 시스템 통합

## 상태

저장된 입력, 프리뷰, 정의 및 시설 Blueprint 에셋은 재로드 후 검증을 완료했다. 남은 작업은 Unreal Editor UI를 사용해야 하거나 네이티브 코드 수정이 필요하다. 현재 MCP에는 안전한 WidgetTree/StateTree 쓰기 기능이 없으며, World Partition 액터 변경 사항도 Editor 재시작 후 디스크에 유지하지 못한다.

Unreal Editor 외부에서 `.uasset` 또는 `.umap` 파일을 수정하지 않는다. `Save All`은 사용하지 않는다.

## 1. `WBP_InteractionPrompt` 복구

Widget Blueprint Designer에서 `/Game/Bathhouse/UI/WBP_InteractionPrompt`를 연다. 부모 클래스 `UInteractionPromptWidget`을 유지하고, 기존의 모든 기본·보조·장비 행을 그대로 둔다.

기존 프롬프트 루트 아래에 다음 Designer 위젯을 추가한다. 각 위젯에서 **Is Variable**을 활성화하고 이름을 정확히 지정한다.

| 이름 | 위젯 타입 |
|---|---|
| `PlacementActionNameText` | Text Block |
| `PlacementFailureReasonText` | Text Block |
| `RecoveryActionNameText` | Text Block |
| `RecoveryFailureReasonText` | Text Block |
| `RecoveryProgressBar` | Progress Bar |

LMB 시설 배치 행과 Q 시설 복구 행을 기존 프롬프트 행과 일관된 형태로 배치한다. Event Graph 로직, 바인딩, 표시 여부 로직, 타임아웃 로직 또는 진행률 계산은 추가하지 않는다.

Compile한다. 위젯 5개와 관련된 missing-bind-widget 오류가 모두 사라져야 한다. 이 에셋만 저장한 뒤 닫고 다시 열어서 한 번 더 Compile한다.

## 2. `ST_CustomerRoutine` 마이그레이션

`/Game/Bathhouse/AI/ST_CustomerRoutine`과 `/Game/Bathhouse/Data/DA_CustomerRoutine_Default`를 연다.

1. 활성 상태인 `StoreShoes`와 `WearShoes` 분기 및 Task만 제거한다. Data Asset의 폐기 예정 네이티브 시간 속성은 삭제하거나 이름을 변경하지 않는다.
2. `Undress`와 `Dress` 양쪽에서 기존의 범용 시설 행동 Task 패턴과 고객 세션 바인딩을 유지한다. 시설 선택 값만 `ClothesLocker`로 변경한다.
3. 사물함 번호, 열쇠 번호 매핑, 캐시된 사물함 슬롯, Blueprint 반환 노드 또는 수동 정리 Task를 추가하지 않는다. 사용 가능한 번호 없는 사물함 Action Slot의 선택과 정리는 네이티브 코드가 담당한다.
4. 기존 체크인·열쇠 흐름, 중단, 타임아웃 및 체크아웃 정리 Transition을 그대로 유지한다.
5. `ST_CustomerRoutine`과 `DA_CustomerRoutine_Default`를 각각 Compile하고 개별 저장한다.

예상 결과: StateTree에 활성 신발 처리 단계가 없어야 하며, `Undress`와 `Dress`는 범용 `ClothesLocker` 행동 경로를 사용해야 한다.

## 3. `DefaultMap` 레벨 액터 구성

PIE를 중지한 상태에서 `/Game/Maps/DefaultMap`을 연다. 백그라운드 MCP가 메모리상에서는 다음 액터를 만들었지만, 재시작 검증 결과 World Partition 저장 경로를 통해 디스크에 유지되지 않았다. Unreal Editor UI에서 직접 생성하고 저장한다.

1. `/Game/Bathhouse/Blueprints/Placement/BP_FacilityPlacementZone`을 하나 배치한다.
   - Location: `(600, -100, 0)`
   - Scale: `(1, 1, 1)`
   - `ZoneBounds`: Extent `(1400, 900, 10)`, Relative Location `(0, 0, 10)`
   - `AllowedFacilityTags`: 정확히 `Facility.Placeable` 하나만 지정
2. `/Game/Bathhouse/Blueprints/Placement/BP_BathhouseExpansionAuthority`를 하나 배치한다.
   - Location: `(0, 0, 0)`
   - `ExpansionDefinition`: `/Game/Bathhouse/Data/Expansion/DA_BathhouseExpansion_Default`
   - `InitialTierIndex`: `0`
3. `/Game/Bathhouse/Blueprints/Interaction/BP_BathhouseKeyRack`을 하나 배치한다.
   - Location: `(0, -260, 50)`
   - Blueprint 기본값은 이미 작성돼 있다. Key Class는 `BP_BathhouseKey`, Hook Class는 `BP_BathhouseKeyHook`, 첫 번째 열쇠 번호는 `1`이며 열쇠·걸이 Transform 쌍은 8개다.
4. 기존 `RecastNavMesh` 액터를 선택하고 **Runtime Generation**을 **Dynamic Modifiers Only**로 설정한다.
5. 새로 만든 World Partition 외부 액터 3개와 수정된 NavMesh/맵 패키지만 저장한다. `DefaultMap`을 닫았다가 다시 연다.

맵을 다시 연 뒤 Placement Zone, Expansion Authority, Key Rack이 각각 정확히 하나씩 존재하는지 확인한다. `RecastNavMesh.RuntimeGeneration`도 `Dynamic Modifiers Only`로 유지돼야 한다.

## 4. 기존 열쇠·걸이 쌍 제거

3번에서 배치한 새 Key Rack이 맵을 다시 열어도 유지되는 것을 확인한 뒤에만 진행한다. 다음 레벨 액터 6개는 Key Rack이 관리하는 구성과 중복되므로 정확히 이 액터들만 삭제한다.

- `/Game/Maps/DefaultMap.DefaultMap:PersistentLevel.BP_BathhouseKeyHook_C_UAID_F02F7433CA3601FC02_1768853783`
- `/Game/Maps/DefaultMap.DefaultMap:PersistentLevel.BP_BathhouseKey_C_UAID_F02F7433CA3601FC02_1768858784`
- `/Game/Maps/DefaultMap.DefaultMap:PersistentLevel.BP_BathhouseKeyHook_C_UAID_F02F7433CA3615F402_1442885866`
- `/Game/Maps/DefaultMap.DefaultMap:PersistentLevel.BP_BathhouseKey_C_UAID_F02F7433CA3615F402_1443544868`
- `/Game/Maps/DefaultMap.DefaultMap:PersistentLevel.BP_BathhouseKeyHook_C_UAID_F02F7433CA3615F402_1443215867`
- `/Game/Maps/DefaultMap.DefaultMap:PersistentLevel.BP_BathhouseKey_C_UAID_F02F7433CA3615F402_1443877869`

해당 외부 액터 삭제 패키지 6개를 저장한다. `BP_BathhouseKey` 또는 `BP_BathhouseKeyHook` Blueprint 에셋 자체는 삭제하지 않는다.

## 5. MCP 검증 중 발견된 네이티브 코드 차단 문제

다음 문제는 에디터에서 수동으로 설정해 해결하면 안 된다.

1. `/Game/Bathhouse/Blueprints/Interaction/BP_BathhouseKeyHook`은 클래스 기본값이 `KeyNumber=0`, `KeyActor=None`인 상태이며 두 속성 모두 `EditInstanceOnly`이기 때문에 Compile 시 유효성 검사에 실패한다. 클래스 기본값에 열쇠 액터를 지정하지 않는다. 네이티브 `ABathhouseKeyHookActor::IsDataValid`가 설정되지 않은 클래스 기본 객체와 Key Rack이 생성한 걸이를 건너뛰도록 하거나, 설정된 레벨 인스턴스만 검사하도록 수정해야 한다.
2. PIE를 두 번 실행했을 때 기존 1칸 사물함 두 개의 작성된 총 수용량이 `2`이고 Tier 0 제한도 `2`인데, 두 사물함이 설치 수용량을 초과했다는 로그가 발생했다. 확장 Authority가 등록되기 전이나 등록되는 동안 초기 배치 도메인 등록이 먼저 실행돼 발생한 문제다. 네이티브 코드는 Authority가 존재할 때까지 등록을 지연하거나 조용히 재시도하고, Authority 기반 수용량 검사가 실제로 실패했을 때만 오류를 출력해야 한다.

두 오류를 Editor 설정 문제로 취급하지 않는다. 정상적인 Blueprint Compile 및 PIE 로그를 수용하기 전에 구현·리뷰 단계로 돌려보내 수정해야 한다.

## 재개 조건

1~4번 작업과 네이티브 코드 문제 두 개를 모두 해결한 뒤 다음을 진행한다.

1. `WBP_InteractionPrompt`, `BP_BathhouseKeyHook`, 마이그레이션된 모든 시설 Blueprint와 `ST_CustomerRoutine`을 경고 및 오류 없이 Compile한다.
2. 시설 배치 정의 9개, 확장 정의, Key Rack, Zone, 시설 및 고객 루틴 Data Asset을 검증한다.
3. PIE를 두 번 실행하고 `.md/PROMPT_UNREAL.md`의 상호작용 수용 검사를 수행한다.

완료한 번호를 Codex에 알려주면 남은 재로드, 유효성 검사 및 PIE 후속 검증을 진행할 수 있다.
