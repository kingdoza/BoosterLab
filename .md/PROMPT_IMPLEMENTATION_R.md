# Implementation Rework Prompt — Queue Teardown, Point Uniqueness And Key Drop Direction

## Objective

Fix the remaining pre-Editor defects in the Counter queue/navigation lifecycle and checkout key placement contract. Preserve the single weak FIFO per lane, `UCustomerQueueNavigationComponent` ownership, native StateTree task, checkout overflow volume, same-key physical return, deprecated reflected compatibility symbols and the C++/Blueprint responsibility boundary.

Do not modify or resave `Content/`. Do not redesign the queue into separate visible/overflow arrays, move navigation into Counter/Session, restore returned-key runtime slots, or spawn a replacement key.

## P1 — Intentional Queue Removal Can Reenter As A Technical Abort

`UCustomerSessionComponent::LeaveQueue()` clears `QueueLane` and then calls `ABathhouseCounterActor::DequeueActor()`. Dequeue synchronously broadcasts `OnQueueChangedNative`. If the leaving customer still has an active `UCustomerQueueNavigationComponent` execution, its bound `HandleQueueChanged()` immediately calls `RefreshAssignment()`, observes `Session->GetQueueLane() != ActiveLane`, and calls `TechnicalAbort()`.

This is reachable during normal Actor teardown because `UCustomerSessionComponent::EndPlay()` calls `LeaveQueue()` while queue-navigation cleanup is owned by another component's `EndPlay()`. UE 5.8 iterates the Actor's `OwnedComponents` set for component `EndPlay`, so ordering between those two components is not a lifecycle contract. The same hazard applies to any intentional leave that occurs before the StateTree queue task has released its execution token.

Affected files:

- `Source/BathhouseSim/Private/Customer/CustomerSessionComponent.cpp`
- `Source/BathhouseSim/Public/Customer/CustomerQueueNavigationComponent.h`
- `Source/BathhouseSim/Private/Customer/CustomerQueueNavigationComponent.cpp`
- customer queue automation in `Source/BathhouseSim/Private/Tests/BathhouseDomainTests.cpp`

Required correction:

1. Add an explicit intentional-removal/teardown path that cancels the active queue execution before the synchronous Counter dequeue notification can be interpreted as assignment corruption, or make the navigation callback distinguish owner-authorized leave from an unexpected invalid assignment.
2. Keep invalid membership, missing Counter and externally corrupted assignment as technical failures during a genuinely active queue operation.
3. Ensure the correction is independent of component `EndPlay` ordering and is idempotent when StateTree `ExitState`, interruption cleanup and Actor teardown converge.
4. Preserve one Counter revision/broadcast for the logical dequeue so remaining customers still refresh and promote.
5. On every normal leave/EndPlay path, cancel the active `UAITask_MoveTo`, remove the Counter delegate, invalidate tokens, disable Tick and restore snapshotted movement flags exactly once.

Acceptance:

- Spawn or construct a real `ABathhouseCustomerCharacter` with Session and Queue Navigation, enqueue it, start native queue navigation, then exercise intentional `LeaveQueue()` and actual Actor destruction/`EndPlay` while the execution is active.
- Neither path may set `TechnicalAbort`, emit the queue-assignment technical-abort error, or reenter cleanup.
- The leaving entry is removed once, remaining FIFO entries receive exactly one lane notification/promotion, and no move/delegate/token/Tick/movement-flag state survives teardown.
- Add a separate negative fixture proving an unexpectedly missing active assignment still produces the technical failure policy.

## P1 — Native Service Points Are Accepted As Queue-Point References

`ABathhouseCounterActor::ResolveConfiguredPoints()` starts `UsedAcrossRoles` empty and only inserts resolved elements from `CheckInQueuePointReferences` and `CheckoutQueuePointReferences`. A queue array can therefore reference inherited `CheckInServicePoint` or `CheckoutServicePoint` and pass native resolution. The front customer and a queued customer can then receive the same component transform, violating the distinct visible pose and authoring contract. `.md/PROMPT_UNREAL.md` warns the Editor step not to do this, but native validation currently does not enforce it.

Affected files:

- `Source/BathhouseSim/Public/Facility/BathhouseCounterActor.h`
- `Source/BathhouseSim/Private/Facility/BathhouseCounterActor.cpp`
- counter point-reference automation in `Source/BathhouseSim/Private/Tests/BathhouseDomainTests.cpp`

Required correction:

