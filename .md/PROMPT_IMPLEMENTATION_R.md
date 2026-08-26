# Implementation Rework Prompt — Physical Carry Atomic Commit And Fall Recovery

## Objective

Fix the physical-carry placement commit order so mechanical state, concrete domain ownership, `HeldObject`, slot occupancy and presentation delegates cannot diverge under synchronous delegate reentry or a late commit failure. Also route held wet-mop fall recovery through the equipment-use/motion coordinator before fixed-slot or last-safe recovery.

Do not modify or resave `Content/`. Preserve the direct `IPhysicalCarryable` implementation policy, exact assigned fixed slots, held-position free drop, key/customer/counter behavior, basket inventory state and the C++/Blueprint presentation boundary.

## P1 — Placement Notifications Run Before The Authoritative Carry Commit

`UPlayerCarryComponent` invokes concrete `Notify*Committed` methods before the final `CommitHeldObject()` or `ClearHeldObject()`:

- fixed-slot take: `PlayerCarryComponent.cpp:166-181`
- fixed-slot store: `PlayerCarryComponent.cpp:218-233`
- free drop: `PlayerCarryComponent.cpp:290-305`

Those concrete methods mutate `Carrier` or key state and synchronously broadcast Blueprint-assignable presentation delegates. The private placement transaction only rolls back attachment, transform, collision, physics and slot occupancy; it does not restore `HeldObject`, concrete ownership/state or already-broadcast delegates. The placement guard rejects the tested high-level nested drop, but public low-level `CommitReleasePhysicalObject()` bypasses that guard.

A synchronous listener can therefore clear the held reference during `OnHeldPresentationChanged(false)`. The outer `ClearHeldObject()` then fails, the mechanical transaction rolls back to the held pose, but the hand and concrete owner remain cleared. Fixed-slot store rollback can additionally broadcast false and true for one failed operation. Actor destruction from a Blueprint presentation listener can likewise invalidate the item before the remaining commit steps.

Affected files:

- `Source/BathhouseSim/Public/Interaction/PhysicalCarryable.h`
- `Source/BathhouseSim/Public/Interaction/PlayerCarryComponent.h`
- `Source/BathhouseSim/Private/Interaction/PlayerCarryComponent.cpp`
- `Source/BathhouseSim/Private/Interaction/PhysicalCarryPlacementTransaction.h/.cpp` only if its private scope needs extension
- key/mop/basket/wrench concrete Actor files
- focused physical-carry automation and test probes

Required correction:

1. Make fixed take, fixed store and free drop true two-phase operations. Validation and fallible mechanical/domain preparation must complete before externally observable delegates run.
2. Commit `HeldObject`, concrete owner/key state and slot occupancy as one coordinator-owned transition. Do not label or broadcast a concrete transition as committed while the final carry mutation can still fail.
3. Defer `OnHeldPresentationChanged`, key-state presentation, `OnHeldObjectChanged` and slot occupancy notifications until the transition can no longer roll back; each logical success broadcasts each applicable change exactly once.
4. Ensure every failure before that point restores attachment, transform, collision, physics, slot occupancy, held identity and concrete domain owner/state without compensating false/true presentation broadcasts.
5. Harden or narrow low-level carry mutation APIs so they cannot bypass `bPhysicalDropCommitInProgress` from a synchronous native callback. Preserve required recovery/legacy key call sites without creating another state owner.
6. Keep `FPhysicalCarryPlacementTransaction` private and non-UObject. Do not move domain state into a common carryable Actor or component.

Acceptance:

- Add forced reentry/late-failure coverage for `FixedSlot -> Held`, `Held -> FixedSlot` and `Held -> FreeWorld`.
- At least one synchronous presentation listener must attempt a low-level held mutation, not only the already-guarded high-level drop API.
- Failure leaves `HeldObject`, concrete `Carrier`/key owner state, slot occupancy, attachment, transform, collision and physics exactly at the pre-operation snapshot.
- Failed operations emit no committed presentation transition; successful operations emit item, carry and slot changes exactly once.
- Item destruction or invalidation from an externally observable callback cannot cause stale access or a partially committed surviving item.

