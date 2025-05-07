#ifndef MESH_SLICER_H
#define MESH_SLICER_H

#include "math_utils.h"
#include "OFFReader.h"
#include "file_utils.h"
#include "plane.h"
#include <vector>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <cmath>

const Vector3f SEGMENT_COLORS[8] = {
    {1.0f, 0.0f, 0.0f},   // Red
    {0.0f, 1.0f, 0.0f},   // Green
    {0.0f, 0.0f, 1.0f},   // Blue
    {1.0f, 1.0f, 0.0f},   // Yellow
    {1.0f, 0.0f, 1.0f},   // Magenta
    {0.0f, 1.0f, 1.0f},   // Cyan
    {1.0f, 0.5f, 0.0f},   // Orange
    {0.5f, 0.0f, 1.0f}    // Purple
};

struct EdgeIntersection {
    Vector3f position;    
    Vector3f normal;      
    float r, g, b;        
    int edgeIndex;        
    int triangleIndex;    
    float t;             
};

struct SlicedVertex {
    Vector3f position;
    Vector3f normal;
    float r, g, b;
    
    bool operator==(const SlicedVertex& other) const {
        const float EPSILON = 0.0001f;
        return (fabs(position.x - other.position.x) < EPSILON &&
                fabs(position.y - other.position.y) < EPSILON &&
                fabs(position.z - other.position.z) < EPSILON);
    }
};

struct SlicedVertexHash {
    std::size_t operator()(const SlicedVertex& v) const {
        return std::hash<float>()(v.position.x) ^ 
               std::hash<float>()(v.position.y) ^ 
               std::hash<float>()(v.position.z);
    }
};

struct MeshSegment {
    std::vector<SlicedVertex> vertices;
    std::vector<unsigned int> indices;
    Vector3f segmentColor;  
    std::vector<bool> regionCode; 
    std::vector<Vector3f> originalPositions;
};

struct MeshSlicerState {
    OffModel* model;
    std::vector<MeshSegment> segments;
};

extern MeshSlicerState g_slicerState;
extern bool g_meshInitialized;

void initMeshSlicer(OffModel* model) {
    g_slicerState.model = model;
    g_slicerState.segments.clear();
    g_meshInitialized = true;
}

void cleanupMeshSlicer() {
    g_slicerState.segments.clear();
    g_meshInitialized = false;
}

void addTriangle(MeshSegment& segment, const SlicedVertex& v1, const SlicedVertex& v2, const SlicedVertex& v3) {
    Vector3f e1 = v2.position - v1.position;
    Vector3f e2 = v3.position - v2.position;
    Vector3f e3 = v1.position - v3.position;
    
    float minLength = 0.00001f;
    if (e1.length() < minLength || e2.length() < minLength || e3.length() < minLength) {
        printf("Warning: Skipped degenerate triangle\n");
        return;
    }
    
    Vector3f normal = e1.Cross(e3 * -1.0f);
    float area = normal.length() * 0.5f;
    
    if (area < 0.00001f) {
        printf("Warning: Skipped zero-area triangle\n");
        return;
    }
    
    unsigned int idx1 = segment.vertices.size();
    unsigned int idx2 = idx1 + 1;
    unsigned int idx3 = idx1 + 2;
    
    segment.vertices.push_back(v1);
    segment.vertices.push_back(v2);
    segment.vertices.push_back(v3);
    
    segment.indices.push_back(idx1);
    segment.indices.push_back(idx2);
    segment.indices.push_back(idx3);
}


SlicedVertex convertVertex(const Vertex& v){
    SlicedVertex sv; sv.position = Vector3f(v.x, v.y, v.z);
    sv.normal = v.normal;
    sv.r = v.r;
    sv.g = v.g;
    sv.b = v.b;
    return sv;
}

bool isOnPositiveSide(const Vector3f& point, const Plane& plane) {
    return plane.evaluate(point) > 0.0f;
}

