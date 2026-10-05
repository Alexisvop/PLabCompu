#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
personaje_act3.py
Practica 05 - Adaptacion y carga de modelos
Laboratorio de Computacion Grafica e Interaccion Humano Computadora, FI-UNAM

Actividad 3 (variacion): adapta en Blender un segundo personaje de Mixamo
(Exo Gray con el clip Walking, In Place) para que lo cargue AnimatedModel.

AnimatedModel (animatedmodel.h) solo anima correctamente un modelo de UNA malla:
la lista de huesos se reinicia en cada aiMesh y todas comparten el mismo arreglo
gBones[100]. Assimp ademas separa una malla en varias si tiene varios
materiales. Por eso el guion:

    1. Importa el FBX de Mixamo (8 mallas, 6 materiales, 112 huesos).
    2. Elimina las mallas de relleno que no aportan forma (placeholder del
       cuerpo, mascara de la cabeza y la capa de brillo de los ojos).
    3. Une en un atlas de 4096 x 2048 las dos texturas de color que quedan y
       recorre las UV de cada parte a su mitad del atlas.
    4. Une las mallas en una sola (Ctrl+J) con un unico material.
    5. Exporta el FBX con la armadura y la animacion horneada.

Uso:
    blender -b -P personaje_act3.py -- <Walking.fbx> <salida.fbx>
"""

import os
import sys

import bpy
import numpy as np

args = sys.argv[sys.argv.index("--") + 1:]
ENTRADA, SALIDA = args[0], args[1]
ATLAS = os.path.splitext(SALIDA)[0] + "_diffuse.png"
RELLENO = ("EXO_Body", "EXO_HeadMask", "EXO_EyesSpec")
MAX_HUESOS = 100           # MAX_RIGGING_BONES de animatedmodel.h

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=ENTRADA)
arm = next(o for o in bpy.data.objects if o.type == 'ARMATURE')
print(f"Importado: {len(arm.data.bones)} huesos, "
      f"{sum(o.type == 'MESH' for o in bpy.data.objects)} mallas")

for nombre in RELLENO:
    o = bpy.data.objects.get(nombre)
    if o:
        bpy.data.objects.remove(o, do_unlink=True)
mallas = [o for o in bpy.data.objects if o.type == 'MESH']


def color_base(mat):
    """Imagen conectada al Base Color del Principled BSDF."""
    bsdf = next(n for n in mat.node_tree.nodes if n.type == 'BSDF_PRINCIPLED')
    nodo = bsdf.inputs["Base Color"].links[0].from_node
    return nodo.image


# Una mitad del atlas por cada textura de color distinta
texturas = []
for o in mallas:
    img = color_base(o.data.materials[0])
    if img not in texturas:
        texturas.append(img)
assert len(texturas) <= 2, [t.name for t in texturas]
lado = 2048
atlas = np.zeros((lado, lado * len(texturas), 4), dtype=np.float32)
for k, img in enumerate(texturas):
    if tuple(img.size) != (lado, lado):
        img.scale(lado, lado)
    px = np.array(img.pixels[:], dtype=np.float32).reshape(lado, lado, 4)
    atlas[:, k * lado:(k + 1) * lado] = px
    print(f"  atlas[{k}] <- {img.name} ({img.filepath})")
imagen = bpy.data.images.new("ExoGray_diffuse", lado * len(texturas), lado, alpha=True)
imagen.pixels = atlas.ravel()
imagen.filepath_raw = ATLAS
imagen.file_format = 'PNG'
imagen.save()

# Las UV de cada malla se comprimen a la mitad del atlas que le toca
for o in mallas:
    k = texturas.index(color_base(o.data.materials[0]))
    uv = o.data.uv_layers.active.data
    co = np.zeros(len(uv) * 2)
    uv.foreach_get("uv", co)
    co = co.reshape(-1, 2)
    co[:, 0] = (np.clip(co[:, 0], 0.0, 1.0) + k) / len(texturas)
    uv.foreach_set("uv", co.ravel())

# Un solo material con el atlas
mat = bpy.data.materials.new("ExoGray_MAT")
mat.use_nodes = True
bsdf = mat.node_tree.nodes["Principled BSDF"]
tex = mat.node_tree.nodes.new("ShaderNodeTexImage")
tex.image = imagen
mat.node_tree.links.new(tex.outputs["Color"], bsdf.inputs["Base Color"])
for o in mallas:
    o.data.materials.clear()
    o.data.materials.append(mat)

# Ctrl+J: todas las mallas en una
principal = max(mallas, key=lambda o: len(o.data.vertices))
with bpy.context.temp_override(active_object=principal, selected_editable_objects=mallas):
    bpy.ops.object.join()
principal.name = principal.data.name = "ExoGray"
usados = {principal.vertex_groups[g.group].name
          for v in principal.data.vertices for g in v.groups if g.weight > 0}
# Grupos sin pesos no deben llegar al FBX como huesos de la malla
for vg in list(principal.vertex_groups):
    if vg.name not in usados:
        principal.vertex_groups.remove(vg)
print(f"Malla unida: {len(principal.data.vertices)} vertices, {len(usados)} huesos con peso")
assert len(usados) <= MAX_HUESOS

# El exportador escribe un cluster por cada hueso de la armadura aunque no
# tenga pesos: con 112, los indices >= 100 caen fuera de gBones[100] y esos
# vertices se colapsan al origen. Se borran los huesos que no deforman nada
# (sus hijos, si los hay, pasan al padre).
bpy.context.view_layer.objects.active = arm
bpy.ops.object.mode_set(mode='EDIT')
for eb in list(arm.data.edit_bones):
    if eb.name not in usados:
        for hijo in eb.children:
            hijo.parent = eb.parent
        arm.data.edit_bones.remove(eb)
bpy.ops.object.mode_set(mode='OBJECT')
print(f"Armadura: {len(arm.data.bones)} huesos")
assert len(arm.data.bones) <= MAX_HUESOS

# Se exporta solo el ciclo del clip (Walking: 32 cuadros), no el rango de la escena
accion = arm.animation_data.action
escena = bpy.context.scene
escena.frame_start, escena.frame_end = (int(f) for f in accion.frame_range)

for o in bpy.context.view_layer.objects:
    o.select_set(o in (arm, principal))
bpy.ops.export_scene.fbx(filepath=SALIDA, use_selection=True,
                         object_types={'ARMATURE', 'MESH'},
                         path_mode='STRIP',        # solo el nombre del PNG, junto al FBX
                         add_leaf_bones=False, use_armature_deform_only=True,
                         bake_anim=True, bake_anim_use_all_actions=False,
                         bake_anim_use_nla_strips=False, bake_anim_simplify_factor=0.0)
print(f"Exportado: {SALIDA} ({os.path.getsize(SALIDA) / 1e6:.1f} MB), atlas {ATLAS}")
