# Implementation Rework Prompt — Facility Actor Conversion Domain Safety And Lifecycle

## Objective

Fix the remaining pre-Editor defects in the dedicated facility-item Actor replacement flow. Keep the approved split between placed facility Actors and the exact native `APlaceableFacilityItemActor`, the existing single-carry owner, typed payload boundary, staged/silent registry transitions, and Blueprint compatibility fields.

Do not modify or resave `Content/`, `Config/`, or `.uproject`. Do not restore the legacy same-Actor `Placed/Packaged` path, add facility items to generic fixed slots, copy complete Actors, or move towel inventory ownership into Placement.

## P1 — Clean Stack And Used Bin Conversion Can Lose Or Duplicate Towel Tokens

`ACleanTowelStackActor` and `AUsedTowelBinActor` inherit the generic `ABathhouseFacilityActor::QueryFacilityRecovery()` and payload import. The generic recovery gate checks facility slots, bath water and locker capacity, but never checks these subclasses' authoritative towel inventory. Their inventories are excluded from `FFacilityPlacementPayload`, so a non-empty stack/bin can currently be destroyed by recovery.

This is not only a missing gate. `UTowelInventoryComponent::EndPlay()` moves non-empty contents to the circulation recovery ledger, while a newly spawned clean stack runs its authored default `InitialCount` (`20` in native defaults). Therefore a recover/place cycle can put the old stock in the recovery ledger and independently create the new stack's default stock. A staged clean stack destroyed during a late placement rollback can also recover its default stock despite never becoming an authoritative placed endpoint.

Affected files:

- `Source/BathhouseSim/Private/Facility/BathhouseFacilityActor.cpp`
- `Source/BathhouseSim/Public|Private/Towel/CleanTowelStackActor.*`
- `Source/BathhouseSim/Public|Private/Towel/UsedTowelBinActor.*`
- `Source/BathhouseSim/Public|Private/Towel/TowelInventoryComponent.*` only if a focused silent staging/reset API is required
- `Source/BathhouseSim/Private/Tests/FacilityPlacementAutomationTests.cpp`
- relevant Towel automation fixtures

Required correction:

1. Give clean stack and used bin domain-owned recovery gates that require authoritative inventory count `0`. Do not serialize or copy towel contents into the placement payload.
2. Ensure item-to-facility placement initializes the new stack/bin in the canonical empty state before `FinishSpawning`/BeginPlay can expose or recover authored initial stock. Preserve the authored capacity and normal initial-level behavior; only Actor-conversion staging should override runtime contents.
3. Ensure destroying a staged or rolled-back target cannot call `RecoverInventory()` for stock that was never committed as an authoritative endpoint. A successful source recovery with the empty gate must likewise produce no towel-ledger mutation.
4. Keep interaction/overflow/transfer guards in the Towel domain. Placement may coordinate lifecycle flags but must not own towel counts or revisions.

Acceptance:

- Non-empty clean stack and used bin recovery are rejected without Actor, inventory, revision, recovery-ledger, registry, Nav or prompt-state mutation.
- An empty clean stack round-trip places an empty stack with the authored capacity, not the class's normal initial stock.
- A forced late placement failure after `FinishSpawning` leaves the original item held and does not change any towel inventory, recovery count, pending spill count or presentation revision.
- Successful empty stack/bin round-trips preserve the global towel-token total and create exactly one authoritative Actor.

## P1 — Facility Item Fall Recovery Leaves The Carry Owner Stuck

`APlaceableFacilityItemActor::FellOutOfWorld()` directly calls `RecoverPhysicalCarryable()`. Unlike the existing key, mop, basket and wrench implementations, `RecoverPhysicalCarryable()` never asks a live `UPlayerCarryComponent` to run `RecoverHeldPhysicalObject()` first. When a held facility item falls below Kill Z, the item clears its own `Carrier`, detaches and returns to the world, but `UPlayerCarryComponent::HeldObject` still points to it. The hand remains occupied while the item reports that it is not held, so placement and G drop fail closed indefinitely.

Affected files:

- `Source/BathhouseSim/Private/Placement/PlaceableFacilityItemActor.cpp`
- `Source/BathhouseSim/Private/Tests/FacilityPlacementAutomationTests.cpp` or the focused physical-carry automation file

Required correction:

