# Implementation Rework Prompt — Equipment Context GC And Customer Recovery Verification

## Objective

Fix the retained equipment-use context so every UObject reference stored across frames participates in Unreal GC, then add native verification for the actual customer interruption/facility recovery orchestration required by the current architecture.

Do not modify or resave `Content/`. Preserve the current input owner, single-carry/drop transaction, cleaning, combat, StateTree hierarchy, session transaction and Blueprint handoff contracts.

## P1 — Retained Equipment Context Is Not Reflected

`UPlayerEquipmentUseComponent` stores `FHeldEquipmentUseContext ActiveContext` from `BeginEquipmentUse()` until end/cancel, including the Hold path. `FHeldEquipmentUseContext` contains multiple `TObjectPtr` fields and an `FHitResult`, but the containing `ActiveContext` member is not a `UPROPERTY`. The inner reflected fields are therefore not traversed through this retained member by Unreal GC.

Affected files:

- `Source/BathhouseSim/Public/Interaction/PlayerEquipmentUseComponent.h`
- focused equipment-use automation if a GC regression test is added

Required correction:

1. Make the retained context participate in reflection with the appropriate transient policy, or replace it with an explicitly safe retained representation that does not contain untracked UObject references.
2. Keep `ActiveEquipment`, camera, carry, interaction and motion ownership unchanged unless a smaller representation makes an existing duplicate reference unnecessary.
3. Preserve fresh context rebuilding during Hold updates and the exact End/Cancel behavior.
4. Clear all retained state on success, failure, cancel, held Actor EndPlay and component EndPlay as today.

Acceptance:

- UHT/Editor build succeeds.
- No UObject-bearing context stored across frames is reachable only through an unreflected field.
- LMB release, G drop, suppression and held Actor EndPlay still end/cancel exactly once.
- A focused test may force GC during a held use and must complete/cancel without stale UObject access.

## P1 — Recovery Tests Bypass The Production Orchestration

The new `SoftInterruptionSerialAndBathTimer` automation creates a plain `AActor`, attaches session/interruption components, calls `BeginSoftInterruption()`, and then manually calls `Session->PauseRoutineTimers()`/`ResumeRoutineTimers()`. Because the owner is not `ABathhouseCustomerCharacter`, this bypasses the production branch in `UCustomerRoutineInterruptionComponent` that owns automatic session pause/resume and facility suspension. The submitted suite also does not exercise `FCustomerRestartableMoveToTask`, facility `Occupied -> Reserved -> Occupied` recovery, or stale-token completion behavior.

Affected files:

- `Source/BathhouseSim/Private/Tests/CombatRecoveryAutomationTests.cpp`
- production files only if the new tests expose a defect
- `.md/PROMPT_REVIEW.md`

Required correction:

1. Replace or supplement the manual timer test with an `ABathhouseCustomerCharacter`-based fixture so `BeginSoftInterruption()` and `EndSoftInterruption()` themselves prove automatic timer pause/resume.
2. Add deterministic native coverage for facility suspension/resume: occupied use becomes reserved without losing its owner/cache, successful resume returns to occupied, and invalidated facility state fails into the existing cleanup/retry contract without duplicating a transaction.
3. Add focused coverage for restart operation invalidation so an old completion cannot decide a replacement request and the current task does not remain `Running` forever solely because its token became stale. Use a transient StateTree/AI fixture or isolate the token/result gate without moving StateTree phase ownership out of the Task.
4. Keep PhysicsAsset/root-body authoring and real `ST_CustomerRoutine` node migration in `.md/PROMPT_UNREAL.md`, but clearly distinguish those Editor-dependent checks from native tests actually executed.
5. Update `.md/PROMPT_REVIEW.md` validation evidence to name the behaviors covered; do not use the total count of 22 as evidence for unexercised recovery paths.

Acceptance:

- The timer test contains no manual `PauseRoutineTimers()`/`ResumeRoutineTimers()` calls that duplicate the production interruption component.
- Facility state/owner and stale-operation assertions pass in UE 5.8 automation.
- Existing 22 tests remain green and the new focused tests also pass.
- The headless log contains no unexpected native/Blueprint errors; the only allowed Blueprint compiler diagnostics before Editor migration remain the three unique missing bindings in `WBP_InteractionPrompt`.

## Documentation And Editor Handoff

- No architecture redesign is required. Keep the current responsibility split among equipment router, domain equipment Actors, session, interruption/knockdown components and native StateTree Tasks.
- Update `.md/PROMPT_REVIEW.md` with the exact final test count, focused recovery assertions and compiler-log scan.
- Change `.md/PROMPT_UNREAL.md` only if a C++ contract/property name changes. Preserve its layout/style/asset wiring and StateTree-node migration boundary.
- Do not add Core Redirects unless a reflected symbol is actually renamed; the preferred GC correction does not require one.

## Regression Checks

- `git diff --check`
- no `Content/`, `Config/`, `.uproject` or module dependency change
- UE 5.8 `Build.bat BathhouseSimEditor Win64 Development` succeeds with Editor/Live Coding closed
- full `BathhouseSim` automation succeeds
- startup log scan reports exactly the expected three unique `WBP_InteractionPrompt` missing BindWidget names and no other `LogBlueprint: Error`
- no active motion/use/request token, facility occupancy or routine timer remains after cancel, failure or EndPlay

## Review Resubmission

Regenerate `.md/PROMPT_REVIEW.md` for this exact task and report:

- the retained-context GC policy chosen
- production-path timer/facility interruption assertions
- stale-token/replacement-request assertions
- build and automation counts
- independent Blueprint/compiler error scan
- confirmation that `Content/` and `Config/` were not changed
