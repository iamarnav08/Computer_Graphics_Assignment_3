#version 330 core

layout(triangles) in;
layout(triangle_strip, max_vertices = 32) out; // Can output up to 12 vertices (4 triangles)

// Input from the vertex shader
in vec3 Position_gs[];
in vec3 Normal_gs[];
in vec3 Color_gs[];

// Output to fragment shader
out vec3 FragPos;
out vec3 Normal_vs;
out vec3 Color_vs;
out float Depth;

// Uniforms
uniform mat4 gWorld;
uniform mat4 gView;
uniform mat4 gProjection;
uniform bool planeSlicingEnabled;

// Define up to 4 planes
uniform bool plane1_enabled;
uniform bool plane2_enabled;
uniform bool plane3_enabled;
uniform bool plane4_enabled;
uniform vec4 plane1_equation; // (a, b, c, d) where ax + by + cz + d = 0
uniform vec4 plane2_equation;
uniform vec4 plane3_equation;
uniform vec4 plane4_equation;

vec3 highlightColor = vec3(1.0, 1.0, 1.0); // White highlight
float highlightIntensity = 0.5;

#define PLANE1_INDEX 0
#define PLANE2_INDEX 1
#define PLANE3_INDEX 2
#define PLANE4_INDEX 3

// Segment colors - same as in mesh_slicer.h
const vec3 SEGMENT_COLORS[8] = vec3[8](
    vec3(1.0, 0.0, 0.0),   // Red
    vec3(0.0, 1.0, 0.0),   // Green
    vec3(0.0, 0.0, 1.0),   // Blue
    vec3(1.0, 1.0, 0.0),   // Yellow
    vec3(1.0, 0.0, 1.0),   // Magenta
    vec3(0.0, 1.0, 1.0),   // Cyan
    vec3(1.0, 0.5, 0.0),   // Orange
    vec3(0.5, 0.0, 1.0)    // Purple
);

// Function to determine if a vertex is on the positive side of a plane
// Modify isOnPositiveSide function to use an epsilon value
bool isOnPositiveSide(vec3 point, vec4 plane) {
    float epsilon = 1e-6; // Add a small epsilon value
    float dist = dot(vec3(plane.x, plane.y, plane.z), point) + plane.w;
    return dist >= -epsilon; // Consider points very close to the plane as positive
}

bool isOnPlane(vec3 point, vec4 plane) {
    float epsilon = 1e-6;
    float dist = abs(dot(vec3(plane.x, plane.y, plane.z), point) + plane.w);
    return dist < epsilon;
}

// Ensure proper handling of degenerate cases in calculateIntersection
vec3 calculateIntersection(vec3 v1, vec3 v2, vec4 plane) {
    float d1 = dot(vec3(plane.x, plane.y, plane.z), v1) + plane.w;
    float d2 = dot(vec3(plane.x, plane.y, plane.z), v2) + plane.w;
    
    // Handle points very close to the plane
    if (abs(d1) < 1e-6) return v1;
    if (abs(d2) < 1e-6) return v2;
    if (abs(d1 - d2) < 1e-6) return (v1 + v2) * 0.5;
    
    float t = d1 / (d1 - d2);
    t = clamp(t, 0.0, 1.0); // Ensure we stay within the edge
    
    return mix(v1, v2, t);
}

// Function to interpolate vertex attributes
vec3 interpolateAttribute(vec3 attr1, vec3 attr2, vec3 p1, vec3 p2, vec3 intersection) {
    float d1 = distance(p1, intersection);
    float d2 = distance(p2, intersection);
    float total = d1 + d2;
    
    if (total < 0.0001) return attr1; // Avoid division by zero
    
    return (attr1 * d2 + attr2 * d1) / total;
}

// Get color for a region based on region code
vec3 getRegionColor(int regionCode) {
    return SEGMENT_COLORS[regionCode % 8];
}

