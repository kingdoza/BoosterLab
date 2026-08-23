# USER Unreal Work — Mass-Independent Impulse Live Coding

## 현재 상태

- Source 수정 완료
- Content 변경 없음
- 사용자 visible UE 5.8 Editor PID `28488`에는 아직 새 코드가 반영되지 않음

## 필요한 조작

현재 열린 Unreal Editor가 PIE 중이 아닌 상태에서 `Ctrl+Alt+F11`을 한 번 누른다.

Output Log에서 다음 중 하나를 확인한다.

- 성공: `LogLiveCoding`의 compile/patch success
- 변경 없음: `No changes`
- 실패: compile error 또는 patch failure

성공하면 에이전트에게 `라이브 코딩 완료`라고 알려 통합 검증을 재개한다.

Live Coding이 시작되지 않으면 에디터를 정상 종료한 뒤 UE 5.8로 프로젝트를 다시 연다. 사용자 에셋을 묻는 저장 창이 뜨면 이번 Source 작업과 무관한 Content를 임의 저장하지 않는다.

## 예상 동작

- `MeleeAttack.ImpulseStrength`와 `VerticalImpulse`가 customer root body 질량과 무관한 속도 변화로 적용된다.
- 장비 `ThrowImpulseStrength`는 기존과 같이 질량 독립적이다.
- Blueprint, Curve, PhysicsAsset와 StateTree를 수정할 필요가 없다.
