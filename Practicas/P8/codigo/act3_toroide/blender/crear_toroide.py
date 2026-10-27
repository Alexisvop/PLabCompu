from pathlib import Path
import json
import math

import bpy
import bmesh
import numpy as np
from mathutils import Vector


ROOT = Path(__file__).resolve().parents[1]
BLEND = ROOT / "blender" / "toroide.blend"
MODEL = ROOT / "assets" / "models" / "toroide.fbx"
TEXTURE = ROOT / "assets" / "textures" / "tablero_uv.png"
SCENE_NAME = "P8_Act3_Revision"
COLLECTION_NAME = "P8_Act3_Toroide"


def enum_value(rna, property_name, requested):
    values = [item.identifier for item in rna.properties[property_name].enum_items]
    if requested not in values:
        raise RuntimeError(f"{property_name}: {requested!r} no disponible; {values}")
    return requested


def point_at(obj, position):
    obj.rotation_euler = (Vector(position) - obj.location).to_track_quat('-Z', 'Y').to_euler()


def main():
    if bpy.context.mode != 'OBJECT':
        raise RuntimeError("Cambia a modo Objeto antes de ejecutar; no se alterará el modo actual.")
    for path in (BLEND, MODEL, TEXTURE):
        if path.exists():
            raise FileExistsError(f"No se sobrescribirá {path}")
    if bpy.data.scenes.get(SCENE_NAME) or bpy.data.collections.get(COLLECTION_NAME):
        raise RuntimeError("La escena o colección de esta actividad ya existe; revisar antes de repetir.")
    if bpy.context.window is None:
        raise RuntimeError("Se requiere una ventana de Blender para revisar el resultado.")

    original_scene = bpy.context.scene
    original_objects = tuple((o.name, tuple(o.matrix_world)) for o in original_scene.objects)
    for path in (BLEND, MODEL, TEXTURE):
        path.parent.mkdir(parents=True, exist_ok=True)

    scene = bpy.data.scenes.new(SCENE_NAME)
    scene.unit_settings.scale_length = 1.0
    bpy.context.window.scene = scene
    collection = bpy.data.collections.new(COLLECTION_NAME)
    scene.collection.children.link(collection)
    preview = bpy.data.collections.new("P8_Act3_VistaPrevia")
    scene.collection.children.link(preview)

    bpy.ops.mesh.primitive_torus_add(
        major_segments=64, minor_segments=32,
        major_radius=1.0, minor_radius=0.35,
        generate_uvs=True, location=(0, 0, 0),
    )
    torus = bpy.context.object
    torus.name = "P8_Toroide"
    torus.data.name = "P8_Toroide_Malla"
    for owner in list(torus.users_collection):
        owner.objects.unlink(torus)
    collection.objects.link(torus)
    mesh = torus.data
    bm = bmesh.new()
    bm.from_mesh(mesh)
    bmesh.ops.triangulate(bm, faces=list(bm.faces))
    bmesh.ops.recalc_face_normals(bm, faces=list(bm.faces))
    bm.to_mesh(mesh)
    bm.free()
    for polygon in mesh.polygons:
        polygon.use_smooth = True
    mesh.update()
    torus["major_radius"] = 1.0
    torus["minor_radius"] = 0.35
    torus["major_segments"] = 64
    torus["minor_segments"] = 32

    # Una imagen real, no un nodo procedural: también puede cargarla stb_image.
    size = 512
    yy, xx = np.indices((size, size))
    checker = ((xx // (size // 16) + yy // (size // 8)) % 2).astype(bool)
    pixels = np.ones((size, size, 4), dtype=np.float32)
    pixels[:, :, :3] = np.where(
        checker[:, :, None],
        np.array([0.055, 0.13, 0.21], dtype=np.float32),
        np.array([0.82, 0.88, 0.92], dtype=np.float32),
    )
    image = bpy.data.images.new("P8_TableroUV_16x8", width=size, height=size, alpha=False)
    image.colorspace_settings.name = enum_value(
        image.colorspace_settings.bl_rna, "name", "sRGB")
    image.pixels.foreach_set(pixels.ravel())
    image.file_format = enum_value(
        bpy.types.ImageFormatSettings.bl_rna, "file_format", "PNG")
    image.filepath_raw = str(TEXTURE)
    image.save()

    material = bpy.data.materials.new("P8_TableroUV")
    material.use_nodes = True
    nodes = material.node_tree.nodes
    principled = next(node for node in nodes if node.type == 'BSDF_PRINCIPLED')
    principled.inputs['Roughness'].default_value = 0.5
    texture = nodes.new('ShaderNodeTexImage')
    texture.image = image
    texture.label = "PNG exportable: tablero UV 16 x 8"
    texture.location = (-350, 220)
    material.node_tree.links.new(texture.outputs['Color'], principled.inputs['Base Color'])
    mesh.materials.append(material)

    # Cámara y luces propias: no se exportan en el FBX.
    camera_data = bpy.data.cameras.new("P8_CamaraRevision")
    camera = bpy.data.objects.new("P8_CamaraRevision", camera_data)
    preview.objects.link(camera)
    camera.location = (3.1, -4.0, 3.4)
    camera_data.lens = 52
    point_at(camera, (0, 0, 0))
    scene.camera = camera
    for name, location, energy, diameter in (
        ("P8_LuzPrincipal", (1.0, -3.0, 4.5), 450, 4.0),
        ("P8_LuzRelleno", (-3.0, 1.0, 2.5), 180, 3.0),
    ):
        light_type = enum_value(bpy.types.Light.bl_rna, "type", "AREA")
        data = bpy.data.lights.new(name, type=light_type)
        data.energy = energy
        data.shape = enum_value(data.bl_rna, "shape", "DISK")
        data.size = diameter
        obj = bpy.data.objects.new(name, data)
        preview.objects.link(obj)
        obj.location = location
        point_at(obj, (0, 0, 0))

    world = bpy.data.worlds.new("P8_FondoRevision")
    world.use_nodes = True
    background = next(node for node in world.node_tree.nodes if node.type == 'BACKGROUND')
    background.inputs['Color'].default_value = (0.055, 0.07, 0.095, 1)
    background.inputs['Strength'].default_value = 0.4
    scene.world = world
    scene.render.resolution_x = 960
    scene.render.resolution_y = 720
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = enum_value(
        scene.render.image_settings.bl_rna, "file_format", "PNG")
    scene.render.image_settings.color_mode = enum_value(
        scene.render.image_settings.bl_rna, "color_mode", "RGB")

    uv = mesh.uv_layers.active
    assert len(mesh.vertices) == 64 * 32
    assert len(mesh.polygons) == 64 * 32 * 2
    assert uv is not None and len(uv.data) == len(mesh.loops)
    degenerate_uvs = 0
    for face in mesh.polygons:
        a, b, c = [uv.data[index].uv for index in face.loop_indices]
        double_area = abs((b.x-a.x)*(c.y-a.y) - (b.y-a.y)*(c.x-a.x))
        degenerate_uvs += double_area < 1e-10
    assert degenerate_uvs == 0
    uv_coordinates = [item.uv for item in uv.data]
    assert all(math.isfinite(value) for item in uv_coordinates for value in item)
    normal_alignment = []
    for vertex in mesh.vertices:
        x, y, z = vertex.co
        distance = math.hypot(x, y)
        center = Vector((x / distance, y / distance, 0))
        expected_normal = (vertex.co - center).normalized()
        normal_alignment.append(vertex.normal.dot(expected_normal))
    assert min(normal_alignment) > 0.99
    validation_mesh = bmesh.new()
    validation_mesh.from_mesh(mesh)
    assert all(edge.is_manifold for edge in validation_mesh.edges)
    validation_mesh.free()

    exporter = bpy.ops.export_scene.fbx.get_rna_type()
    bpy.ops.export_scene.fbx(
        filepath=str(MODEL), use_selection=True,
        object_types={enum_value(exporter, "object_types", "MESH")},
        use_mesh_modifiers=True, use_triangles=True,
        mesh_smooth_type=enum_value(exporter, "mesh_smooth_type", "OFF"),
        axis_forward=enum_value(exporter, "axis_forward", "-Z"),
        axis_up=enum_value(exporter, "axis_up", "Y"),
        bake_space_transform=True,
        path_mode=enum_value(exporter, "path_mode", "RELATIVE"),
        embed_textures=False, bake_anim=False,
    )
    image.pack()
    image.filepath = bpy.path.relpath(str(TEXTURE), start=str(BLEND.parent))

    for screen in bpy.data.screens:
        for area in screen.areas:
            if area.type == 'VIEW_3D':
                space = area.spaces.active
                space.shading.type = enum_value(space.shading.bl_rna, "type", "MATERIAL")
                space.overlay.show_overlays = False
                space.region_3d.view_rotation = camera.rotation_euler.to_quaternion()
                space.region_3d.view_location = (0, 0, 0)
                space.region_3d.view_distance = 5.0

    assert tuple((o.name, tuple(o.matrix_world)) for o in original_scene.objects) == original_objects
    bpy.ops.wm.save_as_mainfile(filepath=str(BLEND), check_existing=True, relative_remap=False)
    print(json.dumps({
        "scene": scene.name,
        "object": torus.name,
        "vertices": len(mesh.vertices),
        "triangles": len(mesh.polygons),
        "uv_layer": uv.name,
        "uv_degenerate_triangles": degenerate_uvs,
        "uv_min": [min(v[i] for v in uv_coordinates) for i in range(2)],
        "uv_max": [max(v[i] for v in uv_coordinates) for i in range(2)],
        "minimum_normal_alignment": min(normal_alignment),
        "dimensions_blender": list(torus.dimensions),
        "original_scene_preserved": original_scene.name,
        "files": [str(p) for p in (BLEND, MODEL, TEXTURE)],
    }, indent=2))


if __name__ == '__main__':
    main()
