# Offline-first drop-in/drop-out co-op

## Architecture and dependencies

UCoopSessionSubsystem is a GameInstance subsystem with explicit Offline, Host,
Join, and Return-to-Menu nodes. It does not create an OnlineSubsystem session,
require an account, or start a listener on initialization. Direct-IP hosting uses
UE's native listen-server travel. Offline play starts with regular absolute map
travel and empty URL options.

AJNKLocalGameMode is the default native mode in Config/DefaultEngine.ini. It works
in standalone or as the authoritative mode after explicitly hosting. Its native
defaults are AJNKPlayerCharacter and APublicOrderPlayerController. Create a
Blueprint subclass and assign the actual player mesh, animation, camera, input,
attack definitions, and other game components. Map World Settings may override
the global mode; inspect them. No startup map is invented by this change.

AJNKPlayerCharacter creates a replicated UCombatComponent and enables actor and
movement replication. Dependencies: Core, CoreUObject, Engine, Networking, and
Sockets, plus existing game-feature dependencies. Configure maps for cooking.
The subsystem does not require OnlineSubsystem/Steam/EOS or a platform login.

Project/build scaffolding is supplied. The Unreal toolchain, authored menu,
and playable pawn assets remain unavailable. These sources are integration architecture, not a tested
multiplayer build.

## Main menu Blueprint

Get Game Instance -> Get Subsystem (CoopSessionSubsystem). Keep the Co-op toggle
unchecked by default and do not persist it as an automatic connection setting.

```text
Co-op unchecked
    Play → StartOfflineGame(GameplayMapPackage)

Co-op checked → SetCoopEnabled(true)
    Host → HostGame(GameplayMapPackage)
    Join → JoinByIP(AddressText)

Leave / Return to menu
    → ReturnToOfflineMenu(MenuMapPackage)
```

Pass full existing map package names such as /Game/JackNKill/World/Maps/Gatorwake,
not asset object suffixes or URL options. That path is an example, not a supplied
map. Host loads the map with ?listen; joining uses ClientTravel to a validated
literal IPv4 endpoint. Default port is 7777. The checkbox is user intent; actual
network state comes from World.GetNetMode through IsNetworkActive.

Disable repeated buttons while bTravelPending is true. Bind OnTravelError to show
the error and offer ReturnToOfflineMenu. Failed travel clears the pending flag;
map load clears it too, but does not prove all replicated gameplay state is ready.
Do not automatically reconnect or call Host from a widget Construct event.

Returning offline uses absolute OpenLevel with empty options, discarding inherited
listen options and tearing down the previous connection through travel. Leaving
as a host disconnects guests; this implementation has no host migration. Joining
requires LAN/routable connectivity, server port access, and platform network/local
network permissions. No NAT traversal, matchmaking, or relay service is provided.

## Combat authority paths

```text
Standalone input
    → RequestAttackAuthoritative directly
    → local timers / Active-only traces / damage
    → local impact delegate (no combat RPC)

Listen-host input / server-controlled AI
    → RequestAttackAuthoritative directly
    → server timers / traces / damage
    → replicated state + cosmetic impact multicast

Owning remote client input
    → ServerRequestAttack(AttackId)
    → server ownership, attack-definition, state, buffer and rate checks
    → server timers / traces / damage
    → replicated state + cosmetic impact multicast
```

Clients cannot submit target actors, damage, directions, or claimed hit results.
Attacks must exist in the server's authored attack map. The server rejects invalid
definitions, missing trace sources, stun states, expired/full buffered queues,
and requests faster than 20 Hz. UE enforces ownership of Server RPC calls. This
is a bounded gameplay check, not a complete anti-cheat system; add authoritative
equipment, resource, and attack-eligibility rules as game mechanics require.

Damage-interface dispatch occurs only on combat authority: standalone or server.
Client copies never execute the Active trace path, timers, stun mutation, or damage
modifier setters. Offline code never calls ServerRequestAttack or the multicast.
The session checkbox does not bypass authority: runtime net mode decides routing.

RequestAttack returns acceptance on authority, but on a client means submitted,
not accepted. There is no client prediction or rejection acknowledgement. Start
with this conservative path, then add prediction/reconciliation if measured latency
requires it. The server's input buffer remains authoritative.

## Animation and replication

Bind OnAttackStarted(AttackId, ElapsedSeconds) to play the corresponding montage:
on the server this drives authoritative weapon sockets; clients play cosmetic
copies and seek using elapsed time. Keep OnStateChanged for phase/stun/cancel
presentation rather than also starting the same montage twice.

NetworkState replicates phase, attack ID, attack sequence, and server timestamps.
Clients do not reproduce state timers. Replication may skip short intermediate
phases; consumers must use the current state rather than assume every event arrives.
An ongoing attack can be presented to a late joiner; an already completed attack
is not replayed. Clock fallback without a synchronized GameState is approximate.

Server meshes must evaluate animation and refresh bones even when not rendered.
AJNKPlayerCharacter enables AlwaysTickPoseAndRefreshBones in network authority
worlds and assigns weapon_tip on its mesh. Author that socket, or override
CombatTraceSocket / SetTraceSource for the actual weapon mesh. Apply the same
server-pose policy to enemy meshes. No skeletal animation asset is supplied here.

Replicate damaged actors and their authoritative health/death state separately;
the damage interface itself does not replicate health. Server-only AIControllers
must not run gameplay decisions on clients. The old standalone MeleeTraceComponent
must not independently apply client damage alongside UCombatComponent in co-op.
Combat actors must replicate, own their components correctly, and be awake/relevant
when activated; pooled/dormant actors need proper dormancy/activation handling.

MulticastImpactFeedback is unreliable and cosmetic. It cannot change health or
freeze server gameplay time. Bind camera shake only for the appropriate local
player; other actors' callbacks must not shake every player's camera. Non-relevant
or unreplicated target references may be null, so use the supplied impact data
for positional effects. Dropped feedback does not undo authoritative damage.

## Drop-in/drop-out scope

Normal GameMode login/spawning permits joining a running host; logout removes
the departing connection. Provide appropriate PlayerStarts and use the project
GameMode's login/logout hooks for inventory, progression, Director registration,
and encounter ownership. This change does not implement those persistence rules.

Hosting from the menu loads a fresh map; it does not promote an existing offline
run into a listen server without travel. The eight-agent AI cap remains server
owned and global, while player pawns are a separately budgeted category. Existing
metabolism, radio, sanity, and ambient-traffic documentation lists their own
replication limitations; enabling this menu does not network those systems.

## Validation

Added the native automation test JackNKill.Network.JoinAddressValidation for IPv4
normalization, port bounds, malformed input, and travel-option rejection. It has
not run here because Unreal is unavailable. Compile/UHT and run it in the editor.

Required gameplay checks: packaged standalone launch without a connection/listener;
offline combat with zero combat RPCs; explicit host and LAN join; late join during
Startup/Active/Recovery/stun; client request ownership/spam/invalid attacks; server
socket evaluation; one authoritative damage event per target/attack; replicated
health; player dropout; host departure; travel failures; return to NM_Standalone;
and simulated latency/loss with at least two separate processes. Inspect network
traces to establish RPC behavior. These checks remain unexecuted.
