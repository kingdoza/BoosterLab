# Implementation Prompt — Cleaning And Towel Circulation

## 목적

기존 player primary interaction, physical key transaction, customer StateTree, facility slot과 native interaction prompt 계약을 보존하면서 다음 target을 native C++로 구현한다.

1. E hold wet-mop water-stain cleaning과 zone 기반 random spawn
2. key/wet mop/towel basket 중 정확히 하나인 physical carry와 G equipment throw
3. E one/F max-possible towel transfer, washer/dryer와 clean-used-wet circulation
4. used bin full 시 주변 floor individual used towel overflow
5. customer clean towel token, shortage fallback과 satisfaction
6. primary/secondary/failure/hold progress native interaction prompt

기준 문서:

- `.md/AGENT_WORKFLOW.md`
- `.md/AGENT_IMPLEMENTATION.md`
- `.md/0_ARCHITECTURE.md`
- `.md/Architecture/InteractionSystem.md`
- `.md/Architecture/CleaningSystem.md`
- `.md/Architecture/TowelSystem.md`
- `.md/Architecture/FacilitySystem.md`
- `.md/Architecture/CustomerSystem.md`
- `.md/Architecture/CharacterSystem.md`
- `.md/Architecture/UISystem.md`

이 파일은 이전 Bath montage/snap 구현 프롬프트를 전부 대체한다.

## 수용 기준

- E는 기존 instant primary를 한 번 실행하고 water stain에서는 누르는 동안만 progress한다.
- F는 target secondary intent이며 towel destination remaining capacity까지만 이동한다.
- G는 mop/basket만 camera forward로 짧게 throw하고 key는 기존 hook-only 반환을 유지한다.
- key/mop/basket 중 둘 이상을 동시에 들거나 자동 swap하지 않는다.
- water stain은 valid authored floor zone에만 생성되고 기존 stain/Pawn과 겹치지 않는다.
- 모든 towel 이동은 source 감소와 destination 증가를 한 native transaction으로 commit하며 실패 시 양쪽을 유지한다.
- used bin 내부는 container 단위 E/F, floor overflow towel은 individual E-only interaction이다.
- machine은 Waiting/Processing/Complete를 가지며 별도 control interaction으로 시작한다.
- clean towel shortage는 bounded wait 후 towel 없이 진행하고 satisfaction을 한 번 감소시킨다.
- used bin full은 customer를 막지 않고 individual floor towel 또는 PendingSpill로 token을 보존한다.
- native prompt가 primary/secondary action, 각각의 failure와 hold progress를 직접 bound widget에 적용한다.
- Source/Config/Content의 기존 reflected 이름을 불필요하게 rename/delete하지 않는다.

## 책임 변화와 분리

- `AFirstPersonCharacter`: E/F/G input intent routing만 추가한다.
- `UPlayerInteractionComponent`: intent dispatch, active hold lifecycle와 attempt result를 소유한다. Cleaning/Towel concrete type을 판별하지 않는다.
- `UPlayerCarryComponent`: one generic held actor를 소유하되 기존 key API/delegate를 호환 유지한다.
- Cleaning 폴더: stain registry/spawn, mop과 stain cleaning state를 소유한다.
- Towel 폴더: inventory, atomic transfer, machine, container/world towel와 recovery ledger를 소유한다.
- `UCustomerSessionComponent`: customer-held towel handle과 satisfaction만 소유한다. endpoint count를 직접 변경하지 않는다.
- Facility는 TowelShelf/기존 TowelBasket 이동 slot만 소유한다.
- UI는 Interaction presentation data만 소비한다.

## Target Files

신규:

- `Source/BathhouseSim/Public|Private/Cleaning/*`
- `Source/BathhouseSim/Public|Private/Towel/*`
- `Source/BathhouseSim/Public/Interaction/PhysicalCarryable.h`
- 필요한 focused native automation test 파일

수정:

- Character `FirstPersonCharacter.h/.cpp`
- Interaction types/interface/component/carry/key 관련 파일
- Facility types
- Customer types/routine definition/session/StateTree Task·Condition 관련 파일
- UI `InteractionPromptWidget.h/.cpp`
- `BathhouseSim.Build.cs`는 실제 새 module dependency가 필요한 경우에만 수정

