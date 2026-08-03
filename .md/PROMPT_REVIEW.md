# Code Review Prompt — Bath Approach Snap And Customer Montage Tasks Rework

## Review Objective

Re-review the complete UE 5.8 C++ Bath snap/customer montage change after the reservation-transform snapshot correction required by `.md/PROMPT_IMPLEMENTATION_R.md`.

Review against:

- `.md/PROMPT_IMPLEMENTATION.md`
- `.md/PROMPT_IMPLEMENTATION_R.md`
- `.md/0_ARCHITECTURE.md`
- `.md/Architecture/CoreSystem.md`
- `.md/Architecture/FacilitySystem.md`
- `.md/Architecture/CustomerSystem.md`

Do not perform Unreal asset work during this review.

## Rework Acceptance Criteria

- a reserved Bath uses one cached approach/action snapshot from navigation through snap, activity begin and cleanup.
- changing the live slot transform after reservation cannot redirect the Bath navigation target, change the action location/rotation or change the return point.
- a Bath target query and Bath use fail clearly if their required reservation snapshot is unavailable.
- `BeginUseCurrentFacility()` does not overwrite the rotation of a customer already snapped to the cached action transform.
- unsnapped non-Bath facilities retain the existing live navigation target, live action-facing and no-teleport behavior.
- normal release and technical abort return a snapped Bath customer to the original cached approach before releasing the slot.
- action-overlap validation, movement-mode restoration, return-before-release and failed-return leak prevention remain unchanged.
- all accepted montage selection, completion, loop and playback-token behavior remains unchanged.
- existing `Timed Customer Activity` and reflected contracts remain available.

## Rework Files

- `Source/BathhouseSim/Private/Customer/CustomerSessionComponent.cpp`
  - `GetCurrentFacilityTransform()` now returns the reservation-time cached transform for Bath approach and action queries.
  - a Bath query logs and fails when its required snapshot is absent.
  - `BeginUseCurrentFacility()` validates the Bath snapshot before committing slot occupancy.
  - an already snapped customer keeps its cached action rotation.
  - an unsnapped Bath uses cached action facing; an unsnapped non-Bath facility retains its live action facing.
- `Source/BathhouseSim/Private/Tests/BathhouseDomainTests.cpp`
  - extends `BathhouseSim.Customer.BathSnapCleanup` with live slot mutation and retained expected snapshots.
  - covers Bath approach/action target stability, snap transform, BeginUse rotation, normal release and technical abort.
  - covers missing Bath snapshot failure and the existing unsnapped non-Bath live-transform path.

No header, reflected type, StateTree Task, montage component, architecture document or Unreal asset required a rework change.

## Preserved Complete Implementation

- Facility still authors approach/action transforms; Session still owns reservation, snap and cleanup.
- action overlap is checked before movement cancellation or transform mutation.
- action entry cancels AI movement, stops velocity, saves movement/custom mode, disables movement and teleports.
- all release/abort/EndPlay fallbacks attempt cached approach return before slot release.
- montage assets and playback state remain absent from Session and RoutineDefinition.
- `UCustomerMontagePlaybackComponent` remains the non-ticking montage/delegate/token owner.
- montage candidate 0/1/many selection, one-shot natural completion, duration loop and stale-token protection are unchanged.
- `FCustomerActivityTask` / `Timed Customer Activity` remains intact.

## Class Growth And Responsibility

- `UCustomerSessionComponent` header remains 165 lines with no new property, function or reflected API.
- `CustomerSessionComponent.cpp`: 789 -> 827 lines for the two Bath/non-Bath transform branches and diagnostics.
- `BathhouseDomainTests.cpp`: 485 -> 595 lines for transform-mutation and legacy-path regression coverage.
- the correction stays inside Session's existing reservation/snap/cleanup responsibility and adds no new state owner.
- `.md/Architecture/CoreSystem.md` still lacks the concrete Class Growth Policy referenced by the agent rules. Report this canonical-document gap to the architecture owner; do not invent thresholds during review.

## Blueprint/API/Core Redirect Impact

- no reflected type, property, function or component name changed in this rework.
- no Core Redirect is required.
- StateTree and Blueprint bindings remain the same.
- `.md/PROMPT_UNREAL.md` remains valid and was not changed because the Editor contract is unchanged.
- no `Content/`, `Config/`, `.uproject` or `BathhouseSim.Build.cs` file was modified by this rework.

## Verification

- Editor was closed before build and automation execution.
- UE version: 5.8.1 only.
- full `BathhouseSimEditor Win64 Development` compile/link succeeded with the bundled .NET 10 UnrealBuildTool, `-WaitMutex -NoHotReloadFromIDE -NoUBTMakefiles`.
  - recompiled `CustomerSessionComponent.cpp` and `BathhouseDomainTests.cpp`.
  - linked `UnrealEditor-BathhouseSim.dll` successfully.
- complete headless `Automation RunTests BathhouseSim`: 11/11 succeeded.
- focused headless `Automation RunTests BathhouseSim.Customer.BathSnapCleanup`: 1/1 succeeded.
- the focused test consumed the expected blocked-action, missing target/use snapshot and technical-abort diagnostics.
- task-scoped trailing-whitespace search passed.
- forbidden-scope search passed: no montage reference exists in Session or RoutineDefinition; montage playback calls remain isolated to `CustomerMontagePlaybackComponent.cpp`.
- global `git diff --check` still reports only the pre-existing unrelated `Config/DefaultEditor.ini:6` blank line at EOF; this rework did not modify Config.
- the worktree's broader project migration and Content changes predate and remain outside this task.

## Review Focus

- verify the Bath conditional cannot fall through to a live slot transform.
- verify snapshot validation occurs before `BeginUse()` changes Reserved to Occupied.
- verify snapped BeginUse performs no rotation write and unsnapped Bath uses cached facing.
- verify non-Bath target and activity behavior remain live and unsnapped.
- verify tests retain expected cached transforms locally instead of reading the live slot after Session cache cleanup.
- re-check return-before-release, failed-return handling and movement restoration for regressions.
- re-check the previously accepted montage lifecycle/token implementation as part of the complete resubmission.

## Not Yet Verified

- inherited component visibility/compile in `BP_BathhouseCustomer`
- AnimBP Slot routing and montage section authoring
- StateTree output/context bindings and shared Bath cleanup branch
- NavMesh approach placement, blocked action geometry and multi-Bath PIE behavior

These remain Unreal Editor and integration-review work after code review approval.
