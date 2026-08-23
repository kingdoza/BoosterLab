# Implementation Prompt — Mass-Independent Authored Impulses

## 목적

장비 드랍과 몽키스패너로 인한 customer knockdown에서 `Strength`로 authoring하는 impulse를 물리 body 질량과 무관한 즉시 속도 변화로 일관되게 적용한다.

기준 문서:

- `.md/AGENT_WORKFLOW.md`
- `.md/AGENT_IMPLEMENTATION.md`
- `.md/Architecture/InteractionSystem.md`
- `.md/Architecture/CombatSystem.md`
- `.md/Architecture/CustomerRecoverySystem.md`
- `.md/Architecture/CoreSystem.md`

## 현재 경로

- `UPlayerCarryComponent::TryReleaseHeldEquipment`는 `ThrowImpulseStrength`를 이미 `AddImpulse(..., bVelChange=true)`로 적용한다.
- `UCustomerKnockdownComponent::HandleHealthDepleted`만 공격의 `ImpulseStrength + VerticalImpulse`를 `bVelChange=false`로 적용해 PhysicsAsset 질량의 영향을 받는다.

## 구현 계약

대상:

- `Source/BathhouseSim/Private/Customer/CustomerKnockdownComponent.cpp`

요구사항:

1. configured root body에 호출하는 `AddImpulse`의 `bVelChange`를 `true`로 변경한다.
2. `CameraDirection * max(ImpulseStrength, 0) + UpVector * VerticalImpulse` 합성 방식은 유지한다.
3. `ImpulseStrength`, `VerticalImpulse`, `ThrowImpulseStrength`의 reflected 이름, 타입, category와 기본값을 변경하지 않는다.
4. root body validation, depleted guard, recovery timer와 soft interruption 순서를 변경하지 않는다.
5. 드랍 경로는 이미 계약을 만족하므로 동작 코드를 중복 수정하지 않는다.

## 금지 범위

- PhysicsAsset 질량, damping, constraint와 collision profile 변경
- Blueprint/Curve/StateTree/Level asset 수정 또는 resave
- `AddForce`, `LaunchCharacter`, 선형 속도 직접 지정으로의 대체
- 새로운 component, state, delegate, Tick 또는 module dependency 추가

## 검증

- Source 전체 `AddImpulse` 호출을 재검색해 authorable `Strength` 경로가 모두 `bVelChange=true`인지 확인한다.
- 기존 physical drop과 Combat/CustomerRecovery automation을 회귀 실행한다.
- `git diff --check`를 통과한다.
- 사용자 Editor가 열려 있어 UBT DLL 교체가 안전하지 않으면 종료하지 말고 build를 보류하거나 사용자 Editor의 Live Coding을 사용한다.

## Editor 영향

- Content 변경은 필요 없다.
- 기존 Blueprint 수치는 그대로 유지되며 의미만 질량 독립적인 속도 변화로 통일된다.
- 몽키스패너 swing rotation 추천값은 현재 `CV_MonkeyWrench_Rotation`의 `X=Roll, Y=Pitch, Z=Yaw` 계약에 맞춰 완료 보고에서 제공한다. 추천만 수행하고 사용자 지시 없이 Curve asset을 변경하지 않는다.
