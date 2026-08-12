# Unreal Prompt — Unified Physical Carry Drop And Wall Sweep Verification

## Status

**Asset 변경 불필요, 검증만 필요.**

이번 C++ 재작업은 Blueprint, `.uasset`, input mapping, layout 또는 presentation 계약을 변경하지 않는다. target-side start-penetration MTD를 보수적으로 거부하도록 native sweep만 수정했다. 기존 authored asset을 resave하지 말고 공통 드랍과 wall sweep을 PIE에서 검증한다.

검증 대상:

- `/Game/Bathhouse/Blueprints/Cleaning/BP_WetMop` — parent `AWetMopActor`
- `/Game/Bathhouse/Blueprints/Towel/BP_TowelBasket` — parent `ATowelBasketActor`
- `/Game/FirstPersonCharacter/BP_FirstPersonCharacter` — parent `AFirstPersonCharacter`
- G가 이미 연결된 `/Game/Input/IMC_FirstPerson`와 `/Game/Input/Actions/IA_DropCarry`
- wet mop/towel basket이 배치된 현재 테스트 map

## Native Preflight

1. 모든 BathhouseSim Editor와 Live Coding을 닫는다.
2. UE 5.8 `Build.bat`으로 `BathhouseSimEditor Win64 Development -WaitMutex -NoHotReloadFromIDE`를 빌드한다.
3. Editor를 새로 열어 native class/property load warning이 없는지 확인한다.
4. C++ 빌드는 2026-08-12 KST에 성공했고 `BathhouseSim.*` automation은 16 success, 0 fail이었다.
5. hot reload class, missing native parent/property가 보이면 asset을 저장하지 말고 Editor를 닫아 정식 빌드부터 다시 수행한다.

## Read-Only Asset Contract Check

두 carryable Blueprint의 inherited `WorldMesh`가 다음 계약을 이미 만족하는지 확인만 한다.

- `WorldMesh`가 actor root다.
- mesh asset이 지정되어 bounds의 X/Y/Z extent가 모두 0보다 크다.
- world 상태에서 collision/physics가 가능한 body setup을 가진다.
- Visibility sweep에서 벽으로 사용할 geometry가 `ECC_Visibility`를 block한다.
- 기존 `ThrowSpawnDistance`와 `ThrowImpulseStrength` 값을 변경하지 않는다.

`UPlayerCarryComponent`의 inherited native default는 다음과 같다.

- `DropSweepChannel = ECC_Visibility`
- `DropSweepClearance = 2.0`

이번 단계에서는 이 값을 Blueprint에서 override하지 않는다. 기존 asset이 계약을 만족하지 않으면 임의 수정·저장하지 말고 정확한 asset 경로, component/property와 현재 값을 결과에 기록해 코드/설계 단계로 반환한다.

## Blueprint Prohibitions

- Event Graph에 detach, actor 위치 계산/이동, collision 전환, physics 활성화 또는 impulse를 추가하지 않는다.
- line trace나 shape sweep, wall offset 또는 start-penetration 보정을 Blueprint로 중복 구현하지 않는다.
- G input mapping, `ThrowSpawnDistance`, `ThrowImpulseStrength`, 물리 재질과 collision profile을 변경하지 않는다.
- pickup/recovery/fell-out-of-world, key free drop, towel/cleaning domain state를 변경하지 않는다.
- unrelated asset을 compile/save/resave하지 않는다.

공통 물리 실행은 `UPlayerCarryComponent`가 소유하고 mop/basket Blueprint는 기존 presentation만 담당한다.

## PIE Verification

1. 빈 공간에서 towel basket을 G로 드랍하면 기존 목표 거리와 세기로 전방 이동한다.
2. 빈 공간에서 wet mop을 G로 드랍하면 기존 목표 거리와 세기로 전방 이동한다.
3. 가까운 수직 벽을 향하면 물체 전체 bounds가 벽 앞에 남는다.
4. 물체 actor origin뿐 아니라 mesh 전체가 벽 내부 또는 벽 너머로 이동하지 않는다.
5. 비스듬한 벽에서도 관통하지 않는다.
6. 벽 모서리에서 심각한 관통, 튕김 폭발 또는 반대편 배치가 없다.
7. player capsule은 sweep 장애물로 취급되지 않아 정상 빈 공간 드랍을 막지 않는다.
8. held 물체가 얇은 벽과 처음부터 겹치거나 목표점 쪽 MTD가 계산돼도 벽 반대편으로 배치되지 않는다. player-side 안전 후보가 없으면 실패하고 같은 물체를 계속 들며 world collision/physics와 held presentation도 바뀌지 않는다.
9. 8번 실패 직후 빈 공간을 향해 G를 누르면 정상 드랍된다.
10. 드랍한 mop/basket을 다시 주울 수 있고 다음 드랍도 정상이다.
11. mop과 basket의 결과 및 실패 동작이 같은 공통 규칙을 따른다.
12. key를 든 상태에서 G는 기존 실패 결과를 내고 key를 계속 든다.

각 항목에서 최소한 actor 종류, view 방향, 벽 형태, 성공/실패, 최종 held 상태와 눈에 보이는 관통 여부를 기록한다. 가능하면 3~9번은 같은 벽과 동일 player 위치에서 두 장비를 각각 반복한다.

## Completion

이번 작업은 검증 전용이므로 asset Compile/Save나 map 저장을 수행하지 않는다. PIE 종료 후 다음을 보고한다.

- 확인한 asset 경로와 parent/root primitive 계약
- 12개 PIE 항목별 pass/fail
- Output Log의 ensure/error/warning
- 기존 Content dirty 상태에 새 변경이 추가되지 않았는지 여부
- 실패 시 재현 위치, actor, view 방향, 벽 형태와 held/physics/presentation 상태

Editor 작업으로 asset 변경이 필요하다고 판단되면 먼저 중단하고 변경 이유와 최소 대상 목록을 코드/설계 단계에 반환한다.
