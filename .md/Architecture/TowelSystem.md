# Towel System

## Target Status

이 문서는 clean stack부터 customer 사용, used bin/바닥 overflow, player basket, washer와 dryer를 거쳐 clean stack으로 돌아오는 수건 순환의 확정 구현 target을 정의한다. 현재 Source와 Content에는 아직 구현되지 않았다.

## Source Target

```text
Source/BathhouseSim/Public/Towel/
  TowelTypes.h
  TowelInventoryComponent.h
  TowelTransferSubsystem.h
  TowelCirculationSubsystem.h
  TowelBasketActor.h
  CleanTowelStackActor.h
  UsedTowelBinActor.h
  WorldUsedTowelActor.h
  TowelProcessingMachineActor.h
  TowelTransferPortComponent.h
  TowelMachineControlComponent.h

Source/BathhouseSim/Private/Towel/
  # matching implementation files
```

## Responsibilities

- homogeneous towel state, count, capacity와 revision
- 모든 towel endpoint 사이의 atomic transfer
- clean stack, used bin, carried basket와 individual overflow towel
- washer/dryer state, timer와 towel state conversion
- customer towel-use token과 overflow recovery 지원
- authoritative count와 분리된 Blueprint presentation target

Towel은 player input mapping, customer StateTree hierarchy, facility reservation과 UI 상태를 소유하지 않는다.

## Towel State

`ETowelState`:

- `None`: count가 0인 homogeneous container
- `Used`
- `Wet`
- `Clean`

`FTowelInventorySnapshot`은 state, count, capacity와 monotonic revision을 제공한다. count가 0이면 state는 반드시 `None`이고, count가 1 이상이면 하나의 non-None state만 가진다.

`ETowelMachineState`:

- `Waiting`
- `Processing`
- `Complete`

`ETowelMachineKind`:

- `Washer`: `Used -> Wet`
- `Dryer`: `Wet -> Clean`

## State Owners

| 책임 | Owner |
|---|---|
| endpoint state/count/capacity/revision | `UTowelInventoryComponent` |
| two-endpoint atomic mutation | `UTowelTransferSubsystem` |
| customer token, spill pending/recovery ledger | `UTowelCirculationSubsystem` |
| machine state/end time/conversion | `ATowelProcessingMachineActor` |
| basket physical lifecycle | `ATowelBasketActor` + `UPlayerCarryComponent` |
| customer-held towel handle | `UCustomerSessionComponent` |
| visual count convergence | towel Actor Blueprint presentation |

## Inventory And Atomic Transfer

`UTowelInventoryComponent`는 외부 public setter를 제공하지 않는다. Transfer Subsystem과 machine의 validated internal transition만 state/count를 변경한다.

`FTowelTransferRequest`:

- source/destination endpoint identity
- requested count
- expected source/destination revision

`FTowelTransferResult`:

- success
- moved count
- failure reason
- committed source/destination revision

`UTowelTransferSubsystem::TryTransfer` 절차:

1. 두 endpoint의 validity, registration과 transaction guard를 확인한다.
2. revision, source count와 destination remaining capacity를 확인한다.
3. state compatibility와 endpoint/machine gate를 확인한다.
4. `Min(Requested, SourceCount, DestinationRemaining)`을 계산한다.
5. 두 endpoint를 잠그고 remove/add의 실패 불가능한 internal mutation을 연속 commit한다.
6. 두 revision을 증가시키고 잠금을 해제한다.
7. 양쪽 Blueprint/native delegate를 commit 후 방송한다.

검증 실패 시 remove/add 어느 쪽도 실행하지 않는다. Unreal game thread serialization과 revision 재검증으로 completion/interaction 동시점과 반복 입력을 처리한다.

Primary E는 requested count 1, Secondary F는 가능한 최대 수량을 요청한다. Secondary는 무조건 전량이 아니라 destination capacity까지 이동한다.

## Physical Towel Basket

`ATowelBasketActor`는 generic physical carry 계약과 하나의 towel inventory를 가진다.

- capacity 제한
- empty면 `None`
- 첫 towel이 들어오면 해당 state로 고정
- 같은 state만 추가 가능
- count가 다시 0이면 `None`
- G로 camera forward 방향에 짧게 throw
- throw/physics/EndPlay 중에도 contents는 actor inventory에 유지

다른 key/mop/basket을 들고 있으면 pickup하지 않는다. basket EndPlay가 정상 world shutdown이 아니라면 contents snapshot을 circulation recovery ledger에 한 번 이전한다.

## Clean Stack And Used Bin

`ACleanTowelStackActor`는 clean towel inventory와 customer facility slot을 가진다.

- player Clean basket -> stack 이동
- E 한 장, F 가능한 수량
- customer가 한 장을 towel-use token으로 획득
- `EBathhouseFacilityType::TowelShelf`로 customer navigation에 등록

`AUsedTowelBinActor`는 기존 `EBathhouseFacilityType::TowelBasket`의 customer return 위치를 유지한다.

- customer Used towel return
- bin -> player Used basket 이동
- E 한 장, F 가능한 수량
- bin capacity가 가득 차면 customer towel을 보유하지 않고 floor overflow로 전환

bin 내부 visible towels는 count 기반 presentation 전용이며 개별 collision/interaction을 갖지 않는다.

## Used Towel Overflow

`AWorldUsedTowelActor`는 used bin 주변 바닥에 넘친 towel 한 장을 나타내는 authoritative token actor다.

