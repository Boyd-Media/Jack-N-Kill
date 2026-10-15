# Mobile architecture

## Rendering baseline

Ship using the standard mobile renderer. Lumen and Nanite are not supported on
that rendering path. Experimental Android desktop rendering requires separate
hardware, driver, shader, and packaging validation; it is not enabled by device
profiles. Use baked lighting, reflection captures, conventional mesh LODs,
World Partition streaming, HLODs, and limited dynamic shadows.

`Config/DefaultDeviceProfiles.ini` supplies opt-in starting profiles. Configure
Android device matching and the iOS profile hierarchy to select them for tested
devices. Profile definitions alone do not select themselves. Set rendering and
shader options in Project Settings before cooking. Content scale factors have
different platform meanings. Measure sustained frame time, thermal behavior,
memory, and visual quality on physical devices before choosing shipping values.

## Folder layout

```text
Content/JackNKill/{Core,Characters,Combat,Survival,Vehicles,World,UI,Audio,FX}
Source/JackNKill/Public/{Framework,Characters,Components,Interfaces}
Source/JackNKill/Private/{Framework,Characters,Components,Interfaces}
Plugins/                         # Optional survival, vehicle, editor features
Assets/{Characters,Environment,Vehicles,Audio,Reference} # Source art
```

Content contains imported Unreal assets. Assets contains source art; use Git LFS
for large binaries. Add plugins only where a reusable feature boundary warrants
them. Use Enhanced Input and Chaos Vehicles through engine plugins.

## Gameplay framework

| Class | Base | Responsibility |
| --- | --- | --- |
| AJNKGameMode | AGameModeBase | Spawn, death/respawn, default classes |
| AJNKGameState | AGameStateBase | World time, encounters, shared conditions |
| AJNKPlayerController | APlayerController | Touch input, camera, vehicle possession |
| AJNKCharacterBase | ACharacter | Movement and damage interface delegation |
| AJNKPlayerCharacter | AJNKCharacterBase | Player targeting and interaction |
| AJNKEnemyCharacter | AJNKCharacterBase | Enemy combat with bounded AI updates |
| AJNKVehiclePawn | APawn | Vehicle movement and driving |

Separate health, combat, interaction, inventory, and survival into components.
Use a GameInstance subsystem and SaveGame for persistence. Framework classes
above are proposed architecture, not implemented classes.

## Combat integration

Add Core, CoreUObject, and Engine module dependencies. The stubs assume the
module name JackNKill and its JACKNKILL_API export macro.

Configure GameTraceChannel1 as a CombatHit trace channel: target capsules block
it, skeletal meshes and irrelevant objects ignore it. Use explicit simple
collision primitives; assets configured as Complex Collision As Simple defeat
the intended collision budget.

Open and close attack windows from animation/combat state. During an active
window, sweep between previous and current weapon sample positions. Sample a
few locations along long weapons and cap subdivisions for fast rotational
motion. Ignore the attacking character and its weapon actor.

SweepMultiByChannel stops at the first blocking hit. This implementation models
single-target contact along each sample sweep; cleaving through multiple targets
requires an intentional overlap/object-query policy.

Implement CanReceiveDamageHit_Implementation and ReceiveDamageHit_Implementation
on receivers. Receivers revalidate armor, immunity, and execution eligibility.
Requested reactions are not guaranteed outcomes. Apply damage on the authority
in multiplayer; this component does not implement replication or prediction.

The trace component has no autonomous tick. Its caller supplies active-window
samples. It reuses query storage and deduplicates targets per attack window.

## Validation status

Project/module scaffolding is supplied alongside the integration sources. There
is no Unreal toolchain, cooked build, or physical-device test result here yet.
