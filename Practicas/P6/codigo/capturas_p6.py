#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
capturas_p6.py
Practica 06 - Texturizado

Capturas de la vista 3D de Blender para el reporte: encuadra cada grupo de
objetos desde un angulo de 3/4, deselecciona (sin contorno naranja) y guarda
solo el area de la vista 3D con screen.screenshot_area.

Se ejecuta desde el listener (orden_blender.py):
    exec(open(".../capturas_p6.py", encoding="utf-8").read(), globals())
    correr(guion_capturas(CAPTURAS))
"""

import math
import os
import sys

import bpy
from mathutils import Euler

IMAGENES = os.path.normpath(os.path.join(os.path.dirname(bpy.data.filepath), "..", "PLabCompu",
                                         "Practicas", "P6", "Imagenes"))

# (archivo relativo a Imagenes/, objetos a encuadrar, rotacion de la vista en grados, acercamiento)
CAPTURAS = [
    ("E1/03_blender_plano_hoja.png", ["PlanoHoja"], (55, 0, 30), 0.9),
    ("E2/02_blender_caja.png", ["CajaCarton"], (65, 0, 40), 1.0),
    ("E3/03_blender_barril.png", ["Barril"], (75, 0, 35), 1.0),
    ("E3/04_blender_barril_tapa.png", ["Barril"], (35, 0, 35), 1.0),
    ("E4/02_blender_planeta.png", ["Planeta"], (80, 0, 60), 1.0),
    ("E5/01_blender_escena_actividades.png", ["PlanoHoja", "CajaCarton", "Barril", "Planeta"], (62, 0, 20), 1.0),
    ("V/05_blender_reja.png", ["Reja"], (80, 0, 25), 1.0),
    ("V/06_blender_edificio_tinaco.png", ["Edificio", "Tinaco"], (70, 0, 35), 1.0),
    ("V/07_blender_balon.png", ["Balon"], (75, 0, 40), 0.9),
    ("V/08_blender_escena_completa.png", ["PlanoHoja", "CajaCarton", "Barril", "Planeta", "Reja", "Edificio", "Tinaco", "Balon"], (62, 0, 25), 1.0),
]


def _vista():
    for win in bpy.context.window_manager.windows:
        for area in win.screen.areas:
            if area.type == "VIEW_3D":
                region = next(r for r in area.regions if r.type == "WINDOW")
                return win, area, region
    raise RuntimeError("No hay vista 3D")


def guion_capturas(capturas):
    win, area, region = _vista()
    esp = area.spaces.active
    esp.shading.type = "MATERIAL"
    esp.overlay.show_extras = False
    for archivo, objetos, rot, zoom in capturas:
        for ob in bpy.context.view_layer.objects:
            ob.select_set(ob.name in objetos)
        esp.region_3d.view_rotation = Euler([math.radians(a) for a in rot]).to_quaternion()
        with bpy.context.temp_override(window=win, area=area, region=region):
            bpy.ops.view3d.view_selected()
        yield 0.6
        esp.region_3d.view_distance *= zoom
        for ob in bpy.context.view_layer.objects:
            ob.select_set(False)
        yield 0.6
        ruta = os.path.join(IMAGENES, archivo)
        os.makedirs(os.path.dirname(ruta), exist_ok=True)
        with bpy.context.temp_override(window=win, area=area, region=region):
            bpy.ops.screen.screenshot_area(filepath=ruta)
        print("[P6] Captura %s" % archivo)
        sys.stdout.flush()
    esp.overlay.show_extras = True
    print("[P6] Listo capturas")
    sys.stdout.flush()
