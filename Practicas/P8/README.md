# Plantilla LaTeX para Reportes de Prácticas de Laboratorio
### Laboratorio de Computación Gráfica e Interacción Humano-Computadora
**Facultad de Ingeniería, UNAM**

Esta plantilla en LaTeX adapta fielmente la estructura del documento oficial `PlantillaPracticaLaboratorio.md` y replica con precisión la **Carátula Oficial** institucional (incluyendo los escudos a color de la Facultad de Ingeniería y de la UNAM, las tablas de datos y la Hoja de Evidencias).

---

## 📁 Estructura del Proyecto

```text
PlantillaLabComp/
├── main.tex                  # Archivo principal y configuración de datos de la práctica
├── main.pdf                  # PDF generado tras la compilación
├── Paquetes/
│   └── paquetes.tex          # Carga de paquetes (geometría, tipografía, listings C++, etc.)
├── ImagenesUNAM/             # Logotipos oficiales (color y monocromático)
│   ├── fi_color.png          # Escudo Facultad de Ingeniería (Color oficial)
│   ├── unam_color.png        # Escudo UNAM (Color oficial)
│   ├── FILogo_jpeg.jpeg      # Escudo FI (Monocromático alta resolución)
│   └── unamLogo_png.png      # Escudo UNAM (Monocromático alta resolución)
├── Imagenes/                 # Carpeta para capturas de pantalla, diagramas y figuras del reporte
├── codigo/                   # Carpeta opcional para scripts auxiliares o código fuente
└── secciones/                # Secciones modulares del reporte
    ├── caratula.tex          # Carátula oficial idéntica a la rúbrica/imagen
    ├── 0_ResumenIE.tex       # Resumen (Español e Inglés, 1 párrafo c/u) y Palabras Clave
    ├── 1_Introduccion.tex    # Contexto, conceptos gráficos y citas APA
    ├── 2_Metodologia.tex     # Requerimientos HW/SW, algoritmos y código por actividad
    ├── 3_Experimentos.tex    # Pruebas exitosas, fallidas y variaciones exploradas
    ├── 4_Resultados.tex      # Capturas debidamente tituladas y análisis de ventajas/desventajas
    ├── 5_Conclusiones.tex    # Conclusiones individuales por #CUENTA
    ├── 6_Video.tex           # Enlace al video demostrativo (5 a 10 minutos)
    └── 7_Referencias.tex     # Fuentes de información en formato APA
```

---

## ⚙️ ¿Cómo personalizar la plantilla para cada práctica?

Solo necesitas modificar los datos al inicio del archivo [`main.tex`](file:///home/alexis/Documents/PlantillaLabComp/main.tex):

```latex
% Modifica estos valores:
\newcommand{\numPractica}{01}
\newcommand{\tituloPractica}{Introducción a OpenGL y Primitivas Gráficas}
\newcommand{\grupoLaboratorio}{06}
\newcommand{\nombreProfesor}{Dr. Sergio Teodoro Vite}
\newcommand{\fechaEntrega}{01 de octubre de 2026}
\newcommand{\enlaceVideo}{https://youtu.be/tu-enlace-del-video}

% Lista de números de cuenta para la Hoja de Evidencias:
\newcommand{\cuentasEvidencias}{321150503, 321137135, 424065465, 321258173, 320224159}

% Filas de la tabla de integrantes en la carátula:
\newcommand{\tablaIntegrantes}{%
    321150503 & Pérez Olvera Alexis Abraham & 01 \\ \hline
    321137135 & Basilio Illescas Andrés & 01 \\ \hline
    ...
}
```

> **Nota:** Si deseas que la carátula aparezca con los renglones vacíos para llenarse a mano (idéntica a la imagen en blanco), solo deja las celdas vacías con `& & \\ \hline`.

---

## 🛠️ ¿Cómo compilar el documento?

### 1. Desde la terminal en Linux / macOS:
```bash
cd PlantillaLabComp
pdflatex main.tex
pdflatex main.tex   # Segunda pasada para resolver referencias cruzadas
```

### 2. En Overleaf o TeXstudio / VS Code:
- Sube la carpeta completa `PlantillaLabComp` a tu proyecto.
- Selecciona `main.tex` como archivo raíz (*Main document*).
- El compilador por defecto `pdfLaTeX` compilará sin necesidad de dependencias externas.
