# Adaptive AI Director and thunderstorms

## Native classes

AMobileAIDirector evaluates player stress and a tension curve, admits agents under
the eight-slot cap, and spawns Alien Stalkers or Gatorwake Public Order Bureau
patrol pawns. AStormWeatherController replicates storm intensity, reduces footstep
hearing radius, and broadcasts weather presentation changes. AStalkerAIController
now applies storm aggression to pursuit speed and investigation persistence.

Create BP_AIDirector derived from AMobileAIDirector; use it as the single encounter
admission owner instead of a separate Blueprint implementation with its own cap.
Keep it and the weather controller loaded for the encounter region. Their direct
reference is intentional; road/spawn-point references remain soft. Do not place
multiple independently budgeting Directors in overlapping playable regions.

Dependencies: Core, CoreUObject, Engine, AIModule, GameplayTasks, NavigationSystem.
No weather plugin is required. Authored rain/lightning effects and pawn/BT assets
must be supplied by the game. No Unreal build or gameplay test has run here.

## Player telemetry

On the server, feed UpdatePlayerStatus(PlayerPawn, Health01, Ammo01) around once
per second and when relevant values change. Use the actual health and inventory
systems; the Director does not invent values or scan components every frame.
Use ammo = 1 when ammo scarcity is irrelevant, such as a melee-only loadout.
Normalize available ammunition against an authored useful capacity, not an
unbounded global inventory count.

NotifyEncounterStarted when that player actually enters combat or detects a new
encounter, not merely when an unseen pawn spawns. RemovePlayerStatus on dropout,
death/respawn, or possession changes before binding the replacement pawn. In co-op
the worst registered player's stress protects the group. Telemetry older than ten
seconds pauses spawning; it is not assumed to represent a recovered player.

```text
RecentEncounter = exp(-TimeSinceLastEncounter / 30 seconds)
Stress = 0.50 * (1 - Health01)
       + 0.30 * (1 - Ammo01)
       + 0.20 * RecentEncounter
DesiredTension = TensionCurve(Phase01) * (1 - Stress)
```

The Director evaluates every two seconds without Actor Tick. The default 120-second
curve ramps to a peak at 78 seconds, then falls through recovery. Assign a
UCurveFloat with normalized time/value axes to author a different shape. Stress
at or above 0.75 holds a rolling 20-second respite and resets the build-up phase.
This suppresses new spawns; it does not delete or pacify existing threats.

Desired tension at least 0.55 can request one encounter. Initial grace is 30
seconds, successful spawns have a 45-second cooldown, and failed attempts retry
after ten seconds. Default Alien selection probability is 40%, increasing to 65%
above storm intensity 0.7. Both encounter types still obey stress and capacity.

## Admission and spawning

Every faction agent must obtain RequestAgentSlot before activation and then call
ActivateReservedAgent. Pending reservations count toward eight and expire after
five seconds. Humans can occupy at most seven slots, preserving Alien capacity.
GUID handles prevent stale callbacks from releasing a reused numeric slot.

The native spawn path reserves before deferred spawning, finishes the pawn, and
commits that reservation. Failure destroys the candidate and releases the slot.
Director-spawned pawns have this Director as Owner: their BeginPlay integration
must not reserve a second slot. External/pool activation uses the public admission
API. If admission/commit fails, leave the pawn inactive or destroy it.

ReleaseAgentSlot only after behavior, fire, and movement stop. EndPlay releases
the record automatically; a corpse that stays alive needs explicit AI deactivation
and release. Passive corpses/physics require their own budget. Players do not count
as AI agents. The hard cap applies only when every active AI path uses this shared
admission contract; agents spawned outside it are not automatically discovered.

Configure AlienStalkerClass and PublicOrderPatrolClass with collision, replicated
pawn state, AIController classes, Auto Possess AI, and navigation support. Police
patrol classes need an authored patrol behavior; spawning a police pawn does not
create a crime or automatically make it hostile. Bind OnEncounterSpawned for
patrol route/encounter setup. Native spawning creates one pawn, not a whole squad.
Squads and the Pro Athletes security system must reserve all their own agents
through the same Director; atomic multi-slot admission is a further integration
step rather than separate uncontrolled spawning.

Supply loaded ATargetPoint soft references. At most eight candidates are sampled
per attempt. Points must be at least 40 m from every registered player and within
150 m of someone. Author them out of sight with valid navigation/collision for
the assigned agent. Distance alone does not prove camera occlusion; this prototype
does not perform a frustum/visibility test. No synchronous point loading occurs.

## Thunderstorm gameplay

SetStormIntensity accepts an authority-side normalized value. At intensity <=0.7
gameplay modifiers remain at their baseline; above it they interpolate toward:

| Modifier | Clear/light weather | Maximum storm |
| --- | --- | --- |
| Footstep noise radius | 1.0x | 0.5x |
| Stalker aggression | 1.0x | 1.5x |
| Stalker chase speed while seeing its target | 1.0x | 1.2x |
| New investigation memory | 8 seconds | 12 seconds |

Call Weather.ReportPlayerFootstep from server movement/footstep events. A base
1200 cm hearing radius becomes 600 cm at maximum storm. It reports one hearing
event with the player pawn as instigator. Do not also send the old unscaled noise
event. Rain does not remove sight perception, and gunfire/vehicle noise remain
unscaled unless explicitly designed otherwise. The reduced radius is an authored
masking approximation, not acoustic simulation. Existing sight/hearing memory
does not retroactively change its expiry when storm intensity changes.

Admitted Stalkers receive current weather on admission, weather change, and each
Director evaluation to handle delayed controller creation. The native controller
owns its baseline MaxWalkSpeed and applies the pursuit multiplier while HasSight
is true. Returning to calm/no sight restores the captured baseline. If other
systems own enemy speed, compose these modifiers in a shared movement resolver.
Weather does not increase the controller tick frequency or sight-query radius.

## Dynamic weather presentation

Bind OnStormChanged on a Blueprint subclass to fade local rain emitters, wind,
storm ambience, wetness material parameters, and lightning presentation. For an
automatic storm cycle, use the world-clock's low-frequency events to evaluate an
authored intensity curve and call SetStormIntensity on authority. The C++ actor
provides state/control, not a fixed forecast or automatically generated rain.

Clients receive intensity through replication, including late joiners. Initialize
presentation from GetStormIntensity as well as the event to avoid missing an
initial callback. Blend locally rather than replicate rain particles. Keep rain
camera-local, bound transparent overdraw, and use authored flashes/audio instead
of spawning many shadowed lightning lights on mobile. Physical-device profiling
must establish their rendering/audio budget.

Offline uses the same local authority path without weather RPCs. Networked worlds
run admission, telemetry, hearing, and Stalker behavior on the server; weather
presentation is client-local. Director debug stress/tension values are server-only
and are not replicated HUD data. Player status must never come from unchecked
client-supplied health/ammo.

## Validation pending

Compile/UHT, pawn/BT assignment, weather effects, and engine tests remain pending.
Test health/ammo starvation, stale telemetry, co-op worst-player stress, recovery,
curve boundaries, spawn failure/collision, simultaneous admission, expired handles,
death/pooling/stream-out, eight-slot enforcement, clear/storm noise radius, sight
preservation, baseline speed restoration, delayed controllers, late joining, and
mobile rain/AI performance. No runtime readiness claim is made by these sources.
