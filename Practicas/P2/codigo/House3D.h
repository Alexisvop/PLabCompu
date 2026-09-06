#ifndef HOUSE3D_H
#define HOUSE3D_H

#include <glad/glad.h> // holds all OpenGL type declarations

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "Vertex.h"
#include <vector>
using namespace std;
using namespace glm;

// Figura de la Actividad 1: una "casa" (cubo como paredes + piramide de
// base cuadrada como techo), centrada en el origen (0,0,0).
class House3D {
public:
	// size       -> mitad del ancho/profundidad/alto del cubo de paredes (la casa mide 2*size de ancho x 2*size de alto)
	// roofHeight -> altura del techo, medida desde la parte superior del cubo
	House3D(float size, float roofHeight) {
		buildVertices(size, roofHeight);
		buildIndices();
	}

	// Constructor de conveniencia: techo con la misma altura que "size"
	House3D(float size) : House3D(size, size) {}

	~House3D() {
		vertices.clear();
		indices.clear();
	}

	vector<Vertex>       vertices;
	vector<unsigned int> indices;

private:

	void setColor(Vertex& v, float r, float g, float b) {
		v.Color.r = r; v.Color.g = g; v.Color.b = b; v.Color.a = 1.0f;
	}

	void buildVertices(float size, float roofHeight) {

		// Color de las paredes (tono claro tipo estuco)
		float wr = 0.85f, wg = 0.80f, wb = 0.65f;

		Vertex v;

		// --- Cubo (paredes), 8 vertices ---
		// Cara inferior (y = -size)
		v.Position = vec3(-size, -size, -size); setColor(v, wr, wg, wb); vertices.push_back(v); // 0: fondo-izq-abajo
		v.Position = vec3(size, -size, -size); setColor(v, wr, wg, wb); vertices.push_back(v); // 1: fondo-der-abajo
		v.Position = vec3(size, -size, size); setColor(v, wr, wg, wb); vertices.push_back(v); // 2: frente-der-abajo
		v.Position = vec3(-size, -size, size); setColor(v, wr, wg, wb); vertices.push_back(v); // 3: frente-izq-abajo

		// Cara superior (y = size) -> es la base del techo
		v.Position = vec3(-size, size, -size); setColor(v, wr, wg, wb); vertices.push_back(v); // 4: fondo-izq-arriba
		v.Position = vec3(size, size, -size); setColor(v, wr, wg, wb); vertices.push_back(v); // 5: fondo-der-arriba
		v.Position = vec3(size, size, size); setColor(v, wr, wg, wb); vertices.push_back(v); // 6: frente-der-arriba
		v.Position = vec3(-size, size, size); setColor(v, wr, wg, wb); vertices.push_back(v); // 7: frente-izq-arriba

		// --- Piramide (techo), vertice 8: apex, color distinto (tejado) ---
		v.Position = vec3(0.0f, size + roofHeight, 0.0f);
		setColor(v, 0.55f, 0.20f, 0.12f);
		vertices.push_back(v); // 8
	}

	void buildIndices() {

		// --- Piso (cara inferior del cubo) ---
		indices.push_back(0); indices.push_back(1); indices.push_back(2);
		indices.push_back(0); indices.push_back(2); indices.push_back(3);

		// --- Pared trasera (z = -size) ---
		indices.push_back(0); indices.push_back(1); indices.push_back(5);
		indices.push_back(0); indices.push_back(5); indices.push_back(4);

		// --- Pared frontal (z = size) ---
		indices.push_back(3); indices.push_back(2); indices.push_back(6);
		indices.push_back(3); indices.push_back(6); indices.push_back(7);

		// --- Pared izquierda (x = -size) ---
		indices.push_back(0); indices.push_back(3); indices.push_back(7);
		indices.push_back(0); indices.push_back(7); indices.push_back(4);

		// --- Pared derecha (x = size) ---
		indices.push_back(1); indices.push_back(2); indices.push_back(6);
		indices.push_back(1); indices.push_back(6); indices.push_back(5);

		// --- Techo: piramide de base cuadrada (4,5,6,7) con apex en 8 ---
		indices.push_back(4); indices.push_back(5); indices.push_back(8); // cara trasera
		indices.push_back(5); indices.push_back(6); indices.push_back(8); // cara derecha
		indices.push_back(6); indices.push_back(7); indices.push_back(8); // cara frontal
		indices.push_back(7); indices.push_back(4); indices.push_back(8); // cara izquierda
	}
};

#endif
