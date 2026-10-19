#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
exportar_planeta.py
Practica 07 - Modelos de iluminacion (Objeto 2)
Laboratorio de Computacion Grafica e Interaccion Humano Computadora, FI-UNAM

Toma el planeta de la escena de la practica 6 (escena_p6.fbx), lo centra en el
origen y lo exporta solo, junto con su textura, a bin/models/IllumModels.

    blender -b --factory-startup -P exportar_planeta.py -- <ruta de bin>
"""

import os
import shutil
import sys

import bpy
from mathutils import Matrix, Vector

BIN = sys.argv[sys.argv.index("--") + 1]
ESCENA_P6 = os.path.join(BIN, "models", "escena_p6.fbx")
DESTINO = os.path.join(BIN, "models", "IllumModels")
TEXTURA = "planeta_tierra.jpg"

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=ESCENA_P6)

# Solo se conserva el planeta
planeta = bpy.data.objects["Planeta_export"]
for ob in list(bpy.context.scene.objects):
    if ob is not planeta:
        bpy.data.objects.remove(ob, do_unlink=True)

# En la escena de la practica 6 el planeta tiene centro en (-3, 0, 1); se lleva al origen
me = planeta.data
me.transform(planeta.matrix_world)
planeta.matrix_world = Matrix.Identity(4)
centro = sum((v.co for v in me.vertices), Vector()) / len(me.vertices)
me.transform(Matrix.Translation(-centro))
planeta.name = "Planeta"

# La textura se copia junto al FBX para que la ruta relativa sea solo el nombre del archivo
shutil.copy2(os.path.join(BIN, "models", TEXTURA), DESTINO)
for img in bpy.data.images:
    img.filepath = os.path.join(DESTINO, TEXTURA)

bpy.ops.export_scene.fbx(filepath=os.path.join(DESTINO, "planeta.fbx"),
                         path_mode="RELATIVE", object_types={"MESH"})
print("EXPORTADO", len(me.vertices), "vertices, centro original", tuple(round(c, 3) for c in centro))
