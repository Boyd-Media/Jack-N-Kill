# Mobile Chaos vehicle and ambient traffic

## Project dependencies

Enable the Chaos Vehicles plugin. The JackNKill module needs Core, CoreUObject,
Engine, and ChaosVehicles dependencies. Add EnhancedInput if the controller uses
Enhanced Input bindings. This repository still has no generated project/module
files or Unreal engine toolchain; the C++ setup has not been compiled or driven.

## Four-wheel drivable pawn

Create BP_MobileCar derived from AMobileChaosVehiclePawn. Assign a vehicle skeletal
mesh with +X forward, +Z up and wheel bones Wheel_FL, Wheel_FR, Wheel_RL, Wheel_RR.
Adjust names/radii to match the asset before physics initialization. Correct
wheel bone orientation and metre-to-centimetre import scale are essential.

Assign a Physics Asset with a simple chassis body and appropriate collision
filtering. Avoid simulated wheel rigid bodies fighting Chaos wheel simulation.
Use the engine Vehicle Animation Instance/Wheel Controller node to animate the
four wheels. Attach your camera/spring arm in the Blueprint. The sample does not
load hardcoded asset paths or provide meshes, physics assets, or animation assets.

The setup uses 1400 kg mass, a lowered centre of mass, automatic transmission,
front steering, rear drive, and simple suspension raycasts. Dimensions are cm;
the torque curve is normalized against MaxTorque. Wheel/suspension/engine values
are tuning starting points, not a calibrated vehicle. Test tyre grip, slopes,
curbs, suspension travel, and rollover on your road collision.

From the possessing controller call SetDriveInputs(throttle 0..1, steering -1..1,
brake 0..1, handbrake). Inputs clear on unpossession. Bind reverse gear/automatic
reverse behavior separately if required; this API does not use negative throttle.
Pawn Tick is disabled, while movement and physics continue through their own
components. Never throttle a drivable vehicle's simulation by skipping its
movement ticks without an explicit physics design.

## Mobile physics settings

Platform config files are supplied at Config/Android/AndroidEngine.ini and
Config/IOS/IOSEngine.ini. Both set:

```ini
[/Script/Engine.PhysicsSettings]
bSubstepping=True
bSubsteppingAsync=False
bTickPhysicsAsync=False
MaxSubstepDeltaTime=0.016666667
MaxSubsteps=2
MaxPhysicsDeltaTime=0.033333333
```

This is a bounded world-physics starting budget: aim for a maximum 16.67 ms
substep and at most two substeps per frame, with physics delta clamped to 33.33 ms.
At steady 60/30 fps, this targets roughly one/two world substeps. Frame hitches
can cause lost simulation time/slow motion rather than an unbounded catch-up
spike. The settings affect all world physics, not just this pawn. They deliberately
do not enable the separate async physics tick option.

Chaos Vehicles has its own asynchronous simulation path; world substep settings
are not proof of a fixed 60 Hz wheel simulation across engine versions/platforms.
Inspect actual solver/vehicle timing in your UE build. Confirm settings are
supported and active in packaged iOS/Android builds. Platform overrides may need
merging with project settings. Do not copy desktop eight-substep budgets to
mobile without profiling. Measure physics thread cost, frame hitches, and thermal
load on the minimum supported processors, then tune timestep and active-vehicle
count together. Keep fully simulated cars limited to player/nearby interactions.

## ATrafficManager setup

Place an ATrafficManager, assign a conventional low-poly static car mesh, and
place ATrafficRoadSpline lane actors along road surfaces. Lane direction is spline
direction; bank/elevation come from spline rotation/position. MeshForwardCorrection
handles a mesh imported with a different forward axis. MeshHeightOffsetCm handles
its pivot height. Use one opaque material slot and conventional mesh LODs.

Keep the manager loaded in the player's region (an always-loaded manager is a
simple initial option). Assign each lane's optional Manager soft reference to
register on BeginPlay and unregister on EndPlay. Alternatively populate manager
Roads with soft references. Only loaded road actors are used; no synchronous
asset load or per-frame world scan occurs. Soft references avoid hard-reference
bundling of unrelated World Partition road actors. Prefer runtime registration
of nearby loaded roads over thousands of inactive road references. Runtime-spawned
managers need registration calls after the manager exists.

SetTrackedPlayer defaults to player zero at BeginPlay. Call it again after the
player spawns, respawns, or changes possession to a vehicle. Location tracking
uses the tracked pawn, so it follows a drivable vehicle only after rebinding.
The actor is cosmetic/client-local and does not tick on dedicated servers.

Default behavior:

- Spawn candidates must be within **150 m** of the tracked pawn.
- Existing instances stay alive until they exceed **250 m**, giving hysteresis.
- Maximum 96 cars, with up to four spawn requests every 0.5 seconds.
- Each spawn request has at most 12 candidate samples near the player's closest
  spline position. Candidates respect 15 m initial spacing.
- Movement updates at 10 Hz with cached transforms and a single batch ISM update.
- Closed lanes loop. Open lanes remove cars at their endpoint; streamed-out roads
  remove their cars. Instance storage rebuilds once after structural changes,
  avoiding assumptions about RemoveInstance index-shifting/swap behavior.

One instanced mesh component represents the traffic: no actor, skeletal animation,
AI controller, overlap events, collision, navigation update, or Chaos simulation
per ambient car. Shadows are disabled. A material slot still costs draws per
rendering pass; multiple materials and high-poly source meshes increase costs.
This is bounded ambient rendering, not a fixed total draw-call guarantee.

The 10 Hz transform updates can visibly step at close range. Use a higher manager
tick rate for near views, or add a measured interpolation/promotion layer. This
sample has spawn spacing but no car-following, junction control, dynamic obstacle
avoidance, or continuous collision. Vehicles can pass through each other/player
and may pop into view inside the spawn radius. Promote selected close instances
to pooled Chaos pawns for interactive traffic; this sample does not implement
promotion or multiplayer traffic replication.

## Required Unreal validation

Compile/UHT, packaged platform config checks, and physical-device tests remain
pending. Verify all four wheels on the expected bones; braking/steering/reverse;
unpossession input reset; substep count/cost at 30/60 fps and during hitches;
spawn boundaries at 149/151 m; retention/removal at 249/251 m; lane loop/endpoints;
road stream-out/reload; player rebinding; transform scale/orientation; the 96-car
cap; and visual motion quality. Pure radius checks run at the manager update
cadence, so boundary despawn can occur up to one tick after crossing.
