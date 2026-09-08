# Integration Review Prompt — Facility Placement, Locker Capacity And Expansion Integration

## Status

Partial completion. The serialized input, Placement Definition, Expansion Definition, Preview Blueprint and Facility Blueprint work is saved and reloaded. WidgetTree/StateTree authoring and reliable World Partition level persistence remain outside the current MCP write surface. Two native validation/initialization defects block a clean compile and PIE acceptance.

## Scope And Ownership

- Input prompt: `.md/PROMPT_UNREAL.md`
- No Source or Config file was changed in this Editor pass.
- No `.uasset`/`.umap` binary patch or `Save All` was used.
- The previous dirty worktree was preserved.
- Required user/UI work is recorded in `.md/USER_UNREAL.md`.

## Persisted Asset Verification

The following assets were loaded from a fresh Editor session and their stored values matched the authored contract.

- `/Game/Input/Actions/IA_RecoverFacility` — `Boolean`
- `/Game/Input/Actions/IA_PlacementSnap` — `Boolean`
- `/Game/Input/Actions/IA_PlacementRotate` — `Axis1D`
- `/Game/Input/IMC_FirstPerson` — Q, LeftControl and MouseWheelAxis mappings target those three actions.
- `/Game/FirstPersonCharacter/BP_FirstPersonCharacter` — the three Facility Placement action defaults are assigned; existing `PrimaryUseAction` stays `IA_PrimaryUse`.
- `/Game/Bathhouse/Data/Placement/DA_FacilityPlacement_{Bath,Shower,ClothesLocker_1,ClothesLocker_4,ClothesLocker_8,Washer,Dryer,CleanTowelStack,UsedTowelBin}` — each has `Facility.Placeable`, a matching preview class and these footprint/slot values: Bath `29x24/0`, Shower `8x11/0`, Locker `6x4/1`, `6x14/4`, `6x28/8`, Washer `6x5/0`, Dryer `6x5/0`, Clean `5x4/0`, Used `5x5/0`.
- `/Game/Bathhouse/Data/Expansion/DA_BathhouseExpansion_Default` — tiers `(KeyPool, LockerLimit) = (3,2), (4,4), (8,8)`.
- `/Game/Bathhouse/Blueprints/Placement/BP_FacilityPlacementZone` — `Facility.Placeable`, bounds extent `(1400,900,10)` at local Z `10`.
- `/Game/Bathhouse/Blueprints/Placement/BP_BathhouseExpansionAuthority` — default definition and initial tier `0` assigned.
- `/Game/Bathhouse/Blueprints/Interaction/BP_BathhouseKeyRack` — key/hook classes, first key `1`, eight unique pair transforms assigned.

All nine migrated facility Blueprints point to their matching definitions, use `Placed` mode, keep bottom-aligned footprints, use `NavArea_Null` placement modifiers, and do not let their physical root create navigation. Locker component-tree verification found exactly 1/4/8 `LockerSlot` components with non-empty IDs and the authored approach transforms.

## Blueprint Compile

Warnings-as-errors compile succeeded and left no dirty asset for 23 assets:

- `BP_FirstPersonCharacter`, Placement Zone, Expansion Authority, Key Rack and Key
- Bath, Shower, Locker 1/4/8, Washer, Dryer, Clean Towel Stack and Used Towel Bin
- all nine facility preview Blueprints

Expected current failures:

1. `/Game/Bathhouse/UI/WBP_InteractionPrompt` lacks the five required bindings: `PlacementActionNameText`, `PlacementFailureReasonText`, `RecoveryActionNameText`, `RecoveryFailureReasonText`, `RecoveryProgressBar`.
2. `/Game/Bathhouse/Blueprints/Interaction/BP_BathhouseKeyHook` fails native validation: `KeyActor must reference the exact key with the same KeyNumber.` Its CDO is intentionally unconfigured (`KeyNumber=0`, `KeyActor=None`) because these are `EditInstanceOnly`; this is a native validation defect for rack-owned runtime pairs, not an asset-default value to fill in.

## Level Persistence Attempt

The MCP created one Placement Zone at `(600,-100,0)`, one Authority at `(0,0,0)`, one Key Rack at `(0,-260,50)`, and set Recast runtime generation to `DynamicModifiersOnly` in memory. `SceneTools.save_actor` failed to find the new World Partition external actor package. The fallback explicit `/Game/Maps/DefaultMap` save returned success and dirty=false, but a fresh Editor reload found zero of the three actors and `RecastNavMesh.RuntimeGeneration=Static`.

This MCP path is therefore not a valid World Partition save method for this project. Section 3 of `.md/USER_UNREAL.md` is required.

The six old level-owned key/hook actors were only recorded, never deleted.

## PIE And Logs

Two 5-second input-free PIE smoke runs started and stopped successfully. They cannot verify player input, placement, recovery, focus priority, ghost visuals, trigger/obstacle semantics or StateTree customer flow.

Both runs emitted the same two errors:

`BP_ClothesLocker_C_UAID_F02F7433CA3615F402_1440894859` and `BP_ClothesLocker_C_UAID_F02F7433CA3615F402_1441212860` could not register their placed state because installed locker capacity was reported over the current expansion limit. Runtime inspection showed each has one `Locker_1_01` slot and the correct one-slot definition; authored Tier 0 capacity is two. The registration happens before/while authority availability is established. The native retry path exists, but the initial failed registration is still logged as an Error. This needs a code-stage fix before clean-log acceptance.

No new Counter or Navigation error was observed in either smoke run.

## Required Next Steps

1. Complete `.md/USER_UNREAL.md` sections 1–4.
2. Return the KeyHook CDO validation and locker registration-order errors to the implementation/review stage.
3. After those changes, run Data Validation, full compile, disk reload and the interactive PIE acceptance matrix from `.md/PROMPT_UNREAL.md`.

## Integration Conclusion

Do not approve integration yet. The persisted asset contract is largely correct, but the unresolved WidgetTree/StateTree/World Partition authoring and two native blockers prevent end-to-end acceptance.
