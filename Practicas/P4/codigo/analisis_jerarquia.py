#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
analisis_jerarquia.py
Practica 04 - Modelado Jerarquico y camara sintetica
Laboratorio de Computacion Grafica e Interaccion Humano Computadora, FI-UNAM

Herramienta de apoyo para las Actividades 2 y 3. Importa en Blender el archivo
FBX exportado desde Adobe Mixamo, recorre la armadura desde el nodo raiz y
produce tres salidas:

  1. jerarquia.txt   Arbol de relaciones padre-hijo en formato de texto.
  2. jerarquia.json  Misma estructura en formato legible por maquina.
  3. diagrama_jerarquia.tex  Diagrama del arbol (paquete forest) para el reporte.

Ademas ejecuta la verificacion numerica de la cadena de transformaciones: para
cada hueso comprueba que la matriz global que reporta Blender coincida con el
producto acumulado M_global = M_global(padre) * M_local, que es la definicion de
cinematica directa. El resultado se imprime como el error maximo encontrado.

Uso:
    blender -b -P analisis_jerarquia.py -- <archivo.fbx> <carpeta_salida>
"""

import json
import os
import sys

import bpy
from mathutils import Matrix

# El separador de Mixamo (":") hay que escaparlo en LaTeX; tambien el guion bajo.
LATEX_ESCAPES = (("\\", r"\textbackslash{}"), ("_", r"\_"), ("&", r"\&"),
                 ("%", r"\%"), ("#", r"\#"), ("$", r"\$"))


def escapar_latex(texto):
    """Devuelve el nombre del hueso listo para insertarse en el documento."""
    for viejo, nuevo in LATEX_ESCAPES:
        texto = texto.replace(viejo, nuevo)
    return texto


def importar_fbx(ruta):
    """Carga el FBX en una escena vacia y regresa el objeto armadura."""
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=ruta)
    for obj in bpy.data.objects:
        if obj.type == 'ARMATURE':
            return obj
    raise RuntimeError("El archivo no contiene ninguna armadura: " + ruta)


def construir_arbol(hueso):
    """Recorrido en profundidad de la armadura a partir de un hueso dado."""
    return {
        "nombre": hueso.name,
        "padre": hueso.parent.name if hueso.parent else None,
        "conectado": bool(hueso.use_connect),
        "longitud": round(hueso.length, 5),
        "hijos": [construir_arbol(h) for h in hueso.children],
    }


def volcar_texto(nodo, salida, prefijo="", es_ultimo=True, raiz=True):
    """Escribe el arbol con los caracteres de dibujo tipicos de un 'tree'."""
    if raiz:
        salida.append(nodo["nombre"] + "   [NODO RAIZ]")
    else:
        rama = "`-- " if es_ultimo else "|-- "
        marca = "" if nodo["conectado"] else "   (desconectado)"
        salida.append(prefijo + rama + nodo["nombre"] + marca)
        prefijo += "    " if es_ultimo else "|   "
    hijos = nodo["hijos"]
    for i, hijo in enumerate(hijos):
        volcar_texto(hijo, salida, prefijo, i == len(hijos) - 1, raiz=False)
    return salida


DEDOS = ("Thumb", "Index", "Middle", "Ring", "Pinky")


def es_dedo(nombre):
    """Identifica los huesos de las falanges, que se repiten cinco veces."""
    return any(d in nombre for d in DEDOS)


def podar_dedos(nodo):
    """Sustituye las cadenas de falanges por un unico nodo resumen.

    El arbol completo de Mixamo mide 68 huesos y casi la mitad son falanges
    identicas entre si, de modo que dibujarlo entero produce una figura de
    proporcion 8:1 ilegible. Para la vista global condensamos cada mano.
    """
    hijos_dedos = [h for h in nodo["hijos"] if es_dedo(h["nombre"])]
    hijos_resto = [h for h in nodo["hijos"] if not es_dedo(h["nombre"])]
    nuevo = dict(nodo)
    nuevo["hijos"] = [podar_dedos(h) for h in hijos_resto]
    if hijos_dedos:
        total = sum(contar(h) for h in hijos_dedos)
        nuevo["hijos"].append({
            "nombre": "[%d dedos: %d huesos]" % (len(hijos_dedos), total),
            "padre": nodo["nombre"], "conectado": False, "longitud": 0,
            "hijos": [], "resumen": True,
        })
    return nuevo


def contar(nodo):
    """Numero de huesos del subarbol, incluido el propio nodo."""
    return 1 + sum(contar(h) for h in nodo["hijos"])


def buscar(nodo, nombre):
    """Localiza el subarbol cuya raiz es el hueso indicado."""
    if nodo["nombre"] == nombre:
        return nodo
    for hijo in nodo["hijos"]:
        encontrado = buscar(hijo, nombre)
        if encontrado:
            return encontrado
    return None


def volcar_forest(nodo, nivel=1, raiz=True, abreviar=None):
    """Genera el cuerpo del diagrama en la sintaxis del paquete forest.

    'abreviar' es un prefijo comun que se sustituye por puntos suspensivos en
    los nodos descendientes. Sirve para que un diagrama de la mano no repita
    catorce veces la cadena 'LeftHand' y el texto pueda imprimirse mas grande.
    """
    sangria = "  " * nivel
    nombre = nodo["nombre"].replace("mixamorig:", "")
    if abreviar and not raiz and nombre.startswith(abreviar):
        nombre = "..." + nombre[len(abreviar):]
    etiqueta = escapar_latex(nombre)
    if nodo.get("resumen"):
        estilo = ", resumen"
    elif raiz:
        estilo = ", raiz"
    else:
        estilo = ""
    lineas = ["%s[{\\ttfamily %s}%s" % (sangria, etiqueta, estilo)]
    for hijo in nodo["hijos"]:
        lineas.extend(volcar_forest(hijo, nivel + 1, raiz=False, abreviar=abreviar))
    lineas.append(sangria + "]")
    return lineas


PLANTILLA_FOREST = r"""\documentclass[border=6pt]{standalone}
\usepackage[utf8]{inputenc}
\usepackage[T1]{fontenc}
\usepackage{forest}
\usepackage{xcolor}
\definecolor{colRaiz}{RGB}{201,218,248}
\definecolor{colResumen}{RGB}{237,237,237}
\forestset{
  raiz/.style={fill=colRaiz, draw=black, line width=0.7pt},
  resumen/.style={fill=colResumen, draw=black!45, dashed},
}
\begin{document}
\begin{forest}
  for tree={
    grow'=east, parent anchor=east, child anchor=west, anchor=west,
    edge path={\noexpand\path[\forestoption{edge}]
      (!u.parent anchor) -- +(5pt,0) |- (.child anchor)\forestoption{edge label};},
    draw=black!55, rounded corners=1.5pt, inner sep=2pt,
    font=\small, l sep=7pt, s sep=2.5pt,
  },