Content, Config와 `.uproject`는 구현 단계에서 수정하지 않는다.

## 1. Input And Interaction Intent

- 기존 `InteractAction`/`TryInteract()`와 primary query fields를 유지한다.
- `SecondaryInteractAction`, `DropCarryAction` reflected property를 Character에 추가한다.
- E Started/Completed/Canceled를 primary begin/end에 bind한다.
- F Started는 secondary attempt 한 번, G Started는 equipment release attempt 한 번이다.
- primary query에 default `Instant`/optional `Hold` mode를 추가하고 secondary 표시/가능/action/failure fields를 추가한다.
- 기존 interactable은 변경 없이 instant primary로 작동하게 optional virtual default를 제공한다.
- hold session은 target identity, same focus, input held, query/carry validity를 Tick마다 검사하고 invalidation에서 정확히 한 번 cancel한다.
- 모든 attempt는 최신 query refresh 뒤 intent가 포함된 result를 정확히 한 번 방송한다.

## 2. Generic Single Carry

- carry authoritative field를 generic held actor 하나로 바꾸고 `IsHandEmpty`, `GetHeldKey`, key commit과 `OnHeldKeyChanged`를 유지한다.
- `IPhysicalCarryable`은 pickup presentation, free-drop 허용, throw parameter와 recovery를 제공한다.
- existing `HeldKeyAnchor` property/default-subobject 이름은 rename하지 않고 공용 anchor로 사용한다.
- key state/owner transition은 기존 `ABathhouseKeyActor` API만 사용하며 G를 거부한다.
- mop/basket pickup은 empty-hand revalidation 뒤 attach/collision-off하고 G는 detach/collision-on/physics impulse를 적용한다.
- carrier/actor EndPlay와 falling-out-of-world cleanup을 idempotent하게 구현한다.

## 3. Cleaning

- `UCleaningWorldSubsystem`은 zone/stain weak registry만 소유한다.
- `ACleaningDirectorActor`는 interval, total limit, bounded placement attempts와 stain class를 authoring한다.
- `AStainSpawnZoneActor`는 Box bounds, kind/weight/per-zone limit, floor trace/tag/slope와 spacing/Pawn clearance를 authoring한다.
- random candidate는 valid floor, zone membership, stain spacing과 Pawn overlap을 모두 통과해야 spawn한다.
- `AWetMopActor`는 carryable pickup/G drop을 구현한다.
- `AWaterStainActor`는 Idle/Cleaning/Removed, cleaner identity, removal duration/progress와 terminal guard를 소유한다.
- E hold release/focus loss/mop loss/EndPlay에서 progress를 0으로 cancel한다.
- Blueprint events는 start/progress/cancel/complete와 held presentation만 제공하고 mutation을 허용하지 않는다.

## 4. Towel Inventory And Transfer

- `ETowelState=None/Used/Wet/Clean`, snapshot state/count/capacity/revision 불변식을 구현한다.
- `UTowelInventoryComponent`에 public count setter를 만들지 않는다.
- `UTowelTransferSubsystem::TryTransfer`가 validity/revision/state/capacity/machine gate를 모두 검증한다.
- E requested count 1, F requested count max; actual은 source/destination/request minimum이다.
- 양쪽 internal mutation과 revision을 commit한 뒤에만 delegate를 방송한다.
- mixed state/full/invalid/processing/reentry 실패는 양쪽 snapshot을 유지한다.
- transaction result에 moved count, failure와 committed revisions를 담는다.

## 5. Towel Actors And Machines

- `ATowelBasketActor`: capacity/homogeneous inventory, carryable/G throw, EndPlay contents recovery.
- `ACleanTowelStackActor`: Clean-only inventory, player basket E/F deposit와 customer one-token acquire, TowelShelf facility.
- `AUsedTowelBinActor`: Used-only inventory, customer return, bin-to-basket E/F, 기존 TowelBasket facility.
- bin 내부 visible towel은 presentation-only이며 interaction collision을 갖지 않는다.
- `AWorldUsedTowelActor`: one Used token, basket-compatible E-only claim, consumed/recovery guard.
- bin full customer return은 valid random floor staged spawn 후 handle-to-actor commit한다. 실패하면 PendingSpill ledger로 commit하고 retry한다.
- washer는 Used->Wet, dryer는 Wet->Clean이다.
- machine transfer port와 separate primary control component를 구분한다.
- Processing 중 mutation을 막고 timer 완료에서 count 보존/state conversion/Complete를 한 번 commit한다.
- Complete output은 empty 또는 same-state non-full basket에만 이동하며 empty가 되면 Waiting/None으로 복귀한다.

