#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
planeta_act4.py
Practica 06 - Texturizado
Laboratorio de Computacion Grafica e Interaccion Humano Computadora, FI-UNAM

Actividad 5.4: mapear una esfera para representar un planeta.
Trabaja sobre el .blend de las Actividades 5.1 y 5.2 (hoja y caja se conservan).

    1. Textura de la Tierra de Solar System Scope (2k_earth_daymap.jpg,
       2048 x 1024, proyeccion equirectangular, CC BY 4.0).
    2. Esfera UV de 64 segmentos y 32 anillos: su UV ya es equirectangular
       (u = longitud, v = latitud), asi que el mapa se ajusta sin deformarse.
    3. Material con la textura; sombreado suave.
    4. Se reexporta la escena completa (hoja + caja + planeta).

Se ejecuta desde el listener de plano_act1.py (orden_blender.py):
    exec(open(".../planeta_act4.py", encoding="utf-8").read(), globals())
    correr(guion())
o sin interfaz:
    blender -b P6_Act1_plano.blend -P planeta_act4.py -- --directo
"""

import os
import shutil
import sys

import bpy

_AQUI = os.path.dirname(os.path.abspath(__file__))
# Funciones comunes (exportar_escena, encuadrar, correr, log) de la Actividad 5.2
with open(os.path.join(_AQUI, "caja_act2.py"), encoding="utf-8") as _fh:
    exec(_fh.read().split('\nif "--directo" in sys.argv')[0], globals())

TEXTURA_ORIGEN = os.path.join(ASSETS, "2k_earth_daymap.jpg")
TEXTURA_PLANETA = os.path.join(BLENDER_DIR, "planeta_tierra.jpg")
POS_PLANETA = (-3.0, 0.0, 1.0)            # radio 1, apoyado en z = 0, a la izquierda de la hoja


def planeta():
    viejo = bpy.data.objects.get("Planeta")
    if viejo:
        bpy.data.objects.remove(viejo, do_unlink=True)
    esfera = nuevo_objeto(bpy.ops.mesh.primitive_uv_sphere_add, segments=64, ring_count=32,
                          radius=1.0, location=POS_PLANETA)
    esfera.name = "Planeta"
    esfera.data.shade_smooth()                # sin operador: funciona dentro de un timer
    log("Esfera UV Planeta (64 x 32) creada en %s" % (POS_PLANETA,))
    return esfera


def material_planeta(esfera):
    shutil.copy2(TEXTURA_ORIGEN, TEXTURA_PLANETA)
    img = bpy.data.images.load(TEXTURA_PLANETA, check_existing=True)
    mat = bpy.data.materials.get("MatPlaneta") or bpy.data.materials.new("MatPlaneta")
    mat.use_nodes = True
    nodos = mat.node_tree.nodes
    bsdf = next(n for n in nodos if n.type == "BSDF_PRINCIPLED")
    tex = next((n for n in nodos if n.type == "TEX_IMAGE"), None) or nodos.new("ShaderNodeTexImage")
    tex.image = img
    tex.location = (-400, 200)
    mat.node_tree.links.new(tex.outputs["Color"], bsdf.inputs["Base Color"])
    bsdf.inputs["Roughness"].default_value = 0.6
    esfera.data.materials.clear()
    esfera.data.materials.append(mat)
    log("Material MatPlaneta con %s (%dx%d)" % (os.path.basename(TEXTURA_PLANETA), *img.size))


def guion():
    esfera = planeta()
    yield PAUSA
    material_planeta(esfera)
    encuadrar()
    yield PAUSA
    bpy.ops.wm.save_mainfile()
    log("Guardado %s" % bpy.data.filepath)
    exportar_escena()
    shutil.copy2(TEXTURA_PLANETA, BIN_MODELS)
    log("Listo planeta")


if "--directo" in sys.argv:
    for _ in guion():
        pass
