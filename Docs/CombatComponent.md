# UCombatComponent integration

## Setup

Create a UE C++ project with the JackNKill module. Ensure its Build.cs declares
Core, CoreUObject, and Engine dependencies. These source files are not a complete
standalone Unreal project.

Add UCombatComponent to your character. Populate Attacks with named definitions
such as Light1 and Light2. Call SetTraceSource with a weapon scene component or
skeletal mesh and an existing socket. A weapon actor must be owned by the
character. Socket local Z is the capsule's long axis.

Configure GameTraceChannel1 as CombatHit. Target capsules and world occluders
block it; character skeletal meshes and irrelevant objects ignore it. Use simple
collision primitives, not Complex Collision As Simple. The component ignores
its owner and the source weapon actor.

Call RequestAttack from input. Accepted inputs start immediately in Idle or enter
the bounded FIFO in Startup, Active, or Recovery. Expired entries are discarded;
a full queue rejects new input. Stuns reject input and clear queued attacks.

## States and timing

Idle -> Startup -> Active -> Recovery -> Idle.

ApplyStun accepts HitStun or BlockStun, interrupts attacks, and returns to Idle
after the specified duration. CancelCombat clears pending input and returns to
Idle. Recovery completion consumes the oldest unexpired valid attack.

Only Active enables component ticking and collision queries. Startup, Recovery,
and stun transitions use world timers. A final Active sample runs before Recovery
to cover short windows. Timing is quantized by game frames; this is not subframe
animation reconstruction. Durations use world game time, including global time
dilation. Input expiration uses the same clock.

Animation playback is owned by the character/animation layer. Bind OnStateChanged
to coordinate animation with these timings. This initial component uses authored
seconds rather than montage notify windows. SetTraceSource cancels a current
attack to avoid tracing between unrelated sample positions.

## Traces

SweepSingleByChannel uses a sphere or capsule from the previous socket position
to its current position. A stationary sample still queries overlaps. Capsules
use the current socket rotation; sweeps do not continuously interpolate rotation.
This is one sample per component tick, plus the final Active sample. It does not
reconstruct a curved blade arc. Add bounded multi-point sampling if long or fast
weapons need it, and measure the resulting query budget on devices.

Each target can receive one accepted contact per attack. Already-hit actors stay
blocking, so this is not a cleaving implementation. World blocking hits do not
dispatch damage or feedback. A positive CanReceiveDamageHit response means an
accepted contact, not necessarily health loss; the receiver owns block/armor
resolution and must revalidate its state.

## Feedback

OnImpactFeedback supplies target, directional hit data, requested hitstop duration,
and an optional camera shake class. Bind the local player presentation layer to
start the shake through PlayerCameraManager. Avoid applying every actor's shake
to every player camera.

The delegate requests hitstop; the component does not freeze the world. Prefer a
coordinator for local animation freezes and overlapping hitstop requests. If
hitstop must pause gameplay state progression, coordinate the StateTimer and
combat ticking too: pausing animation or setting actor custom time dilation alone
does not pause world timers. Use a real-time restoration mechanism if freezing
the clock that normally drives timers, and always restore state on cancellation
or destruction.

## Scope and validation

The component now supports offline play and server-authoritative co-op; see
[networking integration](OfflineAndCoopNetworking.md). Owning clients submit an
attack ID, authority performs traces/damage, and clients receive replicated phase
state plus cosmetic feedback. It does not implement client prediction. Server
animation must evaluate weapon sockets independently of cosmetic client copies.

The repository has no .uproject or Unreal toolchain. Compilation, UHT validation,
collision behavior, timer/animation alignment, and mobile profiling remain
unverified. In an Unreal project, test idle/stun trace suppression, buffer expiry
and capacity, chained recovery, one hit per target, interruption during Active,
weapon destruction, wall occlusion, and feedback cleanup during hitstop.
