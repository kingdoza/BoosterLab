# Integration Review Prompt — Physical Carry Default CCD

## 상태

부분 완료 — native 구현·build·automation·코드 리뷰는 완료했으나 Editor authoring preflight에서 사용자가 연 Unreal Editor 세션이 없어 Content 저장과 PIE 통합 검증을 대기한다.

## 기준 계약

- `.md/PROMPT_REVIEW.md`: 코드 단계 승인, 치명/중요 finding 없음
- `.md/PROMPT_UNREAL.md`: 네 carryable Blueprint physical root `Use CCD=true` authoring/검증
- native API: `FPhysicalCarryPlacementTransaction::ApplyFreeWorld`, 각 concrete physical root default와 last-safe `SetWorldPhysics(true)`

## 완료된 검증

- UE 5.8 exact Editor target build: 보완 전/후 모두 성공
- focused `BathhouseSim.Interaction.PhysicalCarry`: 5/5 성공, exit 0, `GIsCriticalError=0`
- full `BathhouseSim`: 28/28 성공, exit 0, `GIsCriticalError=0`
- static review: common free-world, slot/hook release, late failure rollback과 concrete last-safe recovery가 CCD 계약을 보존함
- Core Redirect/API rename/module dependency 없음

## Editor Preflight 결과

- `.uproject` EngineAssociation: `5.8`
- workspace MCP endpoint: `http://127.0.0.1:8000/mcp`
- preflight 시 `UnrealEditor` process: 없음
- 현재 session의 callable Unreal MCP tool: 없음
- allowlist Content 수정/저장: 0개
- 예상 밖 map/external actor 저장: 0개

## 남은 작업

사용자가 UE 5.8 Editor를 연 뒤 `.md/PROMPT_UNREAL.md`를 수행한다.

- `/Game/Bathhouse/Blueprints/Interaction/BP_BathhouseKey.KeyPhysicsRoot`
- `/Game/Bathhouse/Blueprints/Cleaning/BP_WetMop.WorldMesh`
- `/Game/Bathhouse/Blueprints/Towel/BP_TowelBasket.WorldMesh`
- `/Game/Bathhouse/Blueprints/Combat/BP_MonkeyWrench.WorldMesh`

네 component의 `BodyInstance.bUseCCD=true`, warnings-as-errors Compile, Data Validation, 개별 Save/reload와 PIE drop 검증 후 이 문서를 최종 결과로 교체해야 한다.

## 통합 리뷰 결론

보류. Source 계약과 native regression은 승인하지만 Editor/Content/PIE 수용 기준이 남아 있어 최종 통합 완료로 판정하지 않는다.
