#ifndef CIRCLE_H
#define CIRCLE_H

/*
* Actividad 3 de la Práctica 03.
*
* Versión propia de la clase Circle. Conserva el nombre y la interfaz de la
* clase del proyecto base para no tocar el resto del programa, pero corrige
* tres problemas que detectamos al ejecutar la original:
*
*   1. Sólo se asignaba color a los vértices de 0, 120 y 240 grados; los demás
*      quedaban con el contenido indeterminado de la estructura Vertex.
*   2. El segundo lazo de índices alcanzaba el valor vertices.size(), un índice
*      que no existe en el arreglo.
*   3. Se duplicaba el vértice del centro una vez por cada punto del contorno.
*
* La construcción es un abanico de triángulos: un único vértice en el centro y
* n vértices sobre la circunferencia, unidos de dos en dos con el centro.
*/

#include <glad/glad.h> // holds all OpenGL type declarations

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "Vertex.h"
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <vector>
using namespace std;
using namespace glm;

class Circle {
public:
	// radius: radio de la circunferencia.
	// slices: número de lados del polígono con el que se aproxima.
	Circle(float radius, int slices = 72) {
		if (slices < 3) slices = 3;
		this->slices = slices;
		buildVertices(radius);
		buildIndices();
	}

	~Circle() {
		vertices.clear();
		indices.clear();
	}

	vector<Vertex>       vertices;
	vector<unsigned int> indices;

private:
	int slices;

	// Color del contorno. Se conservan los tres tonos del proyecto base
	// (cian en 0 grados, magenta en 120 y azul en 240) pero ahora se
	// interpolan para que todos los vértices tengan un valor definido.
	vec4 colorContorno(float grados) {
		const vec3 anclas[3] = {
			vec3(0.0f, 1.0f, 1.0f),   //   0 grados
			vec3(1.0f, 0.0f, 1.0f),   // 120 grados
			vec3(0.0f, 0.0f, 1.0f)    // 240 grados
		};

		float t = grados / 120.0f;      // sector de 120 grados
		int   i = (int)t % 3;           // ancla inicial
		float f = t - (float)((int)t);  // avance dentro del sector

		vec3 c = anclas[i] * (1.0f - f) + anclas[(i + 1) % 3] * f;
		return vec4(c, 1.0f);
	}

	void buildVertices(float radius) {
		Vertex v;

		// Vértice 0: centro del círculo
		v.Position = vec3(0.0f, 0.0f, 0.0f);
		v.Color    = vec4(0.20f, 0.20f, 0.25f, 1.0f);
		vertices.push_back(v);

		// Vértices 1..slices: contorno sobre el plano XY
		float paso = 360.0f / (float)slices;
		for (int i = 0; i < slices; i++) {
			float grados = paso * (float)i;
			float rad    = radians(grados);

			v.Position = vec3(radius * cos(rad), radius * sin(rad), 0.0f);
			v.Color    = colorContorno(grados);
			vertices.push_back(v);
		}
	}

	void buildIndices() {
		// Un triángulo por cada lado del polígono. El operador módulo cierra
		// el círculo uniendo el último vértice del contorno con el primero.
		for (int i = 0; i < slices; i++) {
			indices.push_back(0);
			indices.push_back(1 + i);
			indices.push_back(1 + (i + 1) % slices);
		}
	}
};

#endif
