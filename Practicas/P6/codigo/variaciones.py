#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
variaciones.py
Practica 06 - Texturizado
Laboratorio de Computacion Grafica e Interaccion Humano Computadora, FI-UNAM

Una variacion por cada primitiva de las actividades 5.1 a 5.4, en una segunda
fila detras de las originales (que se conservan). Cada textura combina dos o
mas imagenes y todas miden potencias de 2.

    Plano    -> Reja de malla ciclonica (RGBA 1024): rombos de alambre calculados
                como canal alpha + metal galvanizado (Metal011); poste y tubo.
    Cubo     -> Edificio de 3 x 3 x 6 (atlas 2048): ladrillo (Bricks005), concreto
                (Concrete006) en losas, marcos y azotea, ventanas que reflejan el
                cielo del cubemap (Humus Footballfield) y puerta de madera (Planks012).
    Cilindro -> Tinaco sobre la azotea (atlas 1024): plastico negro (Plastic006)
                con estrias; tapa con rosca.
    Esfera   -> Balon de futbol (2048 x 1024 equirectangular): las 32 caras del
                icosaedro truncado calculadas por pixel, cuero (Leather012).

Se ejecuta desde el listener (orden_blender.py):
    exec(open(".../variaciones.py", encoding="utf-8").read(), globals())
    correr(guion_variaciones())
