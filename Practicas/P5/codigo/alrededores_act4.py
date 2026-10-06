#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
alrededores_act4.py
Practica 05 - Adaptacion y carga de modelos
Laboratorio de Computacion Grafica e Interaccion Humano Computadora, FI-UNAM

Actividad 4: se amplia la zona de los edificios de la Actividad 1 con calles,
banquetas y casas, y se exporta el modelo completo a FBX para OpenGL.

    1. File > Import > FBX: la ciudad que se exporto en la Actividad 1
       (Practicas/P5/Blender/Edificio.fbx).
    2. Banqueta: un cubo aplanado bajo la manzana de los edificios.
    3. Calles: un plano de asfalto 0.3 u por debajo de las banquetas, y las
       manzanas de enfrente y de atras con su propia banqueta.
    4. Lineas centrales de las dos avenidas: un cubo delgado repetido con Array.
    5. Casa: cubo escalado (cuerpo) y otro cubo cuya cara superior se escala
       a 0 en Y en Modo Edicion, lo que la convierte en un techo de dos aguas.
    6. Array de la casa: una fila a lo largo de la calle y otra enfrente.
    7. Export a FBX del modelo completo (Practicas/P5/Blender/Ciudad.fbx).

Corre dentro de la interfaz de Blender sobre bpy.app.timers.

Uso:
    blender -P alrededores_act4.py -- <carpeta_capturas> <salida.blend>
    blender -b -P alrededores_act4.py -- --directo <salida.blend>
