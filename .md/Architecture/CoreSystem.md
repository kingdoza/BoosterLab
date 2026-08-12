# Core System

## Responsibilities

Core System은 Source 루트와 모듈 공통 규칙을 문서화한다.

- `BathhouseSim` 런타임 모듈의 빌드 의존성 관리
- Unreal primary game module 등록
- Source/Content/Config 경계 규칙
- Core Redirect 검토 기준
- 시스템 문서 작성 기준

## Source Scope

```text
Source/BathhouseSim/
  BathhouseSim.Build.cs
  BathhouseSim.cpp
  BathhouseSim.h
```

`Core`는 현재 별도 C++ 폴더가 아니라 문서상 공통 경계다.

## Module Rules

`BathhouseSim.Build.cs`는 현재 1인칭 조작과 customer gameplay loop에 실제 사용되는 런타임 의존성을 유지한다.

- `Core`
- `CoreUObject`
- `Engine`
- `InputCore`
- `EnhancedInput`

UE 5.8 customer loop 구현은 실제 include/use site와 함께 다음 runtime 의존성을 사용한다.

- `AIModule`
- `GameplayTasks`
- `NavigationSystem`
- `GameplayTags`
- `StateTreeModule`
- `GameplayStateTreeModule`
- `UMG`

`BathhouseSim.uproject`에는 UE 5.8 `StateTree`, `GameplayStateTree` plugin이 활성화되어 있다. `StateTreeEditorModule`, `Slate`, `SlateCore`는 runtime module에 추가하지 않는다.

## Runtime Entry

`BathhouseSim.cpp`는 `IMPLEMENT_PRIMARY_GAME_MODULE`로 `BathhouseSim` 모듈을 등록한다.

현재 Core System은 게임플레이 상태를 소유하지 않는다. 플레이어 입력, 카메라, 이동, sprint 상태는 Character System이 소유한다.

## Source Boundaries

- `Source/BathhouseSim/Public`에는 외부 모듈 또는 Blueprint native parent가 참조할 수 있는 public header를 둔다.
- `Source/BathhouseSim/Private`에는 cpp 구현과 private helper를 둔다.
- include 경로는 모듈 include root 기준으로 작성한다. 예: `#include "Character/FirstPersonCharacter.h"`
- `#include "Public/..."` 형태는 사용하지 않는다.

## Content And Config Boundaries

- `Content/`는 Blueprint와 asset 데이터이며, 명시 지시 없이 수정하거나 resave하지 않는다.
- `Config/DefaultEngine.ini`는 GameMode/Pawn/Controller 연결 또는 Core Redirect가 필요할 때만 수정한다.
- `Intermediate/`, `Binaries/`, `Saved/`, `DerivedDataCache/`는 문서 기준이나 설계 판단의 출처로 사용하지 않는다.

## Core Redirect Policy

UCLASS/USTRUCT/UENUM rename 또는 삭제가 Blueprint/native asset 참조를 깨뜨릴 수 있으면 `Config/DefaultEngine.ini`의 `[CoreRedirects]`를 검토한다.

Core System은 고정된 native class inventory를 유지하지 않는다. 구현이 변해도 이 문서는 모듈, Source 경계, redirect 판단 기준만 제공한다.

현재 native type과 Blueprint/API 계약은 각 `.md/Architecture/*System.md`의 `Key Classes`, `Blueprint/API Contracts`를 기준으로 확인한다. 외부 asset 또는 이전 이식 단계의 Blueprint가 예전 type 이름을 참조한다면, Editor migration 전 Core Redirect 설계가 필요하다.

## System Documents

현재 구현 시스템 문서는 실제 Source 디렉터리에 맞춰 작성하고 확정 target 문서는 implementation status를 명시한다.

- `CharacterSystem.md`: `Source/BathhouseSim/Public/Character`, `Source/BathhouseSim/Private/Character`
- `CameraSystem.md`: `Source/BathhouseSim/Public/Camera`, `Source/BathhouseSim/Private/Camera`
- `InteractionSystem.md`: player trace, primary/secondary intent와 single physical carry 경계
- `FacilitySystem.md`: facility slot과 counter queue 경계
- `EconomySystem.md`: wallet과 cash claim 경계
- `CustomerSystem.md`: StateTree routine과 customer session 경계
- `UISystem.md`: native Widget/Widget Blueprint 경계
- `CleaningSystem.md`: water stain spawn과 wet mop cleaning 경계
- `TowelSystem.md`: towel inventory, atomic transfer와 processing 경계
- `TowelPresentationSystem.md`: Towel 하위 Stack/Pile/Slot world presentation 경계
- `CoreSystem.md`: Source 루트, 모듈/문서/redirect 공통 규칙

`Source/BathhouseSim/Private/Tests`는 system이 아니라 focused native automation test 경로다.

새 Source 하위 디렉터리를 추가하면 같은 이름의 `*System.md`를 추가하고 책임, 핵심 클래스, runtime flow, 의존성, Blueprint/API 계약, 수동 검토 지점을 문서화한다.

Cleaning/Towel target은 현재 runtime module dependency 안에서 구현한다. 새 dependency는 실제 include/use site가 확인되지 않는 한 추가하지 않는다.

## Manual Review Points

- 새 모듈 의존성을 추가할 때 실제 include/use site가 있는지 확인한다.
- StateTree/GameplayStateTree plugin과 runtime module을 UE 5.8 기준으로 확인한다.
- UCLASS/USTRUCT/UENUM rename/delete 시 Blueprint 참조와 Core Redirect 필요 여부를 확인한다.
- Config 변경은 실제 gameplay 연결 또는 migration 목적이 분명할 때만 수행한다.
- 문서가 Source 구조와 어긋나면 Source 재대조 후 시스템 문서를 갱신한다.
