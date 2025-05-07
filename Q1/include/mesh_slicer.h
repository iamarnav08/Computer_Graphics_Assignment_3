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
    std::unordered_map<SlicedVertex, unsigned int, SlicedVertexHash> vertexMap;
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

unsigned int addVertex(MeshSegment& segment, const SlicedVertex& v) {
    auto it = segment.vertexMap.find(v);
    if (it != segment.vertexMap.end()) {
        return it->second;
    }
    
    unsigned int idx = segment.vertices.size();
    segment.vertices.push_back(v);
    segment.vertexMap[v] = idx;
    return idx;
}

void addTriangle(MeshSegment& segment, const SlicedVertex& v1, const SlicedVertex& v2, const SlicedVertex& v3) {
    unsigned int idx1 = addVertex(segment, v1);
    unsigned int idx2 = addVertex(segment, v2);
    unsigned int idx3 = addVertex(segment, v3);
    
    segment.indices.push_back(idx1);
    segment.indices.push_back(idx2);
    segment.indices.push_back(idx3);
}

SlicedVertex convertVertex(const Vertex& v) {
    SlicedVertex sv;
    sv.position = Vector3f(v.x, v.y, v.z);
    sv.normal = v.normal;
    sv.r = v.r;
    sv.g = v.g;
    sv.b = v.b;
    return sv;
}

bool isOnPositiveSide(const Vector3f& point, const Plane& plane) {
    return plane.evaluate(point) > 0.0f;
}

