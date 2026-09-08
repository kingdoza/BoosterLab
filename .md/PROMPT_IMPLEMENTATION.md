# 구현 프롬프트 — 회수 프롬프트 경로와 낙하 위치 수정

## 목표

설비 회수 UI가 플레이어 배치 컴포넌트의 별도 passive trace에 의존하는 문제를 제거한다. 회수 성공 시 같은 Actor의 포장 물리 루트는 `PlacementFootprint` 중심 X/Y와 월드 바닥 Z에 공통 Z 오프셋을 더한 위치에서 물리 낙하를 시작한다.

## 고정 계약

- 포커스된 `IPlayerInteractable` Actor가 `ISupplementalInteractionIntentSource`를 통해 recovery 표시, 가능 여부, action text와 failure text를 일반 combined query에 합친다.
- `UPlayerFacilityPlacementComponent`의 player-global supplemental 경로는 placement row와 활성 recovery hold progress만 보충한다. passive recovery target을 별도로 trace해 UI를 만들지 않는다.
- Q 입력 lifecycle과 target 고정, 완료 직전 재검증은 기존 `UPlayerFacilityPlacementComponent`가 계속 소유한다.
- 공통 `RecoveryDropZOffsetCm`은 `UFacilityPlacementSettings`가 소유하며 기본값은 `100 cm`, 최소값은 `0 cm`다.
- 회수 예정 위치는 `PlacementFootprint world center X/Y`, `PlacementFootprint world bounds bottom Z + RecoveryDropZOffsetCm`다.
- package overlap은 현재 설치 위치가 아니라 위 예정 위치에서 검사한다.
- 예정 위치 이동 또는 mode 전환 실패 시 원래 transform/mode/domain registration으로 rollback한다.
- 자동 pickup과 impulse는 추가하지 않는다.

## 대상

- `Config/DefaultGame.ini`
- `Source/BathhouseSim/Public/Interaction/SupplementalInteractionIntentSource.h`
- `Source/BathhouseSim/Public/Interaction/PlayerInteractionComponent.h`
- `Source/BathhouseSim/Private/Interaction/PlayerInteractionComponent.cpp`
- `Source/BathhouseSim/Public/Placement/FacilityPlacementSettings.h`
- `Source/BathhouseSim/Public/Placement/FacilityPlacementComponent.h`
- `Source/BathhouseSim/Private/Placement/FacilityPlacementComponent.cpp`
- `Source/BathhouseSim/Private/Placement/PlayerFacilityPlacementValidation.cpp`
- `Source/BathhouseSim/Public/Facility/BathhouseFacilityActor.h`
- `Source/BathhouseSim/Private/Facility/BathhouseFacilityActor.cpp`
- `Source/BathhouseSim/Public/Towel/TowelProcessingMachineActor.h`
- `Source/BathhouseSim/Private/Towel/TowelProcessingMachineActor.cpp`
- `Source/BathhouseSim/Private/Tests/FacilityPlacementAutomationTests.cpp`

## 검증

- `git diff --check`
- UE 5.8 `BathhouseSimEditor Win64 Development` 빌드
- `BathhouseSim.Placement` automation 2개
- PIE에서 bath/washer/dryer를 포커스했을 때 가능/불가능 상태 모두 Q row가 보이는지 확인
- 성공 회수 시 package가 footprint 바닥 기준 공통 offset에서 떨어지는지 확인
