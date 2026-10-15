# Lightweight inventory and world persistence

## Architecture

ULightweightInventoryComponent stores up to 64 stacks of stable item IDs and
quantities. UWorldStateManagerSubsystem retains door overrides and cleared-house
IDs across map travel. UCompactWorldSaveGame explicitly serializes a bounded,
versioned binary payload with a CRC32 corruption check. Player metabolism uses
the existing complete FPlayerMetabolismSnapshot.

Dependencies: Core, CoreUObject, and Engine. AJNKPlayerCharacter now creates
Inventory and Metabolism components alongside Combat, and binds the offline/listen
host's local player to the manager on BeginPlay/possession. Do not add duplicate
Blueprint components of those types when migrating an existing character.

The save represents one persistent world and one bound player's inventory/body
state. Co-op guests do not overwrite the host's binding. Separate per-player
identity/progression storage and replicated world-actor state remain integration
work. The manager accepts mutations/save/load on standalone or server authority,
never on a network client.

## Inventory

Use AddItem and RemoveItem on authority; both return false for invalid requests,
capacity overflow, or insufficient quantity. GetQuantity and GetSnapshot provide
read access. OnInventoryChanged updates UI. Inventory replicates to the owning
connection when the owner actor replicates; clients do not mutate it directly.

Each stack has one unique, case-insensitive FName ItemId, at most 64 ASCII
letters/digits/underscores, and quantity 1..10,000. Example IDs: Ledger, Ration,
Medkit, RifleAmmo. Item visuals, descriptions, mass, and equipment definitions
belong in a separate catalog/Data Asset and are not copied into saves. There are
no per-item UObject instances or asset references in the persistence payload.

This is a stack inventory, not an equipment/grid/crafting system. Trusted gameplay
must validate pickups and consumption; there is no unchecked client AddItem RPC.
A bribe wallet may use authority-side RemoveItem on a Credits stack if its stack
limit suits the economy, or retain a separate authoritative wallet.

## Stable world IDs and streaming

Author a stable FGuid for every persistent door and house in map data. Generate
once in editor/content tooling, never on runtime BeginPlay. Duplicated instances
must get distinct IDs. Keep IDs stable through cooking and World Partition
streaming; they must not depend on actor memory addresses or runtime names.

On a door's BeginPlay, query GetDoorState(Id, AuthoredDefault) and apply its
geometry, collision, lock, and interaction state. After a transition, call
SetDoorState(Id, NewState, AuthoredDefault). Supported states are ClosedUnlocked,
ClosedLocked, Open, and Broken. Returning to the authored default removes the
override. The manager does not automatically operate door animations.

When a house encounter is cleared, call MarkHouseCleared(HouseId). On stream-in,
IsHouseCleared determines whether encounter spawning/loot should remain disabled.
Clearing logic itself is owned by quest/encounter gameplay. Bind already-loaded
actors to OnWorldStateRestored and reapply their records after a load. Newly loaded
actors query records on BeginPlay. In co-op, replicate each actor's resulting
runtime state from the server; this GameInstance subsystem is not a replicated
world-state actor.

## Save contents and hard budgets

The payload contains only:

- Save generation number.
- Up to 16,384 door GUIDs with one-byte state overrides.
- Up to 4,096 cleared-house GUIDs.
- Up to 64 inventory IDs/quantities.
- Fat, Muscular, TargetFat, TargetMuscular, DayProgress, CaloricIntake, StaminaBurn.

A GUID occupies 16 payload bytes. At configured maximum counts, these records
require roughly 350 KB before the small Unreal SaveGame wrapper. No mesh, texture,
actor graph, component object, or content asset is serialized.

Payload cap is 512 KiB. The manager measures the actual serialized SaveGame before
dispatching its write and rejects output above **1,000,000 bytes**, a stricter cap
than the requested 5 MB. Each alternating save slot stays below that limit; two
slots use roughly twice the per-file space. Platform-specific storage wrappers
still require packaged-device verification. Limits reject additional records;
they never silently discard door/house state. Larger worlds need deliberate
sharding/version changes rather than raising caps without profiling.

## Asynchronous persistence

Get the GameInstance's WorldStateManagerSubsystem and call SaveAsync(Profile)
at checkpoints or a debounced autosave interval. Profiles allow 1..32 ASCII
letters/digits/underscores and default to PlayerWorld. User index is zero.

```text
Game thread: capture immutable plain-data snapshot
Worker: validate and encode compact payload
Game thread: create SaveGame, verify complete serialized byte size
AsyncSaveGameToSlot: platform write
Game thread: OnSaveCompleted(success, message, byte count)
```

Only one save/load is allowed at a time. Check bBusy and coalesce later requests
instead of calling every frame. Gameplay may continue during a save; it captures
the state at request time, not mutations made while the write runs. Metadata checks
and snapshot capture remain game-thread work. UE's SaveGame wrapping/serialization
also occurs on the game thread, including the size-check pass. Its small bounded
payload reduces cost but **does not prove zero stutter**. Measure those stages and
flash writes on minimum-spec devices; if needed, further chunk snapshot capture
or save during a safe transition. Worker code touches no live actors/components.

SaveAsync alternates Profile_A and Profile_B. LoadAsync reads both asynchronously,
validates version/CRC and bounded records, and chooses the highest valid generation.
Failed or interrupted writes can leave the previous valid slot available. This is
fallback protection, not a guarantee of atomic platform storage or power-loss
durability. Unknown versions are rejected; migrations must be added explicitly.

Load an existing profile before saving over it so generation ordering stays
correct. If neither slot is valid, loading fails and preserves current state.
Errors and completion are surfaced through OnSaveCompleted/OnLoadCompleted; rejected
calls also return false. Do not exit/suspend assuming an unfinished save is durable.
Autosave proactively rather than relying solely on app-background shutdown time.

## Load and player restoration

Use LoadAsync from a loading/menu state and gate input, inventory transactions,
and the game clock while IsLoading is true. Door/house mutations are rejected
during load; external inventory/metabolism producers must obey the same loading
gate. This prevents a completed load from overwriting new gameplay changes.

On success the manager restores world records and fires OnWorldStateRestored.
If player components are not yet available, it retains the player snapshot until
BindPlayerComponents is called. The native local player binding does that after
spawn/possession. Custom characters should bind explicitly. Loaded metabolism
reapplies morph and movement/damage effects; actual Fat/Muscular mesh assets are
still required. Align/resume the world clock using saved DayProgress without
advancing the same interval twice. A full world clock/quest progression save is
not included by this format.

## Validation pending

Added JackNKill.Persistence.CompactRoundTripAndBudget, exercising maximum record
counts through Unreal SaveGame-to-memory/load-from-memory, item/world ID round-trip,
all metabolism fields, the one-megabyte budget, and invalid record rejection.
It remains unrun because this repository has no .uproject or Unreal toolchain.

Compile/UHT and run the test, then test asynchronous platform slots, interrupted
writes and corrupt headers/payloads, fallback generation selection, missing saves,
version rejection, streaming ID stability, capacity errors, co-op ownership,
deferred player restoration, app suspension, and game-thread/frame-time costs on
iOS/Android. No read/write latency or stutter measurements are claimed here.
