# Facility System

## Implementation Status

이 문서는 현재 구현된 facility slot, 번호 시설과 분리된 counter queue 경계를 정의한다.

## Source Scope

```text
Source/BathhouseSim/Public/Facility/
  BathhouseFacilityTypes.h
  BathhouseFacilitySlotComponent.h
  BathhouseFacilityActor.h
  BathhouseFacilitySubsystem.h
  BathhouseCounterActor.h

Source/BathhouseSim/Private/Facility/
  BathhouseFacilitySlotComponent.cpp
  BathhouseFacilityActor.cpp
  BathhouseFacilitySubsystem.cpp
  BathhouseCounterActor.cpp

Source/BathhouseSim/Private/Tests/
  BathhouseDomainTests.cpp  # slot/queue/reference와 customer Bath snap cleanup coverage
```

## Responsibilities

- 배치된 bathhouse facility와 다중 use slot 등록
- slot reservation, occupancy, release와 대기 notification
- 번호 기반 shoe locker/clothes locker lookup과 유효성 검사
- check-in/checkout의 독립 FIFO queue와 service 순서
- checkout key world object를 보관하는 counter return slot

Facility는 customer phase, key actor state, player interaction과 money를 소유하지 않는다.

## Facility Types

`EBathhouseFacilityType`:

- `ShoeLocker`
- `ClothesLocker`
- `Shower`
- `Bath`
- `DryingSpot`
- `TowelBasket`
- `Exit`

`ShoeLocker`와 `ClothesLocker`는 `FacilityNumber`가 필수다. 나머지는 `INDEX_NONE`을 사용한다.

## `UBathhouseFacilitySlotComponent`

시설 actor 하나에 여러 개를 배치할 수 있는 scene component다.

- component transform은 NPC의 logical `ActionPoint`다.
- `ApproachOffset`은 component-local 위치 offset으로 approach transform에만 적용한다.
- `FacingRotation`은 action/approach transform에 정확히 한 번 합성된다.
- Bath slot의 `ApproachPoint`는 NavMesh 위의 보행 도착점이고 `ActionPoint`는 NavMesh 밖일 수 있는 실제 입욕 위치다.
- Facility는 두 transform만 제공하며 customer 이동, snap과 movement mode를 직접 변경하지 않는다.
- state는 `Available`, `Reserved`, `Occupied`다.
- reservation/occupant는 generic `AActor` identity로 저장한다.
- `TryReserve`, `BeginUse`, `EndUse`, `Release`는 같은 actor만 허용한다.
- state 변경 시 availability delegate를 방송한다.

Facility는 Montage, AnimNotify, Motion Warping과 prop socket 계약을 소유하지 않는다.

## `ABathhouseFacilityActor`

- 시설 Blueprint의 native base다.
- `FacilityType`, `FacilityNumber`와 `SelectionWeight`를 소유한다.
- 소유한 모든 `UBathhouseFacilitySlotComponent`를 등록·검증한다.
- 시설의 domain state는 slot component가 소유하고 Blueprint는 표현 event만 받는다.
- actor destruction 시 모든 reservation을 해제하고 subsystem에서 등록 해제한다.

Blueprint event:

- `OnSlotReservationChanged`
- `OnSlotUseStarted`
- `OnSlotUseEnded`

이번 단계에서 event는 호출하지만 실제 행동 animation이나 mesh 전환을 요구하지 않는다.

## `UBathhouseFacilitySubsystem`

`UWorldSubsystem`으로 동작한다.

- facility BeginPlay/EndPlay 등록·해제
- facility type별 후보 조회
- 번호별 shoe locker/clothes locker 조회
- key number가 두 numbered facility를 정확히 하나씩 가지는지 검증
- 예약 가능한 slot 중 random 선택
- Bath 선택 시 다른 빈 탕이 있으면 직전 bath actor를 제외
- 모든 slot이 점유 중이면 availability delegate 기반 재시도 지원

Customer와 key hook이 반복적인 world actor scan을 하지 않게 한다.

## Reservation Flow

1. Customer StateTree Task가 subsystem에 facility type/number 조건을 전달한다.
2. Subsystem이 등록되고 enabled된 actor와 available slot 후보를 찾는다.
3. 후보가 있으면 weight random 선택 후 `TryReserve`한다.
4. 일반 facility는 authored navigation target으로 이동하고 Bath는 NavMesh 위 approach point로 이동한다.
5. Bath는 approach 도착 후 customer가 action point로 snap하고, 그 뒤 `BeginUse`로 occupancy를 확정한다.
6. Bath 사용 종료 시 customer가 action point에서 approach point로 복귀한 뒤 `EndUse`, `Release`한다.
7. 일반 facility activity 종료 또는 중단도 `EndUse`, `Release` 순서로 정리한다.
8. 이동 실패, StateTree exit와 customer EndPlay는 customer cleanup을 통해 snap 복귀와 release를 대칭적으로 보장한다.

