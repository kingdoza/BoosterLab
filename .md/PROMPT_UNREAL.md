# Unreal Prompt — Verify Key Hook Initialization Order Fix

## Status

Native code, UE 5.8 Editor build, focused initialization-order automation and the full `BathhouseSim.Interaction` test group are complete.

No Blueprint, level, Data Asset, StateTree or Config edit is required. Do not save or resave any package for this fix.

## Preflight

1. Use UE 5.8 and ensure PIE/SIE is stopped.
2. Restart the BathhouseSim Editor so the newly built `UnrealEditor-BathhouseSim.dll` is loaded. Live Coding is not the acceptance path for this lifecycle change.
3. Open `/Game/Maps/DefaultMap` without changing or saving it.
4. Confirm the existing number-2 topology remains exactly:
   - one `KeyHook_2` linked to the exact `Key_2` instance;
   - one enabled `ShoeLocker_2` with `FacilityNumber = 2`;
   - one enabled `ClothesLocker_2` with `FacilityNumber = 2`.
5. If any count is zero or greater than one, stop and report the exact actor paths. The code intentionally keeps invalid topology disabled.

The current map may also contain a number-3 key/hook without matching number-3 shoe/clothes facilities. That pair is expected to remain disabled and is outside this fix; do not add or delete actors under this prompt.

## PIE Verification

1. Start PIE normally.
2. Look directly at the number-2 key/hook with an empty hand.
3. Verify the take-key interaction is available and taking the key succeeds.
4. Return the exact key to `KeyHook_2` and verify it snaps back and can be taken again.
5. Repeat PIE at least three times to exercise different Actor BeginPlay ordering.
6. Verify number 1 continues to work and no key is duplicated, dropped, hidden or reassigned during startup.
7. Check Output Log for unexpected topology, fixed-slot binding, duplicate assignment or access errors.

## Optional Editor Automation

Run:

- `BathhouseSim.Interaction.KeyTopologyInitializationOrder`
- `BathhouseSim.Interaction.KeyRecovery`
- `BathhouseSim.Interaction.PhysicalCarryFixedSlotHeldPoseAndRecovery`

All must succeed. Do not treat unrelated existing montage authoring warnings as a key-topology failure.

## Save And Handoff

- Do not use Save All.
- The expected newly dirty/saved package count is zero.
- Report the number-2 actor paths, three PIE outcomes, automation results and unexpected log entries.
