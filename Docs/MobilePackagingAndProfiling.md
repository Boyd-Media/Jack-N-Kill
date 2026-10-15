# Android/iOS packaging and sustained device profiling

Start with Development builds for device profiling, then validate representative
Shipping builds. This repository still lacks a .uproject, Build.cs files, content
assets, and platform signing configuration. Commands below are integration
examples, not builds executed for this repository.

## Zlib asset compression

Config/DefaultGame.ini enables Pak, Io Store, compressed cooking, and Zlib as the
package compression format. Check Project Settings -> Packaging and platform
overrides to confirm they retain those settings. Inspect packaging logs to verify
the actual codec. Asset-container compression differs from APK/IPA ZIP compression.
Already compressed audio/textures may shrink little; decompression costs CPU.
Measure streaming/loading behavior on minimum-spec devices.

## Android APK

1. Install SDK, NDK, and Java versions supported by the exact UE release, using
   its documented Turnkey workflow.
2. Configure the application identifier and ARM64. Select Vulkan/OpenGL ES based
   on supported devices. Use ASTC only for a validated device set.
3. Disable Generate Bundle (AAB) for a directly installable test APK.
4. Choose Development and include startup/gameplay maps in the cook configuration.
5. Package using Platforms -> Android or the engine's RunUAT script:

```bat
RunUAT.bat BuildCookRun ^
  -project="C:\Projects\JackNKill\JackNKill.uproject" ^
  -platform=Android -clientconfig=Development ^
  -build -cook -stage -pak -iostore -compressed ^
  -package -archive -archivedirectory="C:\Builds\Android"
```

Use the generated installation script, especially when separate game-data files
are present. For an APK with its required data packaged appropriately:

```bash
adb install -r JackNKill.apk
```

APK output names vary by build. Google Play generally uses an AAB and release
signing; configure those separately from this direct-install test workflow.

## iOS IPA

Use a Mac, supported Xcode, signing identity, bundle identifier, and provisioning
profile. Configure Project Settings -> iOS, select Development, cook required
maps, and package using Platforms -> iOS or:

```bash
Engine/Build/BatchFiles/RunUAT.sh BuildCookRun \
  -project="/Projects/JackNKill/JackNKill.uproject" \
  -platform=IOS -clientconfig=Development \
  -build -cook -stage -pak -iostore -compressed \
  -package -archive -archivedirectory="/Builds/iOS"
```

Replace paths with actual project/engine locations. Install through the supported
Xcode/device workflow. The IPA must be signed for that installation method;
App Store distribution requires appropriate distribution signing/submission.

## On-device Unreal commands

Use a Development build. Open the mobile console with four-finger tap where
enabled, or a development-only UI executing console commands. Do not assume
Shipping exposes the console or all stat groups.

| Command | Inspect |
| --- | --- |
| stat unit | Frame, Game, Draw, GPU and available pipeline timings |
| stat unitgraph | Spikes and sustained timing drift |
| stat fps | Frame rate |
| stat RHI | RHI resources and available draw counters |
| stat scenerendering | Mesh draws and scene-rendering statistics |
| stat streaming | Texture streaming pressure |
| stat OpenGLRHI | OpenGL-specific counters, only on an OpenGL build if available |

stat OpenGLRHI does not apply to Android Vulkan or iOS Metal. Counter availability
and GPU timing support vary by engine/build/device. Use platform GPU tools when
overlays do not expose the required information. At 60 FPS the complete frame
budget is 16.67 ms. Game and render thread timing overlap; do not sum them.

## Thermal validation

Stats reveal performance drift, not its thermal cause. Repeat the same gameplay
route for 20-30 minutes, recording scene conditions, frame times, draw counts,
and available thermal/frequency evidence.

On Android:

```bash
adb shell dumpsys thermalservice
adb shell dumpsys battery
```

Manufacturer support differs. Battery temperature is not SoC temperature.
Use Android GPU Inspector/vendor tools where supported to inspect workloads and
frequency changes. On iOS use Xcode Instruments, Metal profiling, and an optional
native NSProcessInfo.thermalState bridge. Unreal has no single console command
that proves thermal throttling across both platforms.

If timing worsens while scene/draw counts stay stable, correlate it with thermal
state and frequency evidence. Also check streaming, memory pressure, and background
activity. Repeat sustained tests in a representative Shipping build because
Development instrumentation changes workload. No device performance results,
packaging output, or signing success is claimed by this guide.