bool calculateIntersection(
    const Vertex& v1, const Vertex& v2, 
    const Plane& plane, EdgeIntersection& intersection) {
    
    Vector3f p1(v1.x, v1.y, v1.z);
    Vector3f p2(v2.x, v2.y, v2.z);
    
    float d1 = plane.evaluate(p1);
    float d2 = plane.evaluate(p2);
    
    if ((d1 * d2) >= 0.0f) {
        return false;  
    }
    
    float t = d1 / (d1 - d2);
    
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
            
            posSide.regionCode.push_back(true);  
            negSide.regionCode.push_back(false);
            
            for (size_t i = 0; i < segment.indices.size(); i += 3) {
                SlicedVertex v1 = segment.vertices[segment.indices[i]];
                SlicedVertex v2 = segment.vertices[segment.indices[i+1]];
                SlicedVertex v3 = segment.vertices[segment.indices[i+2]];
                
                bool v1Pos = isOnPositiveSide(v1.position, plane);
                bool v2Pos = isOnPositiveSide(v2.position, plane);
                bool v3Pos = isOnPositiveSide(v3.position, plane);
                
                int numPositive = (v1Pos ? 1 : 0) + (v2Pos ? 1 : 0) + (v3Pos ? 1 : 0);
                
                if (numPositive == 0) {
                    addTriangle(negSide, v1, v2, v3);
                } 
                else if (numPositive == 3) {
                    addTriangle(posSide, v1, v2, v3);
                }
                else if (numPositive == 1) {
                    SlicedVertex vPos, vNeg1, vNeg2;
                    
                    if (v1Pos) {
                        vPos = v1; vNeg1 = v2; vNeg2 = v3;
                    } else if (v2Pos) {
                        vPos = v2; vNeg1 = v3; vNeg2 = v1;
                    } else {
                        vPos = v3; vNeg1 = v1; vNeg2 = v2;
                    }
                    
                    EdgeIntersection i1, i2;
                    bool found1 = false, found2 = false;
                    
                    Vertex vertex1, vertex2;
                    vertex1.x = vPos.position.x; vertex1.y = vPos.position.y; vertex1.z = vPos.position.z;
                    vertex1.r = vPos.r; vertex1.g = vPos.g; vertex1.b = vPos.b;
                    vertex1.normal = vPos.normal;
                    
                    vertex2.x = vNeg1.position.x; vertex2.y = vNeg1.position.y; vertex2.z = vNeg1.position.z;
                    vertex2.r = vNeg1.r; vertex2.g = vNeg1.g; vertex2.b = vNeg1.b;
                    vertex2.normal = vNeg1.normal;
                    
                    if (calculateIntersection(vertex1, vertex2, plane, i1)) {
                        found1 = true;
                    }
                    
                    vertex2.x = vNeg2.position.x; vertex2.y = vNeg2.position.y; vertex2.z = vNeg2.position.z;
                    vertex2.r = vNeg2.r; vertex2.g = vNeg2.g; vertex2.b = vNeg2.b;
                    vertex2.normal = vNeg2.normal;
                    
                    if (calculateIntersection(vertex1, vertex2, plane, i2)) {
                        found2 = true;
                    }
                    
                    if (found1 && found2) {
                        SlicedVertex iv1, iv2;
                        iv1.position = i1.position;
                        iv1.normal = i1.normal;
                        iv1.r = i1.r; iv1.g = i1.g; iv1.b = i1.b;
                        
                        iv2.position = i2.position;
                        iv2.normal = i2.normal;
                        iv2.r = i2.r; iv2.g = i2.g; iv2.b = i2.b;
                        
                        addTriangle(posSide, vPos, iv1, iv2);
                        addTriangle(negSide, vNeg1, vNeg2, iv2);
                        addTriangle(negSide, vNeg1, iv2, iv1);
                    }
                }
                else if (numPositive == 2) {
                    SlicedVertex vNeg, vPos1, vPos2;
                    
                    if (!v1Pos) {
                        vNeg = v1; vPos1 = v2; vPos2 = v3;
                    } else if (!v2Pos) {
                        vNeg = v2; vPos1 = v3; vPos2 = v1;
                    } else {
                        vNeg = v3; vPos1 = v1; vPos2 = v2;
                    }
                    
                    EdgeIntersection i1, i2;
                    bool found1 = false, found2 = false;
                    
                    Vertex vertex1, vertex2;
                    vertex1.x = vNeg.position.x; vertex1.y = vNeg.position.y; vertex1.z = vNeg.position.z;
                    vertex1.r = vNeg.r; vertex1.g = vNeg.g; vertex1.b = vNeg.b;
                    vertex1.normal = vNeg.normal;
                    
                    vertex2.x = vPos1.position.x; vertex2.y = vPos1.position.y; vertex2.z = vPos1.position.z;
                    vertex2.r = vPos1.r; vertex2.g = vPos1.g; vertex2.b = vPos1.b;
                    vertex2.normal = vPos1.normal;
                    
                    if (calculateIntersection(vertex1, vertex2, plane, i1)) {
                        found1 = true;
                    }
                    
                    vertex2.x = vPos2.position.x; vertex2.y = vPos2.position.y; vertex2.z = vPos2.position.z;
                    vertex2.r = vPos2.r; vertex2.g = vPos2.g; vertex2.b = vPos2.b;
                    vertex2.normal = vPos2.normal;
                    
                    if (calculateIntersection(vertex1, vertex2, plane, i2)) {
                        found2 = true;
                    }
                    
                    if (found1 && found2) {
                        SlicedVertex iv1, iv2;
                        iv1.position = i1.position;
                        iv1.normal = i1.normal;
                        iv1.r = i1.r; iv1.g = i1.g; iv1.b = i1.b;
                        
                        iv2.position = i2.position;
                        iv2.normal = i2.normal;
                        iv2.r = i2.r; iv2.g = i2.g; iv2.b = i2.b;
                        
                        addTriangle(negSide, vNeg, iv1, iv2);
                        addTriangle(posSide, vPos1, vPos2, iv1);
                        addTriangle(posSide, iv1, vPos2, iv2);
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
    const auto& segments = getSegments();
    
    std::vector<Vertex> allVertices;
    std::vector<unsigned int> allIndices;
    
    unsigned int baseIndex = 0;
    
    printf("Uploading %zu segments to GPU\n", segments.size());
    
    for (size_t segIdx = 0; segIdx < segments.size(); segIdx++) {
        const MeshSegment& segment = segments[segIdx];
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
    
    if (vao == 0) {
        glGenVertexArrays(1, &vao);
    }
    
    glBindVertexArray(vao);
    
    if (vbo == 0) {
        glGenBuffers(1, &vbo);
    }
    
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, allVertices.size() * sizeof(Vertex), allVertices.data(), GL_STATIC_DRAW);
    
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
    
    vertexCount = allIndices.size();
    
    glBindVertexArray(0);
}

void updateSlicedMeshExplosion(float explosionFactor, OffModel* model) {
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
    
    g_slicerState.segments = newSegments;
    
    printf("Separated mesh into %zu individual triangles\n", g_slicerState.segments.size());
}

// Get the number of segments
size_t getSegmentCount() {
    return g_slicerState.segments.size();
}

#endif 