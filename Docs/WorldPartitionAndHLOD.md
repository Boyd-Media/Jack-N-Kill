# Mobile World Partition and skyline HLODs

Use 128 m gameplay cells and separate distant HLOD grids. Automatic HLOD generation
does not flatten a city into an impostor; author skyline cards/shallow geometry
separately. One city-wide mesh has poor culling and streaming granularity.

## Gameplay grid

For Runtime Spatial Hash, configure World Settings -> World Partition Setup:

| Setting | Starting value |
| --- | --- |
| Runtime Grid | CityGameplay |
| Cell Size | 12800 cm |
| Loading Range | 25600 cm |
| Block on Slow Streaming | Disabled |

Runtime Hash Set uses different editor controls; configure its equivalent spatial
partition settings for the selected UE version. Mark ordinary buildings/street
props spatially loaded, assign the grid, and avoid hard-reference bundling across
distant regions. Cell size is not a memory cap: textures, dependencies, actor bounds,
overlapping sources, and loading range determine residency. Increase loading range
if vehicle speed and measured streaming latency require more lead time.

## HLOD layer hierarchy

| Layer | Type | Cell Size | Loading Range | Parent |
| --- | --- | --- | --- | --- |
| HLOD_CityBlocks | Mesh Simplify | 25600 cm | 76800 cm | HLOD_CityDistricts |
| HLOD_CityDistricts | Mesh Simplify | 51200 cm | 153600 cm | None |

Initially disable Always Loaded. Assign eligible static buildings to the block
layer, including modular Blueprint components where appropriate. Keep rooflines,
towers, and structural silhouettes. Exclude movable doors, interiors invisible
at distance, tiny clutter/signage, traffic, characters, and effects.

Disable Nanite generation and collision on these distant mobile proxies. Merge
materials into a simple opaque material, choose suitable source LODs, and omit
normal-map baking where it adds no useful distant detail. Begin with these output
acceptance targets rather than assuming an exact triangle-count slider exists:

| Proxy | Triangle target | Base-color atlas |
| --- | --- | --- |
| Block | 2000-5000 | 512-1024 px |
| District | 500-2000 | 512 px |

Adjust simplifier screen-size/merge/approximation controls based on results.
Preserve courtyards and gaps important to silhouettes. Avoid baked specular
highlights. Unlit distant materials need an authored day/night appearance policy.

## Build and inspect

Save actors/map, then use Build -> Build HLODs. An example commandlet pattern:

```bash
UnrealEditor-Cmd.exe Project.uproject /Game/JackNKill/World/Maps/Gatorwake \
  -run=WorldPartitionBuilderCommandlet \
  -Builder=WorldPartitionHLODsBuilder \
  -SetupHLODs -BuildHLODs -AllowCommandletRendering
```

Replace paths with actual assets and inspect logs for the exact engine/runtime
hash version. Use a rendering-capable build machine. Inspect proxy triangles,
material sections, texture memory, rooflines, missing gaps, bleeding, and floating
geometry. Test street/rooftop/river/vehicle views and confirm original actors stream
out when replacement representations become active.

## Flattened skyline

Divide the far skyline into roughly 4-8 sectors. Capture representative approach
views offline, bake color/silhouette masks, and author tight silhouette-cut opaque
cards or shallow geometry with one atlas material per sector. Avoid large masked
rectangles that create excessive mobile overdraw. Several views/shallow meshes
handle parallax better than a single flat mesh in a freely traversable city.

A single flat sector is appropriate for constrained vistas such as a distant
view across Crooked Current River. Place sectors in a dedicated spatially loaded
actor group/Data Layer and explicitly implement their handoff from district HLODs.
Do not leave both representations visible. Custom cards do not automatically
become HLOD-builder replacements.

No map, grid asset, HLOD bake, skyline mesh, or device validation is supplied by
this document. The intended progression is full buildings -> block proxies ->
district proxies -> authored skyline sectors, verified in cooked device builds.
