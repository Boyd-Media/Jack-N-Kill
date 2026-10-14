# Pro Athletes: weekly schedule and Behavior Tree

Pro Athletes is a wealthy faction associated with the fictional Gatorwake Endzone
Royals. Its power comes from money, access, and hired protection. Members retreat
from danger rather than fighting the player directly.

All nine members are persistent NPC records. Schedules continue offscreen;
spawned characters require admission from the existing
[eight-agent AI Director](FactionStateTreeAndAIDirector.md).

This is an editor implementation specification, not serialized Behavior Tree,
Blackboard, Blueprint, or Data Asset files. Runtime validation remains pending.

## Roster

| NPC | Role | Preferred night destination |
| --- | --- | --- |
| Dax Mercer | Captain | Crownwalk VIP lounge |
| Lennox Vale | Veteran | Underground casino |
| Rafe Calder | Star recruit | VIP club |
| Milo Knox | Showman | VIP club |
| Cassian Pike | Defensive specialist | Underground casino |
| Torin Bell | Team organizer | Crownwalk VIP lounge |
| Ellis Rooke | Contract negotiator | Underground casino |
| Nash Voss | Reserve player | VIP club |
| Bram Keene | Veteran mentor | Crownwalk VIP lounge |

Characters and team branding are fictional. Do not copy real players' likenesses,
biographies, uniforms, or logos. Follow the approved
[naming dictionary](ParodyTranslationDictionary.md) for existing setting references.

## Seven-day schedule

Use one shared schedule Data Asset with per-NPC destination overrides. Times are
in-game time, with non-overlapping intervals ending at the next boundary.

| Day | 00:00-07:00 | 07:00-08:00 | 08:00-16:00 | 16:00-18:00 | 18:00-23:00 | 23:00-24:00 |
| --- | --- | --- | --- | --- | --- | --- |
| Monday-Friday | Home / sleep | Commute | Training facility | Personal time | Assigned casino or VIP venue | Return home |
| Saturday | Home / sleep | Personal time | Sponsor appearances / recovery | Personal time | Assigned casino or VIP venue | Return home |
| Sunday | Home / sleep | Personal time | Stadium attendance | Stadium departure | Assigned casino or VIP venue | Return home |

Sunday stadium attendance overrides daytime leisure. Emergency behavior overrides
the schedule. Travel and arrival time can delay activity; do not teleport an
on-screen NPC merely because a schedule boundary passes.

Persistent record fields:

```text
NPCId
FactionTag
HomePOI
TrainingPOI
StadiumPOI
NightVenuePOI
WealthBalance
ScheduleRevision
LastSimulatedLocation
SecurityContractId
```

A world-clock subsystem resolves schedule blocks and broadcasts boundary changes.
Avoid nine independent per-frame calendar checks. Resolve the current block after
save/load or a time jump rather than replaying every missed boundary.

## Assets and Blackboard

Create BB_ProAthlete, BT_ProAthlete, DA_ProAthleteWeeklySchedule, and
DA_ProAthleteRoster. Bind a Behavior Tree controller to the persistent roster
record through NPCId.

| Key | Type | Purpose |
| --- | --- | --- |
| ImmediateThreat | Object: Actor | Perceived nearby danger |
| NeedsSecurityContract | Bool | Confirmed robbery requires a response |
| SecurityRequestPending | Bool | Prevents duplicate requests |
| NeedsScheduleRefresh | Bool | Clock or destination changed |
| DestinationActor | Object: Actor | Loaded destination |
| DestinationLocation | Vector | Navigation arrival point |
| HasDestination | Bool | Destination resolved |
| AtDestination | Bool | Arrival completed |
| Activity | Enum | Sleep, Training, Stadium, Leisure |
| NPCId | Name | Persistent roster identity |
| SafeExitLocation | Vector | Authored retreat destination |

## Behavior Tree

