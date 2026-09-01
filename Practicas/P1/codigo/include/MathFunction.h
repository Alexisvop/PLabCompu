#ifndef MFUNCTION_H
#define MFUNCTION_H

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

class MathFunction {
public:
	MathFunction(float size) {
		buildVertices(size);
		buildIndices();
	}

	~MathFunction() {
		vertices.clear();
		indices.clear();
	}

	vector<Vertex>       vertices;
	vector<unsigned int> indices;

private:

	// theta = [-2pi : 2pi] sampled in quarters of a degree
	static const int samplesPerDegree = 4;
	static const int steps = 360 * samplesPerDegree;

	void buildVertices(float size) {

		// One color for each continuous branch of the function
		const vec3 branchColor[3] = {
			vec3(1.0f, 0.0f, 0.0f),
			vec3(0.0f, 1.0f, 0.0f),
			vec3(0.0f, 0.0f, 1.0f)
		};

		for (int i = -steps; i <= steps; i++) {

			double deg  = (double)i / samplesPerDegree;
			float  eval = size * (float)tan(2.0 * glm::radians(deg));

			// tan(2*theta) diverges at theta = pi/4 + k*pi/2, so the samples
			// that fall outside the normalized device coordinates are skipped
			if (eval > 1.0f || eval < -1.0f)
				continue;

			Vertex v;
			v.Position.x = (float)(deg / 360.0);
			v.Position.y = eval;
			v.Position.z = 0.0f;

			int branch = (int)floor((2.0 * deg + 90.0) / 180.0);
			int c = ((branch % 3) + 3) % 3;
			v.Color.r = branchColor[c].r;
			v.Color.g = branchColor[c].g;
			v.Color.b = branchColor[c].b;
			v.Color.a = 1.0f;

			vertices.push_back(v);
		}
	}

	void buildIndices() {
	}
};

#endif
