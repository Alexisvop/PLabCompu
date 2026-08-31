#ifndef PENTAGON_H
#define PENTAGON_H

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

class Pentagon {
public:
	// filled = true builds the indices for a solid figure, filled = false
	// leaves them empty so that the mesh is drawn as an outline
	Pentagon(float size, bool filled = true) {
		buildVertices(size, filled);
		if (filled)
			buildIndices();
	}

	~Pentagon() {
		vertices.clear();
		indices.clear();
	}

	vector<Vertex>       vertices;
	vector<unsigned int> indices;

private:

	void buildVertices(float size, bool filled) {

		// Requested palette converted to the normalized OpenGL format
		const vec3 palette[5] = {
			vec3(  8.0f, 247.0f, 254.0f) / 255.0f, // #08F7FE
			vec3(  9.0f, 251.0f, 211.0f) / 255.0f, // #09FBD3
			vec3(254.0f,  83.0f, 187.0f) / 255.0f, // #FE53BB
			vec3(245.0f, 211.0f,   0.0f) / 255.0f, // #F5D300
			vec3(113.0f,  34.0f, 250.0f) / 255.0f  // #7122FA
		};

		// Regular pentagon inscribed in a circle of radius size, first vertex
		// at 90 degrees and one vertex each 72 degrees. The outline repeats the
		// first vertex to close the figure
		int count = filled ? 5 : 6;

		for (int i = 0; i < count; i++) {
			Vertex v;
			double rdeg = glm::radians(90.0 + 72.0 * (i % 5));
			v.Position.x = size * (float)cos(rdeg);
			v.Position.y = size * (float)sin(rdeg);
			v.Position.z = 0.0f;
			v.Color.r = palette[i % 5].r;
			v.Color.g = palette[i % 5].g;
			v.Color.b = palette[i % 5].b;
			v.Color.a = 1.0f;
			vertices.push_back(v);
		}
	}

	void buildIndices() {

		// Fan triangulation from the first vertex: (0,1,2), (0,2,3), (0,3,4)
		for (unsigned int i = 1; i + 1 < vertices.size(); i++) {
			indices.push_back(0);
			indices.push_back(i);
			indices.push_back(i + 1);
		}
	}
};

#endif
