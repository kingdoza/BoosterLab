# Unreal Prompt — Bath Approach Snap And Customer Montage Tasks

## Status And Scope

Editor work is required after C++ code review approval. Use UE 5.8 only.

Modify only the assets needed for Bath approach/action authoring, customer montage playback and `ST_CustomerRoutine` wiring. Do not add shoe/clothing mesh, visibility, skin-weight, AnimNotify, prop, Motion Warping, UI, input, Interaction or Economy work.

## Native Preflight

1. Close every BathhouseSim Editor instance.
2. Build `BathhouseSimEditor Win64 Development -WaitMutex -NoHotReloadFromIDE` with UE 5.8.1 and launch fresh.
3. Confirm no missing native type/property errors for:
   - `UCustomerMontagePlaybackComponent`
   - `CustomerMontagePlayback`
   - `FCustomerFacilitySnapTask`
   - `FCustomerBeginActivityTask`
   - `FCustomerFinishActivityTask`
   - `FPlayCustomerMontageOnceTask`
   - `FPlaySelectedMontageLoopForDurationTask`
4. Do not use hot reload or a stale DLL for asset work.

## Existing Assets

- `/Game/Bathhouse/Blueprints/Customer/BP_BathhouseCustomer`
- `/Game/Bathhouse/Blueprints/Facility/BP_Bath`
- `/Game/Bathhouse/AI/ST_CustomerRoutine`
- `/Game/Characters/Mannequins/Anims/Unarmed/ABP_Unarmed` only if this is still the customer mesh Anim Class; otherwise report the actual AnimBP path before editing it.
- `/Game/Maps/DefaultMap`

Create customer Montage assets under `/Game/Bathhouse/Animations/Customer/`. Reuse compatible existing assets only after recording their source path and skeleton. Do not retarget, replace the customer skeleton or invent appearance-state logic in this task.

## Customer Blueprint

- Open `BP_BathhouseCustomer`; native parent remains `ABathhouseCustomerCharacter`.
- Confirm inherited `CustomerSession` remains readable for StateTree binding.
- Confirm inherited `CustomerMontagePlayback` exists, is read-only and uses native defaults; add no Blueprint logic to it.
- Confirm the inherited skeletal mesh has a valid Anim Class and record the exact AnimBP path.
- Compile/save without adding montage delegates or playback Event Graph logic.

## Bath Slot Authoring

- Open `BP_Bath` and every placed Bath instance used by `DefaultMap`.
- For each `UBathhouseFacilitySlotComponent`:
  - component transform plus `FacingRotation` is the exact in-bath `ActionPoint`.
  - `ApproachOffset` produces the NavMesh-reachable `ApproachPoint` outside the bath.
  - leave enough blocking-collision clearance for the customer capsule at the action transform.
- Do not extend NavMesh into the bath and do not project the action point onto navigation.
- Visualize/check both transforms, then save `BP_Bath` and `DefaultMap`.

## AnimBP And Montage Assets

- In the actual customer AnimBP, route the locomotion/base pose through one Slot node named `DefaultGroup.CustomerAction`.
- Preserve the existing locomotion graph; the Slot overlays montage playback and contains no gameplay/domain mutation.
- Create at least one compatible technical-validation one-shot Montage and one Bath loop Montage under:
  - `/Game/Bathhouse/Animations/Customer/AM_Customer_Action_Once`
  - `/Game/Bathhouse/Animations/Customer/AM_Customer_Bath_Loop`
- Both Montages use slot `DefaultGroup.CustomerAction`.
- `AM_Customer_Bath_Loop` must contain a valid section named `BathLoop`; do not author the asset section as an infinite loop. The native Task links `BathLoop -> BathLoop` at runtime.
- The one-shot Montage must reach a natural montage end. Do not use a timer or AnimNotify as its completion signal.
- Additional candidate Montages are optional, but every candidate must use the same compatible skeleton and Slot contract.

## StateTree Context And Bindings

Open `/Game/Bathhouse/AI/ST_CustomerRoutine`:

