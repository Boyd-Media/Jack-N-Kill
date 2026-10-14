# Three-way faction StateTree and AI Director

Use three combat factions: Floodline Syndicate, Gatorwake Public Order Bureau,
and Alien Stalkers. Treat the player as a separate target category. Names follow
the [parody dictionary](ParodyTranslationDictionary.md).

Cap all active combat AI at eight, including Stalkers. This stronger global cap
prevents more than eight active agents appearing on screen during camera turns.
This document is a Blueprint architecture specification, not serialized Blueprint
or StateTree assets. Runtime behavior and performance have not been validated.

The native AMobileAIDirector now supplies admission and adaptive encounter logic;
derive BP_AIDirector from it rather than maintaining an independent cap. See
[Director and storm integration](AIDirectorAndStorms.md). Multi-agent atomic
reservations, vehicle seats, and the faction StateTree assets remain integration
work; the native implementation currently exposes single-agent reservation calls.

## Blueprint classes

| Blueprint | Responsibility |
| --- | --- |
| BP_AIDirector — Actor | Admission, pooling, squad events, seat reservations, activation budgets |
| BP_FactionAIController — AIController | Perception, faction identification, StateTree execution |
| BP_FloodlineEnforcer — Character | Combat, health, equipment, vehicle interaction |
| ST_FloodlineEnforcer — StateTree | Target priority, suppression, evacuation |
| BPC_FactionIdentity — ActorComponent | Faction tag, squad ID, hostility queries |
| BPC_EnforcerContext — ActorComponent | Perceived threats, panic, reserved seat |
| BPC_VehicleSeats — ActorComponent | Seat availability, reservations, occupancy, driver ownership |

Enable StateTree and Gameplay StateTree plugins. Use the engine AI StateTree
component and native asynchronous movement tasks where available. For an enforcer,
choose StateTree as its behavior owner rather than also running an independent
Behavior Tree. The existing Stalker controller can retain its own Behavior Tree
while participating in Director admission.

## Enforcer context and state hierarchy

```text
AlienTarget             Actor reference
PlayerTarget            Actor reference
LastAlienLocation       Vector
AlienMemoryExpiresAt    Float
PanicUntil              Float
ReservedVehicle         Actor reference
ReservedSeat            Integer
DirectorAgentHandle     Integer
```

Update perception context through events, not world searches from StateTree tasks.

```text
Root
├── Disabled / Pooled
├── Dead
├── Evacuate
│   ├── ReserveTransport
│   ├── MoveToBoardingPoint
│   ├── BoardVehicle
│   ├── DriveOrRide
│   └── EscapeComplete
├── SuppressAlien
│   ├── AcquireCover
│   ├── FaceAlien
│   ├── FireControlledBurst
│   └── Reposition
├── InvestigateAlien
├── EngagePlayer
└── Patrol
```

Implement this transition order explicitly with conditions/priorities; hierarchy
layout alone does not guarantee interruption of an already running state:

| Priority | Condition | Destination |
| --- | --- | --- |
| 1 | Dead or activation revoked | Dead / Disabled |
| 2 | Confirmed Stalker kill triggers panic | Evacuate |
| 3 | Valid perceived Alien target | SuppressAlien |
| 4 | Unexpired Alien last-known position | InvestigateAlien |
| 5 | Valid hostile player target | EngagePlayer |
| 6 | Otherwise | Patrol |

Alien suppression overrides player engagement. On Alien acquisition, cancel the
player firing task, clear its aiming target, and transition to SuppressAlien.
Tasks must recheck the intended target immediately before firing because a state
transition alone does not cancel an already queued shot.

Start with four seconds of Alien memory. Investigate the last perceived position,
not an unseen Alien's current position. Panic overrides suppression after a
confirmed squad casualty. Once evacuation begins, retain that intent until escape
or an explicit recovery condition; a brief perception loss must not restart combat.

Suggested custom tasks: STT_SelectCover, STT_FaceThreat, STT_FireBurst,
STT_RequestVehicleSeat, STT_BoardReservedSeat, STT_DriveEscapeRoute. Finish/cancel
outstanding movement, fire timers, and reservations on state exit. Use native
asynchronous movement for character navigation rather than polling paths.

