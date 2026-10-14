# Jack N Kill

UE 5.6+ mobile third-person open-world game architecture for melee combat,
horror survival, and vehicles.

This repository currently contains design documentation and integration stubs,
not a generated Unreal project. Create a UE 5.6+ C++ project named `JackNKill`
before integrating these sources. No Unreal build or device validation has run.

See [mobile architecture](Docs/MobileArchitecture.md).

See [combat component integration](Docs/CombatComponent.md) for the timed melee
state machine, input queue, primitive hit sweeps, and presentation delegates.

See [stalker AI and sanity integration](Docs/StalkerAIAndSanity.md) for perception,
the Behavior Tree asset specification, and mobile visual/audio presentation.

See [Blender GIS city pipeline](Tools/GIS/README.md) for footprint/height-surface
processing, atlas baking, and cell-local FBX export.

See [mobile vehicles and traffic](Docs/MobileVehiclesAndTraffic.md) for the
four-wheel Chaos pawn, platform physics budget, and spline-driven ambient ISMs.

See [mobile rendering and lighting](Docs/MobileRenderingAndLighting.md) for the
filmic post-process preset, Mobile HDR, and supported static-lighting workflows.
