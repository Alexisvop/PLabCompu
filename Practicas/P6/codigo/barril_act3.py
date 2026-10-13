#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
barril_act3.py
Practica 06 - Texturizado
Laboratorio de Computacion Grafica e Interaccion Humano Computadora, FI-UNAM

Actividad 5.3: mapear un cilindro para representar un barril.
Trabaja sobre el .blend de las actividades anteriores (hoja, caja y planeta se conservan).

    1. Atlas de 2048 x 2048 que combina tres imagenes (ambientCG, CC0):
         - madera_planks005.png: duelas del costado (girada 90 grados, tono roble)
         - metal_metal021.png:   aros de acero y remaches, y el borde de las tapas
         - madera_planks012.png: tablas de las tapas
       Mitad superior = costado; mitad inferior = tapa (izquierda) y fondo (derecha).
    2. Cilindro de radio 1 y alto 2.4 apoyado en el piso.
    3. UV del costado por angulo alrededor del eje (con la costura resuelta) y
       UV de las tapas por proyeccion en planta dentro de su circulo.
    4. Material con el atlas; se reexporta la escena completa.

Se ejecuta desde el listener de plano_act1.py (orden_blender.py):
    exec(open(".../barril_act3.py", encoding="utf-8").read(), globals())
    correr(guion())
o sin interfaz:
    blender -b P6_Act1_plano.blend -P barril_act3.py -- --directo
