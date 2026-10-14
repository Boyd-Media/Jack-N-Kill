# Mobile city building pipeline — Blender 4.x

## Run

Use a dedicated external Python environment for GIS packages, avoiding binary
GDAL/PROJ compatibility problems inside Blender's bundled Python:

```bash
python -m venv .venv-gis
.venv-gis/bin/python -m pip install -r Tools/GIS/requirements.txt
blender -b --factory-startup --python-exit-code 1 \
  --python Tools/GIS/build_mobile_city.py -- \
  --footprints /data/buildings.shp \
  --surface /data/ground_dem.tif \
  --target-crs EPSG:32618 \
  --gis-python "$PWD/.venv-gis/bin/python" \
  --output /data/export_city \
  --cell-metres 128 --visible-chunks 9 --max-chunk-triangles 55000 \
  --atlas-size 2048
```

Replace the CRS with a suitable local projected metre CRS. On Windows, use the
venv's Scripts/python.exe. The output directory must be empty. The script resets
the Blender scene; run in background with a disposable scene, not an unsaved art
session. --python-exit-code ensures Blender failures return a nonzero status.

## Inputs and GIS handling

Supply .shp/.shx/.dbf and a .prj file, or explicitly specify --footprint-crs.
OSM exports must already be building polygon shapefiles. Read footprint height
from --height-field (default height), or --levels-field (default levels), or the
9 m fallback. Check field names: shapefile names are often truncated to ten
characters. Heights accept numeric metres, `12 m`, or `40 ft`; ambiguous values
fail rather than silently assigning incorrect geometry. Floor height defaults
to 3 m.

Supply a georeferenced single-band DEM or rasterized LiDAR **ground** surface.
Raw LAS/LAZ point clouds must first be classified/rasterized by your GIS pipeline.
A DSM containing building roofs would double-count height when extruded. Set
--height-scale for non-metre vertical units; horizontal CRS reprojection does
not perform vertical datum transformations. Both datasets need compatible
vertical datums. Missing CRS, nodata, nonfinite samples, or incomplete coverage
fail explicitly. This tool builds city geometry; it does not generate UE terrain.

Footprints are reprojected before topology-preserving simplification (default
0.20 m). Constrained triangulation supports concave roofs and courtyard holes.
Each footprint has a flat foundation at its minimum sampled perimeter elevation
and roof at maximum sampled elevation plus its authored height. This avoids
floating foundations on slopes, but is not survey-grade building reconstruction.
There are no bottom faces or interiors. Inspect clipping against imported terrain.

## Geometry, atlas, and budgets

Buildings are assigned to grid cells by an interior representative point and
joined per cell. Pivots are local to each cell to avoid large GIS coordinates in
mesh vertices. Cell anchors retain projected world positions in manifest.json.
Very large buildings can cross cell bounds; manifest bounds report the actual
extent. Smaller cells improve streaming granularity at a higher draw-call cost.

If source triangles exceed the budget, a collapse decimator reduces them with
10% headroom. Geometry is triangulated and its **actual final count** checked;
the pipeline fails if the cap is still exceeded. Decimation occurs before UV
packing/baking. Aggressive reduction can distort corners, holes, or disconnected
buildings; visually review outputs. It cannot manufacture acceptable art quality
for an arbitrarily small budget.

The per-chunk cap is min(max-chunk-triangles, view-triangles / visible-chunks),
rounded down. Defaults produce at most 495,000 building triangles across nine
visible chunks. This is conditional on the visibility assumption, not a camera
visibility proof. Camera height, draw distance, streaming cells, HLODs, shadows,
terrain, vehicles, and characters must be budgeted in Unreal. Use a lower
building budget if 500k is the total scene limit.

Each chunk gets one 2048px atlas, one opaque material slot, and one FBX mesh.
That yields nine base-pass building sections for nine visible chunks, comfortably
below 700 under that assumption. GPU draw calls also depend on shadows, passes,
HLODs, and the rest of the scene; no offline FBX script can guarantee a total
per-view draw-call limit. Validate with Unreal GPU profiling and representative
mobile cameras.

The bake captures diffuse color only: procedural facade brick by default, or an
optional tileable --facade-texture with --facade-tile-metres. Roofs use a flat
color. This is not recovered photographic facade detail from OSM. Smart-project
UVs and an eight-pixel bake margin provide atlas padding; inspect small islands
and mip bleed at final texture resolution. Shader roughness remains constant.
There are no normal/AO atlas maps, glass material sections, or baked lighting.
Atlas resolution must be sized against total building surface area/texel density;
one atlas for an unlimited city is not a useful mobile texture budget.

## UE5 World Partition integration

Output per cell: SM_City_X_Y.fbx, T_City_X_Y.png, plus a shared manifest.json.
FBX uses metre units, Z up, and -Y forward; textures are external relative files.
Import with scene/unit conversion enabled and verify a known metre control mesh
and compass orientation before batch placement. Unreal uses centimetres. Choose
a projected geospatial origin near the play area and subtract it from anchors
before converting to centimetres. Account for FBX coordinate-system conversion
when mapping easting/northing to Unreal axes; do not assume a raw GIS tuple is
already an Unreal transform.

FBX does not encode World Partition cells or create actors automatically. Place
each imported asset at its converted/rebased manifest anchor in a World Partition
map, enable spatial loading, assign the appropriate HLOD layer, and build HLODs.
Choose streaming cell sizes to match intended chunk visibility. Use conventional
LOD/static meshes on the standard mobile renderer; do not rely on Nanite.
Provide separate simple collision proxies, not per-poly collision on city meshes.
Configure Unreal mobile material/texture compression explicitly rather than
relying on imported Blender shaders.

## Tests

Run `test_pipeline.py` with the GIS Python and Blender path; it creates temporary
synthetic fixtures, executes actual baking/export, reimports FBX files, checks
geometry/UV/material/texture output, and verifies missing DEM coverage fails.
Passing those checks is not real-world dataset or Unreal/device validation.

```bash
.venv-gis/bin/python Tools/GIS/test_pipeline.py --blender blender
```

Verified in Blender 4.3.2: courtyard/concave triangulation, sloped-surface input,
three buildings across two chunks, a 40-to-31 triangle decimation under a 35
triangle cap, procedural and supplied-image atlas baking, FBX reimport preserving
metre dimensions/UVs/one material slot, missing-DEM rejection, and overwrite
protection. Real-world datasets and Unreal mobile rendering remain untested.
