# Player metabolism and body morphs

UPlayerMetabolismComponent tracks food intake and activity energy for each game
day. It drives Fat/Muscular mesh morphs, sprint speed, and the existing
UCombatComponent's metabolism damage modifier. This is an authored game mechanic,
not a physiological or nutritional model.

## Author the morph targets

C++ cannot create meaningful body shapes without authored vertex deltas. In your
DCC character asset, create a neutral basis plus shape keys/blend shapes named
exactly Fat and Muscular. Keep vertex count/order and topology identical. Sculpt
the targets independently and inspect their combined weights for clipping.

Export/import the skeletal mesh with morph targets enabled. Verify both targets
in Unreal's Skeletal Mesh Editor, including intended mobile LODs. Handle clothing
with compatible morph targets or another fitted-clothing solution. This component
updates the character's primary mesh only; it does not automatically reshape
equipment, hair, physics bodies, or the movement capsule. Profile GPU skinning and
morph cost on devices; low update frequency does not eliminate deformation cost.

## Add and connect the component

Attach UPlayerMetabolismComponent and UCombatComponent to the player ACharacter.
Dependencies are Core, CoreUObject, and Engine. The component logs a warning if
the owner or imported mesh targets are missing; numeric gameplay effects can
still operate when the visual targets are absent.

Use these authority-side calls:

```cpp
Metabolism->AddCalories(800.0f);         // Once when the meal is consumed.
Metabolism->RecordStaminaBurn(250.0f);   // Activity kcal-equivalent, not raw stamina.
Metabolism->AdvanceGameDays(ClockDeltaDays);
Metabolism->SetSprintRequested(bSprintHeld);
```

CaloricIntake and StaminaBurn are readable accumulated totals for the current
game day. The stamina system owns the authored conversion from spent stamina
to activity calories. Record actual expenditure once; do not repeatedly add the
remaining stamina value each frame. Food and stamina processing are not automatic.

## Game clock and interpolation

There is no component Tick. Call AdvanceGameDays from the world-clock subsystem,
for example once per game minute with delta 1/1440. Pass elapsed game days, not
real seconds. Align/reset the initial snapshot's DayProgress to the world clock
when creating a save. Save/load must not advance the same time interval twice.

At each completed day:

```text
Expenditure = RestingCaloriesPerDay + StaminaBurn
TargetFat += (CaloricIntake - Expenditure) / CaloriesPerFullFatMorph
TrainingFactor = clamp(StaminaBurn / TrainingCaloriesPerDay, 0, 1)
Fed = CaloricIntake >= 0.9 * Expenditure
TargetMuscular += Fed && TrainingFactor > 0
    ? MuscleGainPerDay * TrainingFactor
    : -MuscleLossPerDay
```

Targets clamp to 0..1 and daily totals reset. Between boundaries, weights converge
using exponential smoothing with a two-game-day half-life by default. Therefore
the first day's diet changes targets at its end, with visible change afterward.
Splitting an elapsed interval into smaller clock calls preserves smoothing
between target changes. Mesh writes below 0.001 weight change are skipped.

AdvanceGameDays handles up to 30 days per call and returns false without mutation
for larger/invalid inputs. Chunk longer skips deliberately. During a multi-day
skip, current accumulated intake/activity apply to the first completed day and
later days have no new meals/activity unless the caller supplies them. Implement
offline meal/activity simulation if you want a different resting/fast-travel policy.

## Gameplay effects

Fat > 0.7 reduces requested sprint speed by FatSprintMultiplier (default 0.75).
BaseSprintSpeed = 650 cm/s becomes 487.5 cm/s; walking remains 450 cm/s. Exactly
0.7 receives no penalty. SetSprintRequested(false) restores the base walk speed.
MaxWalkSpeed is the engine field used for both walking and sprinting here.

This component owns that field while installed. If equipment, injuries, or other
systems also modify speed, replace direct assignment with a shared modifier
resolver; do not let multiple components overwrite MaxWalkSpeed independently.

Muscular > 0.7 sets UCombatComponent's metabolism multiplier to 1.25; otherwise
it is 1.0. The combat component snapshots the multiplier when StartAttack copies
the authored definition, so an attack stays consistent through its active window.
For a 20-damage attack, subsequent attacks deal 25 damage. Authored attack data
is untouched, and repeated updates do not compound the bonus. Any other damage
modifiers should compose separately rather than replace this owned multiplier.

## Save and lifecycle

GetSnapshot captures current/target weights, daily totals, and progress. Store the
returned struct in a USaveGame object; SaveGame annotations alone do not create
a save file. RestoreSnapshot validates finite inputs and reapplies appearance and
effects. Sprint input is transient and should come from current input after load.

On component EndPlay, the owned combat modifier returns to 1 and movement returns
to the configured walk speed. Character pool reuse must restore the intended
snapshot and sprint state; it must not inherit another player's daily totals.

All mutations check owner authority. This initial implementation supports
standalone/server-side simulation; it does not replicate morph weights, provide
sprint RPCs/prediction, or synchronize client movement modifiers. Multiplayer
requires authoritative input routing and replicated/cosmetic body state. Never
accept client-supplied calories or body weights without gameplay validation.

## Validation status

The project descriptor exists, but no Unreal toolchain is available. Compilation/UHT, authored
morph assets, and in-engine tests remain pending. Validate missing targets, both
morphs together, daily rollover, calorie surplus/deficit, activity gain, smoothing
under accelerated time, save restoration, long-skip policy, exact 0.7 boundaries,
sprint release, 20-to-25 damage, unchanged authored definitions, component cleanup,
and low-LOD clothing deformation on the target mobile devices.