bool calculateIntersection(const Vertex& v1, const Vertex& v2, const Plane& plane, EdgeIntersection& intersection) {
    
    Vector3f p1(v1.x, v1.y, v1.z);
    Vector3f p2(v2.x, v2.y, v2.z);
    
    float d1 = plane.evaluate(p1);
    float d2 = plane.evaluate(p2);
    
    const float EPSILON = 0.0001f;
    
    if (fabs(d1) < EPSILON) {
        intersection.position = p1;
        intersection.normal = v1.normal;
        intersection.r = v1.r; intersection.g = v1.g; intersection.b = v1.b;
        intersection.t = 0.0f;
        return true;
    }
    
    if (fabs(d2) < EPSILON) {
        intersection.position = p2;
        intersection.normal = v2.normal;
        intersection.r = v2.r; intersection.g = v2.g; intersection.b = v2.b;
        intersection.t = 1.0f;
        return true;
    }
    
    if ((d1 * d2) >= 0.0f) {
        return false;  
    }
    
    float t = d1 / (d1 - d2);
    
    t = std::max(0.0f, std::min(1.0f, t));
    
    intersection.position = p1 + (p2 - p1) * t;
    
    intersection.normal.x = v1.normal.x + t * (v2.normal.x - v1.normal.x);
    intersection.normal.y = v1.normal.y + t * (v2.normal.y - v1.normal.y);
    intersection.normal.z = v1.normal.z + t * (v2.normal.z - v1.normal.z);
    
    intersection.r = v1.r + t * (v2.r - v1.r);
    intersection.g = v1.g + t * (v2.g - v1.g);
    intersection.b = v1.b + t * (v2.b - v1.b);
    
    intersection.t = t;
    
    float magnitude = sqrtf(
        intersection.normal.x * intersection.normal.x + 
        intersection.normal.y * intersection.normal.y + 
        intersection.normal.z * intersection.normal.z);
    
    if (magnitude > 0.0001f) {
        intersection.normal.x /= magnitude;
        intersection.normal.y /= magnitude;
        intersection.normal.z /= magnitude;
    }
    
    return true;
}

MeshSegment createInitialSegment(OffModel* model) {
    MeshSegment segment;
    
    for (int i = 0; i < model->numberOfPolygons; i++) {
        Polygon* poly = &model->polygons[i];
        
        if (poly->noSides == 3) {
            SlicedVertex v1 = convertVertex(model->vertices[poly->v[0]]);
            SlicedVertex v2 = convertVertex(model->vertices[poly->v[1]]);
            SlicedVertex v3 = convertVertex(model->vertices[poly->v[2]]);
            addTriangle(segment, v1, v2, v3);
        } 
        else if (poly->noSides > 3) {
            for (int j = 1; j < poly->noSides - 1; j++) {
                SlicedVertex v1 = convertVertex(model->vertices[poly->v[0]]);
                SlicedVertex v2 = convertVertex(model->vertices[poly->v[j]]);
                SlicedVertex v3 = convertVertex(model->vertices[poly->v[j+1]]);
                addTriangle(segment, v1, v2, v3);
            }
        }
    }
    
    return segment;
}

void assignSegmentColor(MeshSegment& segment, size_t segmentIndex) {
    segment.segmentColor = SEGMENT_COLORS[segmentIndex % 8];
}