"""

import math
import os
import shutil
import sys

import bmesh
import bpy
import numpy as np
from mathutils import Matrix

_AQUI = os.path.dirname(os.path.abspath(__file__))
with open(os.path.join(_AQUI, "caja_act2.py"), encoding="utf-8") as _fh:
    exec(_fh.read().split('\nif "--directo" in sys.argv')[0], globals())

TEX = {
    "ladrillo": os.path.join(ASSETS, "ladrillo_bricks005.png"),
    "concreto": os.path.join(ASSETS, "concreto_concrete006.png"),
    "madera": os.path.join(ASSETS, "madera_planks012.png"),
    "plastico": os.path.join(ASSETS, "plastico_plastic006.png"),
    "cuero": os.path.join(ASSETS, "cuero_leather012.png"),
    "galvanizado": os.path.join(ASSETS, "metal_metal011.png"),
    "cielo": os.path.join(ASSETS, "humus", "Footballfield", "posz.jpg"),
}
SALIDAS = {n: os.path.join(BLENDER_DIR, n + ".png")
           for n in ("reja_rgba", "edificio_atlas", "tinaco_atlas", "balon_textura")}

# Posiciones (segunda fila, y > 0 queda detras de la primera en OpenGL)
POS_REJA = (2.0, 4.5, 1.0)                # plano de 6 x 2, de pie
EDIFICIO = (3.0, 3.0, 6.0)                # ancho, fondo, alto
POS_EDIFICIO = (2.0, 8.0, 3.0)
R_TINACO, H_TINACO = 0.55, 1.1
POS_TINACO = (2.8, 8.8, 6.0 + H_TINACO / 2)
R_BALON = 0.6
POS_BALON = (-5.5, 3.0, R_BALON)


def _leer(ruta, w, h=None):
    """Imagen escalada a w x h como arreglo RGBA float (fila 0 = abajo)."""
    h = h or w
    img = bpy.data.images.load(ruta, check_existing=False)
    img.scale(w, h)
    px = np.array(img.pixels[:], np.float32).reshape(h, w, 4).copy()
    bpy.data.images.remove(img)
    return px


def _guardar(nombre, px, alpha=False):
    h, w = px.shape[:2]
    vieja = bpy.data.images.get(nombre)
    if vieja:
        bpy.data.images.remove(vieja)
    img = bpy.data.images.new(nombre, w, h, alpha=alpha)
    img.pixels.foreach_set(np.clip(px, 0, 1).ravel())
    img.filepath_raw = SALIDAS[nombre]
    img.file_format = "PNG"
    img.save()
    log("Textura %s %dx%d guardada" % (nombre, w, h))
    return img


def _remuestrear(px, w, h):
    """Recorte/escala por vecino mas cercano (para pegar fotos en regiones pequenas)."""
    ys = (np.arange(h) * px.shape[0] / h).astype(int)
    xs = (np.arange(w) * px.shape[1] / w).astype(int)
    return px[ys][:, xs]


def _material(obj, nombre, img, alpha=False, rugosidad=0.7):
    mat = bpy.data.materials.get(nombre) or bpy.data.materials.new(nombre)
    mat.use_nodes = True
    nodos = mat.node_tree.nodes
    bsdf = next(n for n in nodos if n.type == "BSDF_PRINCIPLED")
    tex = next((n for n in nodos if n.type == "TEX_IMAGE"), None) or nodos.new("ShaderNodeTexImage")
    tex.image = img
    tex.location = (-400, 200)
    mat.node_tree.links.new(tex.outputs["Color"], bsdf.inputs["Base Color"])
    if alpha:
        mat.node_tree.links.new(tex.outputs["Alpha"], bsdf.inputs["Alpha"])
        mat.surface_render_method = "BLENDED"
    bsdf.inputs["Roughness"].default_value = rugosidad
    obj.data.materials.clear()
    obj.data.materials.append(mat)


def _quitar(nombre):
    ob = bpy.data.objects.get(nombre)
    if ob:
        bpy.data.objects.remove(ob, do_unlink=True)


# ---------------------------------------------------------------- Reja (plano)
def textura_reja():
    T, PASO, GROSOR = 1024, 64, 3.0
    metal = _leer(TEX["galvanizado"], T)
    y, x = np.mgrid[:T, :T].astype(np.float32)
    def alambre(s):
        d = np.abs(((s / PASO) + 0.5) % 1.0 - 0.5) * PASO          # distancia al alambre
        return np.clip(1.0 - d / GROSOR, 0, 1)
    a = np.maximum(alambre(x + y), alambre(x - y))                    # rombos
    poste = np.abs(x - T / 2) < 22                                    # poste al centro del tramo
    tubo = (y > T - 40) & (y < T - 8)                                 # tubo superior
    px = metal.copy()
    px[..., :3] *= (0.75 + 0.35 * a)[..., None]
    for m, eje in ((poste, x - T / 2), (tubo, y - (T - 24))):
        relieve = 0.65 + 0.45 * np.cos(np.clip(eje / 22, -1, 1) * math.pi / 2)
        px[m, :3] = metal[m, :3] * relieve[m][:, None]
        a = np.where(m, 1.0, a)
    px[..., 3] = a
    return _guardar("reja_rgba", px, alpha=True)


def reja(img):
    _quitar("Reja")
    ob = nuevo_objeto(bpy.ops.mesh.primitive_plane_add, size=1.0, location=POS_REJA)
    ob.name = "Reja"
    me = ob.data
    me.transform(Matrix.Diagonal((6.0, 2.0, 1.0, 1.0)))             # 6 de ancho x 2 de alto
    me.transform(Matrix.Rotation(math.radians(90), 4, "X"))   # de pie, mirando a -Y
    uv = me.uv_layers.active.data
    for loop in me.loops:
        co = me.vertices[loop.vertex_index].co
        uv[loop.index].uv = ((co.x + 3.0) / 2.0, (co.z + 1.0) / 2.0)   # 3 tramos a lo ancho
    _material(ob, "MatReja", img, alpha=True, rugosidad=0.4)
    log("Reja: plano 6 x 2 de pie con la textura repetida 3 veces")


# ------------------------------------------------------------ Edificio (cubo)
def _fachada(lad, conc, mad, cielo, frente, semilla):
    W, H = 512, 1024
    m = W / EDIFICIO[0]                                               # pixeles por metro
    c = np.tile(lad, (H // lad.shape[0], W // lad.shape[1], 1))[..., :3].copy()
    concf = np.tile(conc, (H // conc.shape[0], W // conc.shape[1], 1))[..., :3]
    def rect(x0, z0, x1, z1):
        return slice(int(z0 * m), int(z1 * m)), slice(int(x0 * m), int(x1 * m))
    for z0, z1 in ((0, 0.3), (1.95, 2.05), (3.95, 4.05), (5.8, 6.0)):  # zocalo, losas, cornisa
        r = rect(0, z0, EDIFICIO[0], z1)
        c[r] = concf[r] * 1.05
    rng = np.random.default_rng(semilla)
    ventanas = [(cx, f * 2 + 0.8) for f in range(3) for cx in (0.75, 2.25)]
    if frente:
        ventanas = [v for v in ventanas if v[1] > 1] + [(0.5, 0.8), (2.5, 0.8)]
        r = rect(1.5 - 0.5, 0.3, 1.5 + 0.5, 2.0)                       # marco de la puerta
        c[r] = concf[r] * 1.1
        r = rect(1.5 - 0.42, 0.3, 1.5 + 0.42, 1.92)
        hh, ww = c[r].shape[:2]
        c[r] = _remuestrear(mad, ww, hh)[..., :3] * 0.9
        jal = rect(1.5 + 0.25, 1.0, 1.5 + 0.31, 1.25)                  # jaladera
        c[jal] = 0.75
    for cx, z0 in ventanas:
        ancho = 0.6 if (frente and z0 < 1) else 0.8
        r = rect(cx - ancho / 2 - 0.06, z0 - 0.1, cx + ancho / 2 + 0.06, z0 + 1.06)
        c[r] = concf[r] * 1.15                                        # marco y repisa
        r = rect(cx - ancho / 2, z0, cx + ancho / 2, z0 + 1.0)
        hh, ww = c[r].shape[:2]
        x0 = int(rng.integers(0, cielo.shape[1] - 160))
        y0 = int(rng.integers(int(cielo.shape[0] * 0.6), cielo.shape[0] - 200))
        reflejo = _remuestrear(cielo[y0:y0 + 200, x0:x0 + 160], ww, hh)[..., :3]
        c[r] = reflejo * np.array([0.62, 0.72, 0.85]) + 0.05            # vidrio azulado
        mx = slice(int((cx - 0.02) * m), int((cx + 0.02) * m))
        c[r[0], mx] = 0.85                                            # parteluz
    return c


def textura_edificio():
    T = 2048
    lad = _leer(TEX["ladrillo"], 256)
    conc = _leer(TEX["concreto"], 512)
    mad = _leer(TEX["madera"], 256)
    cielo = _leer(TEX["cielo"], 1024)
    atlas = np.ones((T, T, 4), np.float32)
    for col in range(4):                                              # -Y (frente), +X, +Y, -X
        atlas[T // 2:, col * 512:(col + 1) * 512, :3] = _fachada(lad, conc, mad, cielo, col == 0, col)
    azotea = np.tile(conc, (2, 2, 1))[..., :3].copy()
    y, x = np.mgrid[:1024, :1024]
    borde = np.minimum(np.minimum(x, 1023 - x), np.minimum(y, 1023 - y))
    azotea[borde < 50] *= 1.15                                       # pretil
    azotea[(borde >= 50) & (borde < 58)] *= 0.5
    esc = (np.abs(x - 300) < 90) & (np.abs(y - 300) < 90)            # registro de la azotea
    azotea[esc] = azotea[esc] * 0.45 + 0.1
    atlas[:1024, :1024, :3] = azotea
    atlas[:1024, 1024:, :3] = np.tile(conc, (2, 2, 1))[..., :3] * 0.5
    return _guardar("edificio_atlas", atlas)


def edificio(img):
    _quitar("Edificio")
    ob = nuevo_objeto(bpy.ops.mesh.primitive_cube_add, size=1.0, location=POS_EDIFICIO)
    ob.name = "Edificio"
    me = ob.data
    me.transform(Matrix.Diagonal((*EDIFICIO, 1.0)))
    bm = bmesh.new()
    bm.from_mesh(me)
    uv = bm.loops.layers.uv.verify()
    hx, hy, hz = (e / 2 for e in EDIFICIO)
    caras = {(0, -1, 0): (0, lambda x, y, z: (x / hx, z / hz)), (1, 0, 0): (1, lambda x, y, z: (y / hy, z / hz)),
             (0, 1, 0): (2, lambda x, y, z: (-x / hx, z / hz)), (-1, 0, 0): (3, lambda x, y, z: (-y / hy, z / hz))}
    p = PAD / 512
    for f in bm.faces:
        n = tuple(int(round(c)) for c in f.normal)
        for l in f.loops:
            x, y, z = l.vert.co
            if n in caras:
                col, proy = caras[n]
                a, b = proy(x, y, z)
                l[uv].uv = ((col + p + (a + 1) / 2 * (1 - 2 * p)) / 4, 0.5 + (p + (b + 1) / 2 * (1 - 2 * p)) / 2)
            else:
                a, b = x / hx, (y if n[2] > 0 else -y) / hy
                cu = 0.0 if n[2] > 0 else 0.5
                l[uv].uv = (cu + (p + (a + 1) / 2 * (1 - 2 * p)) / 2, (p + (b + 1) / 2 * (1 - 2 * p)) / 2)
    bm.to_mesh(me)
    bm.free()
    _material(ob, "MatEdificio", img, rugosidad=0.8)
    log("Edificio %s en %s: 4 fachadas, azotea y base en su celda" % (EDIFICIO, POS_EDIFICIO))


# ------------------------------------------------------------- Tinaco (cilindro)
def textura_tinaco():
    T, M = 1024, 512
    plas = 0.07 + _leer(TEX["plastico"], 512)[..., :3] * 1.8         # negro con rayones visibles
    atlas = np.ones((T, T, 4), np.float32)
    lado = np.tile(plas, (1, 2, 1))                                   # 512 x 1024
    v = (np.arange(M) + 0.5) / M
    estrias = 0.55 + 0.75 * np.cos(v * 2 * math.pi * 7) ** 8         # 7 estrias horizontales
    lado = lado * estrias[:, None, None] * (0.9 + 0.2 * v)[:, None, None]
    atlas[M:, :, :3] = lado
    y, x = np.mgrid[:M, :M].astype(np.float32)
    r = np.hypot(x - (M - 1) / 2, y - (M - 1) / 2) / ((M - 1) / 2)
    tapa = plas * (0.95 + 0.12 * np.cos(r * math.pi * 10))[..., None]   # anillos concentricos
    rosca = r < 0.36
    ang = np.arctan2(y - M / 2, x - M / 2)
    tapa[rosca] = plas[rosca] * (1.25 + 0.2 * np.cos(ang[rosca] * 24))[:, None]
    tapa[(r >= 0.36) & (r < 0.39)] *= 0.4
    tapa[r > 1] = 0.02
    atlas[:M, :M, :3] = tapa
    atlas[:M, M:, :3] = plas * 0.6
    return _guardar("tinaco_atlas", atlas)


def tinaco(img):
    _quitar("Tinaco")
    ob = nuevo_objeto(bpy.ops.mesh.primitive_cylinder_add, vertices=32, radius=R_TINACO,
                      depth=H_TINACO, location=POS_TINACO)
    ob.name = "Tinaco"
    me = ob.data
    me.shade_smooth()
    for pl in me.polygons:
        if abs(pl.normal.z) > 0.5:
            pl.use_smooth = False
    bm = bmesh.new()
    bm.from_mesh(me)
    uv = bm.loops.layers.uv.verify()
    p = PAD / 1024
    for f in bm.faces:
        if abs(f.normal.z) < 0.5:
            us = [(math.atan2(l.vert.co.y, l.vert.co.x) / (2 * math.pi)) % 1.0 for l in f.loops]
            if max(us) - min(us) > 0.5:
                us = [u + 1.0 if u < 0.5 else u for u in us]
            for l, u in zip(f.loops, us):
                l[uv].uv = (u, 0.5 + p + (l.vert.co.z + H_TINACO / 2) / H_TINACO * (0.5 - 2 * p))
        else:
            arriba = f.normal.z > 0
            for l in f.loops:
                x, y = l.vert.co.x / R_TINACO, l.vert.co.y / R_TINACO
                l[uv].uv = ((0.25 if arriba else 0.75) + (x if arriba else -x) * (0.25 - p), 0.25 + y * (0.25 - p))
    bm.to_mesh(me)
    bm.free()
    _material(ob, "MatTinaco", img, rugosidad=0.5)
    log("Tinaco r=%.2f h=%.1f sobre la azotea en %s" % (R_TINACO, H_TINACO, POS_TINACO))


# --------------------------------------------------------------- Balon (esfera)
def _centros():
    f = (1 + 5 ** 0.5) / 2
    ico = [(0, s1, s2 * f) for s1 in (-1, 1) for s2 in (-1, 1)]
    ico = [p for v in ico for p in (v, (v[1], v[2], v[0]), (v[2], v[0], v[1]))]
    dod = [(a, b, c) for a in (-1, 1) for b in (-1, 1) for c in (-1, 1)]
    for s1 in (-1, 1):
        for s2 in (-1, 1):
            v = (s1 / f, 0, s2 * f)                                # dual del icosaedro
            dod += [v, (v[1], v[2], v[0]), (v[2], v[0], v[1])]
    norm = lambda vs: np.array(vs, np.float64) / np.linalg.norm(vs, axis=1, keepdims=True)
    return norm(ico), norm(dod)


def textura_balon():
    W, H = 2048, 1024
    pent, hexa = _centros()                                           # 12 pentagonos, 20 hexagonos
    # Distancia de cada cara al centro del icosaedro truncado (arista 1)
    D_PENT, D_HEX = 2.32744, 2.26728
    normales = np.concatenate([pent / D_PENT, hexa / D_HEX]).astype(np.float32)   # 32 x 3
    u = ((np.arange(W) + 0.5) / W * 2 * math.pi).astype(np.float32)
    es_pent = np.zeros((H, W), bool)
    costura = np.zeros((H, W), bool)
    for f0 in range(0, H, 64):                                        # por bloques de filas
        v = ((np.arange(f0, f0 + 64) + 0.5) / H - 0.5).astype(np.float32) * math.pi
        lat, lon = np.meshgrid(v, u, indexing="ij")
        d = np.stack([np.cos(lat) * np.cos(lon), np.cos(lat) * np.sin(lon), np.sin(lat)], -1)
        # cara que el rayo desde el centro toca primero: maximo de dot(d, n) / distancia
        s = d @ normales.T
        dos = np.partition(s, -2, axis=-1)[..., -2:]
        es_pent[f0:f0 + 64] = s.argmax(-1) < len(pent)
        costura[f0:f0 + 64] = (dos[..., 1] - dos[..., 0]) < 0.0045
    cuero = _leer(TEX["cuero"], 256)[..., :3]
    piel = np.tile(cuero, (H // 256, W // 256, 1))
    px = np.ones((H, W, 4), np.float32)
    px[..., :3] = piel * 1.05
    px[es_pent, :3] = piel[es_pent] * 0.12
    px[costura, :3] = 0.18
    return _guardar("balon_textura", px)


def balon(img):
    _quitar("Balon")
    ob = nuevo_objeto(bpy.ops.mesh.primitive_uv_sphere_add, segments=64, ring_count=32,
                      radius=R_BALON, location=POS_BALON)
    ob.name = "Balon"
    ob.data.shade_smooth()
    _material(ob, "MatBalon", img, rugosidad=0.6)
    log("Balon r=%.1f en %s" % (R_BALON, POS_BALON))


def guion_variaciones():
    reja(textura_reja())
    encuadrar()
    yield PAUSA
    edificio(textura_edificio())
    encuadrar()
    yield PAUSA
    tinaco(textura_tinaco())
    yield PAUSA
    balon(textura_balon())
    encuadrar()
    yield PAUSA
    bpy.ops.wm.save_mainfile()
    log("Guardado %s" % bpy.data.filepath)
    exportar_escena()
    for f in SALIDAS.values():
        shutil.copy2(f, BIN_MODELS)
    log("Listo variaciones")


if "--directo" in sys.argv:
    for _ in guion_variaciones():
        pass
