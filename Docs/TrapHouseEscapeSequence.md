# Trap house escape sequence

## Context and pacing

This encounter supports Act I of the [main quest](MainQuestOutline.md), inside
the house on Ragged Mile Road. Target duration: 90-120 seconds. The player has
retrieved the ledger when an Alien Stalker breaches the building. Escape follows
a locked utility door, a crawlspace chase, and a concealed service hatch.

The breach changes the route while giving readable visual and audio warnings.
This document specifies level design and rendering integration; it does not
create a playable map, visibility data, or scripted encounter assets.

## 1. Utility door — bypass the deadbolt

The player retrieves the ledger from a back bedroom. The front entrance collapses
under the Stalker's first impact, forcing escape through a locked utility room.

Use an abstract lock interaction rather than reproducing real picking techniques:

- Hold Interact to enter a close camera view.
- Align two resistance indicators, then hold steady to complete the action.
- A failed input produces a metallic click and loses progress.
- A nearby key provides a slower exploration alternative.

Place the door around a corner from the breach. Dust, moving furniture, and heavy
footsteps communicate danger without requiring a constant view of the alien.

**Fairness rule:** The first breach cannot kill a player already interacting with
the lock. Escalation begins after a readable warning, with enough time to disengage.

## 2. Crawlspace — evade the breach

The utility room contains a displaced wall panel leading into a maintenance
crawlspace. Use a dedicated crawl movement mode with a smaller collision capsule
and a camera constrained to the passage.

| Section | Encounter |
| --- | --- |
| Entry bend | A solid corner immediately breaks sight with the breached room. |
| Pipe junction | A short passage beneath loose debris or a longer, quieter passage. |
| Exit pocket | A small recess lets the player wait while the Stalker investigates noise. |

The towering Stalker cannot fit inside. Its threat comes from breaking adjacent
walls and reaching through authored openings. Use marked breach sockets and
scripted attack volumes rather than navigating its full capsule through the
crawlspace. Telegraph each intrusion with wall deformation, dust, and a directional
sound before activating damage.

**Checkpoint:** Save after entering the crawlspace. A failed escape restarts here
rather than repeating the lock interaction.

## 3. Service hatch — escape and reveal the city

The crawlspace ends beneath a pantry. The player releases a fictional latch
interaction and emerges into a rear service passage.

Use a final dogleg corridor with two opaque doors. This creates a transition
pocket where the interior remains visually isolated while the destination
exterior becomes ready. Keep at least one door opaque and closed during transition.

The final door opens only after the exterior is ready. The Stalker's next impact
supplies narrative pressure during that transition. On exit, the player chooses
between a parked vehicle and a narrow pedestrian route.

## Visibility and rendering placement

UE5 does not provide the old Anti-Portal actor as a standard visibility tool.
Do not plan around placing Anti-Portals to exclude the city. Opaque walls support
ordinary occlusion, but neither walls nor Precomputed Visibility guarantee
complete exterior culling.

### Supported non-World-Partition maps

Place Precomputed Visibility Volumes around:

- Bedroom and utility room camera space.
- Crawlspace, including lowered camera positions.
- Pantry and transition corridor.

Keep coverage focused on reachable camera positions. Solid walls and dogleg
passages create useful occlusion. Enable precomputed visibility and build it
after layout stabilizes. Inspect cell coverage at actual camera heights.
Visibility data applies to eligible static geometry; it does not eliminate every
exterior actor or rendering pass. Lightmass lighting and visibility are separate
build concerns; see [lighting guidance](MobileRenderingAndLighting.md).

### Existing World Partition approach

Use streaming and explicit rendering isolation rather than relying on Precomputed
Visibility:

1. Keep the house interior and transition pocket independently available.
2. While the player is fully inside, explicitly hide exterior environment
   primitives through the environment visibility manager. Include exterior
   effects and auxiliary scene captures in that policy.
3. Restore exterior visibility while the player remains behind the final opaque
   door, then open it when streaming is complete.

If complete separation is essential, use a separate interior map or an explicitly
isolated interior environment. Exterior windows and breach openings must show
an authored local backdrop while isolation is active. Do not unload gameplay
actors needed for the raid or Stalker encounter simply to remove their visuals.

## Validation and outstanding implementation

Hidden geometry, unloaded geometry, and geometry occluded from the main camera
are different states. Inspect GPU work, shadow passes, and scene captures as well
as the main view before claiming the outside city no longer renders.

In Unreal, validate camera clearance, crawl capsule transitions, attack telegraphs,
checkpoint restoration, the key alternative, exterior streaming readiness, door
gating, and visibility restoration on death or interrupted transitions. Test the
mobile frame budget during both breach effects and exterior restoration.

No encounter map, visibility bake, visibility manager, or device test has been
implemented or validated by this design document.