void sliceWithPlanes(const std::vector<Plane>& planes) {
    if (!g_meshInitialized) {
        printf("Mesh slicer not initialized!\n");
        return;
    }
    
    if (planes.empty()) {
        g_slicerState.segments.clear();
        MeshSegment segment = createInitialSegment(g_slicerState.model);
        segment.regionCode.clear(); 
        assignSegmentColor(segment, 0); 
        g_slicerState.segments.push_back(segment);
        return;
    }
    
    g_slicerState.segments.clear();
    MeshSegment initialSegment = createInitialSegment(g_slicerState.model);
    initialSegment.regionCode.clear(); 
    g_slicerState.segments.push_back(initialSegment);
    
    for (size_t planeIndex = 0; planeIndex < planes.size(); planeIndex++) {
        const Plane& plane = planes[planeIndex];
        std::vector<MeshSegment> newSegments;
        
        for (size_t segIndex = 0; segIndex < g_slicerState.segments.size(); segIndex++) {
            const MeshSegment& segment = g_slicerState.segments[segIndex];
            
            MeshSegment posSide, negSide;
            
            posSide.regionCode = segment.regionCode;
            negSide.regionCode = segment.regionCode;
            
            posSide.regionCode.push_back(true);  // Positive side gets true
            negSide.regionCode.push_back(false); // Negative side gets false
            
            
        for (size_t i = 0; i < segment.indices.size(); i += 3) {
            SlicedVertex v1 = segment.vertices[segment.indices[i]];
            SlicedVertex v2 = segment.vertices[segment.indices[i+1]];
            SlicedVertex v3 = segment.vertices[segment.indices[i+2]];

            bool isDegenerateTriangle = (
                (v1.position - v2.position).length() < 0.0001f ||
                (v2.position - v3.position).length() < 0.0001f ||
                (v3.position - v1.position).length() < 0.0001f
            );
            
            if (isDegenerateTriangle) {
                printf("Warning: Degenerate triangle detected, skipping.\n");
                continue;
            }
            
            bool v1Pos = isOnPositiveSide(v1.position, plane);
            bool v2Pos = isOnPositiveSide(v2.position, plane);
            bool v3Pos = isOnPositiveSide(v3.position, plane);
            
            if (v1Pos && v2Pos && v3Pos) {
                addTriangle(posSide, v1, v2, v3);
                continue;
            } 
            
            if (!v1Pos && !v2Pos && !v3Pos) {
                addTriangle(negSide, v1, v2, v3);
                continue;
            }
            
            Vertex vertex1, vertex2, vertex3;
            
            vertex1.x = v1.position.x; vertex1.y = v1.position.y; vertex1.z = v1.position.z;
            vertex1.r = v1.r; vertex1.g = v1.g; vertex1.b = v1.b;
            vertex1.normal = v1.normal;
            
            vertex2.x = v2.position.x; vertex2.y = v2.position.y; vertex2.z = v2.position.z;
            vertex2.r = v2.r; vertex2.g = v2.g; vertex2.b = v2.b;
            vertex2.normal = v2.normal;
            
            vertex3.x = v3.position.x; vertex3.y = v3.position.y; vertex3.z = v3.position.z;
            vertex3.r = v3.r; vertex3.g = v3.g; vertex3.b = v3.b;
            vertex3.normal = v3.normal;
            
            EdgeIntersection i12, i23, i31;
            bool has_i12 = calculateIntersection(vertex1, vertex2, plane, i12);
            bool has_i23 = calculateIntersection(vertex2, vertex3, plane, i23);
            bool has_i31 = calculateIntersection(vertex3, vertex1, plane, i31);
            
            SlicedVertex iv12, iv23, iv31;
            if (has_i12) {
                iv12.position = i12.position;
                iv12.normal = i12.normal;
                iv12.r = i12.r; iv12.g = i12.g; iv12.b = i12.b;
            }
            
            if (has_i23) {
                iv23.position = i23.position;
                iv23.normal = i23.normal;
                iv23.r = i23.r; iv23.g = i23.g; iv23.b = i23.b;
            }
            
            if (has_i31) {
                iv31.position = i31.position;
                iv31.normal = i31.normal;
                iv31.r = i31.r; iv31.g = i31.g; iv31.b = i31.b;
            }
            
            
            if (v1Pos && v2Pos && !v3Pos) {
                if (has_i23 && has_i31) {
                    addTriangle(posSide, v1, v2, iv23);
                    addTriangle(posSide, v1, iv23, iv31);
                    addTriangle(negSide, v3, iv31, iv23);
                }
                else {
                    printf("Warning: Edge intersection calculation failed for split triangle\n");
                    addTriangle(posSide, v1, v2, v3);
                }
            } 
            else if (v1Pos && !v2Pos && v3Pos) {
                if (has_i12 && has_i23) {
                    addTriangle(posSide, v1, iv12, v3);
                    addTriangle(posSide, iv12, iv23, v3);
                    addTriangle(negSide, v2, iv23, iv12);
                }
                else {
                    printf("Warning: Edge intersection calculation failed for split triangle\n");
                    addTriangle(posSide, v1, v2, v3);
                }
            } 
            else if (!v1Pos && v2Pos && v3Pos) {
                if (has_i12 && has_i31) {
                    addTriangle(posSide, v2, v3, iv31);
                    addTriangle(posSide, v2, iv31, iv12);
                    addTriangle(negSide, v1, iv12, iv31);
                }
                else {
                    printf("Warning: Edge intersection calculation failed for split triangle\n");
                    addTriangle(posSide, v1, v2, v3);
                }
            } 
            else if (v1Pos && !v2Pos && !v3Pos) {
                if (has_i12 && has_i31) {
                    addTriangle(posSide, v1, iv12, iv31);
                    addTriangle(negSide, iv12, v2, v3);
                    addTriangle(negSide, iv12, v3, iv31);
                }
                else {
                    printf("Warning: Edge intersection calculation failed for split triangle\n");
                    addTriangle(negSide, v1, v2, v3);
                }
            } 
            else if (!v1Pos && v2Pos && !v3Pos) {
                if (has_i12 && has_i23) {
                    addTriangle(posSide, iv12, v2, iv23);
                    addTriangle(negSide, v1, iv12, iv23);
                    addTriangle(negSide, v1, iv23, v3);
                }
                else {
                    printf("Warning: Edge intersection calculation failed for split triangle\n");
                    addTriangle(negSide, v1, v2, v3);
                }
            } 
            else if (!v1Pos && !v2Pos && v3Pos) {
                if (has_i23 && has_i31) {
                    addTriangle(posSide, iv31, iv23, v3);
                    addTriangle(negSide, v1, v2, iv23);
                    addTriangle(negSide, v1, iv23, iv31);
                }
                else {
                    printf("Warning: Edge intersection calculation failed for split triangle\n");
                    addTriangle(negSide, v1, v2, v3);
                }
            }
        }
            if (!posSide.vertices.empty()) {
                assignSegmentColor(posSide, newSegments.size());
                newSegments.push_back(posSide);
            }
            
            if (!negSide.vertices.empty()) {
                assignSegmentColor(negSide, newSegments.size());
                newSegments.push_back(negSide);
            }
        }
        
        g_slicerState.segments = newSegments;
        for (MeshSegment& segment : g_slicerState.segments) {
            segment.originalPositions.clear();
            segment.originalPositions.reserve(segment.vertices.size());
            
            for (const SlicedVertex& v : segment.vertices) {
                segment.originalPositions.push_back(v.position);
            }
        }
    }
    
    printf("Created %zu segments with region codes:\n", g_slicerState.segments.size());
    for (size_t i = 0; i < g_slicerState.segments.size(); i++) {
        printf("Segment %zu: Region code [", i);
        for (bool code : g_slicerState.segments[i].regionCode) {
            printf("%c", code ? '+' : '-');
        }
        printf("], Color (%.1f, %.1f, %.1f), Vertices: %zu, Triangles: %zu\n", 
              g_slicerState.segments[i].segmentColor.x,
              g_slicerState.segments[i].segmentColor.y, 
              g_slicerState.segments[i].segmentColor.z,
              g_slicerState.segments[i].vertices.size(),
              g_slicerState.segments[i].indices.size() / 3);
    }
}

