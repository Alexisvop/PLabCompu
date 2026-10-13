#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
escuchar_blender.py
Practica 06 - Texturizado

Reabre el .blend de la practica sin rehacer nada y deja a Blender ejecutando
orden_blender.py (junto al .blend) cada vez que aparece, igual que al final de
plano_act1.py.

Uso:
    blender P6_Act1_plano.blend -P escuchar_blender.py
"""

import os
import sys
import traceback

import bpy

AQUI = os.path.dirname(os.path.abspath(__file__))
ASSETS = os.path.dirname(bpy.data.filepath)
ORDENES = os.path.join(ASSETS, "orden_blender.py")


def log(msg):
    print("[P6] " + msg)
    sys.stdout.flush()


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


bpy.app.timers.register(_escuchar, first_interval=1.0, persistent=True)
log("Escuchando %s" % ORDENES)
