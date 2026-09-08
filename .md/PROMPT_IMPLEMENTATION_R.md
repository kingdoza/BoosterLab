# Implementation Rework Prompt — Facility Placement Atomicity, Preview Safety And Collision Semantics

## Objective

Fix the remaining pre-Editor Source defects in facility placement/recovery without modifying or resaving `Content/` or `Config/`. Preserve the same-Actor `Placed`/`Packaged` model, existing single physical carry owner, expansion-owned key pool, locker-capacity lease owner, supplemental interaction boundary and native Widget contract.

Do not move domain contents, water, reservations, leases or key-pool state into `UPlayerFacilityPlacementComponent`. Do not create a replacement gameplay Actor for placement or recovery.

## P1 — Placed Commit Publishes Intermediate State Before The Transaction Is Complete

`ABathhouseFacilityActor::CommitPlaceableFacilityMode()` calls `UFacilityPlacementComponent::ApplyMode(Placed)` before placed-domain registration. `ApplyMode()` immediately broadcasts `OnModeChanged`, and `RegisterPlacedDomain()` then broadcasts facility availability before validating/registering locker capacity. If locker registration fails, observers have already seen a false Placed/facility transition; synchronous delegates can reserve slots, alter carry state or destroy the Actor before rollback. `bPlacedDomainRegistered` is set only after both event-producing registrations return, so EndPlay during either callback can also skip cleanup and leave stale capacity/registry entries.

The player transaction snapshots only parent/socket/relative transform. If the subsequent carry release fails, it invokes a fresh Packaged transition whose collision gate can fail at the candidate location, ignores that rollback result, and does not restore a complete transform/physics/registry snapshot.

Affected files:

- `Source/BathhouseSim/Private/Facility/BathhouseFacilityActor.cpp`
- `Source/BathhouseSim/Public/Facility/BathhouseFacilityActor.h`
- `Source/BathhouseSim/Private/Placement/FacilityPlacementComponent.cpp`
- `Source/BathhouseSim/Public/Placement/FacilityPlacementComponent.h`
- `Source/BathhouseSim/Private/Placement/PlayerFacilityPlacementComponent.cpp`
- `Source/BathhouseSim/Private/Facility/LockerCapacitySubsystem.cpp`
- placement automation tests

Required correction:

1. Make a successful placement externally observable as one committed transition only. Do not broadcast `OnModeChanged`, facility availability or locker-capacity mutation for a state that can still fail, or provide a transaction/deferred-publication mechanism that publishes only after every fallible step and the exact carry release have committed.
2. Move locker count, non-empty/duplicate stable ID and expansion-limit validation into the side-effect-free placement query/preparation phase. Revalidate immediately before commit, but do not temporarily register a malformed locker as a facility.
3. Guard synchronous delegate reentrancy and Actor EndPlay. Destroying the facility from any commit notification must not leave a registered weak bank, inflated installed capacity, stale facility entry, held pointer or continued access to an ending Actor.
4. Snapshot and restore the exact pre-commit world/relative transform, attachment/socket, mode, collision/physics flags, velocities, navigation/registry membership and carry ownership. Rollback must not call a normal recovery collision gate against the new candidate or ignore a failed restoration.
5. Keep `ABathhouseFacilityActor` as high-level orchestration and `UPlayerFacilityPlacementComponent` as the placement transaction owner. If reusable snapshot mechanics grow, put them in a focused private helper rather than expanding domain ownership.

Acceptance:

- Force locker registration failure from count mismatch, missing/duplicate `LockerSlotId`, expansion-limit change and a synchronous destruction/reentrancy callback.
- Every failure leaves the same Actor attached and held in Packaged mode at the exact prior transform with its prior collision/physics state; facility/capacity registries and revisions have no committed false mutation, and presentation delegates do not emit a false Placed transition.
- A successful placement publishes one coherent final state after the carry hand is committed empty.
- Add a late carry-release failure probe and verify complete rollback rather than only the happy path.

## P1 — Missing Or Destroyed Preview Still Allows Invisible Placement

`HandleHeldObjectChanged()` marks `PreviewFacility` active before verifying that `PreviewActorClass` exists and that spawning succeeded. `ValidateCurrentPlacement()` and `ConfirmPlacement()` never require a live preview. The current runtime automation creates a Definition without `PreviewActorClass` and expects placement to succeed, directly proving the contract violation. A preview Actor destroyed during the session is also retained as a non-null `TObjectPtr` and can be called while pending kill.

