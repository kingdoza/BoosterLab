# Implementation Rework Prompt — Unified Physical Carry Drop And Wall Sweep

## Objective

Fix the start-penetration direction defect and the canonical Interaction document contradiction found in pre-Editor code review. Preserve the approved common drop ownership, existing pickup/recovery behavior, key G rejection, reflected properties and authored throw values.

Do not modify or resave `Content/`. After the correction, regenerate `.md/PROMPT_REVIEW.md` and `.md/PROMPT_UNREAL.md` for this task and submit the complete C++ change for review again.

## P1 — Start Penetration Can Accept A Far-Side Wall Candidate

Files:

- `Source/BathhouseSim/Private/Interaction/PlayerCarryComponent.cpp`
- `Source/BathhouseSim/Private/Tests/CleaningTowelAutomationTests.cpp`
- corresponding private declarations only if required

In `ResolvePhysicalDropLocation()`, the start-penetration candidate is calculated as:

```cpp
SweepStart + Hit.Normal * (Hit.PenetrationDepth + Clearance)
```

It is then projected onto `[SweepStart, SweepEnd]` and accepted when the final overlap test is clear. This does not establish that the candidate is on the player side of the blocking wall.

For a normal wall in front of the player:

- an MTD normal toward the player produces a point behind `SweepStart`; the segment clamp collapses it back to the still-overlapping start and rejects it;
- an MTD normal toward `SweepEnd` can produce an overlap-free point after crossing a thin wall, and the current code accepts that far-side point.

The latter violates the explicit requirement that start penetration must not force the object through or beyond the wall.

Required correction:

- distinguish a player-side resolution from an MTD that advances through the initial blocker;
- never accept a start-penetration candidate merely because it is overlap-free on the target side of the wall;
- if a player-side nonpenetrating candidate cannot be represented safely under the allowed placement constraints, fail conservatively and keep the item held;
- continue using the same shape, channel and ignored actors for final overlap validation;
- keep normal non-starting blocking-hit behavior based on `Hit.Location`, bounds offset and clearance unchanged;
- preserve the no-mutation failure contract: attachment, `HeldObject`, concrete `Carrier`, collision/physics, presentation and impulse remain unchanged.

Regression coverage must include a thin-wall start-overlap where the engine MTD can point toward the throw target. Prove that the result either stays on the player side or fails while held; it must never succeed beyond the wall. Retain the existing large-blocker rejection, retry, normal wall, mop/basket and key tests.

Also add focused delegate-reentry coverage for the implementation's existing `bPhysicalDropCommitInProgress` claim: a nested drop attempt from a completion/held delegate must not duplicate the physical commit or leave `HeldObject` and the concrete actor's held presentation inconsistent.

## P2 — Interaction Canonical Document Contradicts The Implemented G Drop

File:

- `.md/Architecture/InteractionSystem.md`

The `UPlayerCarryComponent` section still says that floor drop is outside the current scope, while the immediately following bullets define and approve mop/basket G world drop. This leaves the canonical responsibility boundary internally contradictory.

Required correction:

- remove or qualify the stale statement so it cannot exclude the implemented mop/basket G physical drop;
- keep arbitrary key floor drop and temporary shelf storage out of scope;
- retain `UPlayerCarryComponent` as the only normal G detach/place/physics/impulse owner;
- do not change `.md/0_ARCHITECTURE.md` unless the corrected wording exposes a real map-level conflict.

## Accepted Areas To Preserve

- `IPhysicalCarryable` provides the root primitive, existing distance/impulse and post-commit notification;
- wet mop and towel basket do not perform normal G detach, placement, physics enable or impulse themselves;
- world AABB extent, identity sweep rotation, `ECC_Visibility`, bounds-center offset and owner/object ignore rules;
- regular blocking hit uses `Hit.Location`, not `ImpactPoint`;
- placement/physics failure restores the held attachment, transform, collision and authoritative reference;
- `ThrowSpawnDistance` and `ThrowImpulseStrength` defaults remain unchanged;
- key remains non-droppable through G;
- pickup, recovery, FellOutOfWorld, input mapping, towel/cleaning domain logic and Blueprint assets remain out of this rework scope.

## Class Responsibility And Blueprint Contract

- `UPlayerCarryComponent` remains the held-object and common physical-drop transaction owner. The private sweep helper has no independent lifecycle/state, so no new component split is required for this correction.
- `AWetMopActor` and `ATowelBasketActor` retain only concrete carrier, recovery, last-safe transform and presentation responsibilities.
- reflected `DropSweepChannel` and `DropSweepClearance` names/types remain unchanged; no Core Redirect is required.
- `.md/Architecture/CoreSystem.md` still does not contain the concrete Class Growth Policy referenced by the agent rules. Report that canonical gap without inventing a threshold.
- `.md/PROMPT_UNREAL.md` remains validation-only unless the corrected C++ contract genuinely changes its PIE instructions.

## Verification

With all BathhouseSim Editor and Live Coding processes closed:

1. run the UE 5.8 `Build.bat` Editor target from the first attempt outside the sandbox;
2. run `Automation RunTests BathhouseSim` and the focused physical carry test;
3. prove the new far-side start-penetration regression, nested delegate reentry, existing normal wall placement and failed-drop retry all pass;
4. run `git diff --check`;
5. search for stale `HandleReleasedBy`, concrete normal-drop `AddImpulse`/`SetActorLocation`, reflected-name changes and the contradictory architecture wording;
6. report exact commands, test count, failures and remaining PIE-only items in regenerated `.md/PROMPT_REVIEW.md`.

## Resubmission Acceptance

- no start-penetration path can place a mop or basket beyond the initial wall;
- unsafe start penetration fails without mutating held state;
- delegate reentry cannot duplicate the commit or split carry/concrete held state;
- normal wall, empty-space and retry behavior remain intact for both equipment types;
- Interaction documentation describes G world drop without contradicting its own scope;
- Editor target and complete BathhouseSim automation suite pass;
- no `Content/`, Config, `.uproject` or unrelated Source change is introduced by this rework.