- Context Actor remains `ABathhouseCustomerCharacter`/`BP_BathhouseCustomer`.
- Bind every Session input from `Context Actor.CustomerSession`.
- Bind montage Task `Customer` from the context Actor itself.
- Keep existing queue, facility, key, cash and navigation logic unchanged except the activity sequences below.
- Do not rename or delete existing `Timed Customer Activity`; retain it for animation-free/timer-only states.

## Animated Activity Sequence

For an activity that uses a one-shot montage, author this order inside its existing facility parent:

1. existing approach target and built-in `Move To`
2. `Begin Customer Activity`
   - bind Session
   - set the matching Activity
3. `Play Customer Montage Once`
   - bind Customer
   - assign valid MontageCandidates
   - author PlayRate, optional StartSection, BlendInTime and BlendOutTime
4. `Finish Customer Activity` with the same Session and Activity
5. leave the facility parent so its native Exit releases the slot

Do not add a duration wait around the one-shot Task. Normal montage end is the only success signal.

## Bath Sequence

Inside the existing `Hold Customer Facility(Bath)` parent, author:

1. `Get Customer Facility Target` with `bUseApproachPoint=true`
2. built-in `Move To` bound to Destination
3. existing successful navigation-result recording
4. `Snap Customer Facility Point(Target=ActionPoint)`
5. `Begin Customer Activity(Activity=BathDwell)`
6. `Play Selected Montage Loop For Duration`
   - Customer bound from context Actor
   - MontageCandidates include `AM_Customer_Bath_Loop` and any approved compatible variants
   - `LoopSection=BathLoop`
   - bind `Duration` from the preceding Begin Task `ResolvedDuration`
7. shared Bath cleanup state:
   - `Finish Customer Activity(Activity=BathDwell)`
   - `Snap Customer Facility Point(Target=ApproachPoint)`
8. only after cleanup, leave the Bath facility parent; its native Exit releases the slot

Both natural loop-duration completion and `Customer.Event.BathStayExpired` must transition to the same cleanup state while still inside the Bath facility parent. Exiting the montage state first lets its native Exit stop only its owned token. Do not transition directly from the action point to the next Bath or MainShower state.

The facility parent Exit is fallback cleanup for interruption/technical abort; do not duplicate release logic in Blueprint or StateTree Tasks.

## Compile And Save

Compile/save:

- actual customer AnimBP
- every new Montage
- `BP_BathhouseCustomer`
- `BP_Bath`
- `ST_CustomerRoutine`
- `DefaultMap`

Restart the Editor, reload all listed assets and compile `ST_CustomerRoutine` again. Record any missing context/output binding or invalid montage section by exact asset path.

## PIE Verification

1. Confirm a customer walks only to the NavMesh approach point, stops, then snaps exactly to the action transform.
2. Confirm movement remains disabled while inside the Bath and is restored at approach before the next `Move To`.
3. Block an action point with collision; snap must fail without moving the customer and must log the diagnostic.
4. Let Bath dwell finish naturally; the selected montage stays unchanged, stops at duration, activity finishes, customer returns to approach and only then releases the slot.
5. Trigger `BathStayExpired` during the loop; montage Exit, activity finish, approach snap and parent release occur in that order before MainShower movement.
6. Interrupt the Bath state and force technical abort; no montage, disabled movement or occupied slot remains.
7. Use one valid loop candidate among null entries; it plays. Use zero valid candidates; the Task fails with the expected error. Use several valid candidates over repeated customers and confirm one selection per EnterState with no mid-loop shuffle.
8. Run the one-shot Task; natural montage end succeeds, while external interruption fails and cleanup cannot stop a newer montage.
9. Run an animation-free state through existing `Timed Customer Activity` and confirm its previous timer behavior still works.
10. Test at least two customers using different Baths and verify exclusivity, approach return and subsequent navigation.

## Forbidden Editor Work

- no shoe/clothing or modular mesh components
- no visibility, skin-weight, AnimNotify or appearance state
- no Blueprint montage delegate/timer completion logic
- no Blueprint movement, snap, facility release or session mutation
- no NavMesh extension/projection to the Bath action point
- no Motion Warping, root-motion alignment or prop/socket work
- no native/reflected rename

Record asset changes, compile/save results, PIE observations and relevant warnings in `.md/PROMPT_INTEGRATION_REVIEW.md`.
