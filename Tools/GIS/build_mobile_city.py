#!/usr/bin/env python3
"""Blender 4.x batch building/atlas/FBX pipeline; see README.md beside this file."""
from __future__ import annotations

import argparse
import json
import math
from pathlib import Path
import subprocess
import sys


def arguments():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else sys.argv[1:]
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--footprints", type=Path, required=True)
    p.add_argument("--surface", type=Path, required=True, help="DEM or rasterized LiDAR GeoTIFF")
    p.add_argument("--target-crs", required=True, help="Projected metre CRS, e.g. EPSG:32618")
    p.add_argument("--footprint-crs", help="Override missing .prj; never assume WGS84")
    p.add_argument("--gis-python", type=Path, help="External Python with GIS requirements")
    p.add_argument("--output", type=Path, required=True)
    p.add_argument("--height-field", default="height")
    p.add_argument("--levels-field", default="levels")
    p.add_argument("--default-height", type=float, default=9.0)
    p.add_argument("--floor-height", type=float, default=3.0)
    p.add_argument("--height-scale", type=float, default=1.0, help="DEM vertical units to metres")
    p.add_argument("--simplify-metres", type=float, default=0.20)
    p.add_argument("--cell-metres", type=float, default=128.0)
    p.add_argument("--origin-z", type=float, default=0.0)
    p.add_argument("--view-triangles", type=int, default=499000)
    p.add_argument("--visible-chunks", type=int, default=9)
    p.add_argument("--max-chunk-triangles", type=int, default=55000)
    p.add_argument("--atlas-size", type=int, choices=(256, 512, 1024, 2048, 4096), default=2048)
    p.add_argument("--facade-texture", type=Path, help="Optional tileable source facade; otherwise procedural brick")
    p.add_argument("--facade-tile-metres", type=float, default=4.0)
    p.add_argument("--gis-worker", action="store_true", help=argparse.SUPPRESS)
    a = p.parse_args(argv)
    for name in ("default_height", "floor_height", "height_scale", "cell_metres", "facade_tile_metres"):
        if not math.isfinite(getattr(a, name)) or getattr(a, name) <= 0:
            p.error(f"{name} must be positive and finite")
    if not math.isfinite(a.origin_z) or not math.isfinite(a.simplify_metres) or a.simplify_metres < 0:
        p.error("invalid origin/simplification")
    if a.visible_chunks < 1 or a.visible_chunks >= 700 or not 1 <= a.view_triangles < 500000:
        p.error("visible chunks must be 1..699 and view triangles 1..499999")
    if a.max_chunk_triangles < 1 or a.view_triangles // a.visible_chunks < 1:
        p.error("triangle budget is too small")
    return a


def positive_height(value):
    """Accept numeric metres, '12 m', or '40 ft'; reject ambiguous OSM values."""
    text = str(value or "").strip().lower()
    if not text:
        return None
    multiplier = 1.0
    if text.endswith("ft"):
        multiplier, text = 0.3048, text[:-2]
    elif text.endswith("m"):
        text = text[:-1]
    result = float(text) * multiplier
    if not math.isfinite(result) or result <= 0:
        raise ValueError(f"Invalid building height: {value!r}")
    return result


