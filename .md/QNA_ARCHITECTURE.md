# QnA — Counter Queue Facing, Overflow And Physical Key Return

각 질문의 `답변:` 줄에 선택한 항목을 입력한다.

## Q53. 손님이 큐 포인트에 도착한 뒤 목표 회전을 어떻게 적용할까?

- A: 이동 완료 후 Editor 조정 가능한 회전 속도로 부드럽게 회전하고 허용 오차 안에 들어오면 완료
- B: 이동 완료 즉시 큐 포인트 회전으로 snap
- C: 이동 중 AI Focus로 계속 목표 방향을 바라보며 접근
- 권장안: A — 이동 중에는 진행 방향을 보고 도착 후 자연스럽게 정렬할 수 있다. StateTree 예상 작업량: 낮음. 기존 check-in/checkout MoveTo Task에 `Facing`과 회전 설정을 binding한다.
- 답변: A

## Q54. 큐 구성원이 쓰러졌다 일어났을 때 어느 큐 포인트로 복귀할까?

- A: 기립 시점의 최신 큐 순번에 해당하는 service point 또는 queue point
- B: 쓰러지기 직전에 할당됐던 포인트
- C: 현재 위치에서 가장 가까운 큐 포인트
- 권장안: A — 쓰러져 있는 동안 앞사람이 빠져도 최신 FIFO 순번과 실제 배치를 일치시킬 수 있다. C++ recovery gate에서 다시 resolve하므로 별도 StateTree 분기는 필요하지 않다.
- 답변: A

## Q55. 체크아웃 최대 대기자 수는 무엇을 기준으로 할까?

- A: 유효한 `CheckoutQueuePointReferences` 개수를 최대 대기자 수로 사용하고 service point의 한 명은 별도로 계산
- B: service point를 포함한 전체 인원 수를 최대 대기자 수로 사용
- C: queue point 개수와 별도 `MaxCheckoutWaitingCustomers` 값을 각각 authoring
- 권장안: A — 실제 배치 가능한 포인트와 제한값이 어긋나지 않는다. visible checkout lane의 총 수용량은 `1 + 유효 queue point 수`가 된다.
- 답변:A

## Q56. 체크아웃 대기열을 초과한 손님의 내부 배회 범위를 어떻게 지정할까?

- A: 하나 이상의 전용 NavMesh 기반 배회 Volume을 Editor에서 배치하고 그 안의 reachable random 위치를 사용
- B: 여러 개의 고정 배회 Point를 Editor에서 배치하고 무작위 선택
- C: Level의 전체 NavMesh에서 무작위 위치 선택
- 권장안: A — 목욕탕 외부나 작업 불가능한 공간으로 나가지 않으면서 반복 동선이 고정되는 현상을 줄일 수 있다.
- 답변:A

## Q57. 단일 카운터 키 드랍 지점에 기존 반환 키가 놓여 있으면 새 키를 어떻게 내려놓을까?

- A: 드랍 지점 주변의 작은 authorable XY 범위에서 충돌하지 않는 위치를 제한 횟수 탐색하고, 실패하면 기존 checkout Task에서 대기 후 재시도
- B: 정확한 드랍 지점만 사용하고 막혀 있으면 플레이어가 기존 키를 치울 때까지 재시도
- C: 기존 키와 겹쳐도 정확한 드랍 지점에서 물리를 강제로 활성화
- 권장안: A — 단일 지점이라는 동선은 유지하면서 초기 물리 겹침과 영구 checkout 정지를 줄일 수 있다. StateTree 추가 작업은 없고 기존 checkout Task의 재시도 흐름을 유지한다.
- 답변: A

## Q58. 큐 이동·도착 회전·체크아웃 초과 배회를 StateTree에 어떻게 연결할까?

- A: native `Move To Current Queue Assignment` Task 하나가 최신 큐 배치 resolve, 이동, 도착 회전, overflow 배회와 promotion cancel을 처리
- B: StateTree에 visible queue와 overflow wander용 상태·조건·반복 transition을 각각 구성
- C: Customer Session Component가 StateTree 밖에서 큐 이동과 배회를 계속 직접 제어
- 권장안: A — queue domain은 Counter/Session에 유지하면서 StateTree 분기를 늘리지 않고 knockdown restart와 같은 native operation token을 재사용할 수 있다. StateTree 예상 작업량: 낮음. check-in/checkout의 기존 Queue Target + MoveTo 조합을 새 Task로 교체하고 `Session`, `Lane`, 회전/배회 설정을 binding한다.
- 답변:A