1. Follow the existing carry recovery handshake: if the recorded carrier still holds this exact Actor, let `UPlayerCarryComponent::RecoverHeldPhysicalObject()` silently clear ownership and publish the final held change once; perform local last-safe free-world recovery only after carry ownership is no longer authoritative.
2. Preserve the placement-consumption/staged guards, Root scale, CCD, Pawn Ignore, last-safe transform and zero velocities.
3. Keep recursion/reentry idempotent across FellOutOfWorld, carrier EndPlay, item EndPlay and placement consumption.

Acceptance:

- A held facility item forced through FellOutOfWorld leaves the carry hand empty, the same item at its last-safe transform in free-world physics, and one held-change publication.
- A free-world fall recovers the same item without creating a facility or a second item.
- Placement-consumed and staged items never run general fall recovery.

## P1 — Placement Publication Dereferences A Facility Destroyed By Its Own Callback

After the item has been consumed, `FFacilityActorConversionTransaction::PlaceItemAsFacility()` checks `NewFacilityWeak` only before calling `PublishPlacedDomainRegistration()`, then unconditionally calls `NewPlacement->EndTransition()`. `ABathhouseFacilityActor::PublishPlacedDomainRegistration()` broadcasts facility availability and then continues to read `this`/`GetWorld()` and publish locker capacity. A synchronous facility observer may destroy the new Actor during the first broadcast. The code then continues through an ending Actor/component, and locker EndPlay can publish removal while the original placement publisher still emits another capacity mutation.

Affected files:

- `Source/BathhouseSim/Private/Placement/FacilityActorConversionTransaction.cpp`
- `Source/BathhouseSim/Private/Facility/BathhouseFacilityPlacementDomain.cpp`
- facility/locker automation fixtures

Required correction:

1. Never dereference `NewFacility`, `NewPlacement`, or Actor-owned state after an external publication without revalidating a weak identity.
2. Make the facility/capacity publication sequence safe if the first callback destroys the Actor. The final registry/capacity state and revision count must reflect the committed placement followed by the explicit external destruction, without a third/stale publication.
3. Preserve the transition guard during publication so synchronous recovery/replacement reentry is rejected, but do not require calling `EndTransition()` on an invalid component.
4. Apply the same callback-destruction audit to recovery publication and item/source destruction callbacks.

Acceptance:

- A facility-availability listener that destroys the just-placed shower or locker causes no invalid UObject access, ensure, stale registry entry or duplicate capacity revision.
- A non-destructive observer still sees empty hand and final facility/locker registry and receives exactly one placement publication.
- Synchronous recovery from the placement notification remains rejected.

## P2 — Custom Recovery Mesh Offset Can Pass Validation But Bypass The Commit Overlap

`ValidateRecoveryMesh()` permits a non-zero simple-box center whenever it equals `Mesh.GetBounds().Origin`. The passive query accounts for that offset, but the post-spawn commit query places `ItemRoot->GetCollisionShape()` at `ItemRoot->GetComponentLocation()` and does not apply the box/bounds center. This disagrees with `PROMPT_UNREAL.md`, which requires an offset-free box, and can approve an asset whose actual physics body overlaps a blocker outside the checked shape.

Affected files:

- `Source/BathhouseSim/Private/Placement/PlaceableFacilityItemCollision.cpp`
- `Source/BathhouseSim/Private/Placement/FacilityActorConversionTransaction.cpp`
- placement collision automation fixtures

Required correction:

- Either enforce zero mesh-bounds origin and zero `FKBoxElem::Center` as the documented asset contract, or derive both passive and post-spawn overlap transforms from the exact same box center/rotation. Keep the single-box, bounds-match and no-rotation constraints coherent.

Acceptance:

- An offset mesh/box pair cannot pass Data Validation under the current offset-free authoring contract.
- Passive and actual-item commit checks use the same world center, rotation and extents for fallback and custom meshes.

## P2 — Payload Validation Does Not Fully Enforce Its Generic Reference Invariant

`FFacilityPlacementPayload::Validate()` iterates only direct `FObjectPropertyBase` fields. Object references nested in arrays, sets, maps or reflected structs are not visited, and unresolved soft/reference variants need an explicit policy. The current two final payload classes contain only scalar fields, which limits immediate exposure, but the base payload contract claims to reject Actor/Component runtime references for every typed instance-data class.

Affected files:

- `Source/BathhouseSim/Private/Placement/FacilityPlacementPayload.cpp`
- payload automation fixtures

Required correction:

- Implement a recursive reflected-property validator, or constrain the allowed property kinds/classes so container/nested/object-reference forms fail closed. Keep exact Outer and exact domain-type validation in each importer.

Acceptance:

- Direct, nested-struct and container Actor/ActorComponent references are rejected; scalar-only facility and machine payloads remain valid.

