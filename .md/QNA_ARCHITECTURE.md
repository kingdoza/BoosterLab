# QnA — 손님 루틴과 단일 소지 상호작용 (답변 완료)

변경 전 답변 보존: Q15=`B(50%)`, Q16=`B(3회)`는 종료 조건에서 제외하고, Q17=`B`는 빈 탕 재선택 규칙으로 유지한다.

## Q1. 플레이어가 상호작용 대상을 찾는 방식은?

- A: 카메라 중앙 line trace
- B: 일정 거리 안의 가장 가까운 대상
- C: 마우스 cursor 선택
- 권장안: A — 현재 1인칭 카메라와 가장 단순하게 결합된다.
- 답변: A

## Q2. 플레이어가 번호 키를 가져오는 방식은?

- A: 번호별 key hook에서 원하는 키를 직접 가져옴
- B: 하나의 key rack이 사용 가능한 키를 자동 지급
- C: 대기 NPC가 자기 번호 키를 player 손에 생성
- 권장안: A — 번호 선택과 물리적 반환 위치가 명확하다.
- 답변: A

## Q3. 키 번호에 연결된 시설이 누락됐을 때 처리 방식은?

- A: 해당 번호 키를 사용 불가 처리
- B: 존재하는 시설만 사용하고 루틴 계속
- C: NPC가 다른 번호 시설을 대신 사용
- 권장안: A — 잘못된 배치가 NPC 진행 오류로 이어지는 것을 막는다.
- 답변: key/shoe locker/clothes locker 중 하나라도 누락되면 해당 번호를 사용 불가 처리

## Q4. 플레이어가 들고 있는 키를 내려놓는 방식은?

- A: 대응하는 key rack에만 반환
- B: 바닥에 drop 가능
- C: 임시 선반에 보관 가능
- 권장안: A — 한 손 규칙과 키 소유권을 가장 단순하게 유지한다.
- 답변: key는 rack

## Q5. Checkout에서 NPC가 건네는 현금을 어떻게 처리할까?

- A: 현금 상호작용 즉시 현금 object를 제거하고 player 돈 증가
- B: 현금을 손에 들고 cash register에 넣어야 돈 증가
- C: 현금 object 없이 NPC 상호작용 즉시 player 돈 증가
- 권장안: A — 현금 object 연출을 유지하면서 한 손 item에서는 제외할 수 있다.
- 답변: NPC는 cash만 건냄. 현금을 들수있는거는 취소하고 현금 상호작용하면 현금 오브젝트 사라짐과 동시에 돈 획득

## Q6. Checkout에서 NPC가 키를 반환하는 방식은?

- A: 빈손 player에게 키를 직접 전달
- B: counter 위에 key world object로 내려놓음
- C: 대응하는 key hook으로 자동 반환
- 권장안: B — player가 물리적으로 회수해 rack에 돌려놓는 흐름을 유지한다.
- 답변: NPC는 key만 counter 위에 world object로 내려놓음

## Q7. Checkout NPC는 언제 퇴장할까?

- A: key object 배치와 cash 지급이 모두 끝난 즉시 퇴장
- B: player가 key object를 집을 때까지 대기
- C: player가 key를 rack에 반환할 때까지 대기
- 권장안: A — counter queue가 player 정리 작업 때문에 막히지 않는다.
- 답변: A

## Q8. Check-in과 checkout의 counter 대기열 구성은?

- A: 하나의 FIFO queue와 service point를 공유
- B: check-in과 checkout queue를 분리
- C: 첫 구현은 NPC 한 명만 허용하고 queue를 만들지 않음
- 권장안: A — 한 카운터 운영 규칙을 한 owner가 관리할 수 있다.
- 답변: B

## Q9. Check-in key timeout은 언제 시작할까?

- A: NPC가 counter queue에 들어온 순간
- B: NPC가 맨 앞 service point에 도착한 순간
- 권장안: B — 앞사람 대기 시간 때문에 퇴장하는 문제를 막는다.
- 답변: B

## Q10. Check-in key timeout 기본값은?

- 선택지: A=30초 / B=60초 / C=직접 지정
- 권장안: B — 첫 플레이 테스트에서 관찰하기 여유롭고 이후 조정 가능하다.
- 답변: B

## Q11. NPC 루틴의 주 실행 방식은?

- A: C++ phase state machine + AIController + Timer
- B: Unreal StateTree
- C: Behavior Tree/Blackboard
- 권장안: A — 현재 선형 루틴의 상태와 cleanup 경계가 가장 명확하다.
- 답변: B

## Q12. NPC 행동 시간과 랜덤 설정의 owner는?