def prepare_gis(a):
    # Compiled geospatial packages live in external Python, not Blender's Python.
    import shapefile
    import rasterio
    from pyproj import CRS, Transformer
    from shapely import constrained_delaunay_triangles, make_valid, transform
    from shapely.geometry import shape

    destination = CRS.from_user_input(a.target_crs)
    if not destination.is_projected or any(
        abs(axis.unit_conversion_factor - 1.0) > 1e-9 for axis in destination.axis_info[:2]
    ):
        raise ValueError("target-crs must use projected metre coordinates")
    prj = a.footprints.with_suffix(".prj")
    source = CRS.from_user_input(a.footprint_crs) if a.footprint_crs else CRS.from_wkt(prj.read_text())
    project = Transformer.from_crs(source, destination, always_xy=True).transform
    chunks = {}
    building_count = 0
    with shapefile.Reader(str(a.footprints)) as reader, rasterio.open(a.surface) as surface:
        if not surface.crs:
            raise ValueError("height surface has no CRS")
        to_surface = Transformer.from_crs(destination, surface.crs, always_xy=True)
        for index, feature in enumerate(reader.iterShapeRecords()):
            attributes = feature.record.as_dict()
            height = positive_height(attributes.get(a.height_field))
            if height is None:
                levels = positive_height(attributes.get(a.levels_field))
                height = levels * a.floor_height if levels else a.default_height
            geometry = make_valid(transform(shape(feature.shape.__geo_interface__), project, interleaved=False))
            polygons = [geometry] if geometry.geom_type == "Polygon" else list(geometry.geoms)
            for part, polygon in enumerate(polygons):
                if polygon.geom_type != "Polygon" or polygon.is_empty or polygon.area <= 0:
                    raise ValueError(f"Feature {index} has non-polygon/degenerate geometry")
                polygon = polygon.simplify(a.simplify_metres, preserve_topology=True)
                rings = [list(polygon.exterior.coords)[:-1]] + [
                    list(ring.coords)[:-1] for ring in polygon.interiors]
                vertices = [xy for ring in rings for xy in ring]
                sample_xy = [to_surface.transform(x, y) for x, y in vertices]
                ground = []
                for xy, sample in zip(sample_xy, surface.sample(sample_xy, indexes=1, masked=True)):
                    row, col = surface.index(*xy)
                    if not (0 <= row < surface.height and 0 <= col < surface.width) or sample.mask.any():
                        raise ValueError(f"Feature {index} falls outside valid DEM coverage")
                    z = float(sample[0]) * a.height_scale
                    if not math.isfinite(z):
                        raise ValueError(f"Feature {index}: nonfinite height sample")
                    ground.append(z)
                # A flat foundation at minimum sampled elevation avoids floating walls.
                base = min(ground)
                roof = max(ground) + height
                centroid = polygon.representative_point()
                cell = (math.floor(centroid.x / a.cell_metres), math.floor(centroid.y / a.cell_metres))
                anchor = (cell[0] * a.cell_metres, cell[1] * a.cell_metres, a.origin_z)
                entry = chunks.setdefault(cell, {"cell": list(cell), "anchor_metres": anchor, "buildings": []})
                local = lambda xy: [xy[0] - anchor[0], xy[1] - anchor[1]]
                triangles = constrained_delaunay_triangles(polygon)
                if not math.isclose(sum(t.area for t in triangles.geoms), polygon.area, rel_tol=1e-7, abs_tol=1e-6):
                    raise ValueError(f"Feature {index}: roof triangulation lost footprint area")
                entry["buildings"].append({
                    "id": f"{index}_{part}", "rings": [[local(xy) for xy in ring] for ring in rings],
                    "roof_triangles": [[local(xy) for xy in list(t.exterior.coords)[:3]] for t in triangles.geoms],
                    "base": base - anchor[2], "roof": roof - anchor[2],
                })
                building_count += 1
    if not building_count:
        raise ValueError("No building polygons found")
    a.output.mkdir(parents=True, exist_ok=True)
    (a.output / "prepared.json").write_text(json.dumps({
        "crs": destination.to_string(), "building_count": building_count,
        "chunks": [chunks[key] for key in sorted(chunks)],
    }))


def source_material(bpy, index, a):
    mat = bpy.data.materials.new(f"FacadeSource_{index}")
    mat.use_nodes = True
    nodes, links = mat.node_tree.nodes, mat.node_tree.links
    shader = nodes.get("Principled BSDF")
    shader.inputs["Roughness"].default_value = 0.85
    geometry = nodes.new("ShaderNodeNewGeometry")
    position = nodes.new("ShaderNodeSeparateXYZ")
    links.new(geometry.outputs["Position"], position.inputs[0])
    horizontal = nodes.new("ShaderNodeMath")
    horizontal.operation = "ADD"
    links.new(position.outputs["X"], horizontal.inputs[0])
    links.new(position.outputs["Y"], horizontal.inputs[1])
    vector = nodes.new("ShaderNodeCombineXYZ")
    links.new(horizontal.outputs[0], vector.inputs["X"])
    links.new(position.outputs["Z"], vector.inputs["Y"])
    brick = nodes.new("ShaderNodeTexBrick")
    links.new(vector.outputs[0], brick.inputs["Vector"])
    tint = 0.25 + (index % 5) * 0.08
    brick.inputs["Color1"].default_value = (tint, tint * 0.9, tint * 0.8, 1)
    brick.inputs["Color2"].default_value = (tint * 0.8, tint * 0.8, tint * 0.8, 1)
    brick.inputs["Mortar"].default_value = (0.06, 0.06, 0.07, 1)
    brick.inputs["Scale"].default_value = 0.8
    facade_color = brick.outputs["Color"]
    if a.facade_texture:
        texture = nodes.new("ShaderNodeTexImage")
        texture.image = bpy.data.images.load(str(a.facade_texture.resolve()), check_existing=True)
        scale = nodes.new("ShaderNodeVectorMath")
        scale.operation = "MULTIPLY"
        scale.inputs[1].default_value = (1.0 / a.facade_tile_metres,) * 3
        links.new(vector.outputs[0], scale.inputs[0])
        links.new(scale.outputs[0], texture.inputs["Vector"])
        facade_color = texture.outputs["Color"]
    normal = nodes.new("ShaderNodeSeparateXYZ")
    links.new(geometry.outputs["Normal"], normal.inputs[0])
    roof_mask = nodes.new("ShaderNodeMath")
    roof_mask.operation = "GREATER_THAN"
    roof_mask.inputs[1].default_value = 0.5
    links.new(normal.outputs["Z"], roof_mask.inputs[0])
    roof_mix = nodes.new("ShaderNodeMixRGB")
    links.new(roof_mask.outputs[0], roof_mix.inputs[0])
    links.new(facade_color, roof_mix.inputs[1])
    roof_mix.inputs[2].default_value = (0.12, 0.14, 0.15, 1)
    links.new(roof_mix.outputs[0], shader.inputs["Base Color"])
    return mat