void resetSegmentsToOriginalPositions() {
    for (size_t i = 0; i < g_slicerState.segments.size(); i++) {
        MeshSegment& segment = g_slicerState.segments[i];
        
        if (segment.originalPositions.size() != segment.vertices.size()) {
            printf("Warning: Cannot reset segment positions - original positions not available\n");
            continue;
        }
        
        for (size_t j = 0; j < segment.vertices.size(); j++) {
            segment.vertices[j].position = segment.originalPositions[j];
        }
    }
}

const std::vector<MeshSegment>& getSegments() {
    return g_slicerState.segments;
}

Vector3f calculateCentroid(const MeshSegment& segment) {
    if (segment.vertices.empty()) {
        return Vector3f(0, 0, 0);
    }
    
    Vector3f sum(0, 0, 0);
    for (const SlicedVertex& v : segment.vertices) {
        sum = sum + v.position;
    }
    
    return sum * (1.0f / segment.vertices.size());
}

void calculateSegmentCentroids(std::vector<Vector3f>& centroids) {
    centroids.clear();
    centroids.reserve(g_slicerState.segments.size());
    
    Vector3f modelCentroid(0, 0, 0);
    size_t totalVertices = 0;
    
    for (const MeshSegment& segment : g_slicerState.segments) {
        for (const SlicedVertex& v : segment.vertices) {
            modelCentroid = modelCentroid + v.position;
            totalVertices++;
        }
    }
    
    if (totalVertices > 0) {
        modelCentroid = modelCentroid * (1.0f / totalVertices);
    }
    
    for (const MeshSegment& segment : g_slicerState.segments) {
        centroids.push_back(calculateCentroid(segment));
    }
}

