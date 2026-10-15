# Gameplay reliability and optimization pass

## Project foundation

JackNKill.uproject, game/editor targets, the runtime Build.cs, and the primary
module now exist. Open the project with UE 5.6+. Generate project files and build
JackNKillEditor. Enhanced Input, Chaos Vehicles, and StateTree plugins are enabled.
The input component defaults and CombatHit trace channel are configured. Mobile
Forward, conventional shadows, disabled screen-space reflections/GI, and disabled
default motion blur are explicit; Mobile HDR remains enabled.

This is not a playable packaged game yet. Meshes, animations, map/content assets,
input actions, Blueprint behavior assets, platform SDKs/signing, and an installed
Unreal engine remain prerequisites. No map is fabricated or silently replaced.

## Implemented changes

| Area | Result |
| --- | --- |
| Combat | Opening-pose trace runs immediately; an expired Active phase cannot apply hitch-delayed damage. Reentrant trace calls and stale legacy-window callbacks are rejected. |
| Legacy melee | Client copies cannot run damage sweeps. Window revision changes invalidate previously collected hits. |
| Persistence | Inventory, food, activity, clock progression and sprint mutation are blocked while loading. Only the manager's guarded snapshot restore is permitted. |
| Save binding | Saving cannot overwrite a deferred player restore or capture components belonging to another world. Snapshot door storage is reserved once to avoid growth reallocations. |
| Metabolism | Stable exponential interpolation uses expm1; strict 0.7 thresholds and stat multipliers share tested rules. Restoring a save clears transient sprint input. |
| AI admission | Squad reservations are all-or-nothing, expired reservations are pruned immediately, and duplicate Directors cannot operate independent budgets. |
| Heavy AI | Default maximum is one Stalker, separately adjustable to two while retaining the eight-agent total and seven-human limit. |
| Deactivation | Possessed agents cannot release their slot early. Unpossessed Stalker controllers stop ticking and path following. |
| Police | Arrest/bribe/force handoffs require line of sight. Unreachable pursuit times out. A busy bribe UI retries instead of becoming hostile; a missing bridge cancels. |
| Economy | Default bribe payment atomically removes Credits from the possessed pawn's inventory. Offline prompt/response dispatch bypasses RPC calls. |
| Traffic | Rebinding the same player does not clear the population. Stale instances clear even if road removal already emptied the agent array. |

These are correctness improvements and bounded-work changes, not measured FPS
gains. Mesh rendering, pathfinding, replication, animations and platform storage
still require engine/device profiling. Existing multiplayer limitations in
metabolism, radio, and world-state presentation are not magically resolved by
this pass.

## Reproduce available tests

```bash
python Tools/run_native_tests.py
python Tools/run_native_tests.py --sanitize
```

The executable includes the same GameplayRules.h used by the actual components;
it is not a Python reimplementation or mocked Unreal runtime. Checks cover attack
deadlines, squad/heavy-agent budgets, load isolation, frame-rate-independent
interpolation, stat thresholds, stress/weather bounds, and compact-save capacity.
Sanitized builds enable AddressSanitizer and UndefinedBehaviorSanitizer. CI runs
this portable suite; CI does not claim to compile Unreal.

The existing GIS fixture suite runs actual Blender baking/export and FBX import:

```bash
python -m venv /tmp/jnk-gis-venv
/tmp/jnk-gis-venv/bin/python -m pip install -r Tools/GIS/requirements.txt
/tmp/jnk-gis-venv/bin/python Tools/GIS/test_pipeline.py --blender blender
```

## Engine validation

Validation recorded in this workspace on 2026-10-15:

| Check | Result |
| --- | --- |
| Production-rule suite, optimized build | Passed: 754 checks |
| Production-rule suite, address/undefined-behavior sanitizers | Passed: 754 checks |
| GIS fixture integration with Blender 4.3.2 | Passed: baking, FBX export/reimport, and failure guards |
| Python runner syntax, project JSON, git diff whitespace | Passed |
| Unreal build and automation | Blocked: no Unreal SDK installed; runner exits with status 2 |
| Mobile packaging, rendered gameplay, sustained FPS/thermals | Not run; requires engine, authored assets, platform SDKs and devices |

With a complete UE installation, run:

```bash
python Tools/run_unreal_tests.py --engine-root /path/to/UnrealEngine
```

The runner builds JackNKillEditor with at most two parallel actions, then runs
JackNKill automation tests with NullRHI. It requires fresh, nonempty JSON results
including address-validation and SaveGame round-trip tests; missing/skipped/failed
results do not pass. UE_ENGINE_ROOT can supply the engine path. Use --skip-build
only after independently building the current sources. Rendering and physical
device tests require a separate real-renderer run, not NullRHI.

Engine integration regression checks still required:

1. Attack callbacks that cancel/restart combat cannot produce a stale hit; a
   simulated hitch past Active expiry causes no late damage.
2. Food, pickups, metabolism time advancement and sprint commands do not mutate
   state during asynchronous loading; the manager can still apply both snapshots.
3. Seven humans plus a reserved Stalker reject another human; batch admission
   never partially allocates a squad; a second Director cannot expand the budget.
4. Two officers encountering one player do not punish a busy prompt. Payment
   charges once, missing funds/refusal still cause hostility, and walls prevent
   interaction/weapon handoff.
5. Removing a road and then losing the player leaves no phantom ISM cars.
6. Standalone/listen/client behavior and player stat prediction are exercised in
   separate processes before claiming complete co-op readiness.

## Production order

Build and package the foundation first. Then prove one player/weapon/enemy, one
house escape, and one representative streamed city block on a physical device.
Validate persistence, one vehicle, and bounded encounters before expanding co-op,
faction schedules or the full narrative. A 60 FPS claim requires sustained device
measurements, not a successful native unit test or a small draw-call estimate.