%s
\end{forest}
\end{document}
"""


def escribir_diagrama(carpeta, nombre, arbol, abreviar=None):
    """Vuelca un arbol al disco como documento standalone compilable."""
    ruta = os.path.join(carpeta, nombre)
    cuerpo = "\n".join(volcar_forest(arbol, abreviar=abreviar))
    with open(ruta, "w", encoding="utf-8") as f:
        f.write(PLANTILLA_FOREST % cuerpo)
    return ruta


def verificar_cinematica_directa(armadura):
    """Comprueba M_global = M_global(padre) * M_local hueso por hueso.

    Blender guarda en pose_bone.matrix la matriz global del hueso y en
    pose_bone.matrix_basis la transformacion que el animador aplica encima de la
    pose de reposo. Reconstruimos la matriz global desde el padre y medimos la
    discrepancia contra la que reporta Blender.
    """
    escena = bpy.context.scene
    escena.frame_set(escena.frame_start)
    bpy.context.view_layer.update()

    error_maximo = 0.0
    peor_hueso = None
    for hueso_pose in armadura.pose.bones:
        padre = hueso_pose.parent
        if padre is None:
            continue
        # Transformacion local del hijo respecto del padre en pose de reposo.
        reposo_local = padre.bone.matrix_local.inverted() @ hueso_pose.bone.matrix_local
        reconstruida = padre.matrix @ reposo_local @ hueso_pose.matrix_basis
        diferencia = max(abs(a - b)
                         for fila_a, fila_b in zip(reconstruida, hueso_pose.matrix)
                         for a, b in zip(fila_a, fila_b))
        if diferencia > error_maximo:
            error_maximo, peor_hueso = diferencia, hueso_pose.name
    return error_maximo, peor_hueso


def profundidad(nodo):
    """Numero de niveles del arbol contando la raiz como nivel 1."""
    return 1 + max([profundidad(h) for h in nodo["hijos"]], default=0)


def medir_desplazamiento_raiz(armadura, nombre_raiz):
    """Mide cuanto viaja el nodo raiz a lo largo de toda la animacion.

    Es la medicion que distingue una exportacion con movimiento de raiz de una
    exportacion 'In Place': en la primera la cadera acumula traslacion y arrastra
    con ella a todos los descendientes; en la segunda la cadera se queda sobre el
    origen y solo giran las articulaciones.
    """
    escena = bpy.context.scene
    hueso = armadura.pose.bones.get(nombre_raiz)
    if hueso is None:
        return {"recorrido_total": 0.0, "rango_x": 0.0, "rango_y": 0.0, "rango_z": 0.0}

    posiciones = []
    for frame in range(int(escena.frame_start), int(escena.frame_end) + 1):
        escena.frame_set(frame)
        bpy.context.view_layer.update()
        posiciones.append((armadura.matrix_world @ hueso.matrix).to_translation())

    recorrido = sum((b - a).length for a, b in zip(posiciones, posiciones[1:]))
    ejes = list(zip(*[(p.x, p.y, p.z) for p in posiciones]))
    return {
        "recorrido_total": round(recorrido, 5),
        "rango_x": round(max(ejes[0]) - min(ejes[0]), 5),
        "rango_y": round(max(ejes[1]) - min(ejes[1]), 5),
        "rango_z": round(max(ejes[2]) - min(ejes[2]), 5),
    }


def main():
    if "--" not in sys.argv:
        print("Uso: blender -b -P analisis_jerarquia.py -- <fbx> <salida>")
        return 1
    argumentos = sys.argv[sys.argv.index("--") + 1:]
    ruta_fbx = argumentos[0]
    carpeta = argumentos[1] if len(argumentos) > 1 else os.path.dirname(ruta_fbx)
    os.makedirs(carpeta, exist_ok=True)

    armadura = importar_fbx(ruta_fbx)
    datos_armadura = armadura.data
    raices = [h for h in datos_armadura.bones if h.parent is None]
    if len(raices) != 1:
        print("AVISO: la armadura tiene %d nodos raiz." % len(raices))
    arbol = construir_arbol(raices[0])

    # --- 1. Arbol en texto -------------------------------------------------
    lineas = volcar_texto(arbol, [])
    ruta_txt = os.path.join(carpeta, "jerarquia.txt")
    with open(ruta_txt, "w", encoding="utf-8") as f:
        f.write("\n".join(lineas) + "\n")

    # --- 2. Volcado JSON ---------------------------------------------------
    malla = next((o for o in bpy.data.objects if o.type == 'MESH'), None)
    resumen = {
        "archivo": os.path.basename(ruta_fbx),
        "armadura": armadura.name,
        "malla": malla.name if malla else None,
        "vertices": len(malla.data.vertices) if malla else 0,
        "grupos_de_vertices": len(malla.vertex_groups) if malla else 0,
        "total_huesos": len(datos_armadura.bones),
        "nodo_raiz": arbol["nombre"],
        "acciones": [{"nombre": a.name,
                      "frame_inicial": int(a.frame_range[0]),
                      "frame_final": int(a.frame_range[1])}
                     for a in bpy.data.actions],
        "arbol": arbol,
    }
    with open(os.path.join(carpeta, "jerarquia.json"), "w", encoding="utf-8") as f:
        json.dump(resumen, f, indent=2, ensure_ascii=False)

    # --- 3. Diagramas para el reporte --------------------------------------
    # Vista global: el esqueleto completo con las falanges condensadas.
    escribir_diagrama(carpeta, "diagrama_jerarquia_global.tex", podar_dedos(arbol))
    # Vista de detalle: una sola mano, donde se ve la profundidad real.
    mano = buscar(arbol, "mixamorig:LeftHand") or buscar(arbol, "mixamorig:RightHand")
    if mano:
        escribir_diagrama(carpeta, "diagrama_jerarquia_mano.tex", mano,
                          abreviar=mano["nombre"].replace("mixamorig:", ""))

    # --- 4. Verificacion de la cadena de transformaciones ------------------
    error, hueso = verificar_cinematica_directa(armadura)
    recorrido = medir_desplazamiento_raiz(armadura, arbol["nombre"])

    resumen["error_cinematica_directa"] = error
    resumen["desplazamiento_raiz"] = recorrido
    with open(os.path.join(carpeta, "jerarquia.json"), "w", encoding="utf-8") as f:
        json.dump(resumen, f, indent=2, ensure_ascii=False)

    print("=" * 66)
    print("Archivo            :", resumen["archivo"])
    print("Malla / vertices   : %s / %d" % (resumen["malla"], resumen["vertices"]))
    print("Grupos de vertices : %d" % resumen["grupos_de_vertices"])
    print("Huesos totales     : %d" % resumen["total_huesos"])
    print("Nodo raiz          :", resumen["nodo_raiz"])
    print("Profundidad maxima : %d niveles" % profundidad(arbol))
    for accion in resumen["acciones"]:
        print("Accion             : %s  (frames %d-%d)"
              % (accion["nombre"], accion["frame_inicial"], accion["frame_final"]))
    print("-" * 66)
    print("Verificacion M_global = M_global(padre) * M_local")
    print("Error maximo       : %.3e   (hueso: %s)" % (error, hueso))
    print("-" * 66)
    print("Desplazamiento del nodo raiz a lo largo de la animacion")
    print("  Recorrido total  : %.4f unidades" % recorrido["recorrido_total"])
    print("  Rango X / Y / Z  : %.4f / %.4f / %.4f"
          % (recorrido["rango_x"], recorrido["rango_y"], recorrido["rango_z"]))
    print("=" * 66)
    print("\n".join(lineas))
    return 0


if __name__ == "__main__":
    sys.exit(main())
