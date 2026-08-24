# Code Review Prompt — Customer Knockdown Collision And Check-In Interaction Recovery

## Status

Ready for code review. This is a focused correction inside the existing Customer Recovery and customer interaction responsibilities.

## Requirement And Acceptance

- A knocked-down check-in customer must expose no interaction prompt and reject direct interaction execution.
- After recovery, the customer's pre-knockdown collision object type, response container, profile behavior and enabled state must be restored.
- A recovered front-of-queue customer must expose the check-in key prompt again.
- Queue membership, check-in wait state, routine timers and StateTree soft interruption behavior must remain unchanged.
- No Blueprint, StateTree, Config or level asset may require mutation.

## Changed Files

- `Source/BathhouseSim/Public/Customer/CustomerKnockdownComponent.h`
- `Source/BathhouseSim/Private/Customer/CustomerKnockdownComponent.cpp`
- `Source/BathhouseSim/Private/Customer/BathhouseCustomerCharacter.cpp`
- `Source/BathhouseSim/Private/Tests/CombatRecoveryAutomationTests.cpp`

## Implementation

### Collision Snapshot And Restore

`UCustomerKnockdownComponent` now snapshots the skeletal mesh collision object type and complete `FCollisionResponseContainer` before switching to the authored Ragdoll profile.

Recovery first reapplies the saved profile, then restores object type and responses only when the loaded profile did not reproduce the saved values. This is important for the Blueprint customer's manual `Custom` profile: applying the Ragdoll profile overwrites its response array, and the reserved `Custom` name cannot reload those values by itself.

The comparison-before-set behavior also preserves a valid named profile when that profile already restores the exact saved object type and responses.

### Knockdown Interaction Gate

`ABathhouseCustomerCharacter::QueryInteraction` returns an empty query while `CustomerKnockdown->IsKnockedDown()` is true. `ExecuteInteraction` independently rejects the same state so a direct call cannot bypass the presentation query.

Standing customer interaction still delegates to `UCustomerSessionComponent`; no key, queue or check-in transaction moved into the Character.

### Regression Coverage

`BathhouseSim.CustomerRecovery.CollisionAndCheckInInteractionRestore` verifies:

- standing queue-front customer exposes the check-in prompt;
- knocked-down customer hides the prompt and rejects direct execution;
- Ragdoll changes Visibility response to Ignore;
- recovery restores Custom profile behavior, QueryOnly, Pawn object type, Visibility Block and authored Pawn Ignore;
- capsule collision is restored;
- recovered customer exposes the check-in prompt again.

## Class Growth And Responsibility

- `CustomerKnockdownComponent.h`: 79 -> 84 lines
- `CustomerKnockdownComponent.cpp`: 219 -> 236 lines
- `BathhouseCustomerCharacter.cpp`: 91 -> 100 lines
- `CombatRecoveryAutomationTests.cpp`: 518 -> 605 lines

The new snapshot fields and private restore helper remain in the component that already owns ragdoll physics snapshots. The Character adds only an interaction routing guard and does not acquire recovery state ownership.

## Blueprint, API And Redirect Impact

- No reflected property, function, component or type was renamed or removed.
- No Blueprint graph or serialized authoring contract changed.
- No Core Redirect is required.
- Existing `BP_BathhouseCustomer` collision defaults remain valid and require no save.

## Verification Evidence

- `git diff --check`: passed.
- UE 5.8 `BathhouseSimEditor Win64 Development`: succeeded.
- `BathhouseSim.CustomerRecovery`: 3 discovered, 3 succeeded.
  - `CollisionAndCheckInInteractionRestore`: succeeded.
  - `FacilityAndOperationInvalidation`: succeeded.
  - `SoftInterruptionSerialAndBathTimer`: succeeded.
- No automation failure, critical error or project Blueprint error was reported.
- The three `UE::UnifiedErrorTest` `LogTemp: Error` startup lines are engine self-test diagnostics, not project failures.
- No `Content/`, `Config/` or `.uproject` file was changed by this implementation.
- A pre-existing user modification to `BP_MonkeyWrench.uasset` was preserved and is outside this review.

## Review Focus

- Confirm that applying a named profile followed by comparison-based restoration preserves both named and manual Custom collision configurations.
- Confirm that query and execute guards jointly enforce the no-interaction-while-knocked-down contract.
- Confirm that the Character remains a routing/composition boundary and the component remains the physics snapshot owner.
- Confirm that recovery ordering restores collision before the actor teleport without altering the existing floor, movement, health or StateTree resume flow.

## Expected Review Result

Approve if collision restoration remains exact for the Custom Blueprint mesh and existing Customer Recovery tests remain green. No Editor authoring work is required after approval; only PIE integration verification remains.
