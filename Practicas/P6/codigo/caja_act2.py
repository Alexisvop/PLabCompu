#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
caja_act2.py
Practica 06 - Texturizado
Laboratorio de Computacion Grafica e Interaccion Humano Computadora, FI-UNAM

Actividad 5.2: mapear un cubo para representar una caja de carton.
Trabaja sobre el .blend de la Actividad 5.1 (el plano con la hoja se conserva).

    1. Atlas cuadrado de 2048 x 2048 (4 x 4 celdas de 512): cuatro lados de
       carton, tapa y fondo con la pestana y la cinta canela; la cinta baja un
       poco por los lados +X y -X, donde termina la pestana.
    2. Cubo de 2 u apoyado en el piso junto a la hoja.
    3. UV por cara: cada cara se proyecta sobre su celda del atlas, con el
       "arriba" de los lados hacia +Z.
    4. Material con el atlas; se exporta la escena completa (hoja + caja).

Se ejecuta desde el listener de plano_act1.py (orden_blender.py):
    exec(open(".../caja_act2.py", encoding="utf-8").read(), globals())
    correr(guion())
o sin interfaz:
    blender -b P6_Act1_plano.blend -P caja_act2.py -- --directo
"""

import os
import shutil
import sys
import traceback

import bmesh
import bpy
import numpy as np

_dir = os.path.dirname(os.path.abspath(globals().get("__file__", "")))
AQUI = _dir if os.path.exists(os.path.join(_dir, "caja_act2.py")) \
    else os.path.join(os.path.dirname(bpy.data.filepath), "..", "PLabCompu", "Practicas", "P6", "codigo")
AQUI = os.path.normpath(AQUI)
BLENDER_DIR = os.path.normpath(os.path.join(AQUI, "..", "Blender"))
ASSETS = os.path.normpath(os.path.join(AQUI, "..", "..", "..", "..", "P6_assets"))
BIN_MODELS = os.path.normpath(os.path.join(AQUI, "..", "..", "..", "..",
                                           "stv-labopengl-main", "bin", "models"))

CARTON = os.path.join(ASSETS, "carton_foto.jpg")
ATLAS = os.path.join(BLENDER_DIR, "caja_atlas.png")
FBX_ESCENA = os.path.join(BLENDER_DIR, "escena_p6.fbx")

CELDA = 512
GRID = 4                                  # cuadricula de 4 x 4 celdas
LADO_ATLAS = GRID * CELDA                 # 2048: potencia de 2
PAD = 4                                   # pixeles de margen dentro de cada celda (evita sangrado)
POS_CAJA = (3.0, 0.0, 1.0)                # cubo de 2 u apoyado en z = 0, a la derecha de la hoja
PAUSA = 2.0

# Celda (columna, fila desde abajo) de cada cara y su proyeccion (u, v) a partir de (x, y, z) locales
CARAS = {
    (0, -1, 0): ((0, 0), lambda x, y, z: (x, z)),     # frente
    (1, 0, 0):  ((1, 0), lambda x, y, z: (y, z)),     # derecha
    (0, 1, 0):  ((2, 0), lambda x, y, z: (-x, z)),    # atras
    (-1, 0, 0): ((0, 1), lambda x, y, z: (-y, z)),    # izquierda
    (0, 0, 1):  ((1, 1), lambda x, y, z: (x, y)),     # tapa
    (0, 0, -1): ((2, 1), lambda x, y, z: (x, -y)),    # fondo
}

CINTA = np.array([0.66, 0.45, 0.22], np.float32)
ANCHO_CINTA = 0.20                        # fraccion de la celda
BAJADA_CINTA = 0.28                       # cuanto baja la cinta por los lados +X / -X


def log(msg):
    print("[P6] " + msg)
    sys.stdout.flush()


def _carton_base():
    """Recorta la foto de carton a cuadrado y la reduce a 512 x 512 (RGB en arreglo numpy)."""
    img = bpy.data.images.load(CARTON, check_existing=False)
    w, h = img.size
    lado = min(w, h)
    px = np.array(img.pixels[:], np.float32).reshape(h, w, 4)
    px = px[(h - lado) // 2:(h - lado) // 2 + lado, (w - lado) // 2:(w - lado) // 2 + lado]
    bpy.data.images.remove(img)
    tmp = bpy.data.images.new("_carton_tmp", lado, lado)
    tmp.pixels.foreach_set(px.ravel())
    tmp.scale(CELDA, CELDA)
    base = np.array(tmp.pixels[:], np.float32).reshape(CELDA, CELDA, 4)[..., :3].copy()
    bpy.data.images.remove(tmp)
    return base


def _celda(base, giro, tapa, cinta_arriba=False, cinta_abajo=False):
    """Una cara de la caja: carton con orillas oscurecidas, pestana y cinta si aplica."""
    c = np.rot90(base, giro).copy()
    v, u = np.meshgrid(np.linspace(0, 1, CELDA), np.linspace(0, 1, CELDA), indexing="ij")
    orilla = np.minimum(np.minimum(u, 1 - u), np.minimum(v, 1 - v))
    c *= (1.0 - 0.25 * np.clip(1 - orilla / 0.04, 0, 1))[..., None]
    if tapa:
        c[np.abs(v - 0.5) < 0.004] *= 0.55                     # union de las pestanas
        franja = np.abs(v - 0.5) < ANCHO_CINTA / 2
        c[franja] = 0.2 * c[franja] + 0.8 * CINTA
        brillo = np.abs(v - 0.5 - ANCHO_CINTA / 6) < 0.01       # reflejo de la cinta
        c[brillo] = np.minimum(c[brillo] * 1.15, 1)
    franja_lado = np.abs(u - 0.5) < ANCHO_CINTA / 2
    for activa, zona in ((cinta_arriba, v > 1 - BAJADA_CINTA), (cinta_abajo, v < BAJADA_CINTA)):
        if activa:
            m = franja_lado & zona
            c[m] = 0.2 * c[m] + 0.8 * CINTA
    return c


def atlas_caja():
    base = _carton_base()
    atlas = np.ones((LADO_ATLAS, LADO_ATLAS, 4), np.float32)
    atlas[..., :3] = np.tile(base, (GRID, GRID, 1))                   # celdas libres: carton liso
    for normal, ((col, fila), _) in CARAS.items():
        lado_x = normal[0] != 0
        celda = _celda(base, giro=(col + fila) % 4, tapa=normal[2] != 0,
                       cinta_arriba=lado_x, cinta_abajo=lado_x)
        atlas[fila * CELDA:(fila + 1) * CELDA, col * CELDA:(col + 1) * CELDA, :3] = celda
    vieja = bpy.data.images.get("caja_atlas")
    if vieja:
        bpy.data.images.remove(vieja)
    img = bpy.data.images.new("caja_atlas", LADO_ATLAS, LADO_ATLAS)
    img.pixels.foreach_set(atlas.ravel())
    img.filepath_raw = ATLAS
    img.file_format = "PNG"
    img.save()
    log("Atlas de la caja %dx%d guardado en %s" % (LADO_ATLAS, LADO_ATLAS, ATLAS))
    return img


def nuevo_objeto(operador, **kw):
    """Ejecuta un operador de bpy.ops.mesh.primitive_* y regresa el objeto que creo.
    (Dentro de un timer bpy.context.active_object puede seguir apuntando al anterior.)"""
    antes = set(bpy.data.objects)
    operador(**kw)
    return (set(bpy.data.objects) - antes).pop()


def cubo():
    vieja = bpy.data.objects.get("CajaCarton")
    if vieja:
        bpy.data.objects.remove(vieja, do_unlink=True)
    caja = nuevo_objeto(bpy.ops.mesh.primitive_cube_add, size=2.0, location=POS_CAJA)
    caja.name = "CajaCarton"
    log("Cubo CajaCarton creado en %s" % (POS_CAJA,))
    return caja


def uv_por_cara(caja):
    me = caja.data
    bm = bmesh.new()
    bm.from_mesh(me)
    uv = bm.loops.layers.uv.verify()
    margen = PAD / CELDA
    for f in bm.faces:
        n = tuple(int(round(c)) for c in f.normal)
        (col, fila), proy = CARAS[n]
        for loop in f.loops:
            a, b = proy(*loop.vert.co)                          # en [-1, 1]
            a = margen + (a + 1) / 2 * (1 - 2 * margen)
            b = margen + (b + 1) / 2 * (1 - 2 * margen)
            loop[uv].uv = ((col + a) / GRID, (fila + b) / GRID)
    bm.to_mesh(me)
    bm.free()
    me.update()
    log("UV asignado: cada cara en su celda del atlas")


def material_caja(caja, img):
    mat = bpy.data.materials.get("MatCaja") or bpy.data.materials.new("MatCaja")
    mat.use_nodes = True
    nodos = mat.node_tree.nodes
    bsdf = next(n for n in nodos if n.type == "BSDF_PRINCIPLED")
    tex = next((n for n in nodos if n.type == "TEX_IMAGE"), None) or nodos.new("ShaderNodeTexImage")
    tex.image = img
    tex.location = (-400, 200)
    mat.node_tree.links.new(tex.outputs["Color"], bsdf.inputs["Base Color"])
    bsdf.inputs["Roughness"].default_value = 0.9
    caja.data.materials.clear()
    caja.data.materials.append(mat)
    log("Material MatCaja con el atlas aplicado")


def encuadrar():
    if bpy.app.background:
        return
    for win in bpy.context.window_manager.windows:
        for area in win.screen.areas:
            if area.type != "VIEW_3D":
                continue
            area.spaces.active.shading.type = "MATERIAL"
            region = next(r for r in area.regions if r.type == "WINDOW")
            with bpy.context.temp_override(window=win, area=area, region=region):
                bpy.ops.object.select_all(action="SELECT")
                bpy.ops.view3d.view_selected()


def exportar_escena():
    """Exporta copias con la transformacion aplicada a los vertices: el Model de OpenGL
    ignora la matriz de cada nodo del FBX y dibujaria todo en el origen."""
    originales = [ob for ob in bpy.context.scene.objects if ob.type == "MESH"]
    copias = []
    for ob in originales:
        me = ob.data.copy()
        me.transform(ob.matrix_world)
        cp = bpy.data.objects.new(ob.name + "_export", me)
        bpy.context.scene.collection.objects.link(cp)
        copias.append(cp)
    for ob in bpy.context.view_layer.objects:
        ob.select_set(ob in copias)
    try:
        bpy.ops.export_scene.fbx(filepath=FBX_ESCENA, use_selection=True, path_mode="RELATIVE",
                                 object_types={"MESH"})
    finally:
        for cp in copias:
            me = cp.data
            bpy.data.objects.remove(cp, do_unlink=True)
            bpy.data.meshes.remove(me)
    for f in (FBX_ESCENA, ATLAS):
        shutil.copy2(f, BIN_MODELS)
    log("Escena (%s) exportada a %s y copiada con el atlas a %s" % (", ".join(o.name for o in originales), FBX_ESCENA, BIN_MODELS))


def guion():
    img = atlas_caja()
    yield PAUSA
    caja = cubo()
    yield PAUSA
    uv_por_cara(caja)
    material_caja(caja, img)
    encuadrar()
    yield PAUSA
    bpy.ops.wm.save_mainfile()
    log("Guardado %s" % bpy.data.filepath)
    exportar_escena()
    log("Listo caja")


def correr(gen):
    """Avanza el guion con bpy.app.timers para que se vea cada paso en la interfaz."""
    def _tick():
        try:
            return next(gen)
        except StopIteration:
            return None
        except Exception:
            traceback.print_exc(file=sys.stdout)
            sys.stdout.flush()
            return None
    bpy.app.timers.register(_tick, first_interval=0.5)


if "--directo" in sys.argv:
    for _ in guion():
        pass
