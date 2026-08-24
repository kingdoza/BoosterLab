# Unreal Prompt — Verify Knockdown Check-In Interaction Recovery

## Status And Scope

Editor asset changes are not required. This stage is read-only compile/reload and PIE verification of the native fix.

Do not modify or resave Blueprint, StateTree, level, PhysicsAsset or collision preset assets for this task.

## Native Contract Under Verification

- Knocked-down customers return no check-in interaction query and reject direct interaction execution.
- `UCustomerKnockdownComponent` restores the skeletal mesh's pre-knockdown collision object type, full response container and enabled state.
- Recovery does not leave the mesh with Ragdoll's `Visibility = Ignore` response.
- Check-in queue/session/timer and StateTree soft pause resume in place.

## Exact Assets To Inspect Without Saving

- `/Game/Bathhouse/Blueprints/Customer/BP_BathhouseCustomer`
- `/Game/Bathhouse/AI/ST_CustomerRoutine`
- the existing gameplay level used for check-in testing

No property, component, graph, binding or asset connection should be changed.

## Preflight

1. Use the existing single UE 5.8 Editor session for `C:/UnrealProjects/BathhouseSim/BathhouseSim.uproject`, or start one only if none exists.
2. Confirm no second Editor or commandlet owns project packages.
3. Confirm PIE is stopped before inspection.
4. Record the initial dirty package list and preserve the existing user-modified `BP_MonkeyWrench` package.
5. Confirm the newly built native module is loaded; restart the Editor once if it was already open with an older DLL.

## Read-Only Contract Inspection

On the `BP_BathhouseCustomer` Class Default Object, confirm without editing:

- Capsule collision ignores the player's `Visibility` interaction trace as currently authored.
- Character Mesh normally blocks `Visibility` and is query-enabled.
- `CustomerKnockdown.RagdollCollisionProfileName` remains `Ragdoll`.
- `KnockdownDurationSeconds` remains the authored value.

Do not replace the Custom mesh collision settings with a new preset as a workaround; native recovery now restores them exactly.

## PIE Verification

1. Start PIE and allow one customer to reach the front of the check-in queue.
2. Hold a valid key and aim at the standing customer.
3. Confirm the `키 전달하기` prompt is visible before damage.
4. Knock the customer down without transferring the key.
5. While the customer is ragdolled:
   - confirm the key interaction prompt is absent;
   - confirm interaction input cannot transfer the key.
6. Wait for native recovery.
7. Aim at the recovered customer at its actual recovered position.
8. Confirm the `키 전달하기` prompt appears again.
9. Transfer the valid key and confirm the transaction succeeds once.
10. Confirm the customer continues the existing routine and the queue advances normally.

## Log Acceptance

- No `CustomerKnockdown` missing physics/root-body error.
- No check-in technical abort or unexpected timeout caused by the knockdown duration.
- No StateTree stop/restart or destructive `ExitState` behavior.
- No collision restoration warning, Blueprint runtime error or access violation.

## Save And Dirty Policy

- Save no assets for this verification.
- Do not use Save All.
- Stop PIE and confirm no new dirty Content, map or external actor package was created.
- Preserve the pre-existing dirty `BP_MonkeyWrench` package without saving or resetting it.

## Acceptance Result

Pass only when all three presentation states are observed in order:

1. standing check-in customer: prompt visible;
2. knocked-down customer: prompt absent and interaction rejected;
3. recovered customer: prompt visible and key transfer succeeds.

If the recovered prompt is still absent, report the runtime mesh collision profile, object type, collision enabled state, Visibility response, `IsKnockedDown`, `IsWaitingForCheckIn` and `IsQueueFront` values without editing assets.