후보가 없으면 customer는 어떤 slot도 보유하지 않은 채 availability event를 기다린다.

## `ABathhouseCounterActor`

하나의 counter actor가 서로 독립된 두 lane을 소유한다.

- Check-in FIFO queue와 service point
- Checkout FIFO queue와 service point

각 lane은 배치된 Counter 인스턴스 소유 component를 가리키는 순서형 `FComponentReference` queue point 목록을 가지며 front customer만 service interaction을 활성화한다. Queue entry는 customer actor와 enqueue sequence만 저장하고 Customer concrete class에 의존하지 않는다.

`CheckInQueuePointReferences`, `CheckoutQueuePointReferences`, `ReturnedKeyPointReferences`는 `EditInstanceOnly` component picker 계약이다. Counter는 BeginPlay에 reference를 resolve해 private transient 배열로 분리하며 null/unresolved, 비-SceneComponent, 다른 Actor 소유와 같은 역할·역할 간 중복을 오류로 기록하고 runtime에서 제외한다. 이름·type·actor scan으로 fallback하지 않으며 유효한 reference의 원래 배열 순서를 유지한다.

Check-in timeout은 queue 진입이 아니라 front customer가 check-in service point에 도착했을 때 시작한다.

## Checkout Key Return Slots

- Counter는 `ReturnedKeyPointReferences`에서 검증·resolve된 여러 key return scene component를 가진다.
- Checkout customer는 available return slot을 예약하고 key actor를 해당 transform에 world object로 배치한다.
- NPC가 떠난 뒤에도 return slot은 key actor가 놓여 있는 동안 occupied다.
- Player가 key를 집으면 slot이 해제된다.
- 모든 return slot이 차 있으면 checkout front customer는 빈 slot이 생길 때까지 기다린다.

이 구조는 player가 key를 늦게 회수해도 여러 key actor가 같은 위치에 겹치는 것을 막는다.

## Queue Flow

1. Customer가 목적에 맞는 lane에 enqueue한다.
2. Counter가 queue index별 authored transform을 전달한다.
3. Customer는 transform 변경 notification을 받을 때 새 위치로 이동한다.
4. Front 도착 후 check-in key 대기 또는 checkout 처리를 시작한다.
5. 완료·timeout·abort 시 dequeue한다.
6. Counter가 나머지 customer에게 새 queue 위치를 통지한다.

Counter는 customer routine phase를 직접 변경하지 않는다.

## Blueprint/API Contracts

Editor authoring 값:

- facility type/number와 selection weight
- 시설별 slot component transform과 facing
- Bath slot별 NavMesh 위 `ApproachOffset`과 NavMesh 밖일 수 있는 정확한 action transform
- check-in/checkout service point
- 두 lane의 `FComponentReference` queue point 목록. 배열 순서가 queue 순서다.
- checkout `FComponentReference` key return slot 목록. 배열 순서가 return slot 순서다.

Blueprint 조회·표현 API:

- slot state와 current occupant 조회
- `ABathhouseCounterActor::OnQueueChanged`
- `ABathhouseCounterActor::OnReturnedKeySlotsChanged`

## Dependencies

- Facility -> Engine Actor/SceneComponent/WorldSubsystem
- Customer -> Facility
- Interaction -> Facility의 numbered facility validation
- Facility는 Customer, Interaction과 UI concrete class에 의존하지 않는다.

## Manual Review Points

- key number마다 shoe locker와 clothes locker가 정확히 하나씩 존재하는지 확인한다.
- 하나의 slot을 두 customer가 동시에 reserve/use하지 않는지 확인한다.
- check-in/checkout queue가 독립적으로 전진하는지 확인한다.
- 각 lane의 front customer만 service 가능한지 확인한다.
- 세 point reference 배열이 정확한 Counter 인스턴스 소유 SceneComponent만 가리키고 같은 component를 역할 안팎에서 재사용하지 않는지 확인한다.
- 잘못된 point reference가 runtime resolved 배열과 returned slot에서 제외되며 자동 fallback되지 않는지 확인한다.
- uncollected key가 다음 checkout key와 겹치지 않는지 확인한다.
- 이동 실패, timeout, StateTree 중단과 actor destruction에서 slot/queue가 정리되는지 확인한다.
- Bath approach point만 NavMesh 위에 있고 action point가 NavMesh 밖이어도 입·퇴탕과 다음 MoveTo가 성공하는지 확인한다.
- BathStay 만료와 technical abort가 action point에서 slot을 먼저 풀지 않고 approach point 복귀를 시도하는지 확인한다.
