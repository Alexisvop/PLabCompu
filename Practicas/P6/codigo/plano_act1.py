#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
plano_act1.py
Practica 06 - Texturizado
Laboratorio de Computacion Grafica e Interaccion Humano Computadora, FI-UNAM

Actividad 5.1: mapear una textura RGBA en un plano y exportarlo a FBX para OpenGL.

    1. Se carga la foto de la hoja (RGB, 719x717, sin canal alpha).
    2. Se recorta a 717x717 para que la textura sea cuadrada.
    3. Se le agrega canal alpha: una mascara eliptica con borde suave deja
       opaca la hoja y transparente el fondo.
    4. Se guarda como PNG RGBA (hoja_rgba.png).
    5. Plano con material: Image Texture -> Base Color y Alpha del BSDF.
    6. Export a FBX (plano_hoja.fbx) junto a la textura.

Corre dentro de la interfaz de Blender sobre bpy.app.timers; al terminar deja
Blender abierto ejecutando orden_blender.py (junto al .blend) si aparece.

Uso:
    blender -P plano_act1.py -- <salida.blend>
    blender -b -P plano_act1.py -- --directo <salida.blend>
"""

import os
import shutil
import sys
import traceback

import bpy
import numpy as np

args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
DIRECTO = "--directo" in args
args = [a for a in args if a != "--directo"]
SALIDA = args[-1] if args else None

PAUSA = 2.0

AQUI = os.path.dirname(os.path.abspath(__file__))
BLENDER = os.path.normpath(os.path.join(AQUI, "..", "Blender"))
ASSETS = os.path.normpath(os.path.join(AQUI, "..", "..", "..", "..", "P6_assets"))
BIN_MODELS = os.path.normpath(os.path.join(AQUI, "..", "..", "..", "..",
                                           "stv-labopengl-main", "bin", "models"))

ORIGINAL = os.path.join(ASSETS, "hoja_original.png")
TEXTURA = os.path.join(BLENDER, "hoja_rgba.png")
FBX = os.path.join(BLENDER, "plano_hoja.fbx")

LADO = 717                       # recorte cuadrado de 717 x 717 de la foto
TAM = 1024                       # textura final: potencia de 2 (1024 x 1024)
SEMILLA = (371, 360)             # pixel en el centro de la hoja (x, y desde arriba)
APERTURA = 3                     # pasos de erosion/dilatacion para separar la hoja del fondo
SUAVE = 2                        # pasadas de desenfoque 3x3 en la orilla del alpha
# Elipse que envuelve la hoja (pixeles, origen arriba a la izquierda) y su margen
CENTRO = (371.0, 364.0)
SEMIEJES = (223.0, 242.0)
LIMITE = 1.04


def log(msg):
    print("[P6] " + msg)
    sys.stdout.flush()


def limpiar_escena():
    for ob in list(bpy.data.objects):
        bpy.data.objects.remove(ob, do_unlink=True)


def _vecinos(m, op):
    """Combina cada pixel con sus 4 vecinos (op = np.logical_and erosiona, logical_or dilata)."""
    r = m.copy()
    for eje, paso in ((0, 1), (0, -1), (1, 1), (1, -1)):
        r = op(r, np.roll(m, paso, axis=eje))
    return r


def _inundar(semilla, permitido):
    """Region de 'permitido' conectada a 'semilla' (relleno por dilatacion repetida)."""
    region = semilla & permitido
    while True:
        nueva = _vecinos(region, np.logical_or) & permitido
        if nueva.sum() == region.sum():
            return region
        region = nueva


def mascara_hoja(px):
    """Alpha de la hoja: naranja saturado, conectado al centro y sin huecos internos."""
    R, G, B = px[..., 0], px[..., 1], px[..., 2]
    m = (R > 0.55) & (G < 0.62 * R) & (B < 0.30 * R)
    for _ in range(APERTURA):                       # apertura: corta puentes con el fondo
        m = _vecinos(m, np.logical_and)
    semilla = np.zeros_like(m)
    semilla[LADO - 1 - SEMILLA[1], SEMILLA[0]] = True
    m = _inundar(semilla, m)
    for _ in range(APERTURA):
        m = _vecinos(m, np.logical_or)
    # Limite: elipse un poco mayor que la hoja, corta las manchas del fondo pegadas a la orilla
    filas = np.arange(LADO, dtype=np.float32)[::-1]          # fila de imagen (0 = arriba)
    yy, xx = np.meshgrid(filas, np.arange(LADO, dtype=np.float32), indexing="ij")
    d = np.hypot((xx - CENTRO[0]) / SEMIEJES[0], (yy - CENTRO[1]) / SEMIEJES[1])
    m &= d < LIMITE
    # Cada fila se rellena de orilla a orilla: cubre la vena, los brillos y la punta
    for f in range(LADO):
        xs = np.flatnonzero(m[f])
        if xs.size:
            m[f, xs[0]:xs[-1] + 1] = True
    m &= ~((B > 0.6 * R) & (R > 0.6))                         # piel del pulgar
    a = m.astype(np.float32)
    for _ in range(SUAVE):                          # orilla suave
        a = sum(np.roll(np.roll(a, dy, 0), dx, 1) for dy in (-1, 0, 1) for dx in (-1, 0, 1)) / 9.0
    return a


def textura_rgba():
    """Recorta la foto a cuadrado y le agrega canal alpha con la mascara de la hoja."""
    src = bpy.data.images.load(ORIGINAL, check_existing=False)
    w, h = src.size
    px = np.array(src.pixels[:], dtype=np.float32).reshape(h, w, 4)   # fila 0 = abajo
    x0 = (w - LADO) // 2
    y0 = (h - LADO) // 2
    px = px[y0:y0 + LADO, x0:x0 + LADO].copy()
    px[..., 3] = mascara_hoja(px)

    vieja = bpy.data.images.get("hoja_rgba")
    if vieja:
        bpy.data.images.remove(vieja)
    img = bpy.data.images.new("hoja_rgba", LADO, LADO, alpha=True)
    img.pixels.foreach_set(px.ravel())
    img.scale(TAM, TAM)                                    # potencia de 2 para OpenGL
    img.filepath_raw = TEXTURA
    img.file_format = "PNG"
    img.alpha_mode = "STRAIGHT"
    bpy.context.scene.render.image_settings.color_mode = "RGBA"
    img.save()
    bpy.data.images.remove(src)
    log("Textura RGBA %dx%d guardada en %s" % (TAM, TAM, TEXTURA))
    return img


def nuevo_objeto(operador, **kw):
    """Ejecuta un operador de bpy.ops.mesh.primitive_* y regresa el objeto que creo.
    (Dentro de un timer bpy.context.active_object puede seguir apuntando al anterior.)"""
    antes = set(bpy.data.objects)
    operador(**kw)
    return (set(bpy.data.objects) - antes).pop()


def plano_con_material(img):
    plano = nuevo_objeto(bpy.ops.mesh.primitive_plane_add, size=2.0, location=(0, 0, 0))
    plano.name = "PlanoHoja"

    mat = bpy.data.materials.new("MatHoja")
    mat.use_nodes = True
    nodos = mat.node_tree.nodes
    bsdf = next(n for n in nodos if n.type == "BSDF_PRINCIPLED")   # el nombre cambia con el idioma
    tex = nodos.new("ShaderNodeTexImage")
    tex.image = img
    tex.location = (-400, 200)
    mat.node_tree.links.new(tex.outputs["Color"], bsdf.inputs["Base Color"])
    mat.node_tree.links.new(tex.outputs["Alpha"], bsdf.inputs["Alpha"])
    mat.surface_render_method = "BLENDED"
    plano.data.materials.append(mat)
    log("Plano con material: Color y Alpha de la textura conectados al BSDF")
    return plano


def vista_material():
    if DIRECTO:
        return
    for win in bpy.context.window_manager.windows:
        for area in win.screen.areas:
            if area.type != "VIEW_3D":
                continue
            area.spaces.active.shading.type = "MATERIAL"
            region = next(r for r in area.regions if r.type == "WINDOW")
            with bpy.context.temp_override(window=win, area=area, region=region):
                bpy.ops.view3d.view_selected()


def exportar():
    bpy.ops.export_scene.fbx(filepath=FBX, use_selection=False, path_mode="RELATIVE",
                             object_types={"MESH"})
    os.makedirs(BIN_MODELS, exist_ok=True)
    for f in (FBX, TEXTURA):
        shutil.copy2(f, BIN_MODELS)
    log("FBX exportado: %s (copiado con la textura a %s)" % (FBX, BIN_MODELS))


def guion():
    os.makedirs(BLENDER, exist_ok=True)
    limpiar_escena()
    yield PAUSA
    img = textura_rgba()
    yield PAUSA
    plano_con_material(img)
    vista_material()
    yield PAUSA
    if SALIDA:
        bpy.ops.wm.save_as_mainfile(filepath=SALIDA)
        log("Guardado %s" % SALIDA)
    exportar()
    log("Listo")


if DIRECTO:
    for _ in guion():
        pass
else:
    ORDENES = os.path.join(os.path.dirname(SALIDA or ASSETS + os.sep), "orden_blender.py")

    def _escuchar():
        if os.path.exists(ORDENES):
            with open(ORDENES, encoding="utf-8") as fh:
                codigo = fh.read()
            os.remove(ORDENES)
            try:
                exec(compile(codigo, ORDENES, "exec"), globals())
            except Exception:
                traceback.print_exc(file=sys.stdout)
            sys.stdout.flush()
        return 1.0

    _gen = guion()

    def _tick():
        try:
            return next(_gen)
        except StopIteration:
            pass
        except Exception:
            traceback.print_exc(file=sys.stdout)
        # Aun si el guion falla, Blender queda escuchando ordenes para corregir en vivo
        sys.stdout.flush()
        bpy.app.timers.register(_escuchar, first_interval=1.0, persistent=True)
        return None

    bpy.app.timers.register(_tick, first_interval=1.0)