"""

import math
import os
import sys
import traceback

import bmesh
import bpy
from mathutils import Euler, Vector

args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
DIRECTO = "--directo" in args
args = [a for a in args if a != "--directo"]
CAPTURAS = args[0] if len(args) > 1 else None
SALIDA = args[-1] if args else None

PAUSA = 2.5
CUADROS = 30
DT = 1 / 30

AQUI = os.path.dirname(os.path.abspath(__file__))
BLENDER = os.path.normpath(os.path.join(AQUI, "..", "Blender"))
# Ciudad de la Actividad 1 (Practicas/P5/Blender/Edificio.fbx) y destino del modelo completo
ENTRADA = os.path.join(BLENDER, "Edificio.fbx")
FBX = os.environ.get("P5_FBX") or os.path.join(BLENDER, "Ciudad.fbx")

# Trazo de la zona: se calcula en trazar() a partir de la caja de la ciudad importada
MARGEN = 6.0                     # banqueta alrededor de los edificios
CALLE = 12.0                     # ancho de las avenidas
FONDO_MANZANA = 14.0             # fondo de las manzanas de casas
BORDE = 0.3                      # altura de la banqueta sobre el asfalto
CASA = (6.0, 5.0, 3.5)           # ancho (x), fondo (y) y alto del cuerpo
TECHO = 2.5                      # altura del techo de dos aguas
PASO_CASAS = 11.5                # separacion entre casas
X0 = X1 = CX = CY = 0.0
MANZANA_CENTRO = MANZANA_FRENTE = MANZANA_ATRAS = PISO_X = (0.0, 0.0)
CASAS = 1
PUERTA = (1.2, 2.2)              # ancho y alto de la puerta
VENTANA = (1.2, 1.2)             # ancho y alto de las ventanas
ALFEIZAR = 1.2                   # altura de la parte baja de las ventanas
HUNDIDO_CASA = 0.2               # Extrude hacia adentro de puertas y ventanas

estado = {}


# ----------------------------------------------------------------------------
# Utilidades de interfaz (no hacen nada en modo --directo)
# ----------------------------------------------------------------------------
def area_3d():
    wm = bpy.context.window_manager
    for ventana in (wm.windows if wm else []):
        for area in ventana.screen.areas:
            if area.type == 'VIEW_3D':
                region = next(r for r in area.regions if r.type == 'WINDOW')
                return ventana, area, region
    return None, None, None


def contexto(**extra):
    ventana, area, region = area_3d()
    if ventana is None:
        return bpy.context.temp_override(**extra)
    return bpy.context.temp_override(window=ventana, area=area, region=region, **extra)


def rotulo(texto):
    print(texto)
    sys.stdout.flush()
    _, area, _ = area_3d()
    if area:
        area.header_text_set(texto)


def redibujar():
    _, area, _ = area_3d()
    if area:
        area.tag_redraw()


def capturar(nombre):
    _, area, _ = area_3d()
    if not CAPTURAS or not area:
        return
    os.makedirs(CAPTURAS, exist_ok=True)
    n = sum(1 for a in os.listdir(CAPTURAS) if a.endswith(".png")) + 1
    ruta = os.path.join(CAPTURAS, f"{n:02d}_{nombre}.png")
    with contexto():
        bpy.ops.screen.screenshot_area(filepath=ruta)
    print("  captura:", ruta)


def encuadrar(x, y, z, distancia, azimut=30, elevacion=60):
    _, area, _ = area_3d()
    if not area:
        return
    r3d = area.spaces.active.region_3d
    r3d.view_perspective = 'PERSP'
    r3d.view_location = Vector((x, y, z))
    r3d.view_distance = distancia
    r3d.view_rotation = Euler((math.radians(elevacion), 0, math.radians(azimut))).to_quaternion()
    area.spaces.active.clip_end = 2000


def interpolar(aplicar):
    for i in range(1, CUADROS + 1):
        t = i / CUADROS
        aplicar(t * t * (3 - 2 * t))
        redibujar()
        yield DT


def activar(obj):
    for o in bpy.context.view_layer.objects:
        o.select_set(o == obj)
    bpy.context.view_layer.objects.active = obj


def losa(nombre, x0, x1, y0, y1, z0, z1):
    """Cubo escalado que ocupa la caja dada (banquetas, asfalto, lineas)."""
    with contexto():
        bpy.ops.mesh.primitive_cube_add(size=1, location=((x0 + x1) / 2, (y0 + y1) / 2,
                                                          (z0 + z1) / 2))
    obj = bpy.context.view_layer.objects.active
    obj.name = obj.data.name = nombre
    obj.scale = (x1 - x0, y1 - y0, z1 - z0)
    return obj


def crecer(obj):
    """Animacion: la losa aparece estirandose desde su centro."""
    s1 = obj.scale.copy()

    def aplicar(t):
        obj.scale = (s1.x * max(t, 0.01), s1.y * max(t, 0.01), s1.z)
    return aplicar


def array(obj, nombre, eje, paso, count=1):
    """Array (Legacy) con offset constante; se compensa la escala del objeto."""
    mod = obj.modifiers.new(nombre, 'ARRAY')
    mod.use_relative_offset = False
    mod.use_constant_offset = True
    desp = [0.0, 0.0, 0.0]
    desp[eje] = paso / obj.scale[eje]
    mod.constant_offset_displace = desp
    mod.count = count
    return mod


def contar(mod, hasta, texto):
    for n in range(2, hasta + 1):
        rotulo(f"{texto}: Count = {n}")
        mod.count = n
        redibujar()
        yield 0.35


# ----------------------------------------------------------------------------
# Pasos
# ----------------------------------------------------------------------------
def trazar(objetos):
    """Manzanas, calles y numero de casas a partir de la caja de la ciudad."""
    global X0, X1, CX, CY, MANZANA_CENTRO, MANZANA_FRENTE, MANZANA_ATRAS, PISO_X, CASAS
    puntos = [o.matrix_world @ Vector(c) for o in objetos for c in o.bound_box]
    xmin, xmax = min(p.x for p in puntos), max(p.x for p in puntos)
    ymin, ymax = min(p.y for p in puntos), max(p.y for p in puntos)
    X0, X1 = math.floor(xmin - MARGEN), math.ceil(xmax + MARGEN)
    MANZANA_CENTRO = (math.floor(ymin - MARGEN), math.ceil(ymax + MARGEN))
    MANZANA_FRENTE = (MANZANA_CENTRO[0] - CALLE - FONDO_MANZANA, MANZANA_CENTRO[0] - CALLE)
    MANZANA_ATRAS = (MANZANA_CENTRO[1] + CALLE, MANZANA_CENTRO[1] + CALLE + FONDO_MANZANA)
    PISO_X = (X0 - CALLE, X1 + CALLE)
    CX, CY = (X0 + X1) / 2, sum(MANZANA_CENTRO) / 2
    CASAS = int((X1 - X0 - 8 - CASA[0]) // PASO_CASAS) + 1
    print(f"  ciudad: x [{xmin:.1f}, {xmax:.1f}], y [{ymin:.1f}, {ymax:.1f}]  ->  "
          f"manzanas x [{X0}, {X1}], {CASAS} casas por fila")


def paso_importar():
    rotulo("Escena vacia  ->  File > Import > FBX: Edificio.fbx (ciudad de la Actividad 1)")
    for o in list(bpy.data.objects):
        bpy.data.objects.remove(o, do_unlink=True)
    with contexto():
        bpy.ops.import_scene.fbx(filepath=ENTRADA)
    estado["ciudad"] = [o for o in bpy.data.objects if o.type == 'MESH']
    for o in list(bpy.data.objects):          # el FBX trae camara/luz vacias, no se usan
        if o.type != 'MESH':
            bpy.data.objects.remove(o, do_unlink=True)
    trazar(estado["ciudad"])
    encuadrar(CX, CY, 8, 150)
    redibujar()
    yield 1.0
    capturar("ciudad_importada")
    yield PAUSA


def paso_banqueta():
    rotulo("Add > Mesh > Cube, S X / S Y / S Z: banqueta bajo la manzana de los edificios "
           f"({X1 - X0:g} x {MANZANA_CENTRO[1] - MANZANA_CENTRO[0]:g} u, {BORDE:g} u de alto)")
    b = losa("BanquetaCentro", X0, X1, *MANZANA_CENTRO, -BORDE, 0)
    yield from interpolar(crecer(b))
    capturar("banqueta_centro")
    yield PAUSA


def paso_calles():
    rotulo(f"Add > Mesh > Plane escalado: asfalto {BORDE:g} u por debajo de las banquetas")
    with contexto():
        bpy.ops.mesh.primitive_plane_add(size=1, location=(
            sum(PISO_X) / 2, (MANZANA_FRENTE[0] + MANZANA_ATRAS[1]) / 2, -BORDE))
    piso = bpy.context.view_layer.objects.active
    piso.name = piso.data.name = "Calles"
    piso.scale = (PISO_X[1] - PISO_X[0], MANZANA_ATRAS[1] - MANZANA_FRENTE[0], 1)
    yield from interpolar(crecer(piso))
    encuadrar(CX, CY, 0, 190, azimut=25, elevacion=55)
    yield PAUSA

    rotulo("Shift+D: banquetas de las manzanas de enfrente y de atras, "
           f"separadas por avenidas de {CALLE:g} u")
    for nombre, ys in (("BanquetaFrente", MANZANA_FRENTE), ("BanquetaAtras", MANZANA_ATRAS)):
        b = losa(nombre, X0, X1, *ys, -BORDE, 0)
        yield from interpolar(crecer(b))
    capturar("calles_banquetas")
    yield PAUSA


def paso_lineas():
    rotulo("Add > Mesh > Cube delgado: linea central de la avenida de enfrente")
    y_frente = (MANZANA_CENTRO[0] + MANZANA_FRENTE[1]) / 2
    y_atras = (MANZANA_CENTRO[1] + MANZANA_ATRAS[0]) / 2
    x = PISO_X[0] + 2
    linea = losa("LineaCentral", x, x + 3.0, y_frente - 0.25, y_frente + 0.25,
                 -BORDE, -BORDE + 0.08)
    encuadrar(x + 10, y_frente, 0, 45, azimut=20, elevacion=55)
    yield PAUSA
    largo = int((PISO_X[1] - PISO_X[0] - 4) // 6) + 1
    rotulo(f"Add Modifier > Array (offset constante de 6 u en X)")
    mod_x = array(linea, "Array X", 0, 6.0)
    encuadrar(CX, y_frente, 0, 130, azimut=20, elevacion=55)
    yield from contar(mod_x, largo, "Array X")
    rotulo(f"Segundo Array: la misma linea en la avenida de atras ({y_atras - y_frente:g} u en Y)")
    mod_y = array(linea, "Array Y", 1, y_atras - y_frente)
    yield from contar(mod_y, 2, "Array Y")
    encuadrar(CX, CY, 0, 190, azimut=25, elevacion=55)
    capturar("lineas_array")
    yield PAUSA


def paso_puertas_casa(cuerpo):
    """Fachadas de enfrente y atras: puerta al centro y una ventana a cada lado;
    costados: una ventana. Subdivide, Inset y Extrude hacia adentro, como en la
    planta baja de la Actividad 1. Se trabaja en coordenadas locales."""
    activar(cuerpo)
    with contexto():
        bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
        bpy.ops.object.mode_set(mode='EDIT')
    bpy.context.tool_settings.mesh_select_mode = (False, False, True)
    me = cuerpo.data
    bm = bmesh.from_edit_mesh(me)
    ax, ay, az = CASA[0] / 2, CASA[1] / 2, CASA[2] / 2

    def muros(eje):
        """Caras verticales cuya normal apunta en el eje dado (0 = X, 1 = Y)."""
        return [f for f in bm.faces if abs(f.normal[eje]) > 0.9]

    rotulo("Tab, 3 (caras): muros de enfrente y de atras  ->  Subdivide en 3 columnas")
    seleccionar_caras(me, bm, muros(1))
    yield PAUSA
    horizontales = list({e for f in muros(1) for e in f.edges
                         if abs(e.verts[0].co.z - e.verts[1].co.z) < 1e-4})
    bmesh.ops.subdivide_edges(bm, edges=horizontales, cuts=2, use_grid_fill=True)
    seleccionar_caras(me, bm, muros(1))
    capturar("casa_subdivide")
    yield PAUSA

    # Rectangulos (horizontal, z0, z1) en coordenadas locales de cada hueco
    piso = -az
    huecos = {}
    for f in muros(1):
        c = f.calc_center_median()
        if abs(c.x) < 0.5:
            huecos[f] = ("puerta", PUERTA[0], piso + 0.02, piso + PUERTA[1])
        else:
            huecos[f] = ("ventana", VENTANA[0], piso + ALFEIZAR, piso + ALFEIZAR + VENTANA[1])
    for f in muros(0):
        huecos[f] = ("ventana", VENTANA[0], piso + ALFEIZAR, piso + ALFEIZAR + VENTANA[1])

    for tipo, texto in (("puerta", "las 2 celdas centrales (puertas)"),
                        ("ventana", "las celdas laterales y los costados (ventanas)")):
        caras = [f for f, h in huecos.items() if h[0] == tipo]
        rotulo(f"Seleccion de caras: {texto}")
        seleccionar_caras(me, bm, caras)
        yield PAUSA

        rotulo(f"I (Inset) y G: hueco de {'%g x %g' % (PUERTA if tipo == 'puerta' else VENTANA)} u")
        bmesh.ops.inset_individual(bm, faces=caras, thickness=0.05, depth=0.0)
        objetivo = {}
        for f in caras:
            _, ancho, z0, z1 = huecos[f]
            c = f.calc_center_median()
            eje_h = 1 if abs(f.normal.x) > 0.9 else 0     # eje horizontal sobre el muro
            for v in f.verts:
                destino = v.co.copy()
                destino[eje_h] = c[eje_h] + (ancho / 2 if v.co[eje_h] > c[eje_h] else -ancho / 2)
                destino.z = z1 if v.co.z > c.z else z0
                objetivo[v] = (v.co.copy(), destino)
        seleccionar_caras(me, bm, caras)

        def colocar(t):
            for v, (a, b) in objetivo.items():
                v.co = a.lerp(b, t)
            bmesh.update_edit_mesh(me)
        yield from interpolar(colocar)
        yield 0.8

        rotulo(f"E hacia adentro {HUNDIDO_CASA:g} u")
        nuevas = bmesh.ops.extrude_discrete_faces(bm, faces=caras)["faces"]
        origen = {f: ([v.co.copy() for v in f.verts], f.normal.copy()) for f in nuevas}
        seleccionar_caras(me, bm, nuevas)

        def adentro(t):
            for f, (cos, n) in origen.items():
                for v, co in zip(f.verts, cos):
                    v.co = co - n * HUNDIDO_CASA * t
            bmesh.update_edit_mesh(me)
        yield from interpolar(adentro)
        # las caras originales quedaron debajo de las extruidas: se borran
        bmesh.ops.delete(bm, geom=[f for f in caras if f.is_valid], context='FACES_ONLY')
        bmesh.update_edit_mesh(me)
        capturar(f"casa_{tipo}s")
        yield PAUSA

    with contexto():
        bpy.ops.object.mode_set(mode='OBJECT')


def seleccionar_caras(me, bm, caras):
    for f in bm.faces:
        f.select_set(False)
    for f in caras:
        f.select_set(True)
    bmesh.update_edit_mesh(me)


def paso_casa():
    x0 = X0 + 4 + CASA[0] / 2
    y = (MANZANA_FRENTE[0] + MANZANA_FRENTE[1]) / 2
    rotulo(f"Add > Mesh > Cube, S X / S Y / S Z: cuerpo de la casa "
           f"({CASA[0]:g} x {CASA[1]:g} x {CASA[2]:g} u)")
    cuerpo = losa("CasaCuerpo", x0 - CASA[0] / 2, x0 + CASA[0] / 2,
                  y - CASA[1] / 2, y + CASA[1] / 2, 0, CASA[2])
    encuadrar(x0, y, 3, 28, azimut=35, elevacion=70)
    yield from interpolar(crecer(cuerpo))
    capturar("casa_cuerpo")
    yield PAUSA
    yield from paso_puertas_casa(cuerpo)

    rotulo("Shift+D: otro cubo encima, del mismo ancho y con un poco de alero, para el techo")
    techo = losa("CasaTecho", x0 - CASA[0] / 2 - 0.3, x0 + CASA[0] / 2 + 0.3,
                 y - CASA[1] / 2 - 0.4, y + CASA[1] / 2 + 0.4, CASA[2], CASA[2] + TECHO)
    with contexto():
        bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    yield PAUSA

    rotulo("Tab, 3 (caras): cara superior del techo seleccionada")
    activar(techo)
    with contexto():
        bpy.ops.object.mode_set(mode='EDIT')
    me = techo.data
    bm = bmesh.from_edit_mesh(me)
    for f in bm.faces:
        f.select_set(False)
    arriba = max(bm.faces, key=lambda f: f.calc_center_median().z)
    arriba.select_set(True)
    bm.select_flush_mode()
    bmesh.update_edit_mesh(me)
    redibujar()
    yield PAUSA

    rotulo("S Y 0: la cara se reduce a una arista  ->  techo de dos aguas")
    verts = list(arriba.verts)
    y0s = {v: v.co.y for v in verts}
    yc = sum(y0s.values()) / len(verts)

    def aplanar(t):
        for v in verts:
            v.co.y = y0s[v] + (yc - y0s[v]) * t
        bmesh.update_edit_mesh(me)
    yield from interpolar(aplanar)
    rotulo("M > By Distance: se funden los vertices que quedaron encimados")
    bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=1e-4)
    bmesh.update_edit_mesh(me)
    with contexto():
        bpy.ops.object.mode_set(mode='OBJECT')
    capturar("casa_techo_dos_aguas")
    yield PAUSA

    rotulo("Ctrl+J: cuerpo y techo se unen en un solo objeto 'Casa'")
    for o in bpy.context.view_layer.objects:
        o.select_set(o in (cuerpo, techo))
    bpy.context.view_layer.objects.active = cuerpo
    with contexto(selected_editable_objects=[cuerpo, techo], active_object=cuerpo):
        bpy.ops.object.join()
    cuerpo.name = cuerpo.data.name = "Casa"
    estado["casa"] = cuerpo
    yield PAUSA


def paso_array_casas():
    casa = estado["casa"]
    activar(casa)
    rotulo(f"Add Modifier > Array: casas a lo largo de la calle ({PASO_CASAS:g} u en X)")
    mod_x = array(casa, "Array X", 0, PASO_CASAS)
    encuadrar(CX, sum(MANZANA_FRENTE) / 2, 2, 110, azimut=15, elevacion=62)
    yield from contar(mod_x, CASAS, "Array X")
    yield PAUSA
    paso_y = sum(MANZANA_ATRAS) / 2 - sum(MANZANA_FRENTE) / 2
    rotulo(f"Segundo Array: la fila se repite en la manzana de atras ({paso_y:g} u en Y)")
    mod_y = array(casa, "Array Y", 1, paso_y)
    encuadrar(CX, CY, 0, 200, azimut=25, elevacion=55)
    yield from contar(mod_y, 2, "Array Y")
    capturar("casas_array")
    yield PAUSA


def paso_final():
    rotulo("Apply de los modificadores Array (de arriba hacia abajo)")
    for o in [o for o in bpy.data.objects if o.type == 'MESH' and o.modifiers]:
        activar(o)
        for mod in list(o.modifiers):
            with contexto(object=o, active_object=o):
                bpy.ops.object.modifier_apply(modifier=mod.name)
    encuadrar(CX, CY, 0, 200, azimut=25, elevacion=55)
    redibujar()
    yield 0.8
    capturar("zona_ampliada")
    yield PAUSA

    rotulo(f"File > Export > FBX: {os.path.basename(FBX)} (Path Mode Copy, "
           "Limit to Selected, Triangulate Faces)")
    for o in bpy.context.view_layer.objects:
        o.select_set(o.type == 'MESH')
    # model.h (Assimp) ignora las transformaciones de los nodos: se aplican antes
    with contexto(selected_editable_objects=bpy.context.selected_objects):
        bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    with contexto():
        bpy.ops.export_scene.fbx(filepath=FBX, use_selection=True, path_mode='COPY',
                                 use_triangles=True, use_mesh_modifiers=True,
                                 axis_forward='-Z', axis_up='Y')
    print(f"  exportado: {FBX} ({os.path.getsize(FBX) / 1e6:.1f} MB)")
    if SALIDA:
        bpy.ops.wm.save_as_mainfile(filepath=SALIDA)
        print("  guardado:", SALIDA)
    sys.stdout.flush()
    yield PAUSA
    rotulo(None)


def guion():
    yield 1.0
    for paso in (paso_importar, paso_banqueta, paso_calles, paso_lineas, paso_casa,
                 paso_array_casas, paso_final):
        yield from paso()


if __name__ != "__main__":
    pass
elif DIRECTO:
    for _ in guion():
        pass
else:
    # Al terminar Blender queda abierto escuchando ordenes (orden_blender.py
    # junto al .blend), igual que en la Actividad 1.
    ORDENES = os.path.join(os.path.dirname(SALIDA or bpy.data.filepath), "orden_blender.py")

    def _escuchar():
        if os.path.exists(ORDENES):
            with open(ORDENES, encoding="utf-8") as fh:
                codigo = fh.read()
            os.remove(ORDENES)
            try:
                exec(compile(codigo, ORDENES, "exec"), globals())
            except Exception:
                traceback.print_exc()
            sys.stdout.flush()
        return 1.0

    _gen = guion()

    def _tick():
        try:
            return next(_gen)
        except StopIteration:
            sys.stdout.flush()
            bpy.app.timers.register(_escuchar, first_interval=1.0, persistent=True)
            return None
        except Exception:
            traceback.print_exc()
            sys.stdout.flush()
            return None

    bpy.app.timers.register(_tick, first_interval=1.0)