def build_mesh(bpy, building, materials):
    vertices, faces, lookup = [], [], {}

    def vertex(x, y, z):
        key = (x, y, z)
        if key not in lookup:
            lookup[key] = len(vertices)
            vertices.append(key)
        return lookup[key]

    for ring in building["rings"]:
        area = sum(ring[i][0] * ring[(i + 1) % len(ring)][1]
                   - ring[(i + 1) % len(ring)][0] * ring[i][1] for i in range(len(ring)))
        # Exterior CCW, courtyard holes CW: outward-facing wall normals.
        is_exterior = ring is building["rings"][0]
        if (area > 0) != is_exterior:
            ring = list(reversed(ring))
        for i, xy in enumerate(ring):
            other = ring[(i + 1) % len(ring)]
            faces.append((vertex(*xy, building["base"]), vertex(*other, building["base"]),
                          vertex(*other, building["roof"]), vertex(*xy, building["roof"])))
    for triangle in building["roof_triangles"]:
        face = [vertex(*xy, building["roof"]) for xy in triangle]
        a, b, c = triangle
        if (b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0]) < 0:
            face.reverse()
        faces.append(tuple(face))
    mesh = bpy.data.meshes.new(f"Building_{building['id']}")
    mesh.from_pydata(vertices, [], faces)
    mesh.update()
    obj = bpy.data.objects.new(mesh.name, mesh)
    bpy.context.collection.objects.link(obj)
    mesh.materials.append(materials[int(building["id"].split("_")[0]) % len(materials)])
    return obj


def triangle_count(obj):
    obj.data.calc_loop_triangles()
    return len(obj.data.loop_triangles)