void main() {
    if (!planeSlicingEnabled) {
        // Just pass through the triangle if no slicing
        for (int i = 0; i < 3; i++) {
            gl_Position = gProjection * gView * gWorld * gl_in[i].gl_Position;
            FragPos = (gView * gWorld * gl_in[i].gl_Position).xyz;
            Normal_vs = normalize(mat3(gView * gWorld) * Normal_gs[i]);
            Color_vs = Color_gs[i];
            
            // Calculate depth
            float distance = length(FragPos);
            float zNear = 1.0;
            float zFar = 30.0;
            Depth = clamp((distance - zNear) / (zFar - zNear), 0.0, 1.0);
            
            EmitVertex();
        }
        EndPrimitive();
        return;
    }
    
    // Extract vertex data
    vec3 positions[3], normals[3], colors[3];
    for (int i = 0; i < 3; i++) {
        positions[i] = gl_in[i].gl_Position.xyz;
        normals[i] = Normal_gs[i];
        colors[i] = Color_gs[i];
    }
    
    // Create flattened arrays with a fixed size
    const int MAX_TRIANGLES = 80;
    vec3 triPositions[240]; // MAX_TRIANGLES * 3
    vec3 triNormals[240];   // MAX_TRIANGLES * 3
    vec3 triColors[240];    // MAX_TRIANGLES * 3
    int triRegionCodes[80]; // Region codes for each triangle
    
    int triangleCount = 1;
    
    // Initialize with the original triangle
    for (int i = 0; i < 3; i++) {
        triPositions[i] = positions[i];
        triNormals[i] = normals[i];
        triColors[i] = colors[i];
    }
    triRegionCodes[0] = 0; // Start with region code 0
    
    // Process each enabled plane
    if (plane1_enabled) {
        int planeIndex = PLANE1_INDEX;
        int newCount = 0;
        
        // Process each existing triangle against this plane
        for (int t = 0; t < triangleCount; t++) {
            // Check which vertices are on positive side
            bool pos_side[3];
            for (int i = 0; i < 3; i++) {
                pos_side[i] = isOnPositiveSide(triPositions[t*3+i], plane1_equation);
            }
            
            int countPositive = int(pos_side[0]) + int(pos_side[1]) + int(pos_side[2]);
            
            if (countPositive == 3) {
                // All vertices on positive side - keep triangle with region code for positive side
                if (newCount < MAX_TRIANGLES) {
                    for (int i = 0; i < 3; i++) {
                        triPositions[newCount*3+i] = triPositions[t*3+i];
                        triNormals[newCount*3+i] = triNormals[t*3+i];
                        triColors[newCount*3+i] = triColors[t*3+i];
                    }
                    // Set bit 0 to 1 for positive side
                    triRegionCodes[newCount] = triRegionCodes[t] | (1 << 0);
                    newCount++;
                }
            }
            else if (countPositive == 0) {
                // All vertices on negative side - keep triangle with region code for negative side
                if (newCount < MAX_TRIANGLES) {
                    for (int i = 0; i < 3; i++) {
                        triPositions[newCount*3+i] = triPositions[t*3+i];
                        triNormals[newCount*3+i] = triNormals[t*3+i];
                        triColors[newCount*3+i] = triColors[t*3+i];
                    }
                    // Keep bit 0 as 0 for negative side
                    triRegionCodes[newCount] = triRegionCodes[t];
                    newCount++;
                }
            }
            else if (countPositive == 1) {
                // One vertex on positive side, two on negative side
                int insideIndices[3], outsideIndices[3];
                int insideCount = 0, outsideCount = 0;
                
                for (int i = 0; i < 3; i++) {
                    if (pos_side[i]) {
                        insideIndices[insideCount++] = i;
                    } else {
                        outsideIndices[outsideCount++] = i;
                    }
                }
                
                int inIdx = insideIndices[0];
                int outIdx1 = outsideIndices[0];
                int outIdx2 = outsideIndices[1];
                
                // Calculate intersection points
                vec3 int1Pos = calculateIntersection(
                    triPositions[t*3+inIdx], triPositions[t*3+outIdx1], plane1_equation);
                vec3 int1Norm = interpolateAttribute(
                    triNormals[t*3+inIdx], triNormals[t*3+outIdx1], 
                    triPositions[t*3+inIdx], triPositions[t*3+outIdx1], int1Pos);
                vec3 int1Color = interpolateAttribute(
                    triColors[t*3+inIdx], triColors[t*3+outIdx1],
                    triPositions[t*3+inIdx], triPositions[t*3+outIdx1], int1Pos);
                
                vec3 int2Pos = calculateIntersection(
                    triPositions[t*3+inIdx], triPositions[t*3+outIdx2], plane1_equation);
                vec3 int2Norm = interpolateAttribute(
                    triNormals[t*3+inIdx], triNormals[t*3+outIdx2], 
                    triPositions[t*3+inIdx], triPositions[t*3+outIdx2], int2Pos);
                vec3 int2Color = interpolateAttribute(
                    triColors[t*3+inIdx], triColors[t*3+outIdx2],
                    triPositions[t*3+inIdx], triPositions[t*3+outIdx2], int2Pos);

                int1Color = mix(int1Color, highlightColor, highlightIntensity);
                int2Color = mix(int2Color, highlightColor, highlightIntensity);
                
                // Create triangle on positive side (set bit 0)
                if (newCount < MAX_TRIANGLES) {
                    triPositions[newCount*3+0] = triPositions[t*3+inIdx];
                    triPositions[newCount*3+1] = int1Pos;
                    triPositions[newCount*3+2] = int2Pos;
                    
                    triNormals[newCount*3+0] = triNormals[t*3+inIdx];
                    triNormals[newCount*3+1] = int1Norm;
                    triNormals[newCount*3+2] = int2Norm;
                    
                    triColors[newCount*3+0] = triColors[t*3+inIdx];
                    triColors[newCount*3+1] = int1Color;
                    triColors[newCount*3+2] = int2Color;
                    
                    triRegionCodes[newCount] = triRegionCodes[t] | (1 << 0); // Positive side
                    newCount++;
                }
                
                // Create triangles on negative side (keep bit 0 as 0)
                if (newCount < MAX_TRIANGLES) {
                    triPositions[newCount*3+0] = int1Pos;
                    triPositions[newCount*3+1] = triPositions[t*3+outIdx1];
                    triPositions[newCount*3+2] = triPositions[t*3+outIdx2];
                    
                    triNormals[newCount*3+0] = int1Norm;
                    triNormals[newCount*3+1] = triNormals[t*3+outIdx1];
                    triNormals[newCount*3+2] = triNormals[t*3+outIdx2];
                    
                    triColors[newCount*3+0] = int1Color;
                    triColors[newCount*3+1] = triColors[t*3+outIdx1];
                    triColors[newCount*3+2] = triColors[t*3+outIdx2];
                    
                    triRegionCodes[newCount] = triRegionCodes[t]; // Negative side
                    newCount++;
                }
                
                if (newCount < MAX_TRIANGLES) {
                    triPositions[newCount*3+0] = int1Pos;
                    triPositions[newCount*3+1] = triPositions[t*3+outIdx2];
                    triPositions[newCount*3+2] = int2Pos;
                    
                    triNormals[newCount*3+0] = int1Norm;
                    triNormals[newCount*3+1] = triNormals[t*3+outIdx2];
                    triNormals[newCount*3+2] = int2Norm;
                    
                    triColors[newCount*3+0] = int1Color;
                    triColors[newCount*3+1] = triColors[t*3+outIdx2];
                    triColors[newCount*3+2] = int2Color;
                    
                    triRegionCodes[newCount] = triRegionCodes[t]; // Negative side
                    newCount++;
                }
            }
            else if (countPositive == 2) {
                // Identify indices of vertices
                int insideIndices[3], outsideIndices[3];
                int insideCount = 0, outsideCount = 0;

                for (int i = 0; i < 3; i++) {
                    if (pos_side[i]) {
                        insideIndices[insideCount++] = i;
                    } else {
                        outsideIndices[outsideCount++] = i;
                    }
                }

                int outIdx = outsideIndices[0];
                int inIdx1 = insideIndices[0];
                int inIdx2 = insideIndices[1];

                // Calculate intersection points
                vec3 int1Pos = calculateIntersection(
                    triPositions[t*3+outIdx], triPositions[t*3+inIdx1], plane1_equation);
                vec3 int1Norm = interpolateAttribute(
                    triNormals[t*3+outIdx], triNormals[t*3+inIdx1],
                    triPositions[t*3+outIdx], triPositions[t*3+inIdx1], int1Pos);
                vec3 int1Color = interpolateAttribute(
                    triColors[t*3+outIdx], triColors[t*3+inIdx1],
                    triPositions[t*3+outIdx], triPositions[t*3+inIdx1], int1Pos);

                vec3 int2Pos = calculateIntersection(
                    triPositions[t*3+outIdx], triPositions[t*3+inIdx2], plane1_equation);
                vec3 int2Norm = interpolateAttribute(
                    triNormals[t*3+outIdx], triNormals[t*3+inIdx2],
                    triPositions[t*3+outIdx], triPositions[t*3+inIdx2], int2Pos);
                vec3 int2Color = interpolateAttribute(
                    triColors[t*3+outIdx], triColors[t*3+inIdx2],
                    triPositions[t*3+outIdx], triPositions[t*3+inIdx2], int2Pos);

                // Add highlight to intersection edges
                int1Color = mix(int1Color, highlightColor, highlightIntensity);
                int2Color = mix(int2Color, highlightColor, highlightIntensity);

                // Create one triangle on the negative side
                if (newCount < MAX_TRIANGLES) {
                    triPositions[newCount*3+0] = triPositions[t*3+outIdx];
                    triPositions[newCount*3+1] = int1Pos;
                    triPositions[newCount*3+2] = int2Pos;

                    triNormals[newCount*3+0] = triNormals[t*3+outIdx];
                    triNormals[newCount*3+1] = int1Norm;
                    triNormals[newCount*3+2] = int2Norm;

                    triColors[newCount*3+0] = triColors[t*3+outIdx];
                    triColors[newCount*3+1] = int1Color;
                    triColors[newCount*3+2] = int2Color;

                    triRegionCodes[newCount] = triRegionCodes[t]; // Negative side
                    newCount++;
                }

                // Create two triangles on the positive side
                if (newCount < MAX_TRIANGLES) {
                    // First triangle on positive side
                    triPositions[newCount*3+0] = int1Pos;
                    triPositions[newCount*3+1] = triPositions[t*3+inIdx1];
                    triPositions[newCount*3+2] = triPositions[t*3+inIdx2];

                    triNormals[newCount*3+0] = int1Norm;
                    triNormals[newCount*3+1] = triNormals[t*3+inIdx1];
                    triNormals[newCount*3+2] = triNormals[t*3+inIdx2];

                    triColors[newCount*3+0] = int1Color;
                    triColors[newCount*3+1] = triColors[t*3+inIdx1];
                    triColors[newCount*3+2] = triColors[t*3+inIdx2];

                    triRegionCodes[newCount] = triRegionCodes[t] | (1 << planeIndex); // Positive side
                    newCount++;
                }

                if (newCount < MAX_TRIANGLES) {
                    // Second triangle on positive side
                    triPositions[newCount*3+0] = int2Pos;
                    triPositions[newCount*3+1] = int1Pos;
                    triPositions[newCount*3+2] = triPositions[t*3+inIdx2];

                    triNormals[newCount*3+0] = int2Norm;
                    triNormals[newCount*3+1] = int1Norm;
                    triNormals[newCount*3+2] = triNormals[t*3+inIdx2];

                    triColors[newCount*3+0] = int2Color;
                    triColors[newCount*3+1] = int1Color;
                    triColors[newCount*3+2] = triColors[t*3+inIdx2];

                    triRegionCodes[newCount] = triRegionCodes[t] | (1 << planeIndex); // Positive side
                    newCount++;
                }
            }
        }
        
        triangleCount = newCount;
        //printf("After plane processing: %d triangles\n", triangleCount);
    }
    
    // Similar processing for plane2
    if (plane2_enabled) {
        int planeIndex = PLANE2_INDEX;
        int newCount = 0;

        // Process each existing triangle against this plane
        for (int t = 0; t < triangleCount; t++) {
            bool pos_side[3];
            for (int i = 0; i < 3; i++) {
                pos_side[i] = isOnPositiveSide(triPositions[t*3+i], plane2_equation);
            }

            int countPositive = int(pos_side[0]) + int(pos_side[1]) + int(pos_side[2]);

            if (countPositive == 3) {
                if (newCount < MAX_TRIANGLES) {
                    for (int i = 0; i < 3; i++) {
                        triPositions[newCount*3+i] = triPositions[t*3+i];
                        triNormals[newCount*3+i] = triNormals[t*3+i];
                        triColors[newCount*3+i] = triColors[t*3+i];
                    }
                    triRegionCodes[newCount] = triRegionCodes[t] | (1 << 1);
                    newCount++;
                }
            } else if (countPositive == 0) {
                if (newCount < MAX_TRIANGLES) {
                    for (int i = 0; i < 3; i++) {
                        triPositions[newCount*3+i] = triPositions[t*3+i];
                        triNormals[newCount*3+i] = triNormals[t*3+i];
                        triColors[newCount*3+i] = triColors[t*3+i];
                    }
                    triRegionCodes[newCount] = triRegionCodes[t];
                    newCount++;
                }
            } else if (countPositive == 1) {
                int insideIndices[3], outsideIndices[3];
                int insideCount = 0, outsideCount = 0;

                for (int i = 0; i < 3; i++) {
                    if (pos_side[i]) insideIndices[insideCount++] = i;
                    else outsideIndices[outsideCount++] = i;
                }

                int inIdx = insideIndices[0];
                int outIdx1 = outsideIndices[0];
                int outIdx2 = outsideIndices[1];

                vec3 int1Pos = calculateIntersection(
                    triPositions[t*3+inIdx], triPositions[t*3+outIdx1], plane2_equation);
                vec3 int1Norm = interpolateAttribute(
                    triNormals[t*3+inIdx], triNormals[t*3+outIdx1], 
                    triPositions[t*3+inIdx], triPositions[t*3+outIdx1], int1Pos);
                vec3 int1Color = interpolateAttribute(
                    triColors[t*3+inIdx], triColors[t*3+outIdx1],
                    triPositions[t*3+inIdx], triPositions[t*3+outIdx1], int1Pos);

                vec3 int2Pos = calculateIntersection(
                    triPositions[t*3+inIdx], triPositions[t*3+outIdx2], plane2_equation);
                vec3 int2Norm = interpolateAttribute(
                    triNormals[t*3+inIdx], triNormals[t*3+outIdx2], 
                    triPositions[t*3+inIdx], triPositions[t*3+outIdx2], int2Pos);
                vec3 int2Color = interpolateAttribute(
                    triColors[t*3+inIdx], triColors[t*3+outIdx2],
                    triPositions[t*3+inIdx], triPositions[t*3+outIdx2], int2Pos);

                if (newCount < MAX_TRIANGLES) {
                    triPositions[newCount*3+0] = triPositions[t*3+inIdx];
                    triPositions[newCount*3+1] = int1Pos;
                    triPositions[newCount*3+2] = int2Pos;

                    triNormals[newCount*3+0] = triNormals[t*3+inIdx];
                    triNormals[newCount*3+1] = int1Norm;
                    triNormals[newCount*3+2] = int2Norm;

                    triColors[newCount*3+0] = triColors[t*3+inIdx];
                    triColors[newCount*3+1] = int1Color;
                    triColors[newCount*3+2] = int2Color;

                    triRegionCodes[newCount] = triRegionCodes[t] | (1 << 1);
                    newCount++;
                }

                if (newCount < MAX_TRIANGLES) {
                    triPositions[newCount*3+0] = int1Pos;
                    triPositions[newCount*3+1] = triPositions[t*3+outIdx1];
                    triPositions[newCount*3+2] = triPositions[t*3+outIdx2];

                    triNormals[newCount*3+0] = int1Norm;
                    triNormals[newCount*3+1] = triNormals[t*3+outIdx1];
                    triNormals[newCount*3+2] = triNormals[t*3+outIdx2];

                    triColors[newCount*3+0] = int1Color;
                    triColors[newCount*3+1] = triColors[t*3+outIdx1];
                    triColors[newCount*3+2] = triColors[t*3+outIdx2];

                    triRegionCodes[newCount] = triRegionCodes[t];
                    newCount++;
                }

                if (newCount < MAX_TRIANGLES) {
                    triPositions[newCount*3+0] = int1Pos;
                    triPositions[newCount*3+1] = triPositions[t*3+outIdx2];
                    triPositions[newCount*3+2] = int2Pos;

                    triNormals[newCount*3+0] = int1Norm;
                    triNormals[newCount*3+1] = triNormals[t*3+outIdx2];
                    triNormals[newCount*3+2] = int2Norm;

                    triColors[newCount*3+0] = int1Color;
                    triColors[newCount*3+1] = triColors[t*3+outIdx2];
                    triColors[newCount*3+2] = int2Color;

                    triRegionCodes[newCount] = triRegionCodes[t];
                    newCount++;
                }
            } else if (countPositive == 2) {
                int insideIndices[3], outsideIndices[3];
                int insideCount = 0, outsideCount = 0;

                for (int i = 0; i < 3; i++) {
                    if (pos_side[i]) insideIndices[insideCount++] = i;
                    else outsideIndices[outsideCount++] = i;
                }

                int outIdx = outsideIndices[0];
                int inIdx1 = insideIndices[0];
                int inIdx2 = insideIndices[1];

                vec3 int1Pos = calculateIntersection(
                    triPositions[t*3+outIdx], triPositions[t*3+inIdx1], plane2_equation);
                vec3 int1Norm = interpolateAttribute(
                    triNormals[t*3+outIdx], triNormals[t*3+inIdx1],
                    triPositions[t*3+outIdx], triPositions[t*3+inIdx1], int1Pos);
                vec3 int1Color = interpolateAttribute(
                    triColors[t*3+outIdx], triColors[t*3+inIdx1],
                    triPositions[t*3+outIdx], triPositions[t*3+inIdx1], int1Pos);

                vec3 int2Pos = calculateIntersection(
                    triPositions[t*3+outIdx], triPositions[t*3+inIdx2], plane2_equation);
                vec3 int2Norm = interpolateAttribute(
                    triNormals[t*3+outIdx], triNormals[t*3+inIdx2],
                    triPositions[t*3+outIdx], triPositions[t*3+inIdx2], int2Pos);
                vec3 int2Color = interpolateAttribute(
                    triColors[t*3+outIdx], triColors[t*3+inIdx2],
                    triPositions[t*3+outIdx], triPositions[t*3+inIdx2], int2Pos);

                if (newCount < MAX_TRIANGLES) {
                    triPositions[newCount*3+0] = triPositions[t*3+outIdx];
                    triPositions[newCount*3+1] = int1Pos;
                    triPositions[newCount*3+2] = int2Pos;

                    triNormals[newCount*3+0] = triNormals[t*3+outIdx];
                    triNormals[newCount*3+1] = int1Norm;
                    triNormals[newCount*3+2] = int2Norm;

                    triColors[newCount*3+0] = triColors[t*3+outIdx];
                    triColors[newCount*3+1] = int1Color;
                    triColors[newCount*3+2] = int2Color;

                    triRegionCodes[newCount] = triRegionCodes[t];
                    newCount++;
                }

                if (newCount < MAX_TRIANGLES) {
                    triPositions[newCount*3+0] = triPositions[t*3+inIdx1];
                    triPositions[newCount*3+1] = triPositions[t*3+inIdx2];
                    triPositions[newCount*3+2] = int1Pos;

                    triNormals[newCount*3+0] = triNormals[t*3+inIdx1];
                    triNormals[newCount*3+1] = triNormals[t*3+inIdx2];
                    triNormals[newCount*3+2] = int1Norm;

                    triColors[newCount*3+0] = triColors[t*3+inIdx1];
                    triColors[newCount*3+1] = triColors[t*3+inIdx2];
                    triColors[newCount*3+2] = int1Color;

                    triRegionCodes[newCount] = triRegionCodes[t] | (1 << 1);
                    newCount++;
                }

                if (newCount < MAX_TRIANGLES) {
                    triPositions[newCount*3+0] = int1Pos;
                    triPositions[newCount*3+1] = triPositions[t*3+inIdx2];
                    triPositions[newCount*3+2] = int2Pos;

                    triNormals[newCount*3+0] = int1Norm;
                    triNormals[newCount*3+1] = triNormals[t*3+inIdx2];
                    triNormals[newCount*3+2] = int2Norm;

                    triColors[newCount*3+0] = int1Color;
                    triColors[newCount*3+1] = triColors[t*3+inIdx2];
                    triColors[newCount*3+2] = int2Color;

                    triRegionCodes[newCount] = triRegionCodes[t] | (1 << 1);
                    newCount++;
                }
            }
        }

        triangleCount = newCount;
    }

    
    // Similar processing for plane3
    if (plane3_enabled) {
        int planeIndex = PLANE3_INDEX;
            int newCount = 0;

            // Process each existing triangle against this plane
            for (int t = 0; t < triangleCount; t++) {
                bool pos_side[3];
                for (int i = 0; i < 3; i++) {
                    pos_side[i] = isOnPositiveSide(triPositions[t*3+i], plane3_equation);
                }

                int countPositive = int(pos_side[0]) + int(pos_side[1]) + int(pos_side[2]);

                if (countPositive == 3) {
                    if (newCount < MAX_TRIANGLES) {
                        for (int i = 0; i < 3; i++) {
                            triPositions[newCount*3+i] = triPositions[t*3+i];
                            triNormals[newCount*3+i] = triNormals[t*3+i];
                            triColors[newCount*3+i] = triColors[t*3+i];
                        }
                        triRegionCodes[newCount] = triRegionCodes[t] | (1 << 2);
                        newCount++;
                    }
                } else if (countPositive == 0) {
                    if (newCount < MAX_TRIANGLES) {
                        for (int i = 0; i < 3; i++) {
                            triPositions[newCount*3+i] = triPositions[t*3+i];
                            triNormals[newCount*3+i] = triNormals[t*3+i];
                            triColors[newCount*3+i] = triColors[t*3+i];
                        }
                        triRegionCodes[newCount] = triRegionCodes[t];
                        newCount++;
                    }
                } else if (countPositive == 1) {
                    int insideIndices[3], outsideIndices[3];
                    int insideCount = 0, outsideCount = 0;

                    for (int i = 0; i < 3; i++) {
                        if (pos_side[i]) insideIndices[insideCount++] = i;
                        else outsideIndices[outsideCount++] = i;
                    }

                    int inIdx = insideIndices[0];
                    int outIdx1 = outsideIndices[0];
                    int outIdx2 = outsideIndices[1];

                    vec3 int1Pos = calculateIntersection(
                        triPositions[t*3+inIdx], triPositions[t*3+outIdx1], plane3_equation);
                    vec3 int1Norm = interpolateAttribute(
                        triNormals[t*3+inIdx], triNormals[t*3+outIdx1], 
                        triPositions[t*3+inIdx], triPositions[t*3+outIdx1], int1Pos);
                    vec3 int1Color = interpolateAttribute(
                        triColors[t*3+inIdx], triColors[t*3+outIdx1],
                        triPositions[t*3+inIdx], triPositions[t*3+outIdx1], int1Pos);

                    vec3 int2Pos = calculateIntersection(
                        triPositions[t*3+inIdx], triPositions[t*3+outIdx2], plane3_equation);
                    vec3 int2Norm = interpolateAttribute(
                        triNormals[t*3+inIdx], triNormals[t*3+outIdx2], 
                        triPositions[t*3+inIdx], triPositions[t*3+outIdx2], int2Pos);
                    vec3 int2Color = interpolateAttribute(
                        triColors[t*3+inIdx], triColors[t*3+outIdx2],
                        triPositions[t*3+inIdx], triPositions[t*3+outIdx2], int2Pos);

                    if (newCount < MAX_TRIANGLES) {
                        triPositions[newCount*3+0] = triPositions[t*3+inIdx];
                        triPositions[newCount*3+1] = int1Pos;
                        triPositions[newCount*3+2] = int2Pos;

                        triNormals[newCount*3+0] = triNormals[t*3+inIdx];
                        triNormals[newCount*3+1] = int1Norm;
                        triNormals[newCount*3+2] = int2Norm;

                        triColors[newCount*3+0] = triColors[t*3+inIdx];
                        triColors[newCount*3+1] = int1Color;
                        triColors[newCount*3+2] = int2Color;

                        triRegionCodes[newCount] = triRegionCodes[t] | (1 << 2);
                        newCount++;
                    }

                    if (newCount < MAX_TRIANGLES) {
                        triPositions[newCount*3+0] = int1Pos;
                        triPositions[newCount*3+1] = triPositions[t*3+outIdx1];
                        triPositions[newCount*3+2] = triPositions[t*3+outIdx2];

                        triNormals[newCount*3+0] = int1Norm;
                        triNormals[newCount*3+1] = triNormals[t*3+outIdx1];
                        triNormals[newCount*3+2] = triNormals[t*3+outIdx2];

                        triColors[newCount*3+0] = int1Color;
                        triColors[newCount*3+1] = triColors[t*3+outIdx1];
                        triColors[newCount*3+2] = triColors[t*3+outIdx2];

                        triRegionCodes[newCount] = triRegionCodes[t];
                        newCount++;
                    }

                    if (newCount < MAX_TRIANGLES) {
                        triPositions[newCount*3+0] = int1Pos;
                        triPositions[newCount*3+1] = triPositions[t*3+outIdx2];
                        triPositions[newCount*3+2] = int2Pos;

                        triNormals[newCount*3+0] = int1Norm;
                        triNormals[newCount*3+1] = triNormals[t*3+outIdx2];
                        triNormals[newCount*3+2] = int2Norm;

                        triColors[newCount*3+0] = int1Color;
                        triColors[newCount*3+1] = triColors[t*3+outIdx2];
                        triColors[newCount*3+2] = int2Color;

                        triRegionCodes[newCount] = triRegionCodes[t];
                        newCount++;
                    }
                } else if (countPositive == 2) {
                    int insideIndices[3], outsideIndices[3];
                    int insideCount = 0, outsideCount = 0;

                    for (int i = 0; i < 3; i++) {
                        if (pos_side[i]) insideIndices[insideCount++] = i;
                        else outsideIndices[outsideCount++] = i;
                    }

                    int outIdx = outsideIndices[0];
                    int inIdx1 = insideIndices[0];
                    int inIdx2 = insideIndices[1];

                    vec3 int1Pos = calculateIntersection(
                        triPositions[t*3+outIdx], triPositions[t*3+inIdx1], plane3_equation);
                    vec3 int1Norm = interpolateAttribute(
                        triNormals[t*3+outIdx], triNormals[t*3+inIdx1],
                        triPositions[t*3+outIdx], triPositions[t*3+inIdx1], int1Pos);
                    vec3 int1Color = interpolateAttribute(
                        triColors[t*3+outIdx], triColors[t*3+inIdx1],
                        triPositions[t*3+outIdx], triPositions[t*3+inIdx1], int1Pos);

                    vec3 int2Pos = calculateIntersection(
                        triPositions[t*3+outIdx], triPositions[t*3+inIdx2], plane3_equation);
                    vec3 int2Norm = interpolateAttribute(
                        triNormals[t*3+outIdx], triNormals[t*3+inIdx2],
                        triPositions[t*3+outIdx], triPositions[t*3+inIdx2], int2Pos);
                    vec3 int2Color = interpolateAttribute(
                        triColors[t*3+outIdx], triColors[t*3+inIdx2],
                        triPositions[t*3+outIdx], triPositions[t*3+inIdx2], int2Pos);

                    if (newCount < MAX_TRIANGLES) {
                        triPositions[newCount*3+0] = triPositions[t*3+outIdx];
                        triPositions[newCount*3+1] = int1Pos;
                        triPositions[newCount*3+2] = int2Pos;

                        triNormals[newCount*3+0] = triNormals[t*3+outIdx];
                        triNormals[newCount*3+1] = int1Norm;
                        triNormals[newCount*3+2] = int2Norm;

                        triColors[newCount*3+0] = triColors[t*3+outIdx];
                        triColors[newCount*3+1] = int1Color;
                        triColors[newCount*3+2] = int2Color;

                        triRegionCodes[newCount] = triRegionCodes[t];
                        newCount++;
                    }

                    if (newCount < MAX_TRIANGLES) {
                        triPositions[newCount*3+0] = triPositions[t*3+inIdx1];
                        triPositions[newCount*3+1] = triPositions[t*3+inIdx2];
                        triPositions[newCount*3+2] = int1Pos;

                        triNormals[newCount*3+0] = triNormals[t*3+inIdx1];
                        triNormals[newCount*3+1] = triNormals[t*3+inIdx2];
                        triNormals[newCount*3+2] = int1Norm;

                        triColors[newCount*3+0] = triColors[t*3+inIdx1];
                        triColors[newCount*3+1] = triColors[t*3+inIdx2];
                        triColors[newCount*3+2] = int1Color;

                        triRegionCodes[newCount] = triRegionCodes[t] | (1 << 2);
                        newCount++;
                    }

                    if (newCount < MAX_TRIANGLES) {
                        triPositions[newCount*3+0] = int1Pos;
                        triPositions[newCount*3+1] = triPositions[t*3+inIdx2];
                        triPositions[newCount*3+2] = int2Pos;

                        triNormals[newCount*3+0] = int1Norm;
                        triNormals[newCount*3+1] = triNormals[t*3+inIdx2];
                        triNormals[newCount*3+2] = int2Norm;

                        triColors[newCount*3+0] = int1Color;
                        triColors[newCount*3+1] = triColors[t*3+inIdx2];
                        triColors[newCount*3+2] = int2Color;

                        triRegionCodes[newCount] = triRegionCodes[t] | (1 << 2);
                        newCount++;
                    }
                }
            }

            triangleCount = newCount;
        }

    
    // Similar processing for plane4
    if (plane4_enabled) {
        int planeIndex = PLANE4_INDEX;
            int newCount = 0;

            // Process each existing triangle against this plane
            for (int t = 0; t < triangleCount; t++) {
                bool pos_side[3];
                for (int i = 0; i < 3; i++) {
                    pos_side[i] = isOnPositiveSide(triPositions[t*3+i], plane4_equation);
                }

                int countPositive = int(pos_side[0]) + int(pos_side[1]) + int(pos_side[2]);

                if (countPositive == 3) {
                    if (newCount < MAX_TRIANGLES) {
                        for (int i = 0; i < 3; i++) {
                            triPositions[newCount*3+i] = triPositions[t*3+i];
                            triNormals[newCount*3+i] = triNormals[t*3+i];
                            triColors[newCount*3+i] = triColors[t*3+i];
                        }
                        triRegionCodes[newCount] = triRegionCodes[t] | (1 << 3);
                        newCount++;
                    }
                } else if (countPositive == 0) {
                    if (newCount < MAX_TRIANGLES) {
                        for (int i = 0; i < 3; i++) {
                            triPositions[newCount*3+i] = triPositions[t*3+i];
                            triNormals[newCount*3+i] = triNormals[t*3+i];
                            triColors[newCount*3+i] = triColors[t*3+i];
                        }
                        triRegionCodes[newCount] = triRegionCodes[t];
                        newCount++;
                    }
                } else if (countPositive == 1) {
                    int insideIndices[3], outsideIndices[3];
                    int insideCount = 0, outsideCount = 0;

                    for (int i = 0; i < 3; i++) {
                        if (pos_side[i]) insideIndices[insideCount++] = i;
                        else outsideIndices[outsideCount++] = i;
                    }

                    int inIdx = insideIndices[0];
                    int outIdx1 = outsideIndices[0];
                    int outIdx2 = outsideIndices[1];

                    vec3 int1Pos = calculateIntersection(
                        triPositions[t*3+inIdx], triPositions[t*3+outIdx1], plane4_equation);
                    vec3 int1Norm = interpolateAttribute(
                        triNormals[t*3+inIdx], triNormals[t*3+outIdx1], 
                        triPositions[t*3+inIdx], triPositions[t*3+outIdx1], int1Pos);
                    vec3 int1Color = interpolateAttribute(
                        triColors[t*3+inIdx], triColors[t*3+outIdx1],
                        triPositions[t*3+inIdx], triPositions[t*3+outIdx1], int1Pos);

                    vec3 int2Pos = calculateIntersection(
                        triPositions[t*3+inIdx], triPositions[t*3+outIdx2], plane4_equation);
                    vec3 int2Norm = interpolateAttribute(
                        triNormals[t*3+inIdx], triNormals[t*3+outIdx2], 
                        triPositions[t*3+inIdx], triPositions[t*3+outIdx2], int2Pos);
                    vec3 int2Color = interpolateAttribute(
                        triColors[t*3+inIdx], triColors[t*3+outIdx2],
                        triPositions[t*3+inIdx], triPositions[t*3+outIdx2], int2Pos);

                    if (newCount < MAX_TRIANGLES) {
                        triPositions[newCount*3+0] = triPositions[t*3+inIdx];
                        triPositions[newCount*3+1] = int1Pos;
                        triPositions[newCount*3+2] = int2Pos;

                        triNormals[newCount*3+0] = triNormals[t*3+inIdx];
                        triNormals[newCount*3+1] = int1Norm;
                        triNormals[newCount*3+2] = int2Norm;

                        triColors[newCount*3+0] = triColors[t*3+inIdx];
                        triColors[newCount*3+1] = int1Color;
                        triColors[newCount*3+2] = int2Color;

                        triRegionCodes[newCount] = triRegionCodes[t] | (1 << 3);
                        newCount++;
                    }

                    if (newCount < MAX_TRIANGLES) {
                        triPositions[newCount*3+0] = int1Pos;
                        triPositions[newCount*3+1] = triPositions[t*3+outIdx1];
                        triPositions[newCount*3+2] = triPositions[t*3+outIdx2];

                        triNormals[newCount*3+0] = int1Norm;
                        triNormals[newCount*3+1] = triNormals[t*3+outIdx1];
                        triNormals[newCount*3+2] = triNormals[t*3+outIdx2];

                        triColors[newCount*3+0] = int1Color;
                        triColors[newCount*3+1] = triColors[t*3+outIdx1];
                        triColors[newCount*3+2] = triColors[t*3+outIdx2];

                        triRegionCodes[newCount] = triRegionCodes[t];
                        newCount++;
                    }

                    if (newCount < MAX_TRIANGLES) {
                        triPositions[newCount*3+0] = int1Pos;
                        triPositions[newCount*3+1] = triPositions[t*3+outIdx2];
                        triPositions[newCount*3+2] = int2Pos;

                        triNormals[newCount*3+0] = int1Norm;
                        triNormals[newCount*3+1] = triNormals[t*3+outIdx2];
                        triNormals[newCount*3+2] = int2Norm;

                        triColors[newCount*3+0] = int1Color;
                        triColors[newCount*3+1] = triColors[t*3+outIdx2];
                        triColors[newCount*3+2] = int2Color;

                        triRegionCodes[newCount] = triRegionCodes[t];
                        newCount++;
                    }
                } else if (countPositive == 2) {
                    int insideIndices[3], outsideIndices[3];
                    int insideCount = 0, outsideCount = 0;

                    for (int i = 0; i < 3; i++) {
                        if (pos_side[i]) insideIndices[insideCount++] = i;
                        else outsideIndices[outsideCount++] = i;
                    }

                    int outIdx = outsideIndices[0];
                    int inIdx1 = insideIndices[0];
                    int inIdx2 = insideIndices[1];

                    vec3 int1Pos = calculateIntersection(
                        triPositions[t*3+outIdx], triPositions[t*3+inIdx1], plane4_equation);
                    vec3 int1Norm = interpolateAttribute(
                        triNormals[t*3+outIdx], triNormals[t*3+inIdx1],
                        triPositions[t*3+outIdx], triPositions[t*3+inIdx1], int1Pos);
                    vec3 int1Color = interpolateAttribute(
                        triColors[t*3+outIdx], triColors[t*3+inIdx1],
                        triPositions[t*3+outIdx], triPositions[t*3+inIdx1], int1Pos);

                    vec3 int2Pos = calculateIntersection(
                        triPositions[t*3+outIdx], triPositions[t*3+inIdx2], plane4_equation);
                    vec3 int2Norm = interpolateAttribute(
                        triNormals[t*3+outIdx], triNormals[t*3+inIdx2],
                        triPositions[t*3+outIdx], triPositions[t*3+inIdx2], int2Pos);
                    vec3 int2Color = interpolateAttribute(
                        triColors[t*3+outIdx], triColors[t*3+inIdx2],
                        triPositions[t*3+outIdx], triPositions[t*3+inIdx2], int2Pos);

                    if (newCount < MAX_TRIANGLES) {
                        triPositions[newCount*3+0] = triPositions[t*3+outIdx];
                        triPositions[newCount*3+1] = int1Pos;
                        triPositions[newCount*3+2] = int2Pos;

                        triNormals[newCount*3+0] = triNormals[t*3+outIdx];
                        triNormals[newCount*3+1] = int1Norm;
                        triNormals[newCount*3+2] = int2Norm;

                        triColors[newCount*3+0] = triColors[t*3+outIdx];
                        triColors[newCount*3+1] = int1Color;
                        triColors[newCount*3+2] = int2Color;

                        triRegionCodes[newCount] = triRegionCodes[t];
                        newCount++;
                    }

                    if (newCount < MAX_TRIANGLES) {
                        triPositions[newCount*3+0] = triPositions[t*3+inIdx1];
                        triPositions[newCount*3+1] = triPositions[t*3+inIdx2];
                        triPositions[newCount*3+2] = int1Pos;

                        triNormals[newCount*3+0] = triNormals[t*3+inIdx1];
                        triNormals[newCount*3+1] = triNormals[t*3+inIdx2];
                        triNormals[newCount*3+2] = int1Norm;

                        triColors[newCount*3+0] = triColors[t*3+inIdx1];
                        triColors[newCount*3+1] = triColors[t*3+inIdx2];
                        triColors[newCount*3+2] = int1Color;

                        triRegionCodes[newCount] = triRegionCodes[t] | (1 << 3);
                        newCount++;
                    }

                    if (newCount < MAX_TRIANGLES) {
                        triPositions[newCount*3+0] = int1Pos;
                        triPositions[newCount*3+1] = triPositions[t*3+inIdx2];
                        triPositions[newCount*3+2] = int2Pos;

                        triNormals[newCount*3+0] = int1Norm;
                        triNormals[newCount*3+1] = triNormals[t*3+inIdx2];
                        triNormals[newCount*3+2] = int2Norm;

                        triColors[newCount*3+0] = int1Color;
                        triColors[newCount*3+1] = triColors[t*3+inIdx2];
                        triColors[newCount*3+2] = int2Color;

                        triRegionCodes[newCount] = triRegionCodes[t] | (1 << 3);
                        newCount++;
                    }
                }
            }

            triangleCount = newCount;
        }

    
    // Finally, emit all triangles with colors based on their region codes
    for (int t = 0; t < triangleCount; t++) {
        // Get color for this region
        vec3 regionColor = getRegionColor(triRegionCodes[t]);
        
        for (int i = 0; i < 3; i++) {
            gl_Position = gProjection * gView * gWorld * vec4(triPositions[t*3+i], 1.0);
            FragPos = (gView * gWorld * vec4(triPositions[t*3+i], 1.0)).xyz;
            Normal_vs = normalize(mat3(gView * gWorld) * triNormals[t*3+i]);
            
            // Use region-based color instead of the original vertex color
            // Color_vs = regionColor;
            Color_vs = mix(triColors[t*3+i], regionColor, 0.7);
            
            // Calculate depth
            float distance = length(FragPos);
            float zNear = 1.0;
            float zFar = 30.0;
            Depth = clamp((distance - zNear) / (zFar - zNear), 0.0, 1.0);
            
            EmitVertex();
        }
        EndPrimitive();
    }
}