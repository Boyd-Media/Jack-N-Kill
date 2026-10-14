# Chaos vehicle radio

## Playback type

UVehicleRadioSubsystem uses UAudioComponent and an array of USoundBase references.
Imported Sound Waves and Sound Cues both derive from USoundBase. UMediaSoundComponent
cannot play those assets directly: it receives decoded audio from a UMediaPlayer.
For a future media-file/stream radio, use a separate MediaPlayer/MediaSound path
and its supported media sources. This implementation does not pretend those APIs
are interchangeable.

## Dependencies and assets

The subsystem requires Core, CoreUObject, and Engine. The updated vehicle pawn
also requires ChaosVehicles and EnhancedInput, with their engine plugins enabled.
Add EnhancedInput to Build.cs; the previous pawn did not require it directly.

Import the original soundtrack as Sound Waves or construct Sound Cues. Assign
the assets to BP_MobileCar's RadioPlaylist. Registration filters null entries,
rejects empty playlists, and creates an actor-owned, spatialized AudioComponent
attached at the vehicle root. Adjust the attachment/emitter location for the
actual cabin pivot if required. Sound assets are referenced for cooking; ensure
their compression/streaming policy suits the device memory budget.

Create Boolean Input Actions IA_RadioNext, IA_RadioPrevious, and IA_RadioToggle.
Assign them to NextTrackAction, PreviousTrackAction, and ToggleRadioAction. Add
them to an IMC_Vehicle mapping context and assign VehicleInputMapping. The pawn
binds Started, so held buttons do not skip a track every frame. It installs the
context at priority 1 for a local driver and removes it on exit. Use a dedicated
vehicle context that is not concurrently owned by another input system.

Touch buttons may call NextRadioTrack, PreviousRadioTrack, and ToggleVehicleRadio
directly. The subsystem exposes NextTrack, PreviousTrack, and ToggleRadio with a
Vehicle argument for other Blueprint controls. Drive input bindings remain owned
by the existing controller setup.

## Playback behavior

Radio starts disabled. Toggle enables/stops the selected track. Next and Previous
wrap through the playlist; while disabled they change selection without starting
playback. Track switches and toggling back on restart playback at the beginning.
There is no pause/resume, beat-aligned transition, or automatic track advance.
Use looping/sequence Sound Cues or add an explicit completion-driven playlist
policy if uninterrupted programming is required.

Registration is idempotent for the same vehicle. To replace a playlist, unregister
then register it. Vehicle EndPlay unregisters and destroys its AudioComponent;
world subsystem teardown stops all registered audio. At most eight radios are
registered locally. Registration failure is logged by the pawn. Ambient ISM
traffic does not create radios; add a separate audible-radio concurrency budget
if many interactive vehicles need music at once.

## Spatialization and doors

The emitter uses spherical linear attenuation: full-volume radius 300 cm and
1700 cm falloff beyond that, approximately silent by 20 m. It is not UI audio.
Inside listener: 20 kHz low-pass cutoff and volume multiplier 0.55.
Outside with doors closed: 1.8 kHz cutoff and multiplier 0.30, plus distance falloff.
The subsystem smoothly blends those values through a temporary 20 Hz timer and
stops the timer when settled. No permanent audio-management Tick is needed.

Possession/restart/controller replication update driver entry/exit state. Radio
continues playing on unpossession, becoming muffled instead of following the
player. Passenger interaction must call SetListenerInside explicitly. This simple
policy assumes doors close upon exit; integrate actual door state to hold the
inside cutoff while a door remains open. Occlusion traces are disabled; wall
occlusion is not implemented by this door filter.

Sound asset settings and Sound Cue nodes must not override the intended
attenuation/filter policy unexpectedly. Verify stereo spatialization on the
selected audio mixer; use a mono cabin emitter or appropriate stereo-spread
settings if the source collapses/positions poorly. Calibrate levels with engine,
dialogue, and ambience rather than assuming these multipliers prevent clipping.

## Networking and scope

Playback is local cosmetic state and does not run on dedicated servers. Radio
selection/power are not replicated. Each network client's world owns its playback
and listener filter; synchronized shared radio state needs a separate replicated
vehicle state. An AudioComponent filter affects its entire output, so this setup
assumes one local listener. Split-screen listeners need per-listener playback or
another routing architecture rather than conflicting calls to SetListenerInside.

## Validation status

No soundtrack assets, Input Action/Mapping assets, .uproject, or Unreal toolchain
are present. Compilation/UHT and playback/device tests remain pending. Test
empty/null playlists, one-track wrapping, Next/Previous while off, toggle restart,
Sound Wave and Sound Cue playback, input-context removal, local/network possession,
exit muffling, distance falloff, passenger/door state, destruction/stream-out,
registration capacity, and packaged mobile asset cooking.