```text
Root
└── Selector
    ├── Sequence: RetreatFromThreat
    │   ├── Decorator: ImmediateThreat Is Set [Observer Aborts: Both]
    │   ├── Task: CancelCurrentActivity
    │   ├── Task: RequestProtection
    │   ├── Task: SelectSafeExit
    │   ├── Move To: SafeExitLocation
    │   └── Task: NotifyDirectorOfSafeArrival
    ├── Sequence: CommissionSecurity
    │   ├── Decorator: NeedsSecurityContract == true [Observer Aborts: Both]
    │   ├── Decorator: SecurityRequestPending == false
    │   └── Task: SubmitSecurityContract
    ├── Sequence: RefreshSchedule
    │   ├── Decorator: NeedsScheduleRefresh == true [Observer Aborts: Both]
    │   └── Task: ResolveScheduledDestination
    ├── Sequence: Travel
    │   ├── Decorator: HasDestination == true
    │   ├── Decorator: AtDestination == false
    │   ├── Move To: DestinationLocation [Observe Blackboard Value: true]
    │   └── Task: ConfirmArrival
    ├── Sequence: PerformActivity
    │   ├── Decorator: AtDestination == true
    │   └── Task: PerformScheduledActivity
    └── Wait: 1 second
```

Decorators belong on their respective Sequence nodes. Activity tasks remain
active until a schedule event or higher-priority branch interrupts them. Abort
handlers cancel movement, animation interactions, and timers. Repeated protection
requests must be deduplicated by contract ID, including requests made while fleeing.

ResolveScheduledDestination clears stale arrival state and resolves the selected
POI. It clears NeedsScheduleRefresh even when resolution fails, sets HasDestination
accordingly, and schedules a bounded retry. This prevents an immediate retry loop.
Use explicit arrival points outside buildings; local walking uses navigation,
while long-distance travel uses vehicles or offscreen simulation.

## Robbery response

Trigger retaliation from a confirmed inventory/economy theft transaction, not
damage or proximity alone:

```text
Confirmed robbery
    → Record victim, stolen value, and known offender
    → Retreat if threatened
    → Persistent faction record requests a security contract
    → Director queues a Hit Squad encounter
```

SubmitSecurityContract:

1. Sets SecurityRequestPending before dispatch.
2. Uses a unique contract ID to deduplicate requests.
3. Reserves funds if the faction can afford the contract.
4. Clears NeedsSecurityContract after acceptance.
5. Commits payment on successful deployment; releases funds on cancellation.

The persistent manager owns the contract so despawning an athlete does not cancel
or duplicate it. Failures receive a cooldown; insufficient funds do not generate
free replacements. The athlete has no attack task.

## Hit Squad tree

```text
BT_ProAthleteHitSquad
└── Selector
    ├── Abort / Withdraw
    ├── RespondToAlienThreat
    ├── EngagePerceivedPlayer
    ├── InvestigateLastKnownPlayerLocation
    └── SearchAssignedArea
```

Use event-driven interruption and the Alien-first threat policy where appropriate.
Begin with the robbery location or a legitimately observed escape position.
Acquire the player through perception, not hidden current-coordinate updates.
Expire the encounter after a bounded duration or completed withdrawal. Pending
contracts must not generate unlimited replacement squads.

## Admission and mobile budget

Nine scheduled NPCs do not require nine active pawns. Keep all nine roster records
simulated and activate relevant members only through Director admission. Athletes,
squad members, police, and Stalkers share the same global cap of eight.

Start with a two-member Hit Squad request. For simultaneous deployment, reserve
two available slots atomically through Director admission; otherwise queue the
request. Count reservations while agents activate. Never visibly despawn athletes
to manufacture capacity. Respect the existing reserved Stalker slot.

Offscreen records advance between POIs using travel-time estimates. Admitted
NPCs materialize at concealed arrival points, not inside the player's view.
Perception, navigation, and Behavior Trees stay stopped for inactive records.

## Validation checklist

In Unreal, test all seven days and midnight rollover; save/load and skipped time;
unloaded POIs; interrupted travel; repeated robbery events; insufficient wealth;
deployment failure/refunds; athlete pooling; squad perception; contract expiry;
and simultaneous admission with the existing eight-agent cap. These behaviors
have not been implemented or tested by this document.