void uploadToGPU(GLuint& vao, GLuint& vbo, GLuint& ibo, int& vertexCount) {
    std::vector<Vertex> allVertices;
    std::vector<unsigned int> allIndices;
    
    unsigned int baseIndex = 0;
    
    printf("Uploading %zu segments to GPU\n", g_slicerState.segments.size());
    
    if (g_slicerState.segments.empty()) {
        printf("WARNING: No segments to upload! Check slicing logic.\n");
        return;
    }
    
    for (size_t segIdx = 0; segIdx < g_slicerState.segments.size(); segIdx++) {
        const MeshSegment& segment = g_slicerState.segments[segIdx];
        const Vector3f& color = segment.segmentColor;
        
        printf("Segment %zu: Color (%.1f, %.1f, %.1f), %zu vertices, %zu triangles\n",
               segIdx, color.x, color.y, color.z,
               segment.vertices.size(), segment.indices.size() / 3);
        
        for (const SlicedVertex& sv : segment.vertices) {
            Vertex v;
            v.x = sv.position.x;
            v.y = sv.position.y;
            v.z = sv.position.z;
            
            v.r = color.x;
            v.g = color.y;
            v.b = color.z;
            
            v.normal = sv.normal;
            allVertices.push_back(v);
        }
        
        for (unsigned int index : segment.indices) {
            allIndices.push_back(baseIndex + index);
        }
        
        baseIndex += segment.vertices.size();
    }
    
    printf("Total: %zu vertices, %zu triangles\n", 
           allVertices.size(), allIndices.size() / 3);
    
    if (allVertices.empty() || allIndices.empty()) {
        printf("WARNING: No vertices or indices to upload. Check slicing logic.\n");
        return;
    }
    
    if (vao == 0) {
        glGenVertexArrays(1, &vao);
    }
    
    glBindVertexArray(vao);
    
    if (vbo == 0) {
        glGenBuffers(1, &vbo);
    }
    
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    GLint bufferSize = 0;
    glBufferData(GL_ARRAY_BUFFER, allVertices.size() * sizeof(Vertex), allVertices.data(), GL_STATIC_DRAW);
    glGetBufferParameteriv(GL_ARRAY_BUFFER, GL_BUFFER_SIZE, &bufferSize);
    printf("VBO size: %d bytes (%zu vertices)\n", bufferSize, bufferSize / sizeof(Vertex));
    
    glEnableVertexAttribArray(0); // Position
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, x));
    
    glEnableVertexAttribArray(1); // Color
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, r));
    
    glEnableVertexAttribArray(2); // Normal
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));
    
    if (ibo == 0) {
        glGenBuffers(1, &ibo);
    }
    
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, allIndices.size() * sizeof(unsigned int), allIndices.data(), GL_STATIC_DRAW);
    glGetBufferParameteriv(GL_ELEMENT_ARRAY_BUFFER, GL_BUFFER_SIZE, &bufferSize);
    printf("IBO size: %d bytes (%zu indices)\n", bufferSize, bufferSize / sizeof(unsigned int));
    
    vertexCount = allIndices.size();
    
    printf("Successfully uploaded %d vertices and %zu indices to GPU\n", 
           vertexCount, allIndices.size());
           
    glBindVertexArray(0);
}