- A: NPC Blueprint의 config struct
- B: 공용 Data Asset
- C: 각 시설 Blueprint
- 권장안: B — NPC 유형별 설정을 재사용하고 시설 상태와 분리할 수 있다.
- 답변: B

## Q13. 시설의 동시 이용 slot 구조는?

- A: facility actor 하나당 use slot 하나
- B: facility actor 하나가 여러 use slot을 소유
- C: Unreal Smart Object slot 사용
- 권장안: B — 탕과 샤워의 다중 수용을 지원하면서 별도 plugin 계약을 피한다.
- 답변: B

## Q14. NPC가 사용할 수 있는 시설이 모두 점유 중이면?

- A: 빈 시설이 생길 때까지 대기
- B: 일정 시간 뒤 해당 행동을 건너뜀
- C: 즉시 퇴장 루틴으로 전환
- 권장안: A — 정상 루틴을 보존하고 reservation 해제 event로 재시도할 수 있다.
- 답변: A

## Q15. 고정 목욕 체류시간의 기본값은?

- 권장안: 10분 — 여러 탕 이동을 관찰할 시간을 확보하고 Data Asset에서 조정한다.
- 답변: 1분

## Q16. 목욕 체류시간 timer는 언제 시작할까?

- A: pre-shower가 끝난 순간
- B: 첫 번째 탕 사용을 시작한 순간
- 권장안: A — 탕 선택과 이동 대기도 전체 목욕 체류시간에 포함할 수 있다.
- 답변: A

## Q17. 체류시간이 탕 이용 도중 끝나면?

- A: 현재 탕 행동을 즉시 종료하고 main shower로 이동
- B: 현재 탕의 이번 체류를 마친 뒤 main shower로 이동
- 권장안: B — animation과 facility reservation을 정상 종료하기 쉽다.
- 답변: A

## Q18. 한 탕에서 다음 탕으로 이동하는 간격은?

- A: 탕마다 같은 고정 체류시간 사용
- B: 설정된 최소·최대 범위에서 매번 random 결정
- 권장안: B — 전체 체류시간은 고정하면서 NPC별 이동 패턴은 반복적으로 보이지 않는다.
- 답변: B

## Q19. 첫 구현의 이용료 정책은?

- A: 모든 NPC가 동일한 고정 이용료, 거스름돈 없음
- B: NPC 유형별 이용료, 거스름돈 없음
- C: 지폐·동전과 거스름돈까지 계산
- 권장안: A — cash 상호작용과 NPC 루틴 검증에 집중할 수 있다.
- 답변: A

## Q20. 기본 이용료 표기값은?

- 선택지: A=10,000원 / B=15,000원 / C=직접 지정
- 권장안: A — 임시 기본값으로 두고 Blueprint에서 조정하기 쉽다.
- 답변: A

## Q21. 첫 구현에 상호작용 prompt UI를 포함할까?

- A: 대상명·행동명·실행 불가 이유를 표시
- B: 행동명만 표시
- C: UI 없이 상호작용만 구현
- 권장안: A — 키 상태와 NPC 단계가 맞지 않는 이유를 player가 확인할 수 있다.
- 답변: A

## Q22. 플레이어가 들고 있는 키 번호를 어떻게 표시할까?

- A: first-person 3D key에만 표시
- B: HUD text에만 표시
- C: 3D key와 HUD text에 모두 표시
- 권장안: A — inventory/hotbar 없는 물리적 소지 방식을 유지한다.
- 답변: A

## Q23. 기존 UI·mesh·animation asset이 있는가?

- A: 없음
- B: 있음 — 답변에 Content 경로 작성
- 권장안: A — 없다면 C++ 계약만 만들고 실제 연결은 Unreal 단계로 넘긴다.
- 답변: A

## Q24. Key를 받은 NPC의 navigation이 반복 실패하면?

- A: 계속 재시도
- B: counter로 돌아가 key를 반환하고 무료 퇴장
- C: 자원을 복구한 뒤 exit로 이동하고 개발 오류 기록
- 권장안: C — gameplay 규칙과 기술 오류를 분리하고 점유 누수를 막는다.
- 답변: C

## Q25. Check-in 외 gameplay timeout을 둘까?

- A: 두지 않음
- B: 시설 대기에만 적용
- C: 모든 routine 단계에 적용
- 권장안: A — 사용자가 명시한 check-in timeout만 gameplay 규칙으로 유지한다.
- 답변: A

## Q26. 손님 생성 방식은?

- A: level에 미리 배치된 NPC가 루틴 시작
- B: spawner actor가 간격과 최대 인원을 관리
- C: 외부 game mode가 필요할 때 직접 생성
- 권장안: B — 입장 간격과 동시 인원을 독립적으로 조정할 수 있다.
- 답변: B
