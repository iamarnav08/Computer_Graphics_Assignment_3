#version 330 core

layout(location = 0) in vec3 Position;
layout(location = 1) in vec3 Color;
layout(location = 2) in vec3 Normal;

uniform mat4 gWorld;
uniform mat4 gProjection;
uniform mat4 gView;  
uniform vec3 viewPos;

// Output to geometry shader
out vec3 Position_gs;
out vec3 Normal_gs;
out vec3 Color_gs;

void main() {
    // Transform vertex to world space
    gl_Position = vec4(Position, 1.0);
    Position_gs = Position;
    Normal_gs = Normal;
    Color_gs = Color;
}