Return flow:

1. customer handle이 Used token을 보유한다.
2. bin에 공간이 있으면 bin count +1로 commit한다.
3. full이면 bin의 authorable annulus 안 random point를 선택한다.
4. floor trace, facility/wall blocking, existing towel spacing과 Pawn overlap을 검증한다.
5. valid point에 staged actor를 생성한 뒤 token owner를 customer handle에서 actor로 commit한다.
6. 성공 후 customer는 return 단계에서 벗어난다.

`AUsedTowelBinActor` authoring 값:

- overflow min/max radius
- floor trace channel/distance
- placement attempts
- towel spacing/Pawn clearance
- `WorldUsedTowelClass`

후보 또는 spawn에 실패하면 token을 `PendingSpill` ledger로 원자 이전하고 customer를 진행시킨다. Circulation Subsystem이 bin 유효 시 재시도한다.

World towel interaction은 개별 Primary E만 지원한다.

- held object가 compatible basket인지 확인
- basket이 empty/Used이며 여유가 있는지 확인
- actor token 감소와 basket Used +1을 한 transaction으로 commit
- 성공 후 actor를 `Consumed`로 표시하고 제거
- Secondary F는 숨김

비정상 EndPlay의 unconsumed world towel은 PendingSpill/recovery ledger로 돌아가며, consumed actor는 다시 recovery하지 않는다.

## Washer And Dryer

`ATowelProcessingMachineActor`는 inventory, machine state와 process end time을 소유한다.

- transfer port: basket과 E/F towel 이동
- separate control component: E로 processing 시작
- `Waiting`에서만 correct input towel 투입
- count > 0일 때만 control interaction으로 시작
- `Processing` 중 port/control mutation 차단
- timer 완료 시 count를 유지한 채 전체 state를 output state로 한 번 변환
- `Complete`에서만 output 회수
- count가 0이 되면 `Waiting`/`None`으로 복귀

완료 machine에서 basket 회수:

- empty basket은 허용
- 같은 output state와 남은 capacity가 있는 basket은 허용
- 다른 state 또는 full basket은 정확한 이유로 실패

machine capacity와 process duration은 instance/default authoring 값이다. normalized progress는 stored end time에서 파생하며 Blueprint가 timer 정본을 복제하지 않는다.

## Transfer Direction

| Target | Direction |
|---|---|
| Used bin | bin Used -> held basket |
| World used towel | actor token -> held basket, E only |
| Waiting washer | held basket Used -> washer |
| Complete washer | washer Wet -> held basket |
| Waiting dryer | held basket Wet -> dryer |
| Complete dryer | dryer Clean -> held basket |
| Clean stack | held basket Clean -> stack |

Port/interactable가 held basket과 target snapshot으로 방향을 결정한다. Character, Widget과 input action은 towel type을 판별하지 않는다.

## Customer Token And Fallback

`FTowelUseHandle`은 customer가 clean stack에서 가져간 towel 한 장의 owner를 명시한다.

- acquired clean token
- used 여부
- returned/cleanup terminal guard
- original stack identity

Clean towel이 없으면 customer는 authorable wait limit 동안 availability event를 기다린다. 만료 시 towel 없이 routine을 계속하고 towel-dependent use/return을 건너뛰며 session satisfaction을 authorable amount만큼 감소시킨다.

Used bin capacity는 acquisition을 차단하지 않는다. full return은 floor overflow/PendingSpill로 보존한다.

Customer interruption:

- 사용 전: original clean stack으로 반환, 불가능하면 recovery ledger
- 사용 후: used bin 또는 overflow/PendingSpill로 commit
- terminal cleanup은 idempotent하고 token owner가 항상 정확히 하나여야 한다.

## Blueprint Presentation Contract

Inventory commit event는 previous/new state, previous/new count, capacity, revision과 transaction id를 제공한다.

Blueprint는 stack/bin/basket visible meshes를 한 장씩 빠르게 변화시킬 수 있지만 authoritative count를 변경하지 않는다. 새 revision 또는 presentation interruption 시 기존 animation을 취소하고 최신 snapshot count로 수렴한다. BeginPlay/reconstruction도 current snapshot에서 다시 그린다.

Machine 표현 event:

- `OnMachineStateChanged`
- `OnMachineProgressChanged`
- `OnMachineContentsChanged`

Towel actor 표현 event:

- `OnTowelPresentationTargetChanged`
- basket held/world presentation event

## Dependencies

- Towel -> Interaction의 intent/carry public 계약
- Towel -> Facility의 generic actor/slot registration
- Customer -> Towel transaction/token API
- UI -> Interaction prompt data
- Towel은 Customer concrete StateTree와 UI concrete class에 의존하지 않는다.

## Manual Review Points

- 모든 E/F transfer 전후 총 towel token 수가 보존되는지 확인한다.
- bin 내부 towel은 container 단위 E/F이고 overflow towel만 개별 E인지 확인한다.
- mixed state, full capacity, processing state와 repeated input이 양쪽 endpoint를 변경하지 않는지 확인한다.
- bulk stack presentation 중단 뒤 C++ count와 visible count가 재동기화되는지 확인한다.
- customer interruption과 actor EndPlay에서 token이 정확히 한 owner 또는 recovery ledger에 남는지 확인한다.
- 다른 towel state/tool durability/consumable 기능이 이번 범위에 추가되지 않았는지 확인한다.