## 6. Customer Integration

- `EBathhouseFacilityType::TowelShelf`를 enum 끝에 추가하고 기존 ordinal/`TowelBasket`을 유지한다.
- `FTowelUseHandle`은 token, original stack, used flag와 terminal cleanup guard를 가진다.
- undress 뒤 TowelShelf에서 acquire; shortage면 authorable limit 동안 event wait 후 towel-less branch로 진행한다.
- `UCustomerRoutineDefinition`에 wait limit과 satisfaction penalty를 추가한다.
- `UCustomerSessionComponent`에 satisfaction getter/change event와 handle transaction API를 추가한다.
- towel handle이 있으면 Drying에서 Used로 mark하고 ReturnTowel에서 bin/overflow/PendingSpill로 commit한다.
- handle이 없으면 towel-dependent use/return을 건너뛴다.
- normal, StateTree interruption, TechnicalAbort와 EndPlay cleanup이 token stage에 따라 clean return 또는 used recovery를 한 번 수행한다.

## 7. Prompt And Blueprint Boundary

- 기존 `ActionNameText`, `FailureReasonText`와 `OnInteractionPromptChanged` signature를 primary compatibility로 유지한다.
- `SecondaryActionNameText`, `SecondaryFailureReasonText`, `InteractionProgressBar` BindWidget을 추가한다.
- native widget이 E/F row visibility/enabled/text, intent별 transient failure와 hold percent를 직접 적용한다.
- secondary/progress용 새 optional Blueprint presentation hook을 만들되 정확성이 Event Graph에 의존하지 않게 한다.
- bulk towel presentation event는 previous/new state/count, capacity, revision, transaction id를 제공한다.
- Blueprint count animation은 최신 authoritative snapshot에 수렴하며 count를 직접 변경하지 않는다.

## Tests And Verification

- existing key/carry/interaction/customer/facility/economy tests를 유지한다.
- instant primary, hold begin/cancel/complete, F intent와 G key rejection/mop-basket release를 검증한다.
- stain zone/limit/floor/spacing/Pawn rejection과 EndPlay registry cleanup을 검증한다.
- towel conservation, primary/secondary partial, mixed/full/revision/reentry와 source/destination EndPlay를 검증한다.
- bin container E/F와 individual overflow E-only, staged spawn failure/PendingSpill/consumed recovery를 검증한다.
- machine start/processing block/completion/output와 same-frame revalidation을 검증한다.
- customer acquire/shortage satisfaction/used overflow와 interruption 단계별 token owner를 검증한다.
- reflected legacy names와 enum ordinal을 focused search로 확인하고 `git diff --check`를 실행한다.
- UE 5.8 `Build.bat`을 첫 시도부터 승인된 sandbox 밖에서 실행해 Editor target을 빌드한다.

구현 완료 후 `.md/PROMPT_REVIEW.md`와 `.md/PROMPT_UNREAL.md`만 정기 결과물로 작성한다. Unreal 프롬프트에는 E/F/G InputAction/IMC, WBP BindWidget, cleaning director/zones, mop/stain/towel/machine Blueprint, facility slot, customer StateTree와 presentation PIE 검증을 인계한다.

## 금지사항

- inventory/hotbar, multi-carry 또는 key free drop을 추가하지 않는다.
- UI/Character/StateTree Blueprint에서 Cleaning/Towel domain state를 변경하지 않는다.
- transfer 중 한 endpoint만 먼저 delegate에 노출하거나 실패 후 rollback에 의존하지 않는다.
- bin 내부 towel을 individual interactable actor로 만들지 않는다.
- 다른 stain/tool/towel state, mop washing, bucket, durability와 consumable을 추가하지 않는다.
- satisfaction을 이용료나 다른 customer 행동에 연결하지 않는다.