## P1 — Wet Mop FellOutOfWorld Leaves Equipment Use And Motion Active

`AWetMopActor::RecoverPhysicalCarryable()` creates a context containing only `User`, calls `StopMopping()`, and then directly calls `Carrier->CommitReleasePhysicalObject(this)` (`WetMopActor.cpp:352-380`). Because the context has no `MotionComponent` and the carry coordinator's placement cancellation is bypassed:

- `UPlayerEquipmentUseComponent::bInputActive`, `ActiveEquipment` and retained context can remain active;
- `UHeldEquipmentMotionComponent` can continue targeting the recovered mop;
- fixed-slot recovery can be overwritten on the next motion tick or later LMB release by the old held baseline transform.

This violates the required active-use/motion cancellation and `FellOutOfWorld` recovery lifecycle.

Affected files:

- `Source/BathhouseSim/Private/Cleaning/WetMopActor.cpp`
- `Source/BathhouseSim/Public/Interaction/PlayerCarryComponent.h` and `.cpp` if a coordinator-owned recovery entry point is needed
- equipment-use/motion files only if a focused defect requires it
- physical-carry or combat/recovery automation

Required correction:

1. Before a held mop is released or recovered, route cleanup through the configured player carry/equipment-use coordinator so active equipment use and held motion are canceled exactly once with the complete retained context.
2. Only after cleanup may the same mop instance recover to its valid empty fixed slot or last-safe world transform.
3. Preserve fixed-slot-first recovery, no replacement spawn, stain cancellation and next-use behavior.
4. Do not make the mop discover player components independently or duplicate equipment-use ownership in the Actor.

Acceptance:

- Use an `AFirstPersonCharacter` fixture with configured carry, equipment-use and motion components.
- Begin Hold mopping, invoke the actual mop fall-recovery path, and assert the equipment input owner is inactive, motion is stopped, the carry hand is empty and mopping/stain state is canceled.
- For fixed-slot recovery, tick the world and issue the later input-release path; the mop must remain at the exact slot anchor with physics disabled and occupancy true.
- Cover last-safe fallback as appropriate and prove the same Actor can be taken and used again.

## Documentation And Editor Handoff

- No architecture redesign is required. Keep `UPlayerCarryComponent` as transition/commit owner and concrete Actors as domain-state owners.
- Update `.md/PROMPT_REVIEW.md` with the new atomicity and active-fall recovery assertions plus the exact final automation count.
- Update `.md/PROMPT_UNREAL.md` only if the actual reflected C++ contract changes. Otherwise preserve its Blueprint instance/layout/asset authoring scope.
- No Core Redirect is required unless a reflected symbol is actually renamed or removed; prefer append-only/private corrections.

## Regression Checks

- `git diff --check`
- no `Content/`, `Config/`, `.uproject` or runtime module dependency change
- Editor/Live Coding closed
- UE 5.8 `Build.bat BathhouseSimEditor Win64 Development` succeeds
- full `BathhouseSim` automation succeeds, including the new focused regressions
- headless log contains no unexpected `LogBlueprint: Error`, `LogScript: Error`, ensure, assertion, fatal error or access violation
- key drop/re-pick/hook/customer/counter, basket inventory/revision, exact/wrong/duplicate slots, overlap rollback, two-mass `120/15`, Pawn Ignore and carrier/slot/item lifecycle regressions remain green

## Review Resubmission

Regenerate `.md/PROMPT_REVIEW.md` for this exact task and report:

- the final two-phase commit and delegate ordering
- how low-level mutation reentry is prevented
- full rollback assertions for all three transitions
- active mop `FellOutOfWorld` cleanup and post-tick slot-pose assertions
- build, automation count and independent log scan
- confirmation that `Content/` and `Config/` were not changed