## Stalker kill and evacuation

```text
Authoritative enforcer death
    → Confirm damage source is an Alien Stalker
    → Director receives OnEnforcerKilledByAlien
    → Notify eligible same-squad survivors
    → Set panic
    → Send StateTree event AI.Event.Evacuate
```

Require witnessing, hearing, or squad communication for eligibility. Do not
inform every agent in the city automatically.

1. Reserve a reachable vehicle and a specific seat before movement starts.
2. Move to an authored exterior boarding transform with navigation access.
3. Board, disable character movement/collision as appropriate, and attach to the
   seat. The driver controls the vehicle; passengers remain occupants.
4. Reach a concealed exit/unloading region, then pool occupants and transport.

Reservations carry an owner handle and timeout. Release on death, cancellation,
vehicle destruction, failed navigation, or interrupted boarding. If no transport
is available, use an authored on-foot escape route. Retry blocked transport after
a bounded delay rather than every frame.

Keep the enforcer's existing agent handle while driving. Do not spawn another
driver AI or count the vehicle as an additional combat agent. Boarding transfers
control to a single vehicle-driving task, avoiding competing movement owners.
Chaos driving requires steering, braking, and obstacle handling; character Move To
does not drive vehicles. Begin with authored escape splines and look-ahead steering.
The existing ambient ISM traffic is visual only and must be promoted to a real,
seat-enabled vehicle before boarding; promotion is not implemented by this design.

## BP_AIDirector hard cap

Keep one Director available throughout the encounter:

```text
MaxActiveAgents = 8
ActiveAgents = Map<AgentHandle, AgentRecord>
PendingRequests = Array<ActivationRequest>
NextHandle = Integer
MaxActiveTrafficEscapeVehicles = 2
VehicleReservations = Map<VehicleAndSeat, AgentHandle>
```

AgentRecord contains pawn, controller, faction, squad, and lifecycle state.
Activating and Deactivating records both consume slots. The vehicle simulation
cap is separate from the agent cap; passengers retain their agent slots.

Expose RequestActivation, BeginDeactivation, ConfirmDeactivated, NotifyAgentDeath,
ReserveEscapeSeat, and ReleaseEscapeSeat.

```text
RequestActivation
    → ActiveAgents.Count < 8?
        No: queue request; leave pawn inactive
        Yes:
            Reserve handle immediately
            Add Activating record
            Activate pooled pawn/controller
            Start perception and StateTree/Behavior Tree
            Mark Active
```

No latent nodes belong between capacity checking and slot reservation. Every AI
spawn/activation path, including the Stalker and reinforcements, must use admission.
Failed activation releases its reservation through a guarded cleanup path.
Prevent duplicate activation and make release idempotent; stale callbacks must
not release another agent's reused handle.

For removal, stop firing, behavior execution, perception, and movement, then hide
or destroy the pawn as intended. Release the slot only after deactivation completes.
A visible dead body may become a passive corpse with AI and combat disabled.
Ensure ragdoll/corpse physics have a separate budget.

Reserve one of the eight slots for the encounter Stalker, leaving up to seven
human agents across the other factions. Human admission must respect that
reservation even before the Stalker activates; queued reinforcements wait for
capacity. Death/destruction callbacks and encounter shutdown perform cleanup.

## Mobile budget and validation

- Use perception callbacks and StateTree events for urgent changes.
- Run nearby tactical selection around 5 Hz and farther selection around 1-2 Hz.
- Keep active movement and Chaos simulation updating normally.
- Bound cover/vehicle searches and use registered candidate lists.
- Pool agents; stop inactive controllers, perception, and behavior execution.
- Keep ambient ISM traffic outside the combat system until interactive promotion.

The cap limits active AI, not visual crowds, corpses, draw calls, or total physics
cost. In Unreal, test simultaneous reinforcement requests, reserved Stalker
admission, death callbacks, interrupted boarding, vehicle destruction, streaming,
pool reuse, shutdown, and camera turns. Active-record count must never exceed
eight; no in-engine test has run for this specification.
