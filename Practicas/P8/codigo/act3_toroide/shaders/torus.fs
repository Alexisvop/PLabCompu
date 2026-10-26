#version 330 core
out vec4 FragColor;

in vec3 worldPosition;
in vec3 worldNormal;
in vec2 texCoords;

struct Material {
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
    float shininess;
};

uniform Material material;
uniform sampler2D texture_diffuse1;
uniform vec3 lightPosition;
uniform vec3 lightColor;
uniform vec3 eye;
uniform float uvRepeat;
uniform bool useLighting;

void main()
{
    vec4 texel = texture(texture_diffuse1, texCoords * uvRepeat);
    vec3 color = texel.rgb;
    if (useLighting) {
        vec3 N = normalize(worldNormal);
        vec3 toLight = lightPosition - worldPosition;
        float distanceToLight = length(toLight);
        vec3 L = normalize(toLight);
        vec3 E = normalize(eye - worldPosition);
        float lambert = max(dot(N, L), 0.0);
        float highlight = lambert > 0.0
            ? pow(max(dot(E, reflect(-L, N)), 0.0), material.shininess)
            : 0.0;
        float attenuation = 1.0 / (1.0 + 0.05 * distanceToLight
                                      + 0.01 * distanceToLight * distanceToLight);
        vec3 ambient = material.ambient * texel.rgb;
        vec3 diffuse = material.diffuse * texel.rgb * lambert;
        vec3 specular = material.specular * highlight;
        color = ambient + (diffuse + specular) * lightColor * attenuation;
    }
    // La textura sRGB se decodifica al muestrear; la salida vuelve a sRGB.
    FragColor = vec4(pow(max(color, vec3(0.0)), vec3(1.0 / 2.2)), texel.a);
}