void updateSlicedMeshExplosion(float explosionFactor, OffModel* model) {
    if (g_slicerState.segments.empty()) {
        return;
    }
    
    if (explosionFactor == 0.0f) {
        resetSegmentsToOriginalPositions();
        return;
    }
    
    Vector3f modelCentroid(0, 0, 0);
    size_t totalVertices = 0;
    
    for (size_t segIdx = 0; segIdx < g_slicerState.segments.size(); segIdx++) {
        const MeshSegment& segment = g_slicerState.segments[segIdx];
        for (const SlicedVertex& v : segment.vertices) {
            modelCentroid = modelCentroid + v.position;
            totalVertices++;
        }
    }
    
    if (totalVertices > 0) {
        modelCentroid = modelCentroid * (1.0f / totalVertices);
    }
    
    printf("Applying primitive explosion factor %.2f\n", explosionFactor);
    
    for (size_t segIdx = 0; segIdx < g_slicerState.segments.size(); segIdx++) {
        MeshSegment& segment = g_slicerState.segments[segIdx];
        
        std::vector<Vector3f> currentPositions;
        currentPositions.reserve(segment.vertices.size());
        
        for (const SlicedVertex& v : segment.vertices) {
            currentPositions.push_back(v.position);
        }
        
        for (size_t i = 0; i < segment.indices.size(); i += 3) {
            Vector3f triangleCentroid = (
                currentPositions[segment.indices[i]] + 
                currentPositions[segment.indices[i+1]] + 
                currentPositions[segment.indices[i+2]]
            ) * (1.0f / 3.0f);
            
            Vector3f explosionDir = (triangleCentroid - modelCentroid).Normalize();
            float explosionDistance = explosionFactor * (model->extent / 10.0f);
            Vector3f displacement = explosionDir * explosionDistance;
            
            segment.vertices[segment.indices[i]].position = 
                currentPositions[segment.indices[i]] + displacement;
            
            segment.vertices[segment.indices[i+1]].position = 
                currentPositions[segment.indices[i+1]] + displacement;
            
            segment.vertices[segment.indices[i+2]].position = 
                currentPositions[segment.indices[i+2]] + displacement;
        }
    }
}

void updateSlicedMeshExtremeExplosion(float explosionFactor, OffModel* model) {
    if (g_slicerState.segments.empty()) {
        return;
    }
    
    resetSegmentsToOriginalPositions();
    
    if (explosionFactor == 0.0f) {
        return;
    }
    
    Vector3f modelCentroid(0, 0, 0);
    size_t totalVertices = 0;
    
    for (size_t segIdx = 0; segIdx < g_slicerState.segments.size(); segIdx++) {
        const MeshSegment& segment = g_slicerState.segments[segIdx];
        for (const SlicedVertex& v : segment.vertices) {
            modelCentroid = modelCentroid + v.position;
            totalVertices++;
        }
    }
    
    if (totalVertices > 0) {
        modelCentroid = modelCentroid * (1.0f / totalVertices);
    }
    
    printf("Applying extreme triangle explosion factor %.2f\n", explosionFactor);
    
    std::vector<MeshSegment> newSegments;
    
    for (size_t segIdx = 0; segIdx < g_slicerState.segments.size(); segIdx++) {
        const MeshSegment& segment = g_slicerState.segments[segIdx];
        
        for (size_t i = 0; i < segment.indices.size(); i += 3) {
            MeshSegment triangleSegment;
            triangleSegment.segmentColor = segment.segmentColor;
            triangleSegment.regionCode = segment.regionCode;
            
            SlicedVertex v1 = segment.vertices[segment.indices[i]];
            SlicedVertex v2 = segment.vertices[segment.indices[i+1]];
            SlicedVertex v3 = segment.vertices[segment.indices[i+2]];
            
            Vector3f triangleCentroid = (v1.position + v2.position + v3.position) * (1.0f / 3.0f);
            
            Vector3f explosionDir = (triangleCentroid - modelCentroid).Normalize();
            float explosionDistance = explosionFactor * (model->extent / 10.0f);
            Vector3f displacement = explosionDir * explosionDistance;
            
            v1.position = v1.position + displacement;
            v2.position = v2.position + displacement;
            v3.position = v3.position + displacement;
            
            addTriangle(triangleSegment, v1, v2, v3);
            
            triangleSegment.originalPositions.clear();
            for (const SlicedVertex& v : triangleSegment.vertices) {
                triangleSegment.originalPositions.push_back(v.position - displacement);
            }
            
            newSegments.push_back(triangleSegment);
        }
    }
    
    // Replace the original segments with the new triangle-per-segment design
    g_slicerState.segments = newSegments;
    
    printf("Separated mesh into %zu individual triangles\n", g_slicerState.segments.size());
}

size_t getSegmentCount() {
    return g_slicerState.segments.size();
}

#endif // MESH_SLICER_H