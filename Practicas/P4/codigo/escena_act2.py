#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
escena_act2.py
Practica 04 - Modelado Jerarquico y camara sintetica
Laboratorio de Computacion Grafica e Interaccion Humano Computadora, FI-UNAM

Prepara el archivo .blend de la Actividad 2 a partir del FBX descargado de
Adobe Mixamo. Deja la escena lista para inspeccionar la jerarquia: la armadura
visible por encima de la malla, el rango de fotogramas ajustado a la accion
importada y la camara colocada para encuadrar al personaje de cuerpo completo.

Uso:
    blender -b --factory-startup -P escena_act2.py -- <archivo.fbx> <salida.blend>
"""

import math
import os
import sys

import bpy
from mathutils import Vector


def limpiar_escena():
    """Elimina el cubo, la camara y la luz que trae la escena de fabrica."""
    bpy.ops.object.select_all(action='SELECT')
    bpy.ops.object.delete(use_global=False)
    for bloque in (bpy.data.meshes, bpy.data.cameras, bpy.data.lights):
        for dato in list(bloque):
            if dato.users == 0:
                bloque.remove(dato)


def configurar_armadura(armadura):
    """Deja el esqueleto visible a traves de la malla, como en la Actividad 1."""
    datos = armadura.data
    datos.display_type = 'OCTAHEDRAL'
    datos.show_names = False
    datos.show_axes = False
    armadura.show_in_front = True          # Rayos X: los huesos atraviesan la piel
    return datos


def ajustar_rango(armadura):
    """Hace coincidir el rango de la linea de tiempo con la accion de Mixamo."""
    escena = bpy.context.scene
    accion = None
    if armadura.animation_data and armadura.animation_data.action:
        accion = armadura.animation_data.action
    elif bpy.data.actions:
        accion = bpy.data.actions[0]
    if accion:
        inicio, fin = accion.frame_range
        escena.frame_start = int(inicio)
        escena.frame_end = int(fin)
    escena.frame_set(escena.frame_start)
    return accion


def caja_envolvente(objetos):
    """Esquinas minima y maxima del conjunto, en coordenadas de mundo."""
    puntos = []
    for obj in objetos:
        if obj.type != 'MESH':
            continue
        puntos.extend([obj.matrix_world @ Vector(v) for v in obj.bound_box])
    if not puntos:
        return Vector((0, 0, 0)), Vector((1, 1, 1))
    minimo = Vector((min(p.x for p in puntos), min(p.y for p in puntos),
                     min(p.z for p in puntos)))
    maximo = Vector((max(p.x for p in puntos), max(p.y for p in puntos),
                     max(p.z for p in puntos)))
    return minimo, maximo


def colocar_camara(objetos):
    """Camara de tres cuartos que encuadra al personaje completo."""
    minimo, maximo = caja_envolvente(objetos)
    centro = (minimo + maximo) / 2.0
    altura = max(maximo.z - minimo.z, 1e-4)

    camara_datos = bpy.data.cameras.new("CamaraJerarquia")
    camara = bpy.data.objects.new("CamaraJerarquia", camara_datos)
    bpy.context.collection.objects.link(camara)

    distancia = altura * 2.4
    camara.location = centro + Vector((distancia * 0.75, -distancia, altura * 0.25))
    direccion = centro - camara.location
    camara.rotation_euler = direccion.to_track_quat('-Z', 'Y').to_euler()
    bpy.context.scene.camera = camara

    # Luz suave para que el sombreado solido no dependa del estudio por defecto.
    luz_datos = bpy.data.lights.new("LuzClave", type='SUN')
    luz_datos.energy = 3.0
    luz = bpy.data.objects.new("LuzClave", luz_datos)
    luz.rotation_euler = (math.radians(55), 0.0, math.radians(35))
    bpy.context.collection.objects.link(luz)
    return camara


def main():
    if "--" not in sys.argv:
        print("Uso: blender -b --factory-startup -P escena_act2.py -- <fbx> <blend>")
        return 1
    argumentos = sys.argv[sys.argv.index("--") + 1:]
    ruta_fbx, ruta_blend = argumentos[0], argumentos[1]

    limpiar_escena()
    bpy.ops.import_scene.fbx(filepath=ruta_fbx)

    armadura = next(o for o in bpy.data.objects if o.type == 'ARMATURE')
    mallas = [o for o in bpy.data.objects if o.type == 'MESH']
    armadura.name = "Armadura_Remy"

    configurar_armadura(armadura)
    accion = ajustar_rango(armadura)
    colocar_camara(mallas)

    armadura.select_set(True)
    bpy.context.view_layer.objects.active = armadura

    os.makedirs(os.path.dirname(ruta_blend) or ".", exist_ok=True)
    bpy.ops.wm.save_as_mainfile(filepath=ruta_blend)

    print("=" * 60)
    print("Escena guardada en :", ruta_blend)
    print("Armadura           : %s (%d huesos)"
          % (armadura.name, len(armadura.data.bones)))
    print("Mallas             : " + ", ".join("%s (%d vertices)"
          % (m.name, len(m.data.vertices)) for m in mallas))
    print("Accion             : %s" % (accion.name if accion else "ninguna"))
    print("Rango de fotogramas: %d - %d"
          % (bpy.context.scene.frame_start, bpy.context.scene.frame_end))
    print("Transformacion del objeto armadura (tal como llega de Mixamo):")
    print("  escala   : %.4f, %.4f, %.4f" % tuple(armadura.scale))
    print("  rotacion : %.1f, %.1f, %.1f grados"
          % tuple(math.degrees(a) for a in armadura.rotation_euler))
    print("=" * 60)
    return 0


if __name__ == "__main__":
    sys.exit(main())
