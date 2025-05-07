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

in vec3 FragPos;
in vec3 Normal_vs;
in vec3 Color_vs;
in float Depth;

out vec4 FragColor;

void main() {
    // Get the color from vertex attribute - this will be the segment color
    vec3 baseColor = Color_vs;
    
    // Normalize the normal - ensure it's not zero
    vec3 normal = normalize(Normal_vs);
    if (length(normal) < 0.001) {
        normal = vec3(0.0, 0.0, 1.0); // Default normal if invalid
    }
    
    // Simple ambient and diffuse lighting to show shape
    float ambientStrength = 0.5;
    vec3 ambient = ambientStrength * baseColor;
    
    // Apply simple directional light from above
    vec3 lightDir = normalize(vec3(0.0, 1.0, 0.0));
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 diffuse = diff * baseColor * 0.5;
    
    // Final color is just ambient + diffuse
    vec3 result = ambient + diffuse;
    
    // Output color with full opacity
    FragColor = vec4(result, 1.0);
}