## P2 — Data Validation, Required Failure Coverage And Canonical Status Are Incomplete

`UFacilityPlacementDefinition::IsDataValid()` returns `Super::IsDataValid()` unchanged on a valid asset. UE's base implementation returns `NotValidated`, while neighboring project validators normalize that result to `Valid`. Consequently the Editor handoff cannot reliably satisfy its stated Data Validation success gate.

The current placement suite passes two tests, but the rewrite removed still-relevant suppression, external preview destruction, non-local owner, late carry failure, unexpected locker loss and synchronous destruction fixtures. The new implementation prompt additionally requires forced spawn/import/domain/carry/source-destroy/silent-unregister rollback, item lifecycle, CDO scale and clean/used endpoint coverage. `.md/PROMPT_REVIEW.md` explicitly admits that source-destroy and silent-unregister fault injection was not added.

The changed canonical documents also still say the Actor-replacement Source is “implementation pending” while `.md/PROMPT_REVIEW.md` and `.md/PROMPT_UNREAL.md` say it is complete. `PlacementSystem.md` omits `PlaceableFacilityItemCollision.cpp` from its Source scope.

Affected files:

- `Source/BathhouseSim/Private/Placement/FacilityPlacementDefinition.cpp`
- `Source/BathhouseSim/Private/Tests/FacilityPlacementAutomationTests.cpp`
- focused carry/towel test files as appropriate
- `.md/0_ARCHITECTURE.md`
- `.md/Architecture/PlacementSystem.md`
- `.md/Architecture/PhysicalCarrySystem.md`
- `.md/Architecture/TowelSystem.md`
- `.md/PROMPT_REVIEW.md`
- `.md/PROMPT_UNREAL.md`

Required correction:

1. Return `Valid` when the superclass result is `NotValidated` and all facility-definition checks pass; retain warnings for Cube fallback without turning a valid asset into `NotValidated`.
2. Restore/adapt the removed lifecycle and atomicity coverage, and add deterministic test seams for every fallible stage. Assertions must cover Actor counts/identity, payload Outer/type, carry snapshot, Root scale, registry/Nav/capacity revisions, towel recovery ledger, event order and reentry.
3. Add a non-unit target CDO/root/footprint test that proves the CDO-derived candidate matches the actual deferred-spawn result and neither source nor item scale leaks across the conversion.
4. Update canonical implementation-status and Source-scope text only after the corrected Source and tests pass. Regenerate the review and Unreal handoff prompts so they no longer claim completion prematurely and include clean/used empty-state PIE checks.

## Blueprint, Compatibility And Ownership

- Preserve `EPlaceableFacilityMode` and existing enum ordinals.
- Preserve reflected `Mode`, `HeldTransform`, release values, `OnModeChanged`, `PackagePhysicalRoot`, native parents and existing BlueprintCallable/Assignable names for the migration cycle.
- Keep `RecoveryItemClass` as the exact native common class and keep facility items `FreeDrop`-only.
- No Core Redirect is required unless the rework introduces a reflected rename/delete; none is needed for the corrections above.
- Keep C++ responsible for payload, collision, lifecycle, spawn/destroy and rollback. Blueprint remains class/mesh/layout/presentation authoring only.
- Keep `UPlayerFacilityPlacementComponent` as session coordinator, `UPlayerCarryComponent` as held-reference owner, Towel inventory/circulation as token owner, and the private conversion helper as replacement mechanics owner.

## Regression Verification

- `git diff --check`
- no `Content/`, `Config/` or `.uproject` mutation
- Editor/Live Coding closed
- exact UE 5.8 `Build.bat BathhouseSimEditor Win64 Development` succeeds
- corrected `BathhouseSim.Placement`, `BathhouseSim.Interaction.PhysicalCarry` and `BathhouseSim.Towel` groups pass
- independent log scan contains no assertion, ensure, fatal error, access violation or unexpected Blueprint/script error; report the existing animation and intentional towel-presentation warnings separately

## Review Resubmission

Regenerate `.md/PROMPT_REVIEW.md` with concrete evidence for:

- clean/used empty gates, empty converted initialization and towel-token conservation
- held facility-item FellOutOfWorld recovery through the carry owner
- callback-destruction-safe publication and exact revision/event counts
- custom-mesh collision center consistency
- recursive payload reference rejection
- every forced failure rollback and non-unit CDO/Root scale case
- UE 5.8 build and exact focused automation pass counts
- Blueprint/Core Redirect compatibility and the remaining Editor-only checks
