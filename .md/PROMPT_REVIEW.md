# Code Review Prompt — Mass-Independent Authored Impulses

## Review Objective

장비 드랍과 customer knockdown에서 Blueprint가 `Strength`로 authoring하는 impulse가 물리 body 질량과 무관한 velocity change로 일관되게 적용되는지 검토한다.

기준:

- `.md/PROMPT_IMPLEMENTATION.md`
- `.md/Architecture/InteractionSystem.md`
- `.md/Architecture/CombatSystem.md`
- `.md/Architecture/CustomerRecoverySystem.md`

## Implemented Change

- `UCustomerKnockdownComponent::HandleHealthDepleted`의 configured root body `AddImpulse` 호출을 `bVelChange=false`에서 `true`로 변경했다.
- 기존 합성식 `CameraDirection * max(ImpulseStrength, 0) + UpVector * VerticalImpulse`는 유지했다.
- 공통 장비 드랍 `UPlayerCarryComponent`는 이미 `ThrowImpulseStrength`를 `bVelChange=true`로 적용하고 있어 수정하지 않았다.

## Changed Files

- `Source/BathhouseSim/Private/Customer/CustomerKnockdownComponent.cpp`
- `.md/Architecture/InteractionSystem.md`
- `.md/Architecture/CombatSystem.md`
- `.md/Architecture/CustomerRecoverySystem.md`

## Responsibility And API Impact

- 기존 Customer Recovery physics execution owner 내부의 한 인자 변경이다.
- 새 상태, component, lifecycle, Tick, delegate와 dependency를 추가하지 않았다.
- reflected property 이름·타입·category·기본값을 변경하지 않았다.
- Blueprint migration, Core Redirect, Config와 Content 변경은 없다.

## Static Validation

- Source 전체 `AddImpulse` 재검색 결과 authorable Strength 경로는 2개다.
- customer knockdown attack impulse: `bVelChange=true`.
- common equipment drop impulse: `bVelChange=true`.
- `git diff --check`: whitespace error 없음. 기존 line-ending warning만 존재.

## Runtime Validation Status

- 사용자 visible UE 5.8 Editor PID `28488`이 실행 중이므로 DLL 잠금이 있는 일반 UBT build는 수행하지 않았다.
- 해당 사용자 Editor에 MCP로 연결해 PIE가 아님을 확인했다.
- MCP toolset에는 Live Coding compile 명령이 없고 Python Remote Execution node도 발견되지 않았다.
- Live Coding 단축키 전달을 두 번 시도했으나 새 `LogLiveCoding` compile entry가 없어 적용 성공으로 간주하지 않는다.
- Editor를 종료하거나 별도 background Editor를 실행하지 않았다.

## Review Focus

- `AddImpulse` 마지막 인자 `true`가 UE의 mass-independent velocity-change 의미와 일치하는가.
- negative `VerticalImpulse` 허용과 horizontal strength clamp가 기존 계약대로 유지되는가.
- repeated depleted guard, root-body validation과 recovery 순서가 변하지 않았는가.

## Expected Conclusion

한 줄 내부 동작 변경으로 API/asset 위험은 없다. 정적 코드 리뷰 후 코드 단계 승인이 가능하며, 실행 중 Editor에는 사용자가 Live Coding을 직접 실행하거나 다음 Editor 재시작 뒤 반영 여부를 검증한다.
