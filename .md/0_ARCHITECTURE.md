# BathhouseSim Architecture Map

## 문서 기준

- 기준일: 2026-08-12(KST) Towel Stack/Pile/Slot presentation 확정 설계 기준
- 상태: Cleaning/Towel gameplay와 기본 Blueprint 구현 완료, Stack/Pile runtime 연결과 미연결 Slot component Source 구현 완료, Editor authoring 대기
- 정본 문서: `.md/0_ARCHITECTURE.md`와 `.md/Architecture/*.md`
- legacy 문서: 현재 별도 legacy architecture 문서는 없다.

## 분석 범위

- 주 분석 범위:
  - `Source/BathhouseSim/Public`
  - `Source/BathhouseSim/Private`
- 현재 구현된 C++ 하위 시스템:
  - Core
  - Character
  - Camera
  - Interaction
  - Facility
  - Economy
  - Customer
  - UI
  - Cleaning
  - Towel
- `Content`는 Blueprint 참조 검증 범위로만 다룬다. C++ 시스템 책임의 정본은 Source 하위 문서에 둔다.
- `Config/DefaultEngine.ini`는 GameMode/Pawn/Controller 연결 또는 Core Redirect가 필요한 rename 호환 경로로만 문서화한다.

## 시스템 문서

- [CharacterSystem.md](Architecture/CharacterSystem.md): 1인칭 입력, 컨트롤러 입력 매핑, 이동, 점프, sprint, 캐릭터 조립
- [CameraSystem.md](Architecture/CameraSystem.md): 이동/착지 기반 카메라 셰이크, camera manager 기반 pitch limit
- [InteractionSystem.md](Architecture/InteractionSystem.md): camera trace, primary/secondary intent와 단일 physical carry
- [FacilitySystem.md](Architecture/FacilitySystem.md): 다중 facility slot과 분리된 check-in/checkout queue
- [EconomySystem.md](Architecture/EconomySystem.md): PlayerState wallet과 일회성 cash 획득
- [CustomerSystem.md](Architecture/CustomerSystem.md): UE 5.8 StateTree customer routine, session과 cleanup
- [UISystem.md](Architecture/UISystem.md): native Widget/Widget Blueprint 경계와 interaction prompt target
- [CleaningSystem.md](Architecture/CleaningSystem.md): water stain spawn, wet mop hold cleaning과 presentation
- [TowelSystem.md](Architecture/TowelSystem.md): towel circulation, atomic transfer, overflow와 processing machine
- [TowelPresentationSystem.md](Architecture/TowelPresentationSystem.md): towel quantity mesh profile과 Stack/Pile/Slot world presentation
- [CoreSystem.md](Architecture/CoreSystem.md): 공통 문서 규칙, 모듈 경계, Source/Content/Config 경계, Core Redirect

## Source 구조

```text
Source/BathhouseSim/
  Public/
    Character/
    Camera/
    Interaction/
    Facility/
    Economy/
    Customer/
    UI/
    Cleaning/
    Towel/
  Private/
    Character/
    Camera/
    Interaction/
    Facility/
    Economy/
    Customer/
    UI/
    Cleaning/
    Towel/
    Tests/
```

- `Core`는 소스 폴더가 아니라 문서상 공통 경계다.
- 시스템 하위 폴더명은 include 경로의 1차 네임스페이스 역할을 한다.
- 현재 파일 분포는 하위 시스템 문서를 기준으로 확인한다.
  - Character: 1인칭 캐릭터, 플레이어 컨트롤러, movement/sprint 책임
  - Camera: 이동 상태와 착지 상태 기반 camera shake, camera manager 기반 pitch limit 책임
  - Interaction: player focus와 단일 key carry 책임
  - Facility: facility slot, numbered lookup와 counter queue 책임
  - Economy: player money와 cash claim 책임
  - Customer: StateTree routine과 customer session 책임
  - UI: interaction query의 local HUD 표현 책임
  - Cleaning: zone/stain registry, 물 얼룩 spawn/hold cleaning과 wet mop 책임
  - Towel: 수건 재고/전송, 설비, overflow, 처리 기계와 recovery ledger 책임
  - Core: 모듈/redirect/문서 경계 책임

