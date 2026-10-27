#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform mat3 normalMatrix;

out vec3 worldPosition;
out vec3 worldNormal;
out vec2 texCoords;

void main()
{
    vec4 position = model * vec4(aPos, 1.0);
    worldPosition = position.xyz;
    worldNormal = normalMatrix * aNormal;
    texCoords = aTexCoords;
    gl_Position = projection * view * position;
}
