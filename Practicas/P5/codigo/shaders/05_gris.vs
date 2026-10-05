#version 330 core
layout (location = 0) in vec3  aPos;
layout (location = 1) in vec3  aNormal;

out vec3 ex_N;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
    gl_Position = projection * view * model * vec4(aPos, 1.0f);
    // La normal se lleva al mundo con la misma rotacion del modelo
    ex_N = mat3(model) * aNormal;
}