"""

import math
import os
import shutil
import sys

import bmesh
import bpy
import numpy as np

_AQUI = os.path.dirname(os.path.abspath(__file__))
# Funciones comunes (nuevo_objeto, exportar_escena, encuadrar, correr, log) de la Actividad 5.2
with open(os.path.join(_AQUI, "caja_act2.py"), encoding="utf-8") as _fh:
    exec(_fh.read().split('\nif "--directo" in sys.argv')[0], globals())

MADERA_DUELAS = os.path.join(ASSETS, "madera_planks005.png")
MADERA_TAPAS = os.path.join(ASSETS, "madera_planks012.png")
METAL = os.path.join(ASSETS, "metal_metal021.png")
ATLAS_BARRIL = os.path.join(BLENDER_DIR, "barril_atlas.png")

T = 2048                                  # atlas de 2048 x 2048 (potencia de 2)
M = T // 2                                # costado: T x M arriba; tapas: dos celdas de M x M abajo
RADIO, ALTO, LADOS = 1.0, 2.4, 32
POS_BARRIL = (6.0, 0.0, ALTO / 2)         # apoyado en z = 0, a la derecha de la caja
AROS = (0.10, 0.31, 0.69, 0.90)           # centro de cada aro, fraccion de la altura
MEDIO_ARO = 0.035                         # medio alto del aro, fraccion de la altura
REMACHES = 16                             # remaches por aro
BORDE_TAPA = 0.90                         # radio (fraccion) donde empieza el aro de la tapa
ROBLE = np.array([0.78, 0.52, 0.30], np.float32)


def _cargar(ruta, lado):
    """Imagen como arreglo RGB de lado x lado (fila 0 = abajo)."""
    img = bpy.data.images.load(ruta, check_existing=False)
    img.scale(lado, lado)
    px = np.array(img.pixels[:], np.float32).reshape(lado, lado, 4)[..., :3].copy()
    bpy.data.images.remove(img)
    return px


def _costado(duelas, metal):
    """Franja del costado (M filas x T columnas): duelas verticales con aros y remaches."""
    tira = np.rot90(duelas)                                  # tablas horizontales -> duelas verticales
    c = np.concatenate([tira, tira], axis=1)                 # M x T, repetible en u
    lum = c.mean(axis=2, keepdims=True)
    c = 0.45 * c + 0.55 * lum * ROBLE / ROBLE.mean()         # tono roble
    acero = np.tile(metal, (1, T // metal.shape[1], 1))      # M x T

    v = (np.arange(M, dtype=np.float32) + 0.5) / M           # 0 abajo, 1 arriba
    u = (np.arange(T, dtype=np.float32) + 0.5) / T
    for centro in AROS:
        t = (v - centro) / MEDIO_ARO                          # -1..1 dentro del aro
        dentro = np.abs(t) <= 1
        sombra = (np.abs(t) > 1) & (np.abs(t) <= 1.35)       # sombra del aro sobre la madera
        c[sombra] *= 0.55
        relieve = (0.70 + 0.45 * np.cos(t[dentro] * math.pi / 2))[:, None, None]
        c[dentro] = acero[dentro] * relieve
        # remaches: circulos en el centro del aro, repartidos en u
        fila_c = centro * M
        for k in range(REMACHES):
            col_c = (k + 0.5) / REMACHES * T
            yy, xx = np.ogrid[:M, :T]
            d = np.hypot(yy - fila_c, xx - col_c)
            c[d < 9] = acero[d < 9] * 1.25
            c[(d >= 9) & (d < 12)] *= 0.45
    return np.clip(c, 0, 1)


def _tapa(tablas, metal):
    """Celda M x M de una tapa: tablas dentro del circulo y aro de acero en la orilla."""
    yy, xx = np.mgrid[:M, :M].astype(np.float32)
    r = np.hypot(xx - (M - 1) / 2, yy - (M - 1) / 2) / ((M - 1) / 2)
    c = tablas * 0.85
    aro = (r >= BORDE_TAPA) & (r <= 1.0)
    t = (r[aro] - (1 + BORDE_TAPA) / 2) / ((1 - BORDE_TAPA) / 2)
    c[aro] = metal[aro] * (0.70 + 0.40 * np.cos(t * math.pi / 2))[:, None]
    c[(r >= BORDE_TAPA - 0.02) & (r < BORDE_TAPA)] *= 0.5    # junta madera-aro
    c[r > 1.0] = 0.08
    return np.clip(c, 0, 1)


def atlas_barril():
    duelas = _cargar(MADERA_DUELAS, M)
    tablas = _cargar(MADERA_TAPAS, M)
    metal = _cargar(METAL, M)
    atlas = np.ones((T, T, 4), np.float32)
    atlas[M:, :, :3] = _costado(duelas, metal)
    atlas[:M, :M, :3] = _tapa(tablas, metal)
    atlas[:M, M:, :3] = _tapa(np.rot90(tablas, 1), metal)
    vieja = bpy.data.images.get("barril_atlas")
    if vieja:
        bpy.data.images.remove(vieja)
    img = bpy.data.images.new("barril_atlas", T, T)
    img.pixels.foreach_set(atlas.ravel())
    img.filepath_raw = ATLAS_BARRIL
    img.file_format = "PNG"
    img.save()
    log("Atlas del barril %dx%d (duelas + aros de metal + tapas) en %s" % (T, T, ATLAS_BARRIL))
    return img


def cilindro():
    viejo = bpy.data.objects.get("Barril")
    if viejo:
        bpy.data.objects.remove(viejo, do_unlink=True)
    barril = nuevo_objeto(bpy.ops.mesh.primitive_cylinder_add, vertices=LADOS, radius=RADIO,
                          depth=ALTO, location=POS_BARRIL)
    barril.name = "Barril"
    me = barril.data
    me.shade_smooth()
    for p in me.polygons:                     # tapas planas, costado suave
        if abs(p.normal.z) > 0.5:
            p.use_smooth = False
    log("Cilindro Barril (%d lados, r=%.1f, h=%.1f) en %s" % (LADOS, RADIO, ALTO, POS_BARRIL))
    return barril


def uv_barril(barril):
    me = barril.data
    bm = bmesh.new()
    bm.from_mesh(me)
    uv = bm.loops.layers.uv.verify()
    pad = PAD / T
    for f in bm.faces:
        if abs(f.normal.z) < 0.5:             # costado: u = angulo, v = altura (mitad superior)
            us = [(math.atan2(l.vert.co.y, l.vert.co.x) / (2 * math.pi)) % 1.0 for l in f.loops]
            if max(us) - min(us) > 0.5:       # cara que cruza la costura: continuar mas alla de 1
                us = [x + 1.0 if x < 0.5 else x for x in us]
            for l, x in zip(f.loops, us):
                h = (l.vert.co.z + ALTO / 2) / ALTO
                l[uv].uv = (x, 0.5 + pad + h * (0.5 - 2 * pad))
        else:                                 # tapas: planta dentro de su circulo
            arriba = f.normal.z > 0
            cu = 0.25 if arriba else 0.75
            for l in f.loops:
                x, y = l.vert.co.x / RADIO, l.vert.co.y / RADIO
                if not arriba:
                    x = -x                    # el fondo se ve desde abajo
                l[uv].uv = (cu + x * (0.25 - pad), 0.25 + y * (0.25 - pad))
    bm.to_mesh(me)
    bm.free()
    me.update()
    log("UV del barril: costado por angulo y tapas en planta")


def material_barril(barril, img):
    mat = bpy.data.materials.get("MatBarril") or bpy.data.materials.new("MatBarril")
    mat.use_nodes = True
    nodos = mat.node_tree.nodes
    bsdf = next(n for n in nodos if n.type == "BSDF_PRINCIPLED")
    tex = next((n for n in nodos if n.type == "TEX_IMAGE"), None) or nodos.new("ShaderNodeTexImage")
    tex.image = img
    tex.location = (-400, 200)
    mat.node_tree.links.new(tex.outputs["Color"], bsdf.inputs["Base Color"])
    bsdf.inputs["Roughness"].default_value = 0.7
    barril.data.materials.clear()
    barril.data.materials.append(mat)
    log("Material MatBarril con el atlas aplicado")


def guion():
    img = atlas_barril()
    yield PAUSA
    barril = cilindro()
    yield PAUSA
    uv_barril(barril)
    material_barril(barril, img)
    encuadrar()
    yield PAUSA
    bpy.ops.wm.save_mainfile()
    log("Guardado %s" % bpy.data.filepath)
    exportar_escena()
    shutil.copy2(ATLAS_BARRIL, BIN_MODELS)
    log("Listo barril")


ANILLOS = 9                               # cortes horizontales para poder curvar el costado
PANZA = 1.18                              # escala del anillo central (ensanchamiento del barril)


def _vista3d():
    """Contexto de la vista 3D para operadores de transformacion lanzados desde un timer."""
    for win in bpy.context.window_manager.windows:
        for area in win.screen.areas:
            if area.type == "VIEW_3D":
                region = next(r for r in area.regions if r.type == "WINDOW")
                return dict(window=win, area=area, region=region)
    raise RuntimeError("No hay vista 3D abierta")


def guion_forma():
    """Forma de barril con edicion proporcional: anillos nuevos y escala del central."""
    barril = bpy.data.objects["Barril"]
    ctx = _vista3d()
    for ob in bpy.context.view_layer.objects:
        ob.select_set(ob == barril)
    bpy.context.view_layer.objects.active = barril
    with bpy.context.temp_override(**ctx):
        bpy.ops.object.mode_set(mode="EDIT")
        bpy.ops.mesh.select_mode(type="VERT")
    me = barril.data
    bm = bmesh.from_edit_mesh(me)
    verticales = [e for e in bm.edges if abs(e.verts[0].co.z - e.verts[1].co.z) > ALTO * 0.5]
    bmesh.ops.subdivide_edges(bm, edges=verticales, cuts=ANILLOS, use_grid_fill=False)
    bmesh.update_edit_mesh(me)
    log("Costado subdividido: %d anillos horizontales" % ANILLOS)
    yield PAUSA

    for v in bm.verts:
        v.select_set(abs(v.co.z) < 1e-4)                    # anillo central (z local = 0)
    bm.select_flush_mode()
    bmesh.update_edit_mesh(me)
    log("Anillo central seleccionado (%d vertices)" % sum(v.select for v in bm.verts))
    yield PAUSA

    ts = bpy.context.scene.tool_settings
    ts.use_proportional_edit = True
    ts.proportional_edit_falloff = "SMOOTH"
    with bpy.context.temp_override(**ctx):
        bpy.ops.transform.resize(value=(PANZA, PANZA, 1.0), constraint_axis=(True, True, False),
                                 use_proportional_edit=True, proportional_edit_falloff="SMOOTH",
                                 proportional_size=ALTO / 2)
    log("Edicion proporcional (Suave, radio %.1f): centro escalado a %.2f" % (ALTO / 2, PANZA))
    yield PAUSA

    with bpy.context.temp_override(**ctx):
        bpy.ops.object.mode_set(mode="OBJECT")
    ts.use_proportional_edit = False
    radios = sorted({round(math.hypot(v.co.x, v.co.y), 3) for v in me.vertices if abs(v.co.z) < 1e-4 or abs(abs(v.co.z) - ALTO / 2) < 1e-4})
    log("Radio en los bordes y al centro: %s" % radios)
    bpy.ops.wm.save_mainfile()
    log("Guardado %s" % bpy.data.filepath)
    exportar_escena()
    log("Listo forma barril")


if "--directo" in sys.argv:
    for _ in guion():
        pass