1. Reject either lane's native service component when resolving queue-point references. Also keep existing rejection of unset, unresolved, non-scene, foreign-owner, within-role duplicate and cross-role duplicate references.
2. Log an actionable Counter path, role and array index, and exclude the invalid reference while preserving the relative order of all valid authored points.
3. Do not scan by name/type/Actor and do not make Blueprint graph validation authoritative.
4. Keep the existing `EditInstanceOnly` component-picker contract and resolved private transient arrays.

Acceptance:

- Extend `BathhouseSim.Facility.CounterPointReferences` with each native service component referenced from queue arrays.
- Both references are rejected and omitted, expected errors are counted, valid point ordering is unchanged, and check-in/checkout cross-role duplicate coverage remains green.
- Assignment coverage proves no two logical visible indices are backed by the same accepted SceneComponent.

## P2 — Checkout Velocity Ignores The Authored Drop-Point Forward

`.md/Architecture/PhysicalCarrySystem.md` defines checkout velocity as the Counter drop-point forward plus world up. `CheckoutKeyPlacementUtils.cpp` instead uses `Counter.GetActorForwardVector()`. Rotating `ReturnedKeyDropPoint` independently changes the candidate transform but not the release direction, so Source and the current canonical architecture disagree. The current Unreal handoff only asks to position the point and therefore also omits the rotation authoring/verification implied by the canonical contract.

Affected files:

- `Source/BathhouseSim/Private/Interaction/CheckoutKeyPlacementUtils.cpp`
- checkout physical-key automation in `Source/BathhouseSim/Private/Tests/BathhouseDomainTests.cpp`
- `.md/PROMPT_REVIEW.md`
- `.md/PROMPT_UNREAL.md`

Required correction:

1. Use the stable `ReturnedKeyDropPoint` component's world forward vector for the key forward velocity-change term, with world up for the upward term, matching the canonical Physical Carry document.
2. Keep candidate orientation and local XY search based on the same drop-point transform.
3. Update the Unreal handoff to author and verify `ReturnedKeyDropPoint` rotation/Yaw as well as position. Keep Blueprint work limited to transform/search values and presentation event wiring.
4. Do not change the key's authorable forward/upward magnitudes or the common free-world transaction.

Acceptance:

- Add a test where the Counter Actor forward and `ReturnedKeyDropPoint` forward deliberately differ.
- The same key Actor is placed `OnCounter`, and its initial velocity-change direction follows the drop point rather than the Counter Actor while retaining the authored world-up component.
- Existing heavy/light, CCD, Pawn Ignore, candidate separation, idempotence, blocked rollback and cash-gate assertions remain green.

## Blueprint, Compatibility And Documentation

- Preserve `FCustomerQueueTargetTask`, `GetQueueTargetTransform`, `ReturnedKeyPointReferences`, `OnReturnedKeySlotsChanged` and the returned-slot C++ functions for the current deprecated migration cycle. Canonical runtime must not read or broadcast deprecated returned-slot state.
- No Core Redirect is required because no reflected symbol needs to be renamed or removed.
- Update `.md/PROMPT_REVIEW.md` with the corrected lifecycle order, service-point rejection, drop-point direction and exact final verification results.
- Update `.md/PROMPT_UNREAL.md` only to match the corrected native contract, including drop-point rotation authoring and the existing compile/save/reload/PIE checks.
- Architecture redesign is not required. Do not change the established ownership split among Counter, Queue Navigation, Session, Key and the private physical placement helper.

## Regression Checks

- `git diff --check`
- no `Content/`, `Config/`, `.uproject` or runtime module-dependency change
- Editor/Live Coding closed
- UE 5.8 exact `Build.bat BathhouseSimEditor Win64 Development` succeeds
- focused queue cleanup/navigation, Counter reference/assignment and checkout key-drop automation succeeds
- full `BathhouseSim` automation succeeds with the updated exact count
- independent headless log scan contains no unexpected technical-abort error, test failure, ensure, assertion, fatal error, access violation, `LogBlueprint: Error` or `LogScript: Error`

## Review Resubmission

Regenerate `.md/PROMPT_REVIEW.md` and report:

- how intentional queue leave is distinguished from assignment corruption without relying on component order
- active move/delegate/token/Tick/movement-flag teardown assertions
- native service-point-reference rejection and distinct visible pose coverage
- drop-point-forward velocity coverage and matching Unreal authoring instructions
- preserved deprecated reflected symbols and Core Redirect conclusion
- UE 5.8 build, focused tests, full automation count and independent log scan
