# Horror stalker AI and player sanity

## Integration

Open JackNKill.uproject with UE 5.6+. The runtime module declares AIModule,
GameplayTasks, NavigationSystem, Core, CoreUObject, and Engine. Assign AStalkerAIController
to the monster character, configure Auto Possess AI, and generate navigation for
its large capsule. Validate streamed navigation and narrow passages.

The C++ controller, native BT task, and sanity component are provided. This
repository has no Unreal Editor to serialize Behavior Tree or material .uasset
files; create the editor assets using the specifications below.

## Blackboard: BB_HorrorStalker

Create these exact keys:

| Key | Type | Meaning |
| --- | --- | --- |
| TargetActor | Object, base class Actor | Visible tracked player |
| HasSight | Bool | Confirmed sight |
| InvestigationLocation | Vector | Last sight/noise position |
| HasInvestigation | Bool | Location remains worth investigating |

## Behavior Tree: BT_HorrorStalker

Use BB_HorrorStalker and assign the asset to BehaviorTreeAsset on the controller.
Create the following graph in Content/JackNKill/Characters/Enemies/Stalker/AI:

```text
Root
└── Selector
    ├── Sequence: Pursue
    │   ├── Decorator: HasSight == true [Observer Aborts: Both]
    │   ├── Move To: TargetActor
    │   │   [Observe Blackboard Value: true, Track Moving Goal: true]
    │   └── Wait: 0.25 seconds
    ├── Sequence: Investigate
    │   ├── Decorator: HasInvestigation == true [Observer Aborts: Both]
    │   ├── Move To: InvestigationLocation [Observe Blackboard Value: true]
    │   ├── Wait: 2.0 seconds
    │   └── Clear Stalker Investigation [native task supplied]
    └── Wait: 1.0 second
```

Decorators belong on their Sequence nodes. Sight aborts lower-priority branches.
Tune Move To acceptance radii to capsule size; start at 150 cm. Investigation
expires after eight game-time seconds. No ticking BT service is required. The
final Wait is idle behavior; patrol and attack selection are separate extensions.

Register the player for AISense_Sight with AIPerceptionStimuliSourceComponent.
Emit noise on footsteps/impacts, not every frame:

```cpp
#include "Perception/AISense_Hearing.h"
UAISense_Hearing::ReportNoiseEvent(
    GetWorld(), NoiseLocation, 1.0f, PlayerPawn, 2500.0f, TEXT("Footstep"));
```

The controller filters the tracked pawn, so use PlayerPawn as Instigator for
vehicle noise too. Emit engine noise at a low cadence while moving. Call
SetTrackedPlayer after respawn/possession changes. Hearing reveals the reported
location, not the player's later hidden position.

## AI scheduling

Controller tick intervals are 0.10 s within 15 m, 0.35 s within 40 m, and 1 s
beyond 40 m. Perception callbacks remain immediate. Distance controls scheduling
only; it is not sensory evidence. SetActorTickInterval does not throttle the
global perception system, path following, character movement, or animation.
Profile sight query budgets separately and use animation update-rate optimization
and encounter streaming for distant actors. Do not disable movement ticks just
because the controller is distant.

## Sanity and visibility

Add UPlayerSanityComponent to the player and assign Monster at spawn. It updates
local presentation at 10 Hz. Camera cone and distance checks precede at most one
simple visibility trace. Walls prevent drain. GetNormalizedSanity returns 1 for
stable and 0 for maximum distress. Default drain is 0.08/second and recovery is
0.025/second in game time. Smooth the value in presentation if updates are visible
or audible. Override the monster's GetActorEyesViewPoint if its default eye point
does not represent a visible head/chest location. One ray is a visibility
approximation, not a screen-coverage test. This is not replicated survival state.

## Mobile material: M_SanityMobile

Use Material Domain = Post Process and a scalar Sanity parameter. Choose a
blendable location supported by your mobile renderer, commonly Before Tonemapping.
Enable Mobile HDR if required by that rendering path before cooking and measure
its bandwidth cost. Use only PostProcessInput0 and viewport UVs:

```text
Distress = 1 - saturate(Sanity)
P = ViewportUV * 2 - 1
Vignette = smoothstep(0.25, 1.25, dot(P, P))
Source = PostProcessInput0.rgb
Luma = dot(Source, float3(0.2126, 0.7152, 0.0722))
Desaturated = lerp(Source, Luma.xxx, 0.45 * Distress)
Tinted = Desaturated * lerp(float3(1,1,1), float3(0.88,0.96,0.90), Distress)
Emissive = Tinted * (1 - Vignette * 0.5 * Distress)
```

InitializePresentation creates/registers a dynamic material and updates Sanity.
Use a dedicated unbound post-process component with BlendWeight = 1 for this
single-player setup. Other blendables are preserved. Split-screen needs per-camera
post-process settings. Validate the shader on your renderer/device; fall back to
built-in vignette/color grading if mobile material support or HDR cost is unsuitable.

## Audio / RTPC

Pass a dedicated ambience UAudioComponent to InitializePresentation. It receives
the Sanity float input and pitch scales from its initial multiplier at sanity 1
to 80% of that multiplier at sanity 0. Rebinding or destruction restores the
original pitch and resets the component-owned Sanity input to 1. Ordinary pitch
multiplier also changes playback speed; use suitable DSP for time-preserving
pitch shifts. Expose Sanity as a MetaSound input for additional modulation, and
avoid accidentally applying the pitch curve twice.

For Wwise/FMOD, bind OnAudioParameter to an object-scoped RTPC/parameter setter.
The delegate supplies the parameter name and normalized value. Author a 0..1
range or scale by 100 for an existing 0..100 RTPC. Map lower sanity to lower pitch.
Pass nullptr for native ambience when middleware owns audio. Restore its
parameter during presentation teardown. No middleware plugin API is assumed.

## Validation

Project/build scaffolding exists; the engine toolchain, BT/material assets, and
device results are unavailable here.
Compilation/UHT and editor integration remain unverified. Test visible pursuit,
sight-loss investigation, occluded noise, respawn rebinding, distance intervals,
navigation for the monster capsule, sanity bounds, wall occlusion, material
parameter updates, audio pitch/range, and cleanup/reinitialization in Unreal.
