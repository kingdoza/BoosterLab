# Review Prompt — Order-Independent Key Hook Initialization

## Objective

Review the native fix for a placed key that is visually attached to its hook but remains non-interactable when the hook begins play before its numbered shoe/clothes facilities.

The required behavior is:

- a key hook may initially reject an incomplete numbered topology;
- registration of the matching enabled `ShoeLocker` and `ClothesLocker` must trigger revalidation;
- once exactly one hook and one matching facility of each type exist, the same hook/key pair becomes operational without recreating actors or resaving assets;
- runtime teardown must not revalidate a hook or release a key because facility EndPlay order happens to run first.

## Changed Files

- `Source/BathhouseSim/Public/Facility/BathhouseFacilityActor.h`
- `Source/BathhouseSim/Private/Facility/BathhouseFacilitySubsystem.cpp`
- `Source/BathhouseSim/Public/Interaction/BathhouseKeyActor.h`
- `Source/BathhouseSim/Public/Interaction/BathhouseKeyHookActor.h`
- `Source/BathhouseSim/Private/Interaction/BathhouseKeyHookActor.cpp`
- `Source/BathhouseSim/Private/Tests/BathhouseDomainTests.cpp`
- `.md/Architecture/FacilitySystem.md`

No `Content/`, `Config/`, reflected property, reflected type, Blueprint contract, Core Redirect or module dependency changed. Header changes are private lifecycle state/helpers and automation-test friendship only.

## Review Points

- `RegisterFacility` and `UnregisterFacility` broadcast `OnKeyTopologyChanged` only for registered `ShoeLocker`/`ClothesLocker` actors.
- duplicate registration does not broadcast; unregistering a facility that was not registered does not broadcast.
- `ABathhouseKeyHookActor` registers itself before subscribing, then performs its own initial validation once.
- the hook removes its delegate before unregistering itself in `EndPlay`, preventing self-callback and dangling callbacks.
- `HandleKeyTopologyChanged` ignores hook EndPlay and world teardown, while runtime topology mutations still revalidate.
- successful revalidation clears the stale failure reason, restores `bRuntimeOperational`, derives occupancy from the exact key's current state and does not spawn/replace the key.
- an incomplete topology stays safely disabled; duplicate hook/facility counts retain existing rejection semantics.
- revalidation is idempotent if unrelated numbered lockers cause a topology notification.
- no public gameplay setter or test-only runtime API was added; existing friend-based automation access is preserved.

## Class Growth

- `ABathhouseKeyHookActor` gains one private delegate handle and one private callback within its existing fixed-slot lifecycle responsibility.
- `UBathhouseFacilitySubsystem` gains no state; it only emits the existing topology delegate from the two missing facility mutation sites.
- no new Actor, Component, Subsystem or Tick responsibility was introduced.

## Verification Evidence

- UE 5.8 `Build.bat BathhouseSimEditor Win64 Development`: succeeded after final changes.
- `BathhouseSim.Interaction.KeyTopologyInitializationOrder`: succeeded. It explicitly begins hook/key first, verifies initial disablement, then begins numbered shoe and clothes facilities and verifies automatic activation with the same key in `AtHook` state.
- full `BathhouseSim.Interaction` automation group: process exit 0; all discovered interaction tests succeeded, including key recovery, checkout physical key drop and physical fixed-slot tests.
- `git diff --check`: no whitespace errors; line-ending notices are pre-existing working-tree policy warnings.

## Review Conclusion Required

Report findings by severity with exact file/line evidence. Reject any change that introduces polling/Tick, asset mutation, replacement key spawning, permanent acceptance of incomplete topology, or teardown-time key release. If there are no findings, approve the code stage and pass `.md/PROMPT_UNREAL.md` to Editor verification.
