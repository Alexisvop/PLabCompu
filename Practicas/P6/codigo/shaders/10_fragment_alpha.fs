#version 330 core
out vec4 FragColor;

in vec2 TexCoords;
in vec3 ex_N; 

uniform sampler2D texture_diffuse1;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

// Practica 06: igual que 10_fragment_simple.fs, pero descarta los fragmentos
// transparentes (hoja, reja) para que no escriban profundidad y no tapen lo
// que se dibuje despues.
void main()
{    
    vec4 texel = texture(texture_diffuse1, TexCoords);
    if (texel.a < 0.1)
        discard;
    vec4 ambientColor = vec4(1.0,1.0,1.0,1.0);
    FragColor = ambientColor * texel;
}