## 시스템 간 책임 흐름

- Character는 로컬 플레이어 입력과 1인칭 캐릭터 조립 지점이다.
- Character는 Enhanced Input action을 이동, 시점, 점프, sprint 의도로 변환한다.
- Character는 sprint 상태와 속도 변경을 movement 책임으로 위임한다.
- Character는 1인칭 카메라와 camera shake component를 조립하지만, camera shake 상태 계산은 Camera가 담당한다.
- Camera는 owner의 이동 상태, sprint 상태, falling/landing 상태를 읽고 camera shake 재생/중단을 결정한다.
- Camera는 player camera manager를 통해 상하 시야각 제한 기본값을 제공하고, Blueprint 파생 class에서 값을 조정할 수 있게 한다.
- Camera는 비로컬 플레이어에서 Tick interval 조정과 shake 중단으로 비용을 줄인다.
- Character는 Interact 입력을 Interaction에 전달하고 carry/held key component를 조립한다.
- Interaction은 camera trace와 key/wet mop/towel basket 중 physical actor 하나의 held state를 소유한다.
- Facility는 다중 use slot과 check-in/checkout 독립 FIFO queue를 소유한다.
- Economy는 PlayerState wallet을 소유하고 cash claim을 한 번만 반영한다.
- Customer StateTree는 routine을 조율하고 session/queue/facility/key/wallet API에 실행을 위임한다.
- Customer bath stay는 pre-shower 완료부터 고정 60초이며 그동안 available bath를 random 이동한다.
- Bath의 `ApproachPoint`와 `ActionPoint`는 모두 캐릭터 발바닥 transform으로 authoring하며, Customer Session이 scaled capsule half height를 한 번 더해 실제 actor/capsule-center transform으로 변환한다. 고객은 NavMesh 위 `ApproachPoint`까지 이동한 뒤 `ActionPoint`로 snap하고, 퇴탕 시 `ApproachPoint`로 복귀한 뒤 navigation을 재개한다.
- Customer 행동 montage는 native StateTree Task가 유효 후보 중 하나를 EnterState에서 선택하며 one-shot 종료 또는 선택된 한 montage의 duration loop를 완료 기준으로 사용한다.
- UI는 Interaction query를 표시하고 domain 상태를 직접 판단하거나 변경하지 않는다.
- Cleaning은 zone 기반 water stain spawn과 wet mop hold-cleaning state를 소유한다.
- Towel은 homogeneous count, atomic transfer, used-bin overflow와 washer/dryer state를 소유한다.
- Towel Presentation은 inventory snapshot을 읽어 clean stack/used bin/basket의 Stack과 기존 washer/dryer의 Pile을 표시한다. Slot은 component/preview까지만 구현하고 gameplay actor에 연결하지 않는다.
- 사용 수건통 내부는 container 단위 E/F interaction이고, overflow world towel만 개별 E interaction이다.
- Customer는 clean towel token과 satisfaction을 session에 보관하고, used bin full이면 floor overflow로 반납한다.
- Core는 런타임 gameplay 상태를 소유하지 않고 모듈 의존성, Source 경계, Content/Config 정책, Core Redirect 기준을 문서화한다.

## 주요 의존 방향

- Character -> Camera
- Character -> Interaction
- Character -> EnhancedInput
- Character -> Engine Character/Movement
- Camera -> Character
- Camera -> Engine Camera/CameraShake
- Camera -> Engine PlayerCameraManager
- Interaction -> Facility validation
- Economy -> Interaction
- Customer -> Facility
- Customer -> Interaction
- Customer -> Economy
- Customer -> UE GameplayStateTree/AI/Navigation
- UI -> Interaction
- Cleaning -> Interaction
- Towel -> Interaction
- Towel -> Facility
- Towel Presentation -> Towel
- Customer -> Towel
- Core -> Engine module boundary

의존 방향은 Unreal 컴포넌트 조합을 반영한다. 새 기능을 추가할 때는 "상태 오너"와 "입력/표시 라우터"를 먼저 구분한다.

