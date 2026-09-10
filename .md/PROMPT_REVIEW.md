# 코드 리뷰 프롬프트 — 설비 회수 진행률 런타임 연결 보강

## 목표

기존 Tick 기반 Q 회수와 자동 완료 동작은 유지하면서, Blueprint 파생 플레이어 인스턴스에서 runtime-only supplemental intent 참조가 유실되어 `RecoveryProgress`가 0으로 남을 수 있는 경로를 제거한다.

## 변경 파일

- `Source/BathhouseSim/Private/Placement/PlayerFacilityPlacementComponent.cpp`
- `Source/BathhouseSim/Private/Character/FirstPersonCharacter.cpp`
- `Source/BathhouseSim/Public/Character/FirstPersonCharacter.h`
- `Source/BathhouseSim/Private/Tests/FacilityPlacementAutomationTests.cpp`
- `Source/BathhouseSim/Public/Interaction/PlayerInteractionComponent.h`
- `.md/Architecture/PlacementSystem.md`
- `.md/Architecture/InteractionSystem.md`

## 구현 요약

- Q `Started`는 기존처럼 회수 대상과 elapsed 세션을 고정한다.
- 활성 recovery session이 켜 둔 Placement Component Tick에서 대상/조건을 먼저 재검증한다.
- 유효한 Tick에서 `RecoveryElapsed`를 증가시키고 combined interaction query를 갱신한다.
- elapsed가 `RecoveryHoldSeconds`에 도달하면 같은 Tick에서 `CompleteRecoveryHold()`를 호출한다.
- commit 직전 기존 시선·대상·domain 조건을 다시 검사하고 기존 Actor 교체 transaction을 그대로 사용한다.
- Character의 Q `Triggered` 바인딩과 handler를 제거했다.
- Q `Completed`와 `Canceled`는 모두 `CancelRecoveryHold()`만 호출한다. 이미 자동 완료된 세션에는 no-op이다.
- `AFirstPersonCharacter::BeginPlay()`에서 실제 인스턴스의 `PlayerFacilityPlacement`를 Camera/Carry/Interaction에 다시 configure한다.
- 같은 `BeginPlay()`에서 `PlayerInteraction`의 supplemental intent source를 실제 `PlayerFacilityPlacement` 인스턴스로 다시 지정하고 query를 갱신한다.
- Q hold가 실제로 시작되는 순간에도 live Placement Component가 자신을 supplemental provider로 재등록해 Blueprint reinstance 이후의 유실을 방어한다.
- 실제 `AFirstPersonCharacter`를 game world에 생성해 두 runtime 참조가 연결되는지 자동화 검증을 추가했다.

## 호환성

- reflected `UPROPERTY`, `UFUNCTION`, component 이름과 enum ordinal 변경 없음. lifecycle override만 추가했다.
- Input Action/Mapping Context, Widget Blueprint 및 다른 Content 변경 없음.
- `UpdateRecoveryHold()`와 `CompleteRecoveryHold()` C++ API는 유지했다.
- 회수 조건, 낙하 위치, 기존 held item 불변 및 conversion transaction은 변경하지 않았다.

## 클래스 성장

- `FirstPersonCharacter.cpp`: 기존 컴포넌트 wiring 책임 안에서 `BeginPlay()` 약 16줄 증가.
- `FirstPersonCharacter.cpp/.h`: 반복 입력 handler 제거로 축소.
- 신규 클래스, 상태, delegate, reflected API 없음.
- 독립 책임 추가가 아니라 기존 session lifecycle 수정이므로 분리 불필요.

## 검증 결과

- `git diff --check`: 통과.
- UE 5.8 빌드는 열린 Editor가 빌드 mutex를 보유해 최신 수정분에 대해서는 완료하지 못했다. Editor 종료 후 full build가 필요하다.
- 이전 회수 Tick 수정분의 `BathhouseSim.Placement.ActorReplacementTransaction`은 성공했으며, 이번 runtime-character wiring assertion은 아직 실행 전이다.
- 자동화는 half-hold progress `0.5`, 조기 release 취소, 시선 이탈 취소, slot 상태 변경 취소, 기준 시간 자동 commit, release 후 중복 publication 없음 경로를 포함한다.

## 리뷰 중점

1. Component Tick 중 query refresh나 target destruction callback이 발생해도 invalid target을 재사용하지 않는지 확인한다.
2. 기준 시간 도달 Tick에서 transaction이 최대 한 번만 실행되는지 확인한다.
3. 조기 Q release가 원본 설비를 유지하고 progress를 0으로 되돌리는지 확인한다.
4. 자동 완료 뒤 Q release가 두 번째 결과/event/Actor를 만들지 않는지 확인한다.
5. 입력 반복 빈도와 무관하게 progress가 프레임 DeltaTime 기준으로 증가하는지 확인한다.
6. Blueprint 파생 캐릭터의 `BeginPlay()` 후 supplemental source와 Placement의 Interaction 참조가 실제 인스턴스를 가리키는지 확인한다.
7. 테스트에서 supplemental source를 의도적으로 비운 뒤 Q Started가 live provider를 복구하고 half-hold progress `0.5`를 전달하는지 확인한다.

## 미검증

- 실제 `WBP_InteractionPrompt`에서 진행률이 눈에 보이는지 PIE 확인이 필요하다.
- 현재 열려 있던 Editor는 빌드 전 DLL을 로드했을 수 있으므로 Editor 재시작 후 PIE해야 한다.
