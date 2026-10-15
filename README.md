# Jack N Kill

Start with [gameplay optimization and validation](Docs/GameplayOptimization.md)
for build instructions, changes, tests, and remaining engine integration gates.

UE 5.6+ mobile third-person open-world game architecture for melee combat,
horror survival, and vehicles.

This repository contains a UE 5.6 project descriptor, runtime module, game/editor
build targets, gameplay sources, and design specifications. Content assets and a
playable map are still required. Unreal compilation and device validation have
not run; portable production-rule and Blender pipeline tests are available.

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

See [parody translation dictionary](Docs/ParodyTranslationDictionary.md) for the
approved fictional names required in future code and dialogue.

See [five-act main quest](Docs/MainQuestOutline.md) for Mara Venn's ledger-driven
rescue story amid the turf war and Alien Stalker invasion.

See [trap house escape sequence](Docs/TrapHouseEscapeSequence.md) for the Act I
encounter layout, crawlspace pursuit, and interior rendering isolation guidance.

See [faction StateTree and AI Director](Docs/FactionStateTreeAndAIDirector.md) for
Alien-first targeting, vehicle evacuation, and the eight-agent activation cap.

See [Pro Athletes schedule and Behavior Tree](Docs/ProAthletesScheduleAndBehaviorTree.md)
for the nine-member roster, weekly routines, and wealth-funded security encounters.

See [Public Order corruption and bribes](Docs/PublicOrderCorruptionAndBribes.md)
for the officer controller, server-validated payment flow, and client UI bridge.

See [player metabolism](Docs/PlayerMetabolism.md) for daily body-morph progression,
sprint penalties, and the melee damage modifier.

See [vehicle radio](Docs/VehicleRadio.md) for imported soundtrack playback,
Enhanced Input controls, and spatialized cabin/door filtering.

See [offline-first co-op networking](Docs/OfflineAndCoopNetworking.md) for the
menu travel subsystem, replicated player template, and server-authoritative melee.

See [adaptive AI Director and thunderstorms](Docs/AIDirectorAndStorms.md) for
stress-driven encounters, shared admission limits, and storm gameplay modifiers.

See [inventory and world persistence](Docs/InventoryAndWorldPersistence.md) for
compact door/house/player saves, asynchronous storage, and bounded inventory.

See [World Partition and HLODs](Docs/WorldPartitionAndHLOD.md) for 128 m gameplay
cells and distant skyline representations.

See [mobile packaging and profiling](Docs/MobilePackagingAndProfiling.md) for
Zlib asset compression, APK/IPA workflows, and device/thermal measurements.
