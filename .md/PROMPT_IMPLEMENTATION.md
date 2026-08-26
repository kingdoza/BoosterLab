# Implementation Prompt — Physical Carry Default CCD

## 목적

현재 모든 physical carryable의 free-world rigid body에 CCD를 기본 적용한다. 네 concrete item의 native class default와 Blueprint component template을 같은 값으로 맞추고, 공통 placement transaction이 모든 free-world 진입에서 CCD를 강제하며 late failure rollback은 이전 CCD 값까지 복구해야 한다.

기준 문서:

- `.md/AGENT_WORKFLOW.md`
- `.md/AGENT_IMPLEMENTATION.md`
- `.md/0_ARCHITECTURE.md`
- `.md/Architecture/CoreSystem.md`
- `.md/Architecture/InteractionSystem.md`
- `.md/Architecture/PhysicalCarrySystem.md`
- `.md/Architecture/CleaningSystem.md`
- `.md/Architecture/TowelSystem.md`
- `.md/Architecture/CombatSystem.md`

이 파일은 이전 physical-carry 구현 프롬프트를 대체한다.

## 보존 및 금지 범위

- 구현 단계는 `Source/`와 native automation만 수정한다. `Content/` authoring은 코드 리뷰 승인 후 Unreal MCP 단계에서 수행한다.
- `IPhysicalCarryable`, concrete Actor 계층, component 이름과 reflected API를 rename/delete하지 않는다.
- 공통 carry Actor/Component, item별 CCD 예외 분기, Tick 또는 신규 runtime module dependency를 추가하지 않는다.
- key/customer/counter/hook, cleaning, towel inventory, combat damage와 fixed-slot state machine을 변경하지 않는다.
- collision geometry/profile, throw velocity, held transform과 drop placement를 이 작업에서 재설계하지 않는다.
- CCD가 simple collision 및 WorldStatic/WorldDynamic block 응답을 대체한다고 가정하지 않는다.

## 1. Native Class Defaults

다음 physical root default subobject에 `SetUseCCD(true)`를 설정한다.

- `ABathhouseKeyActor::KeyPhysicsRoot`
- `AWetMopActor::WorldMesh`
- `ATowelBasketActor::WorldMesh`
- `AMonkeyWrenchActor::WorldMesh`

held/fixed-slot 상태에서 collision과 simulate physics를 끄는 기존 동작은 유지한다. CCD 플래그 자체는 상태 전환 때 지우지 않는다.

## 2. Common Free-World Enforcement

`FPhysicalCarryPlacementTransaction::ApplyFreeWorld`에서 collision/physics 활성화 직전에 physical root의 CCD를 항상 켠다.

- player G free drop
- slot destruction release
- hook destruction release
- 같은 transaction을 사용하는 recovery/free-world 전환

모든 경로는 공통 transaction 계약을 받아야 하며 concrete item type cast나 예외 분기를 추가하지 않는다. 성공 조건은 detached, QueryAndPhysics, Pawn Ignore, simulating physics와 CCD 활성화를 확인한다.

`FellOutOfWorld`에서 fixed-slot 복구가 불가능해 concrete `SetWorldPhysics(true)`로 last-safe 위치에 복구하는 기존 경로도 CCD를 다시 켜야 한다. 이는 기존 네 helper의 free-world physics invariant 보강이며 별도 상태나 예외 정책을 추가하지 않는다.

## 3. Atomic Rollback

transaction snapshot에 이전 CCD 값을 저장한다. commit 전 실패로 rollback하면 collision/object response/gravity/simulate/velocity와 함께 이전 CCD 값을 복구한다.

- snapshot은 `UPrimitiveComponent` body instance의 기존 CCD 값을 읽는다.
- rollback에서 physics 재활성화 전에 이전 CCD 값을 적용한다.
- invalid/destroyed primitive 처리와 기존 slot occupancy rollback 순서는 보존한다.

## 4. Automation

기존 physical-carry automation을 확장한다.

- key/mop/towel basket/monkey wrench CDO physical root가 모두 CCD 기본값을 갖는다.
- common G free drop 성공 후 physical primitive가 CCD를 사용한다.
- fixed slot 파괴로 free world에 풀린 item도 CCD를 사용한다.
- last-safe free-world recovery가 이전 false 값을 남기지 않고 CCD를 다시 켠다.
- late free-drop 실패 전에 CCD를 임의로 끈 fixture가 rollback 후 이전 false 값을 복구한다.
- 기존 attach, collision, Pawn Ignore, mass-independent velocity, inventory와 domain state assertion이 회귀하지 않는다.

필요하면 `Combat/MonkeyWrenchActor.h` include만 테스트에 추가한다. 테스트 전용 public API를 만들지 않는다.

## 5. 검증 및 산출물

- exact Unreal 5.8 `Build.bat` Editor target build를 수행한다. 사용자가 연 Editor 때문에 build가 충돌하면 종료하지 말고 조건을 리뷰 문서에 기록한다.
- focused `BathhouseSim.Interaction.PhysicalCarry` automation과 가능한 전체 `BathhouseSim` suite를 실행한다.
- `.md/PROMPT_REVIEW.md`에 변경 파일, build/test 결과, CCD 계약과 rollback 검토점을 기록한다.
- `.md/PROMPT_UNREAL.md`에는 정확히 네 Blueprint physical root의 `Use CCD=true` authoring, compile/save/reload/PIE 검증만 요청한다.
- Core Redirect가 필요하지 않음을 명시한다.

## 완료 조건

- 모든 현재 physical carryable이 native class default와 free-world 공통 전환에서 CCD를 사용한다.
- Blueprint serialized override와 관계없이 runtime free-world 진입이 CCD를 강제한다.
- 실패 rollback이 이전 CCD 값을 포함한 snapshot을 완전히 복구한다.
- 아이템별 임시 분기나 공통 소지 계약의 불일치가 생기지 않는다.
