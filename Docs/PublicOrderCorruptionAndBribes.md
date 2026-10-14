# Gatorwake Public Order Bureau: corruption and bribes

## Classes and dependencies

APublicOrderAIController implements a local officer encounter state machine.
APublicOrderPlayerController supplies an owning-client UI/RPC bridge and a
server-side economy extension point. Integrate that bridge into the final game
PlayerController rather than running two PlayerController instances per player.
The JackNKill module requires Core, CoreUObject, Engine, AIModule, GameplayTasks,
and NavigationSystem. Add UMG to the module implementing a native widget if needed.

The repository has no generated Unreal project or engine toolchain. Compilation,
UHT, network behavior, UMG assets, arrest implementation, and weapon integration
remain unverified. These are integration sources, not a complete police system.

## Corruption and crime reporting

On server BeginPlay, each officer randomizes CorruptionLevel in 0..1. Values below
0.5 are clean; values at or above 0.5 are corrupt. CorruptionSeed = INDEX_NONE
uses per-instance randomness. For repeatable encounters provide unique saved
seeds and restore officer identity/seed through the save system. Reusing one seed
for all officers produces the same corruption value. Pooling does not rerun BeginPlay.

Invoke ReportCrime(CrimeId, SuspectPawn, Kind, bUseForceAllowed) only from trusted
server crime/witness systems. The caller verifies that the crime occurred, the
suspect identity/faction, and the officer's awareness. BlueprintAuthorityOnly is
an integration annotation; C++ also checks authority. There is no client RPC for
reporting crimes or assigning suspect kind/force permission.

Clean officers pursue either Player or GangMember suspects. They request arrest
at close range unless the trusted gameplay rules allow force, in which case they
approach within ForceRangeCm and request combat. Corrupt officers ignore gang
crime reports and approach player suspects to request a bribe.

One encounter runs per officer. Other reports should be queued by the Director.
The last 32 accepted crime IDs are deduplicated locally; persistent/global crime
resolution belongs in the authoritative crime system. A paid incident should not
be resubmitted as a newly fabricated crime ID. Officer death, pooling, or encounters
cancelled by other systems must call ClearEncounter and stop combat bindings.

## UI and payment flow

Use APublicOrderPlayerController (or merge its bridge into your controller) as the
player controller class. Bind OnBribePrompt and OnBribeClosed on the owning client.
Create a widget showing the requested credits, Accept/Refuse buttons, and a timeout
indicator. Store RequestId and call SubmitBribeResponse(RequestId, bAccept).

```text
Corrupt officer approaches player within 200 cm
    → Stop movement and begin 15-second server timer
    → Reserve one prompt on that player's controller
    → ClientShowBribePrompt → OnBribePrompt → UMG
    → SubmitBribeResponse → ServerSubmitBribeResponse
    → Validate officer/session/suspect/deadline/distance
    → Authoritative wallet check/debit
    → Close prompt
        Paid: clear enforcement
        Refused/unaffordable: request hostile combat
```

The default price is 500 credits. Override TrySpendBribeCredits on the server to
atomically check and subtract from the real wallet, returning true only on success.
It deliberately returns false until an economy is connected. The client never
supplies an amount, balance, or claimed payment result. Consume the request ID
before calling the wallet to prevent duplicate/reentrant charges. Keep the wallet
override nonlatent; asynchronous payment needs a separate reserved-funds state.

UI messages use IDs rather than an AIController reference: AIControllers normally
exist only on the server. Reliable RPCs on the owning PlayerController deliver
the prompt and closure. Client countdowns are cosmetic; server game time decides
expiry. Refusing, failing payment, exceeding the timer, leaving three times the
approach distance, or changing suspect possession makes the officer hostile.

Only one prompt per player is permitted. A second simultaneous officer request
fails rather than creating duplicate payable prompts, and that officer becomes
hostile in this initial policy. A missing controller bridge also prevents bribery
and triggers hostility. Configure these cases through encounter coordination if
you want waiting officers instead. Do not ship with the bridge missing.

## Arrest and combat integration

Bind OnArrestRequested to your arrest/custody gameplay. Call FinishArrest(true)
after successful custody or FinishArrest(false) to escalate. Bind OnUseForceRequested
to a weapon/combat behavior that owns pursuit, aim, line-of-sight checks, firing,
and damage. The controller emits requests; it does not implement gun ballistics,
handcuff animations, detention UI, or autonomous hostile pursuit after handoff.

Bind OnEnforcementEnded to cancel combat and arrest actions. Do not let both this
controller and an independent Behavior Tree/StateTree own movement simultaneously.
Once force is requested, the combat layer owns movement until the encounter ends.
If the player's pawn changes while hostile, resolve the crime target explicitly
through the authoritative encounter system rather than firing at an abandoned pawn.

## Mobile and Director integration

Idle officer Tick is disabled. Active encounter decisions update every 0.2 seconds;
navigation/path following continues normally. Movement requests retry at most
once every two seconds. Prompt expiry uses a server timer. No per-frame world
scan, widget polling on the officer, or replication of corruption is required.

Activate these officers through the existing eight-agent Director cap. They count
as human agents, alongside enforcers, athletes, squads, and Stalkers. Director
admission, death notifications, and pooled-controller resets must be bound by the
game integration; these sources do not instantiate the documented Director.

## Required validation

Compile/UHT and test clean/corrupt threshold behavior; player/gang crime selection;
navigation and arrest handoff; owning-client UI; successful server debit; insufficient
funds; refusal; timeout; fleeing; replayed/forged request IDs; two officers requesting
one player; disconnect; pawn changes; officer destruction/pooling; and Director
capacity. Verify no stale widget, fire task, reservation, or timer survives cleanup.
No in-engine or multiplayer tests have run for these sources.