Affected files:

- `Source/BathhouseSim/Private/Placement/PlayerFacilityPlacementComponent.cpp`
- `Source/BathhouseSim/Public/Placement/PlayerFacilityPlacementComponent.h`
- `Source/BathhouseSim/Private/Placement/FacilityPlacementDefinition.cpp`
- `Source/BathhouseSim/Private/Tests/FacilityPlacementAutomationTests.cpp`

Required correction:

1. A Definition with no preview class, a failed spawn, or a lost/destroyed preview must fail closed. It may not expose an enabled placement row or commit an invisible placement.
2. Track preview validity with `IsValid`, bind/unbind destruction if needed, clear zone grid visibility exactly once, and report a localized placement failure while keeping the gameplay Actor held and unchanged.
3. Preserve Editor asset validation, but enforce the runtime safety invariant independently because cooked/runtime code cannot rely on `IsDataValid()`.

Acceptance:

- Missing class, spawn failure and external preview destruction each disable placement, clear the prior grid/preview session safely and preserve the held Actor.
- A valid preview follows the candidate and successful commit destroys only the preview, never the gameplay Actor.
- Update the happy-path test to supply a real preview class; it must no longer demonstrate success without one.

## P1 — Overlap Checks Do Not Implement “Blocking Overlap”

Both placement and recovery call `OverlapMultiByObjectType()` and treat any returned Actor as blocking. This rejects QueryOnly triggers or components whose collision response is intentionally overlap/ignore. Recovery checks only `WorldStatic` and `WorldDynamic`, so an object using another object type that the package profile actually blocks, notably `PhysicsBody`, can be missed immediately before collision and simulation are enabled.

Affected files:

- `Source/BathhouseSim/Private/Placement/FacilityPlacementComponent.cpp`
- `Source/BathhouseSim/Private/Placement/PlayerFacilityPlacementComponent.cpp`
- placement collision automation tests

Required correction:

1. Evaluate the package/placement collision contract using UE 5.8 blocking semantics and the intended collision profile/channel, including every object type the enabled package can actually block.
2. Do not reject non-blocking overlap volumes. Continue to ignore the facility itself, player owner and active placement zone where required.
3. Keep footprint containment separate from collision. Preserve four-corner floor support and validate the transformed footprint component rather than the Actor origin.
4. Verify scaled/rotated zone-local bounds consistently; do not compare inverse-scaled local coordinates with scaled world extents.

Acceptance:

- A non-blocking trigger inside the footprint does not invalidate placement/recovery.
- A blocking `WorldStatic`, `WorldDynamic`, `Pawn` where intended, and blocking `PhysicsBody` are rejected according to the actual enabled profile.
- The exact support floor remains accepted without allowing penetration.
- Rotated and non-unit-scaled zone tests prove full containment and floor checks use consistent coordinate spaces.

## P2 — Suppression, Target EndPlay And Local-Player Lifecycle Are Polling-Dependent

Placement/recovery entry and commit functions do not recheck `Interaction->IsInteractionSuppressed()`. Suppression is noticed only by the component Tick, while the Character's Q Completed path calls `CompleteRecoveryHold()` without a computer-focus check. A focus/suppression change and Q completion in the same input frame can therefore commit before polling cancels it. Target destruction invalidates the weak pointer but does not run the cancellation path until a later completion, leaving internal elapsed/query state stale. The component also ticks and traces without the local-control guard used by `UPlayerInteractionComponent`, then forces an additional interaction refresh every frame even when idle.

Affected files:

- `Source/BathhouseSim/Private/Placement/PlayerFacilityPlacementComponent.cpp`
- `Source/BathhouseSim/Private/Character/FirstPersonCharacter.cpp`
- placement lifecycle/input automation tests

Required correction:

1. Revalidate suppression/local ownership in Begin, update and final commit paths, not only on Tick. A suppressed placement/recovery attempt must fail/cancel without mutation.
2. Cancel target/owner EndPlay deterministically, reset progress/query once and remove any destruction delegates during cleanup.
3. Limit preview creation, camera traces and prompt refresh to the locally controlled player. Avoid the unconditional duplicate interaction refresh/trace while idle; enable work only when required without losing the passive recovery prompt.
4. Preserve LMB ownership `Computer > Placement > Equipment`, held-item-independent Q recovery and idempotent cancel semantics.

