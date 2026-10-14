#!/usr/bin/env python3
"""Run with the external GIS Python: test_pipeline.py --blender /path/to/blender."""
import argparse
import json
import math
from pathlib import Path
import subprocess
import tempfile
import sys

import numpy as np
from pyproj import CRS
import rasterio
from rasterio.transform import from_origin
import shapefile


def run(command, expect_success=True):
    result = subprocess.run(command, text=True, capture_output=True)
    if (result.returncode == 0) != expect_success:
        raise AssertionError(result.stdout[-4000:] + result.stderr[-4000:])
    return result


def fixture(root):
    with shapefile.Writer(str(root / "buildings"), shapeType=shapefile.POLYGON) as writer:
        writer.field("height", "C")
        writer.field("levels", "N")
        writer.poly([[(10,10),(10,40),(40,40),(40,10),(10,10)],
                     [(20,20),(30,20),(30,30),(20,30),(20,20)]])
        writer.record("12 m", 0)
        writer.poly([[(50,10),(50,40),(60,40),(60,20),(80,20),(80,10),(50,10)]])
        writer.record("40 ft", 0)
        writer.poly([[(140,10),(140,30),(160,30),(160,10),(140,10)]])
        writer.record("", 3)
    (root / "buildings.prj").write_text(CRS.from_epsg(32618).to_wkt())
    for name, data in (("ground", 100 + np.indices((256,256))[1] * 0.01),
                       ("missing", np.full((256,256), -9999))):
        with rasterio.open(root / f"{name}.tif", "w", driver="GTiff",
            height=256, width=256, count=1, dtype="float32", crs="EPSG:32618",
            transform=from_origin(0,256,1,1), nodata=-9999) as raster:
            raster.write(data.astype("float32"), 1)


VERIFY_FBX = r'''
import bpy, addon_utils, json, sys, math
from pathlib import Path
root = Path(sys.argv[-1])
manifest = json.loads((root / "manifest.json").read_text())
addon_utils.enable("io_scene_fbx", default_set=False, persistent=False)
for chunk in manifest["chunks"]:
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=str(root / chunk["fbx"]))
    meshes = [obj for obj in bpy.context.scene.objects if obj.type == "MESH"]
    assert len(meshes) == 1
    mesh = meshes[0].data
    mesh.calc_loop_triangles()
    assert len(mesh.loop_triangles) == chunk["triangles"]
    assert len(mesh.materials) == 1 and len(mesh.uv_layers) >= 1
    for i, bound in enumerate(chunk["bounds_local_metres"]):
        assert math.isclose(meshes[0].dimensions[i], bound[1]-bound[0], rel_tol=1e-5, abs_tol=1e-4)
    image = bpy.data.images.load(str(root / chunk["atlas"]), check_existing=True)
    assert tuple(image.size) == (256,256)
    px = list(image.pixels)
    values = [max(px[i:i+3]) for i in range(0,len(px),4) if max(px[i:i+3]) > 0.001]
    assert values and max(values)-min(values) > 0.01, "Atlas is blank or uniform"
print("FBX round-trip, metre dimensions, UVs, material slots and baked pixels passed")
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--blender", default="blender")
    args = parser.parse_args()
    script = Path(__file__).with_name("build_mobile_city.py")
    with tempfile.TemporaryDirectory(prefix="jnk-gis-test-") as directory:
        root = Path(directory)
        fixture(root)
        common = [args.blender, "-b", "--factory-startup", "--python-exit-code", "1",
                  "--python", str(script), "--", "--footprints", str(root / "buildings.shp"),
                  "--target-crs", "EPSG:32618", "--gis-python", sys.executable,
                  "--atlas-size", "256", "--max-chunk-triangles", "35"]
        output = root / "export"
        run(common + ["--surface", str(root / "ground.tif"), "--output", str(output)])
        manifest = json.loads((output / "manifest.json").read_text())
        prepared = json.loads((output / "prepared.json").read_text())
        assert prepared["building_count"] == 3 and len(manifest["chunks"]) == 2
        assert all(0 < c["triangles"] <= 35 and c["material_slots"] == 1 for c in manifest["chunks"])
        assert any(c["triangles"] < c["source_triangles"] for c in manifest["chunks"])
        areas = []
        for building in prepared["chunks"][0]["buildings"]:
            area = 0.0
            for a,b,c in building["roof_triangles"]:
                area += abs((b[0]-a[0])*(c[1]-a[1])-(b[1]-a[1])*(c[0]-a[0])) * 0.5
            areas.append(area)
        assert math.isclose(sum(areas), 1300.0), "Courtyard/concave roof coverage is incorrect"
        result = run([args.blender, "-b", "--factory-startup", "--python-exit-code", "1",
                      "--python-expr", VERIFY_FBX, "--", str(output)])
        print(result.stdout.strip())
        # Exercise the photographic/tileable facade source branch using a generated tile.
        textured = root / "textured"
        run(common + ["--surface", str(root / "ground.tif"), "--output", str(textured),
                      "--facade-texture", str(output / manifest["chunks"][0]["atlas"])])
        run([args.blender, "-b", "--factory-startup", "--python-exit-code", "1",
             "--python-expr", VERIFY_FBX, "--", str(textured)])
        failed = root / "failed"
        result = run(common + ["--surface", str(root / "missing.tif"), "--output", str(failed)], False)
        assert "valid DEM coverage" in result.stdout + result.stderr
        assert not list(failed.glob("*.fbx"))
        result = run(common + ["--surface", str(root / "ground.tif"), "--output", str(output)], False)
        assert "existing exports will not be overwritten" in result.stdout + result.stderr
    print("PASS: GIS topology, extrusion, decimation cap, procedural/texture atlases, FBX round-trip, nodata failure, overwrite protection")


if __name__ == "__main__":
    main()
