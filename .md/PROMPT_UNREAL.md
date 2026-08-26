# Unreal Prompt — Physical Carry Blueprint CCD Defaults

## 상태와 목적

- Editor 작업 필요.
- native build는 Unreal 5.8 `BathhouseSimEditor`에서 성공했다.
- 사용자가 현재 연 Editor를 종료하지 않고 같은 세션을 사용한다. loaded module이 이전 DLL이면 asset authoring 전에 새 DLL 로드 여부를 확인하고, 불일치 시 저장을 중단한다.
- 모든 현재 physical carryable Blueprint의 실제 physical root template을 `Use CCD=true`로 명시 저장하고 native common free-world 계약을 검증한다.

## 저장 Allowlist

다음 네 asset만 수정·저장한다.

| Asset | Parent Class | Component | Property |
|---|---|---|---|
| `/Game/Bathhouse/Blueprints/Interaction/BP_BathhouseKey` | `ABathhouseKeyActor` | `KeyPhysicsRoot` | `BodyInstance.bUseCCD=true` |
| `/Game/Bathhouse/Blueprints/Cleaning/BP_WetMop` | `AWetMopActor` | `WorldMesh` | `BodyInstance.bUseCCD=true` |
| `/Game/Bathhouse/Blueprints/Towel/BP_TowelBasket` | `ATowelBasketActor` | `WorldMesh` | `BodyInstance.bUseCCD=true` |
| `/Game/Bathhouse/Blueprints/Combat/BP_MonkeyWrench` | `AMonkeyWrenchActor` | `WorldMesh` | `BodyInstance.bUseCCD=true` |

`/Game/Bathhouse/Blueprints/Facility/BP_TowelBasket`는 동명 facility Actor이므로 열거나 저장하지 않는다. Level, World Partition external actor, fixed slot Blueprint, mesh asset, Config와 다른 Content도 저장하지 않는다.

## Preflight

1. `.uproject` EngineAssociation이 `5.8`인지 확인한다.
2. 현재 Editor가 BathhouseSim/UE 5.8이며 PIE/SIE가 중지 상태인지 확인한다.
3. 현재 dirty package 기준선을 기록한다. allowlist 외 사용자의 기존 변경은 보존하고 저장하지 않는다.
4. loaded `BathhouseSim` module이 방금 성공한 native build를 반영하는지 확인한다. 네 native CDO physical root의 `BodyInstance.bUseCCD`가 모두 true여야 한다.
5. CDO가 false이면 stale module 상태이므로 Blueprint를 저장하지 말고 Editor 재시작 필요로 중단한다. 사용자가 연 Editor는 임의 종료하지 않는다.

## Authoring

각 Blueprint에서 지정된 inherited physical root component만 선택하고 Collision > Advanced > Use CCD를 true로 설정한다.

- 다른 collision profile, response, simple/complex geometry, simulate physics, mass, gravity, transform와 mesh/material은 변경하지 않는다.
- component rename/reparent, Construction Script/Event Graph와 변수 추가를 하지 않는다.
- class default가 이미 true여도 Blueprint template 값을 확인하고 정확한 네 asset만 개별 저장한다.

## Compile, Validation, Save

1. 네 Blueprint를 warnings-as-errors로 Compile한다.
2. 네 asset을 개별 Data Validation한다.
3. compile error/warning 또는 validation error가 없을 때만 allowlist 네 asset을 개별 Save한다. Save All은 사용하지 않는다.
4. 저장 후 asset을 재로드하여 exact component `BodyInstance.bUseCCD=true`를 다시 확인한다.
5. 작업 전 dirty 기준선과 비교해 allowlist 외 신규 dirty package가 없는지 확인한다.

## Automation

새 DLL이 로드된 동일 Editor에서 다음을 실행한다.

- focused: `BathhouseSim.Interaction.PhysicalCarry`
- full: `BathhouseSim`

target marker, found/succeeded/failed 수, `GIsCriticalError`, Error/Failed 로그를 기록한다. negative fixture의 의도된 메시지는 프로젝트 오류와 구분한다.

## PIE 수용 기준

네 item 각각을 실제로 들고 G free drop한다.

- actual held pose에서 drop되고 기존 약한 velocity change를 유지한다.
- free-world physical root는 `QueryAndPhysics`, simulate physics, Pawn Ignore와 CCD true다.
- floor/WorldStatic을 block하며 key/mop/basket/wrench가 바닥 아래로 관통하지 않는다.
- 특히 `BP_MonkeyWrench`를 정지/걷기/달리기 중 각 여러 번 드롭해 얇은 5 cm collision의 관통이 재현되지 않는지 확인한다.
- pickup, exact fixed-slot store/take, key hook return, towel inventory, mop/wrench use가 기존대로 동작한다.

CCD는 관통 가능성을 줄이지만 invalid/missing simple collision을 보정하지 않는다. 관통이 계속되면 mesh collision geometry와 물리 substep을 별도 문제로 기록하고 이 작업에서 item별 임시 코드 분기를 추가하지 않는다.

## 금지 사항

- Blueprint graph에서 CCD를 상태별로 토글하지 않는다.
- monkey wrench만 별도 drop/teleport/impulse 로직을 만들지 않는다.
- collision geometry나 scale을 이번 계약에 맞추려고 변경하지 않는다.
- 사용자 에디터 종료, Save All, allowlist 밖 asset 저장과 temporary Editor-world actor 저장을 하지 않는다.

## 완료 산출물

`.md/PROMPT_INTEGRATION_REVIEW.md`에 다음을 현재 작업 하나만 기록한다.

- 완료/부분 완료/중단 상태와 loaded native API 확인
- 수정·저장한 exact 네 asset과 CCD 기대값/실제값
- Compile, Data Validation, Save, reload, focused/full automation과 PIE 결과
- allowlist 외 dirty package, 미검증과 수동 재시작 필요 여부
