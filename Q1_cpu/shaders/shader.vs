#version 330 core

layout(location = 0) in vec3 Position;
layout(location = 1) in vec3 Color;
layout(location = 2) in vec3 Normal;

uniform mat4 gWorld;
uniform mat4 gProjection;
uniform mat4 gView;  
uniform vec3 viewPos;

out vec3 FragPos;
out vec3 ViewPos;  
out vec3 Normal_vs;
out vec3 Color_vs;
out float Depth;

void main() {
    // Transform vertex to world space
    vec4 worldPos = gWorld * vec4(Position, 1.0);

    // Transform to view space
    vec4 viewPos = gView * worldPos;
    FragPos = viewPos.xyz;  // Now in view space
    
    // Project to clip space
    gl_Position = gProjection * viewPos;
    
    // Transform normal to view space
    Normal_vs = normalize(mat3(gView * gWorld) * Normal);
    Color_vs = Color;
    
    // Calculate depth in view space
    float distance = length(FragPos);  // Distance from camera in view space
    float zNear = 1.0;
    float zFar = 30.0;
    
    Depth = (distance - zNear) / (zFar - zNear);
    Depth = clamp(Depth, 0.0, 1.0);
}