#version 330 core
// Actividad 4: Phong con dos luces + textura + reflejo del cubemap.
// El cristal usa la aproximación de Schlick para mezclar reflexión y refracción.
out vec4 FragColor;

in vec3 Position;
in vec3 Normal;
in vec2 TexCoords;

struct Material {
    vec3  ambient;
    vec3  diffuse;
    vec3  specular;
    float shininess;
};

uniform Material material;
uniform sampler2D texMaterial;   // mármol, madera o metal cepillado
uniform samplerCube cubetex;     // entorno para reflejos
uniform bool  useTexture;
uniform float uvRepeat;
uniform float reflectivity;      // cuánto del entorno se refleja (0 = nada)
uniform bool  isGlass;
uniform float ior;               // índice de refracción del cristal

uniform vec3 lightPos[2];
uniform vec3 lightColor[2];
uniform vec3 eye;

void main()
{
    vec3 N = normalize(Normal);
    vec3 E = normalize(eye - Position);
    vec3 texel = useTexture ? texture(texMaterial, TexCoords * uvRepeat).rgb : vec3(1.0);

    vec3 ambient = material.ambient * texel;
    vec3 diffuse = vec3(0.0);
    vec3 specular = vec3(0.0);
    for (int i = 0; i < 2; ++i) {
        vec3 L = normalize(lightPos[i] - Position);
        vec3 R = reflect(-L, N);
        float lambert = max(dot(N, L), 0.0);
        diffuse  += material.diffuse * texel * lambert * lightColor[i];
        if (lambert > 0.0)
            specular += material.specular * pow(max(dot(R, E), 0.0), material.shininess) * lightColor[i];
    }

    vec3 reflected = texture(cubetex, reflect(-E, N)).rgb;
    vec3 color;
    if (isGlass) {
        float f0 = pow((1.0 - ior) / (1.0 + ior), 2.0);
        float fresnel = f0 + (1.0 - f0) * pow(1.0 - max(dot(N, E), 0.0), 5.0);
        vec3 refracted = texture(cubetex, refract(-E, N, 1.0 / ior)).rgb;
        color = mix(refracted * material.diffuse, reflected, fresnel) + specular;
    } else {
        // Los metales tiñen el reflejo con su color especular.
        color = ambient + diffuse + specular + reflectivity * reflected * material.specular;
    }
    FragColor = vec4(color, 1.0);
}