def export_blender(a):
    import bpy
    import addon_utils
    if bpy.app.version < (4, 0, 0) or bpy.app.version >= (5, 0, 0):
        raise RuntimeError("This pipeline targets Blender 4.x")
    if not a.gis_python or not a.gis_python.is_file():
        raise ValueError("--gis-python must point to a Python with GIS dependencies")
    a.output.mkdir(parents=True, exist_ok=True)
    if any(a.output.iterdir()):
        raise ValueError("Use a fresh output directory; existing exports will not be overwritten")
    if a.facade_texture and not a.facade_texture.is_file():
        raise ValueError("facade texture does not exist")
    child_args = sys.argv[sys.argv.index("--") + 1:]
    subprocess.run([str(a.gis_python), str(Path(__file__).resolve()), *child_args, "--gis-worker"], check=True)
    prepared = json.loads((a.output / "prepared.json").read_text())
    bpy.ops.wm.read_factory_settings(use_empty=True)
    addon_utils.enable("io_scene_fbx", default_set=False, persistent=False)
    scene = bpy.context.scene
    scene.unit_settings.system = "METRIC"
    scene.unit_settings.scale_length = 1.0
    scene.render.engine = "CYCLES"
    scene.cycles.device = "CPU"
    scene.cycles.samples = 1
    scene.render.bake.use_pass_direct = False
    scene.render.bake.use_pass_indirect = False
    scene.render.bake.use_pass_color = True
    scene.render.bake.margin = 8
    budget = min(a.max_chunk_triangles, a.view_triangles // a.visible_chunks)
    reports = []
    for chunk in prepared["chunks"]:
        bpy.ops.object.select_all(action="SELECT")
        bpy.ops.object.delete(use_global=False)
        # Remove per-chunk images/meshes/materials to bound batch memory.
        for collection in (bpy.data.meshes, bpy.data.materials, bpy.data.images):
            for datablock in list(collection):
                if datablock.users == 0:
                    collection.remove(datablock)
        materials = [source_material(bpy, i, a) for i in range(5)]
        objects = [build_mesh(bpy, b, materials) for b in chunk["buildings"]]
        for obj in objects:
            obj.select_set(True)
        bpy.context.view_layer.objects.active = objects[0]
        if len(objects) > 1:
            bpy.ops.object.join()
        obj = bpy.context.object
        cell_id = f"{chunk['cell'][0]}_{chunk['cell'][1]}"
        obj.name = f"SM_City_{cell_id}"
        before = triangle_count(obj)
        if before > budget:
            modifier = obj.modifiers.new("MobileBudget", "DECIMATE")
            modifier.ratio = max(0.001, budget / before * 0.90)
            bpy.ops.object.modifier_apply(modifier=modifier.name)
        tri = obj.modifiers.new("ExportTriangles", "TRIANGULATE")
        bpy.ops.object.modifier_apply(modifier=tri.name)
        after = triangle_count(obj)
        if after == 0 or after > budget:
            raise RuntimeError(f"{cell_id}: {after} triangles exceeds {budget}; simplify inputs or use smaller cells")
        bpy.ops.object.mode_set(mode="EDIT")
        bpy.ops.mesh.select_all(action="SELECT")
        bpy.ops.uv.smart_project(angle_limit=math.radians(66), island_margin=0.02)
        bpy.ops.object.mode_set(mode="OBJECT")
        image = bpy.data.images.new(f"T_City_{cell_id}", width=a.atlas_size, height=a.atlas_size)
        image.colorspace_settings.name = "sRGB"
        for mat in obj.data.materials:
            node = mat.node_tree.nodes.new("ShaderNodeTexImage")
            node.image = image
            mat.node_tree.nodes.active = node
        bpy.ops.object.bake(type="DIFFUSE")
        atlas_path = a.output / f"T_City_{cell_id}.png"
        image.filepath_raw = str(atlas_path.resolve())
        image.file_format = "PNG"
        image.save()
        atlas_material = bpy.data.materials.new(f"M_City_{cell_id}")
        atlas_material.use_nodes = True
        shader = atlas_material.node_tree.nodes.get("Principled BSDF")
        shader.inputs["Roughness"].default_value = 0.85
        texture = atlas_material.node_tree.nodes.new("ShaderNodeTexImage")
        texture.image = image
        atlas_material.node_tree.links.new(texture.outputs["Color"], shader.inputs["Base Color"])
        obj.data.materials.clear()
        obj.data.materials.append(atlas_material)
        for polygon in obj.data.polygons:
            polygon.material_index = 0
        bounds = [[min(v.co[i] for v in obj.data.vertices), max(v.co[i] for v in obj.data.vertices)] for i in range(3)]
        fbx_path = a.output / f"{obj.name}.fbx"
        bpy.ops.export_scene.fbx(filepath=str(fbx_path.resolve()), use_selection=True,
            object_types={"MESH"}, global_scale=1.0, apply_unit_scale=True,
            axis_forward="-Y", axis_up="Z", use_mesh_modifiers=True,
            mesh_smooth_type="FACE", add_leaf_bones=False, bake_anim=False,
            path_mode="RELATIVE", embed_textures=False)
        reports.append({"cell": chunk["cell"], "anchor_metres": chunk["anchor_metres"],
            "bounds_local_metres": bounds, "fbx": fbx_path.name, "atlas": atlas_path.name,
            "buildings": len(chunk["buildings"]), "source_triangles": before,
            "triangles": after, "material_slots": 1})
        print(f"EXPORTED {cell_id}: {before} -> {after} triangles, 1 material", flush=True)
    manifest = {"crs": prepared["crs"], "units": "metres", "fbx_axes": {"forward": "-Y", "up": "Z"},
        "chunk_budget": budget, "assumed_visible_chunks": a.visible_chunks,
        "building_triangle_bound_under_assumption": budget * a.visible_chunks,
        "base_pass_sections_under_assumption": a.visible_chunks,
        "world_partition_assets_created": False, "chunks": reports}
    (a.output / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")


if __name__ == "__main__":
    try:
        options = arguments()
        prepare_gis(options) if options.gis_worker else export_blender(options)
    except Exception as error:
        print(f"CITY PIPELINE FAILED: {error}", file=sys.stderr, flush=True)
        raise
