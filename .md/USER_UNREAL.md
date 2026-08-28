# User Unreal Action — ST_CustomerRoutine Queue Task Migration

## 필요한 이유

Counter/drop/overflow asset authoring은 완료했다. 남은 필수 작업은 `/Game/Bathhouse/AI/ST_CustomerRoutine`의 두 queue movement region을 새 native task로 바꾸는 것이다.

현재 MCP에는 StateTree write 기능이 없고 UE 5.8 Python reflection도 node 생성과 Context binding 저장을 노출하지 않는다. 따라서 이 항목만 StateTree Editor에서 직접 편집해야 한다. `.uasset` binary patch는 하지 않는다.

## 변경 전 확인

StateTree를 열고 다음 parent task와 기존 transition은 그대로 둔다.

- `/Root/CheckIn`의 `Hold Customer Queue`
- `/Root/Checkout`의 `Hold Customer Queue`
- success/failure/retry/timeout/checkout transition 전부

새 state, transition, overflow state, queue index 변수, destination 변수 또는 Blueprint Task는 추가하지 않는다.

## CheckIn 변경

1. `/Root/CheckIn/QueueMove`를 선택한다.
2. 다음 두 Task만 삭제한다.
   - `Get Customer Queue Target (Deprecated)`
   - `Restartable Customer Move To`
3. Task 하나를 추가하고 `Move To Current Queue Assignment`를 선택한다.
4. Task binding을 다음처럼 설정한다.
   - `Customer` → 기존 Customer context의 Customer actor
   - `Session` → 기존 Customer Session context
   - `ExpectedLane` → `CheckIn`

## Checkout 변경

1. `/Root/Checkout/QueueMove`를 선택한다.
2. 다음 두 Task만 삭제한다.
   - `Get Customer Queue Target (Deprecated)`
   - `Restartable Customer Move To`
3. Task 하나를 추가하고 `Move To Current Queue Assignment`를 선택한다.
4. Task binding을 다음처럼 설정한다.
   - `Customer` → 기존 Customer context의 Customer actor
   - `Session` → 기존 Customer Session context
   - `ExpectedLane` → `Checkout`

## 완료 조건

1. StateTree를 Compile한다.
2. error/warning이 없는지 확인한다.
3. `ST_CustomerRoutine`만 저장한다.
4. 각 `QueueMove`에 `Move To Current Queue Assignment`가 정확히 하나씩 있고 legacy 두 Task가 없는지 다시 확인한다.
5. `Hold Customer Queue`와 기존 transition이 그대로인지 확인한다.

완료 후 Codex에 `StateTree 수정 완료`라고 알려주면 저장 재로드 검증과 남은 PIE 수용 검증을 이어서 진행한다.
