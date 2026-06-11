# BoosterLab Architecture Map

## 문서 기준

- 기준일: 2026-06-11(KST) Source 재대조 기준
- 상태: 범용 1인칭 조작, sprint, 이동/착지 기반 camera shake, camera pitch limit 기본 class 반영 후 현재 Source 구조 기준
- 정본 문서: `.md/0_ARCHITECTURE.md`와 `.md/Architecture/*.md`
- legacy 문서: 현재 별도 legacy architecture 문서는 없다.

## 분석 범위

- 주 분석 범위:
  - `Source/BoosterLab/Public`
  - `Source/BoosterLab/Private`
- 현재 대상 C++ 하위 시스템:
  - Core
  - Character
  - Camera
- `Content`는 Blueprint 참조 검증 범위로만 다룬다. C++ 시스템 책임의 정본은 Source 하위 문서에 둔다.
- `Config/DefaultEngine.ini`는 GameMode/Pawn/Controller 연결 또는 Core Redirect가 필요한 rename 호환 경로로만 문서화한다.

## 시스템 문서

- [CharacterSystem.md](Architecture/CharacterSystem.md): 1인칭 입력, 컨트롤러 입력 매핑, 이동, 점프, sprint, 캐릭터 조립
- [CameraSystem.md](Architecture/CameraSystem.md): 이동/착지 기반 카메라 셰이크, camera manager 기반 pitch limit
- [CoreSystem.md](Architecture/CoreSystem.md): 공통 문서 규칙, 모듈 경계, Source/Content/Config 경계, Core Redirect

## Source 구조

```text
Source/BoosterLab/
  Public/
    Character/
    Camera/
  Private/
    Character/
    Camera/
```

- `Core`는 소스 폴더가 아니라 문서상 공통 경계다.
- 시스템 하위 폴더명은 include 경로의 1차 네임스페이스 역할을 한다.
- 현재 파일 분포는 하위 시스템 문서를 기준으로 확인한다.
  - Character: 1인칭 캐릭터, 플레이어 컨트롤러, movement/sprint 책임
  - Camera: 이동 상태와 착지 상태 기반 camera shake, camera manager 기반 pitch limit 책임
  - Core: 모듈/redirect/문서 경계 책임

## 시스템 간 책임 흐름

- Character는 로컬 플레이어 입력과 1인칭 캐릭터 조립 지점이다.
- Character는 Enhanced Input action을 이동, 시점, 점프, sprint 의도로 변환한다.
- Character는 sprint 상태와 속도 변경을 movement 책임으로 위임한다.
- Character는 1인칭 카메라와 camera shake component를 조립하지만, camera shake 상태 계산은 Camera가 담당한다.
- Camera는 owner의 이동 상태, sprint 상태, falling/landing 상태를 읽고 camera shake 재생/중단을 결정한다.
- Camera는 player camera manager를 통해 상하 시야각 제한 기본값을 제공하고, Blueprint 파생 class에서 값을 조정할 수 있게 한다.
- Camera는 비로컬 플레이어에서 Tick interval 조정과 shake 중단으로 비용을 줄인다.
- Core는 런타임 gameplay 상태를 소유하지 않고 모듈 의존성, Source 경계, Content/Config 정책, Core Redirect 기준을 문서화한다.
- 현재 문서화된 Core, Character, Camera 외 도메인 기능은 BoosterLab의 Source 책임이 아니다.

## 주요 의존 방향

- Character -> Camera
- Character -> EnhancedInput
- Character -> Engine Character/Movement
- Camera -> Character
- Camera -> Engine Camera/CameraShake
- Camera -> Engine PlayerCameraManager
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

## Update 2026-06-11 (BoosterLab Source Baseline)

- Source 기준 하위 시스템을 Core, Character, Camera로 정리했다.
- Character는 1인칭 입력, 이동, 점프, sprint, 플레이어 조립 책임을 가진다.
- Camera는 이동 상태와 착지 상태 기반 camera shake와 camera manager 기반 pitch limit 책임을 가진다.
- Core는 모듈 경계, Source/Content/Config 경계, Core Redirect 정책을 가진다.
- 현재 문서화된 Core, Character, Camera 외 도메인 기능은 현재 Source 범위에서 제외했다.
