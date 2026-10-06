#version 330 core
out vec4 FragColor;

in vec3 ex_N;

// La ciudad no trae textura: gris con una luz direccional para distinguir
// las caras (sin luz todo el modelo se veria como una silueta plana)
void main()
{
    vec3 luz = normalize(vec3(0.4, 1.0, 0.6));
    float difusa = max(dot(normalize(ex_N), luz), 0.0);
    float gris = 0.35 + 0.55 * difusa;
    FragColor = vec4(vec3(gris), 1.0);
}
