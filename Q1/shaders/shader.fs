#version 330 core

struct Light {
    vec3 position;
    vec3 color;
    bool enabled;
    float intensity;
};

const int NUM_LIGHTS = 3;
uniform Light lights[NUM_LIGHTS];
uniform vec3 viewPos;
uniform bool planeSlicingEnabled;

in vec3 FragPos;
in vec3 Normal_vs;
in vec3 Color_vs;
in float Depth;

out vec4 FragColor;

void main() {
    vec3 baseColor = Color_vs;
    
    if (planeSlicingEnabled) {
        baseColor = mix(baseColor, vec3(0.8, 0.4, 0.8), 0.2);
    }
    
    vec3 normal = normalize(Normal_vs);
    
    float ambientStrength = 0.5;
    vec3 ambient = ambientStrength * baseColor;
    
    vec3 lightDir = normalize(vec3(0.0, 1.0, 0.0));
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 diffuse = diff * baseColor * 0.5;
    
    vec3 result = ambient + diffuse;
    
    FragColor = vec4(result, 1.0);
}