## Blueprint/API 변경 주의

- Blueprint native parent, BlueprintCallable API, serialized `UPROPERTY`/component 이름은 Content asset 참조와 분리해 판단하지 않는다.
- `UCLASS`/`USTRUCT`/`UENUM`/`UFUNCTION`/`UPROPERTY` rename 또는 삭제는 Core Redirect 필요 여부, Editor 재시작, Blueprint compile/save, post-migration scan까지 함께 계획한다.
- 현재 Blueprint/API 계약의 상세 목록은 관련 시스템 문서의 `Blueprint/API Contracts` 또는 `Design Notes`를 우선 확인한다.
- Core Redirect와 cross-system migration 규칙은 [CoreSystem.md](Architecture/CoreSystem.md)를 따른다.
- 이전 이식 단계의 클래스명 또는 Blueprint parent 참조가 Content/외부 asset에 남아 있다면 rename 완료 전 redirect 계획을 별도로 세운다.

## 현재 설계 원칙

- Actor는 composition root에 가깝게 유지하고, 상태와 반복 가능한 기능은 Component 또는 명확한 owner로 분리한다.
- 입력, 표시, 상태 변경 책임을 구분한다. 입력/표시 계층은 의도를 전달하고, 실제 runtime state mutation은 해당 상태 owner가 수행한다.
- 새 기능을 추가할 때는 먼저 상태 owner, 실행 주체, 표시/입력 라우터, Blueprint/API 계약을 정의한다.
- 시스템 간 의존은 필요한 방향으로만 추가하고, 순환 의존이나 임의의 cross-system 직접 참조를 만들기 전에 interface, event, subsystem 경계를 검토한다.
- Blueprint native parent, BlueprintCallable API, serialized `UPROPERTY` 이름은 Content asset 계약으로 취급한다.
- UCLASS/USTRUCT/UENUM rename 또는 삭제는 Core Redirect, Editor 재시작, Blueprint compile/save, post-migration scan까지 한 세트로 계획한다.
- 새 시스템, 새 의존 방향, Blueprint/API 계약 변경은 이 문서와 관련 `.md/Architecture/*System.md`를 함께 갱신한다.
- Content asset 수정이나 resave가 필요한 변경은 별도 사용자 지시와 Editor 검증 계획 없이는 진행하지 않는다.
- Player carry는 inventory/hotbar가 아닌 key/wet mop/towel basket 중 physical actor 하나만 허용한다. key의 hook/customer transaction과 cash 비소지 계약은 유지한다.
- E는 primary, F는 secondary, G는 droppable equipment release intent다. Character는 intent만 routing하고 domain mutation은 상태 owner가 수행한다.
- 모든 towel endpoint 이동은 source 감소와 destination 증가를 단일 native transaction으로 commit한다.
- Customer routine의 gameplay 상태 변경은 native C++ API를 통해 수행하고 StateTree/Blueprint asset에 domain mutation을 두지 않는다.

## Implementation Boundary

- 현재 Source에는 Core, Character, Camera, Interaction, Facility, Economy, Customer와 UI native class가 존재한다.
- `ST_CustomerRoutine`, Data Asset, Blueprint facility/key/customer/cash/UI와 Level 배치는 C++ 코드 리뷰 승인 후 Unreal 단계 target이다.
- 현재 Source는 customer-owned montage playback component, Bath action/approach snap과 두 native montage StateTree Task까지 구현한다. AnimNotify, Motion Warping, 신발·의상 전환은 포함하지 않는다.
- Cleaning/Towel Source, secondary/drop input, native prompt 확장과 customer towel StateTree Task/Condition은 구현되었다. InputAction/IMC, WBP hierarchy, Blueprint actor, facility 배치와 `ST_CustomerRoutine` asset 연결은 Unreal 후속 단계다.
- Towel Stack/Pile/Slot native presentation은 collision-free `TowelPresentationVisual` reflected 계약으로 구현되었다. 기존 `BP_Washer`/`BP_Dryer`의 inherited Pile authoring과 profile 지정은 Editor 후속 단계이며 신규 machine이나 drying-rack gameplay actor는 만들지 않는다.
