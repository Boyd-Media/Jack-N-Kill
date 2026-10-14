# Mobile filmic rendering and precomputed lighting

## Rendering baseline

Use Mobile Forward rendering, Mobile HDR, baked environment lighting, and a
short-range directional shadow budget. Config/DefaultEngine.ini enables Mobile
HDR and the mobile film tonemapper. Restart the editor and rebuild/cook affected
shaders after changing HDR. Disable Lumen and screen-space reflection methods in
Project Settings. Use reflection captures for environment reflections.

This is a starting preset, not a 60 FPS guarantee. The repository has no Unreal
project, serialized Post-Process Volume, lighting build, or device measurements.

## Post-Process Volume preset

Create an unbound volume with Priority 10 and Blend Weight 1. Enable each listed
property's override checkbox:

| Setting | Value |
| --- | --- |
| Infinite Extent / Unbound | Enabled |
| Exposure Metering Mode | Manual |
| Apply Physical Camera Exposure | Disabled |
| Exposure Compensation | 0.0 |
| Bloom Intensity | 0.0 |
| Lens Flare Intensity | 0.0 |
| Screen Space Reflection Intensity | 0.0 |
| Ambient Occlusion Intensity | 0.0 |
| Motion Blur Amount | 0.0 |
| Chromatic Aberration Intensity | 0.0 |
| Vignette Intensity | 0.30 |
| Film Grain Intensity | 0.0 |
| Film Slope | 0.88 |
| Film Toe | 0.55 |
| Film Shoulder | 0.26 |
| Film Black Clip | 0.0 |
| Film White Clip | 0.04 |
| Global Saturation | (0.85, 0.85, 0.85, 1.0) |
| Global Contrast | (1.05, 1.05, 1.05, 1.0) |
| Global Gamma | (1.0, 1.0, 1.0, 1.0) |
| Global Gain | (1.0, 1.0, 1.0, 1.0) |
| Global Offset | (0.0, 0.0, 0.0, 0.0) |

Disable depth of field in the gameplay camera/post-process configuration. Mobile
feature support differs from desktop; properties unsupported by a particular
mobile path do not create equivalent desktop effects. Verify the output on-device.

With physical camera exposure disabled, ISO/shutter/aperture do not control
exposure. Calibrate illumination and Exposure Compensation on a reference shot.
If explicitly enabling physical camera exposure instead, ISO 100, shutter 1/60,
and aperture f/4 are an alternative calibration starting point, not an extra
setting to apply to the disabled mode. Maintain stable exposure while grading.

## Anamorphic-inspired treatment

Start around an 80-degree horizontal FOV and adjust to screen aspect ratio.
Use restrained authored horizontal streaks on selected lamps rather than a
fullscreen multi-pass filter. Emissive cards still have overdraw costs. Put grime
and uneven surface response in environment textures/materials. Preserve useful
shadow detail for navigation. Optional UI letterboxing must respect touch controls
and safe areas. These choices approximate anamorphic aesthetics; grading does not
reproduce optical squeeze or oval bokeh.

## Static lighting and visibility are separate

Lightmass stores illumination. Precomputed Visibility stores which static objects
may be visible from particular camera cells; it does not bake lighting and does
not cull movable actors.

Do not assume conventional Lightmass and visibility workflows are available in a
World Partition map. Static-lighting support depends on UE version/configuration,
including experimental paths. Precomputed Visibility is not a supported general
World Partition visibility solution. For conventional Lightmass plus visibility,
use non-World-Partition levels with streaming sublevels. If retaining World
Partition, validate its supported lighting path in the chosen engine build and
use streaming, HLODs, and distance culling for visibility. These instructions do
not change the project's existing World Partition architecture automatically.

## Bake workflow for supported non-World-Partition levels

1. Set buildings, roads, and environment props to Static. Supply a separate,
   non-overlapping lightmap UV channel with padding. Facade atlas UVs are not
   automatically appropriate lightmap UVs.
2. Start at 64-128 lightmap resolution for small props and 128-256 for building
   sections; size by surface area and inspect memory/texel density. Split huge
   city meshes rather than putting a low-resolution lightmap on an entire block.
3. Use Static local fixtures and a baked Sky Light. Use one Stationary Directional
   Light for baked environment lighting plus nearby movable-character lighting
   and shadows.
4. Bound playable areas with Lightmass Importance Volumes. Build streamed levels
   using their intended persistent-level lighting arrangement.
5. Enable Allow Static Lighting and disable Force No Precomputed Lighting. Build
   Preview lighting during iteration, inspect UV/seam errors, then build Production.
6. Enable Precompute Visibility in World Settings. Place visibility volumes around
   occluded streets, courtyards, and interiors. Match cells to third-person camera
   positions, including camera height/motion. Build visibility after layout stabilizes.

Inspect cell coverage and visibility memory before expanding across large areas.
Open terrain and elevated viewpoints often provide little useful precomputed
occlusion. Visibility volumes are not replacements for world streaming.

## Player and Alien Stalker lighting

Set the player and stalker to Movable. Verify the precomputed indirect-lighting
representation supported by the actual mobile path rather than assuming desktop
Volumetric Lightmap behavior.

Start the Stationary Directional Light with Dynamic Shadow Distance (Stationary)
5000 cm, one Dynamic Shadow Cascade, Shadow Resolution Scale 0.5, Cast Dynamic
Shadows enabled, and Inset Shadows for Movable Objects disabled. Enable the
appropriate mobile CSM shader support before cooking. Allow static ground to
receive character shadows where required.

A Stationary Directional Light is not automatically restricted to those two
characters. Keep environment lighting baked, disable unnecessary dynamic shadow
casting on other movables, and avoid extra dynamic lights. Drivable vehicles
also require an explicit lighting strategy. The supplied ambient ISM traffic
already disables shadow casting; that does not make its materials unlit.

## Validation

60 FPS allows 16.67 ms for the complete frame. Profile Mobile HDR bandwidth,
directional shadows, visible geometry, character materials, and authored streak
overdraw together on minimum supported iOS/Android devices under thermal load.
Verify exposure, tone mapping, shadow readability, lightmap/visibility memory,
packaged shader support, and camera paths. No Unreal or device checks have run
for this preset; configuration and editor settings are recommendations only.
