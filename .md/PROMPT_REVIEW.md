# Review Prompt — Physical Carry Default CCD

## 요구사항과 수용 기준

모든 현재 physical carryable의 physical root는 CCD를 class default로 사용하고, 공통 free-world placement transaction은 serialized Blueprint 상태와 무관하게 CCD를 다시 켜야 한다. commit 전 실패는 transaction 진입 전 CCD 값을 복구해야 하며 item별 임시 분기는 없어야 한다.

수용 기준:

- key/mop/towel basket/monkey wrench native physical root CDO가 `bUseCCD=true`다.
- `FPhysicalCarryPlacementTransaction::ApplyFreeWorld` 성공은 CCD가 실제로 켜진 경우에만 성립한다.
- player drop, slot/hook destruction을 포함해 공통 transaction을 쓰는 free-world 경로가 같은 계약을 받는다.
- rollback이 이전 CCD 값을 collision/physics snapshot과 함께 복구한다.
- 기존 held/fixed-slot, Pawn Ignore, mass-independent velocity, domain state와 API가 회귀하지 않는다.

## 변경 파일과 구현 요약

- `Source/BathhouseSim/Private/Interaction/PhysicalCarryPlacementTransaction.h/.cpp`
  - snapshot에 `bPreviousUseCCD` 추가
  - free-world 전환에서 `SetUseCCD(true)` 강제 및 성공 후 검증
  - rollback에서 이전 CCD 복구
- `Source/BathhouseSim/Private/Interaction/BathhouseKeyActor.cpp`
- `Source/BathhouseSim/Private/Cleaning/WetMopActor.cpp`
- `Source/BathhouseSim/Private/Towel/TowelBasketActor.cpp`
- `Source/BathhouseSim/Private/Combat/MonkeyWrenchActor.cpp`
  - 각 physical root default subobject와 기존 last-safe `SetWorldPhysics(true)`에 `SetUseCCD(true)` 추가
- `Source/BathhouseSim/Private/Tests/PhysicalCarryFixedSlotAutomationTests.cpp`
  - 네 CDO 기본값, common drop, slot destruction, late-failure rollback과 재시도 검증
- `Source/BathhouseSim/Private/Tests/CleaningTowelAutomationTests.cpp`
  - 기존 mop/key common drop 경로의 CCD assertion 추가

## 클래스 크기와 책임

- `FPhysicalCarryPlacementTransaction`: 약 `45/165` lines(header/cpp), 기존 mechanical snapshot/rollback 책임 안에서 bool snapshot 하나와 free-world flag 적용만 추가했다.
- concrete item 네 클래스: 새 상태·delegate·Tick 없이 default subobject physics flag 한 줄씩만 추가했다.
- public/reflected API, UObject 계층과 상태 owner 변화는 없다. 별도 class/component 분리는 필요하지 않다.

## Blueprint/API/Core Redirect 영향

- reflected symbol rename/delete/add 없음, Core Redirect 불필요.
- 네 Blueprint가 native component template의 과거 serialized `Use CCD=false`를 보존할 수 있어 Editor 단계에서 정확한 physical root에 `Use CCD=true`를 명시 저장해야 한다.
- `/Game/Bathhouse/Blueprints/Facility/BP_TowelBasket`는 동명 facility presentation Actor이며 수정 대상이 아니다.

## 정적 검증과 빌드

- `git diff --check`: whitespace error 없음. 기존 working-copy LF→CRLF 안내만 발생.
- Unreal 5.8 exact `Build.bat` `BathhouseSimEditor Win64 Development -WaitMutex -NoHotReloadFromIDE`: 최초 13 actions 및 last-safe 보완 후 8 actions compile/link 모두 성공.
- 신규 module dependency 없음.
- fresh Unreal 5.8 commandlet focused `BathhouseSim.Interaction.PhysicalCarry`: 5/5 성공, exit 0, `GIsCriticalError=0`.
- fresh Unreal 5.8 commandlet full `BathhouseSim`: 28/28 성공, exit 0, `GIsCriticalError=0`. towel presentation negative fixture의 기존 의도된 warning만 기록되었다.

## 코드 리뷰 중점

- `SetUseCCD(true)`가 collision/physics 활성화 전에 실행되고 성공 조건에도 포함되는가.
- rollback의 `SetUseCCD(bPreviousUseCCD)`가 simulate physics 복구 전에 실행되는가.
- failed free drop fixture가 false→transaction true→rollback false를 실제로 검증하는가.
- 네 concrete class 외의 common transaction 사용자도 item cast 없이 계약을 받는가.
- fixed-slot 복구가 불가능한 각 concrete last-safe helper도 free-world CCD invariant를 복구하는가.
- CCD를 held/fixed-slot마다 끄지 않아 stale Blueprint 값이 free-world 계약을 우회하지 않는가.
- 기존 dirty worktree의 다른 physical-carry 변경을 이번 CCD 변경으로 오인하지 않는가.

## 미검증 항목

- 네 Blueprint component template의 serialized CCD 값 authoring과 저장 후 reload.
- PIE에서 네 item drop과 특히 얇은 monkey wrench의 바닥 관통 회귀.
- 위 Editor 항목은 `.md/PROMPT_UNREAL.md`로 인계한다.
