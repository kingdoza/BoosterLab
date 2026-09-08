# Unreal 작업 프롬프트 — 회수 프롬프트와 낙하 위치 검증

## 상태

에셋 수정은 필요 없다. 새 native 코드와 config property를 읽도록 Editor를 새로 시작한 뒤 Compile/PIE 검증만 수행한다. Widget Blueprint에 recovery 로직을 추가하지 않는다.

## 설정 확인

Project Settings > Game > Facility Placement에서 다음 값을 확인한다.

- `Recovery Drop ZOffset Cm = 100.0`

필요하면 프로젝트 전체 설비에 공통으로 적용할 높이만 이 값에서 조정한다. 개별 설비 Blueprint에 별도 recovery offset을 만들지 않는다.

## PIE 검증

1. 빈 목욕탕, 빈 세탁기, 빈 건조기를 각각 포커스한다.
2. 일반 상호작용 prompt와 같은 HUD에서 `Q 설비 회수` row가 즉시 보이는지 확인한다.
3. 목욕탕 물이 있거나 slot이 사용 중인 상태, 기계가 비어 있지 않거나 processing 중인 상태에서도 Q row와 정확한 실패 사유가 보이는지 확인한다.
4. 회수 가능한 설비에서 Q를 누르고 유지해 progress가 증가하는지 확인한다.
5. 회수 성공 시 package가 `PlacementFootprint` 중심 X/Y와 footprint 월드 바닥 Z + `100 cm` 위치에서 시작해 중력으로 떨어지는지 확인한다.
6. 그 예정 위치에 blocking object를 놓으면 Q row는 유지되면서 `포장 설비가 다른 물체와 겹쳐 회수할 수 없습니다.`가 표시되고 회수가 실패하는지 확인한다.

## 수용 기준

- recovery row는 성공 가능 여부와 관계없이 포커스 중 표시된다.
- Q hold 중 progress가 표시되고 취소 시 0으로 돌아간다.
- 성공 회수는 자동 pickup/impulse 없이 같은 Actor를 packaged physics로 전환한다.
- 충돌 실패 시 설비 transform, placed mode와 domain 등록이 유지된다.
