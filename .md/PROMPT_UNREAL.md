# Unreal 작업 프롬프트 — 설비 회수 진행률 런타임 재연결 확인

## 상태

Editor 에셋 변경은 불필요하다. C++에서 Q hold elapsed와 자동 commit 소유권을 Placement Component Tick으로 유지하고, 플레이어 `BeginPlay()`에서 회수 진행률 provider를 Interaction에 다시 연결한다.

## 변경 금지

- `IA_RecoverFacility` 또는 `IMC_FirstPerson`에 Hold Trigger를 추가하지 않는다.
- `WBP_InteractionPrompt`에 Percent binding, Tick/Event Graph 진행률 계산 또는 회수 실행 로직을 추가하지 않는다.
- 설비 Definition, Blueprint, Level Actor를 이번 작업 때문에 수정하거나 저장하지 않는다.

## 사전 조건

1. 현재 Editor에서 Live Coding을 실행하거나, Editor를 완전히 종료한 뒤 `BathhouseSimEditor Win64 Development`를 빌드하고 다시 실행한다.
2. `/Game/Input/Actions/IA_RecoverFacility`가 Boolean이고 action-level Trigger가 없는지 확인만 한다.
3. `/Game/Input/IMC_FirstPerson`의 Q → `IA_RecoverFacility` 매핑에 mapping-level Trigger가 없는지 확인만 한다.
4. `/Game/Bathhouse/UI/WBP_InteractionPrompt`의 `RecoveryProgressBar` 이름과 native parent를 유지한다.

## PIE 수용 기준

1. 회수 가능한 빈 설비를 응시하고 Q를 누른다.
2. `RecoveryProgressBar`가 `0 → 1`로 연속 증가해야 한다.
3. `RecoveryHoldSeconds`에 도달하면 Q를 계속 누르고 있어도 즉시 회수되어야 한다.
4. 자동 회수 뒤 Q를 놓아도 추가 item, 추가 결과 또는 중복 event가 없어야 한다.
5. 기준 시간 전에 Q를 놓으면 회수되지 않고 원본 설비가 유지되며 progress가 0으로 돌아가야 한다.
6. hold 도중 시선을 떼거나 범위를 벗어나면 회수되지 않아야 한다.
7. hold 도중 설비 조건이 바뀌면 회수되지 않아야 한다.
   - Bath: 물 또는 사용 중 slot
   - Washer/Dryer: contents 또는 processing
   - Locker: reserved/occupied slot
8. 다른 물건을 든 상태에서 자동 회수해도 기존 held object는 바뀌지 않아야 한다.

## 저장

이번 작업은 native-only이므로 검증만으로 Editor asset이 dirty가 되어서는 안 된다. `Save All`을 사용하지 않는다.

## 보고

- Editor 재시작 여부
- progress 연속 증가 여부
- release 전 자동 회수 여부
- 조기 release/시선 이탈/조건 변경 취소 결과
- 중복 item/event 또는 새 Error/Ensure 로그 여부
