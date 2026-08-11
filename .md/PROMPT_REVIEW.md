# Code Review Prompt — FacilitySlot Foot-Point Bath Snap

## Review Objective

Review the UE 5.8 C++ change that standardizes Bath `ActionPoint` and `ApproachPoint` authoring on the character's feet instead of the capsule center.

Review against:

- `.md/PROMPT_IMPLEMENTATION.md`
- `.md/PROMPT_IMPLEMENTATION_R.md`
- `.md/0_ARCHITECTURE.md`
- `.md/Architecture/CoreSystem.md`
- `.md/Architecture/FacilitySystem.md`
- `.md/Architecture/CustomerSystem.md`

Do not perform Unreal asset work during this review.

## Acceptance Criteria

- a facility slot component transform is the authored action feet transform.
- `ApproachOffset` produces the authored approach feet transform.
- `UCustomerSessionComponent` adds the customer's scaled capsule half height exactly once when resolving either feet transform to an actor/capsule-center transform.
- action collision validation uses the resolved actor/capsule-center transform, not the authored feet transform.
- Bath snap and Bath return use the same feet-to-actor conversion.
- reservation-time approach/action snapshots remain stable if the live slot changes later.
- rotation, cached snapshot, movement cancellation/restoration, return-before-release and failed-return behavior remain unchanged.
- unsnapped non-Bath facility behavior and montage behavior remain unchanged.

## Changed Files

- `Source/BathhouseSim/Private/Customer/CustomerSessionComponent.cpp`
  - adds one internal feet-to-character transform helper using `GetScaledCapsuleHalfHeight()`.
  - resolves cached Bath action and approach feet transforms before collision checks or actor movement.
- `Source/BathhouseSim/Public/Customer/CustomerSessionComponent.h`
  - passes the resolved character action transform into the private collision helper.
  - no reflected API changed.
- `Source/BathhouseSim/Private/Tests/BathhouseDomainTests.cpp`
  - authors facility test transforms as feet positions.
  - verifies both actor-center transforms and `GetActorFeetLocation()` for snap, release and technical abort.
  - places the blocking obstacle at the resolved capsule center.
- `.md/0_ARCHITECTURE.md`, `.md/Architecture/FacilitySystem.md`, `.md/Architecture/CustomerSystem.md`
  - record the feet-authoring and Session conversion ownership contract.
- `.md/PROMPT_UNREAL.md`, `.md/USER_UNREAL.md`
  - record the required Editor values and verification procedure.

## Class Growth And Responsibility

- `CustomerSessionComponent.h`: 165 -> 167 lines; private collision-helper signature only.
- `CustomerSessionComponent.cpp`: 827 -> 850 lines; one conversion helper and two call sites.
- `BathhouseDomainTests.cpp`: 595 -> 612 lines; feet and actor-center regression assertions.
- the change remains inside Session's existing Bath snap/cleanup responsibility and introduces no new state owner.
- `.md/Architecture/CoreSystem.md` still lacks the concrete Class Growth Policy referenced by the agent rules. Report this canonical-document gap to the architecture owner; do not invent thresholds during review.

## Blueprint/API/Core Redirect Impact

- no reflected type, property, function or component name changed.
- no Core Redirect is required.
- StateTree and Blueprint bindings remain the same.
- the Editor authoring meaning changes: slot transforms must now be feet transforms, not capsule-center transforms.
- implementation did not modify or resave `Content/`, `Config/`, `.uproject` or `BathhouseSim.Build.cs`.

## Verification

- UE version: 5.8.1 only.
- `BathhouseSimEditor Win64 Development` compiled and linked successfully with the bundled .NET 10 UnrealBuildTool and `-NoHotReloadFromIDE -NoUBTMakefiles`.
- focused headless `Automation RunTests BathhouseSim.Customer.BathSnapCleanup`: 1/1 succeeded with exit code 0.
- the test consumed its expected blocked-action and technical-abort diagnostics.
- implementation preserved the pre-existing user-owned dirty Content assets without resaving them.

## Review Focus

- verify the scaled half height is added once and only once.
- verify the helper preserves the authored rotation and scale behavior expected by `SetActorLocationAndRotation`.
- verify the collision capsule is tested at the same resolved transform used by the teleport.
- verify action and approach paths cannot diverge in their vertical convention.
- verify cached transforms remain logical feet transforms throughout their lifetime.
- verify non-Bath behavior, return-before-release and movement restoration have no regression.

## Unreal Follow-Up

After code review approval and a fresh Editor launch, apply the exact Bath slot feet values in `.md/PROMPT_UNREAL.md`. For the current DefaultMap blockout, author relative action Z `74.5` and `ApproachOffset.Z = -72.5`; do not author capsule-center Z `162.5` into the slot component.
