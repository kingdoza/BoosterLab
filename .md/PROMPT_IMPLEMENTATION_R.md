# Implementation Rework Prompt — Towel Presentation Blueprint Contract

## Objective

Fix the native towel presentation naming collision that currently breaks an existing Blueprint, then correct the review/Unreal handoff so it matches the actual C++ contract and cannot report a false-positive approval.

Do not modify or resave `Content/`. Preserve inventory/transfer/machine authority, presentation revision behavior, transient ISM ownership, Stack/Pile/Slot layout behavior and existing gameplay lifecycles.

## P0 — Existing `BP_CleanTowelStack` Cannot Compile

Evidence from both the submitted full-automation log and an independent focused run:

```text
BP_CleanTowelStack.uasset: [Compiler] Internal Compiler Error:
Tried to create a property StackVisual ... but another object
(/Script/BathhouseSim.CleanTowelStackActor:StackVisual) already exists there.
```

The existing asset already serializes a Blueprint property/component symbol named `StackVisual`. The new reflected native member at `ACleanTowelStackActor::StackVisual` therefore collides while the Blueprint skeleton/generated class is created. Automation tests still return success because this compiler error occurs during project startup, outside the focused test result.

Affected files:

- `Source/BathhouseSim/Public/Towel/CleanTowelStackActor.h`
- `Source/BathhouseSim/Private/Towel/CleanTowelStackActor.cpp`
- the equivalent new native presentation member/default-subobject names in `UsedTowelBinActor`, `TowelBasketActor` and `TowelProcessingMachineActor` if a consistent contract rename is selected
- `.md/0_ARCHITECTURE.md`
- `.md/Architecture/TowelSystem.md`
- `.md/Architecture/TowelPresentationSystem.md`
- `.md/PROMPT_REVIEW.md`
- `.md/PROMPT_UNREAL.md`

Required correction:

1. Inspect the five target Blueprint assets read-only for every proposed reflected property and default-subobject name before choosing the final native names.
2. Rename the newly introduced native member/default-subobject contract to collision-free, stable names. Apply the naming consistently where that improves the shared Stack/Pile contract; do not rename or delete any pre-existing reflected gameplay API.
3. Update all C++ references and canonical documents to the exact final native names.
4. Do not use a Core Redirect to conceal a live same-scope collision. These presentation members are new and have not passed Editor authoring; a redirect is unnecessary unless inspection proves a saved asset already references the discarded native name.
5. Do not open and resave the broken Blueprint as a migration technique. The C++ contract must load the existing asset without the duplicate-property compiler error first.

Acceptance:

- `BP_CleanTowelStack` loads and compiles without skeleton/generated-class duplicate-property errors.
- The other four target Blueprints load without native member/default-subobject name collisions.
- Each actor still owns exactly one intended native presentation component and binds only its own inventory in `BeginPlay`, then unbinds in `EndPlay`.
- `Content/` remains unchanged.

## P1 — Verification Must Reject Blueprint Compiler Errors

The submitted evidence reports focused `1 success` and full `17 success / 0 fail`, but the same full-run log contains two `LogBlueprint: Error` entries for `BP_CleanTowelStack`. Test result counts alone are therefore insufficient for this change.

Required correction:

1. After the naming fix, run the UE 5.8 Editor target build with Editor/Live Coding closed.
2. Run the focused towel presentation automation and the full `BathhouseSim` suite.
3. Explicitly scan startup and automation logs for at least:
   - `LogBlueprint: Error`
   - `[Compiler] Internal Compiler Error`
   - `another object ... already exists`
   - missing native parent/property/default subobject warnings
4. Load/compile the five target Blueprints in a read-only verification pass, or use an equivalent Blueprint compile commandlet that does not save assets.
5. Report test counts and the independent Blueprint/compiler log scan. Do not describe invalid-slot and equal-revision fixture warnings as the only diagnostics unless the entire log confirms it.

## P2 — Unreal Handoff Uses A Nonexistent Property Name

`.md/PROMPT_UNREAL.md` tells the Editor agent to configure `PositionZJitter`, but `UTowelPileVisualComponent` exposes `MaxZJitter`.

Required correction:

- Replace the nonexistent name with the exact reflected C++ property name `MaxZJitter`.
- Update every `StackVisual`/`PileVisual` reference in the handoff to the final collision-free native contract chosen for P0.
- Keep the handoff limited to profile creation, inherited component layout/asset authoring and PIE verification. Do not add Blueprint inventory calculations or Event Graph ISM creation.

## Regression Checks

- `git diff --check`
- no `Content/`, `Config/`, `.uproject` or `Build.cs` change
- UE 5.8 `BathhouseSimEditor Win64 Development` build succeeds
- `BathhouseSim.Towel.Presentation.StackPileSlotAndLifecycle`: success
- full `BathhouseSim`: all tests succeed
- target Blueprint load/compile log has zero compiler errors and zero duplicate-property diagnostics
- profile selection, state swap, top removal, Slot clamp, unregister/re-register and EndPlay assertions remain covered

## Review Resubmission

Regenerate `.md/PROMPT_REVIEW.md` and `.md/PROMPT_UNREAL.md` for this exact task. Include:

- the final collision-free reflected property/default-subobject names
- read-only asset-symbol inspection result
- Blueprint compile/load result for all five target assets
- build/test counts plus compiler-error log scan
- Core Redirect decision and why it is safe
- confirmation that `Content/` was not changed or resaved