Acceptance:

- Computer focus or generic interaction suppression immediately before LMB/Q completion cannot commit.
- Target destruction and owner/component EndPlay reset the session with no stale progress, delegate or preview/grid state.
- A non-locally-controlled pawn spawns no preview and performs no placement/recovery camera traces.
- Existing primary, secondary and equipment rows remain unchanged, and placement still suppresses only the equipment row.

## P2 — Weak Registry Recovery And Required Automation Coverage Are Incomplete

`ULockerCapacitySubsystem` stores weak banks, slots and customer owners but never compacts invalid entries. `InstalledLockerCapacity` and `Leases.Num()` can therefore remain inflated if normal EndPlay cleanup is bypassed or reentered. The focused tests cover two happy-path aggregates but omit many minimum cases required by `.md/PROMPT_IMPLEMENTATION.md`, including placement late-failure rollback, preview cleanup/loss, E/G behavior, washer/bath recovery gates, customer check-in rollback/cleanup, random clothes-slot use and weak-entry recovery.

Affected files:

- `Source/BathhouseSim/Public/Facility/LockerCapacitySubsystem.h`
- `Source/BathhouseSim/Private/Facility/LockerCapacitySubsystem.cpp`
- `Source/BathhouseSim/Private/Tests/FacilityPlacementAutomationTests.cpp`
- relevant customer/towel automation fixtures

Required correction:

1. Add a deterministic, mutation-safe weak-entry compaction/teardown policy so invalid bank/slot/customer entries cannot grant capacity or block it forever. Preserve active customer leases when a locker bank unexpectedly ends, as required by the canonical architecture; do not conflate bank loss with customer-owner loss.
2. Keep release idempotent and keep provisional leases counted against admission/removal until commit or rollback.
3. Add the missing minimum automation coverage from the implementation prompt, with concrete state/delegate/revision assertions rather than only mode/count happy paths.
4. Treat the existing towel-presentation warning fixture separately; do not suppress unexpected new warnings/errors globally.

## Blueprint, Compatibility And Documentation

- Do not modify or resave `Content/` or `Config/` in this rework.
- Preserve the five newly required Widget fields exactly: `PlacementActionNameText`, `PlacementFailureReasonText`, `RecoveryActionNameText`, `RecoveryFailureReasonText`, `RecoveryProgressBar`. C++ continues to own visibility, text, progress and transient failure state; Blueprint remains layout/style/animation/asset wiring only.
- Preserve existing reflected signatures and the ordinals of `EPhysicalCarryKind`, `EPlayerInteractionIntent`, `EBathhouseFacilityType` and `EBathhouseCustomerActivity`. Keep deprecated shoe/numbered symbols for migration and out of the active routine.
- No Core Redirect is currently required because this task does not need a reflected rename/delete. Do not introduce one to solve runtime logic.
- `DeveloperSettings` remains a justified runtime dependency through `UFacilityPlacementSettings`.
- After Source corrections, update the affected architecture status/flow text, `.md/PROMPT_REVIEW.md` and `.md/PROMPT_UNREAL.md` to match the actual final contract. Do not describe the Source stage as implemented until the new rollback/lifecycle tests pass.

## Regression Checks

- `git diff --check`
- no `Content/`, `Config/` or `.uproject` mutation
- Editor/Live Coding closed
- exact UE 5.8 `Build.bat BathhouseSimEditor Win64 Development` succeeds
- focused placement tests include every new failure/reentrancy/lifecycle fixture and pass
- full `BathhouseSim` automation group passes; report exact success/warning/failure counts
- independent headless log scan contains only the five expected pre-Editor `WBP_InteractionPrompt` missing-bind errors, plus separately identified pre-existing warnings; no assertion, ensure, fatal error, access violation or unexpected Blueprint/script error

## Review Resubmission

Regenerate `.md/PROMPT_REVIEW.md` and report:

- the transaction snapshot, commit order, deferred publication and forced-failure rollback evidence
- preview spawn/loss fail-closed behavior
- collision profile/channel and blocking-overlap tests, including `PhysicsBody` and non-blocking triggers
- suppression, target/owner EndPlay and local-player trace cleanup
- weak-registry compaction and customer lease preservation behavior
- exact Widget/Blueprint/Core Redirect compatibility conclusion
- UE 5.8 build, focused tests, full automation counts and independent log scan
