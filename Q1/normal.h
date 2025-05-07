#ifndef NORMAL_H
#define NORMAL_H

#include <stdio.h>
#include <iostream>
#include <fstream>
#include <string.h>
#include <stdlib.h>
#include <string>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <vector>

#include "file_utils.h"
#include "math_utils.h"

#include "OFFReader.h"

Vector3f* calculateFaceNormals(OffModel* model) {
    Vector3f* normals = new Vector3f[model->numberOfPolygons];
    
    for(int i = 0; i < model->numberOfPolygons; i++) {
        Polygon* poly = &model->polygons[i];
        
        Vector3f v1(model->vertices[poly->v[0]].x,
                   model->vertices[poly->v[0]].y,
                   model->vertices[poly->v[0]].z);
                   
        Vector3f v2(model->vertices[poly->v[1]].x,
                   model->vertices[poly->v[1]].y,
                   model->vertices[poly->v[1]].z);
                   
        Vector3f v3(model->vertices[poly->v[2]].x,
                   model->vertices[poly->v[2]].y,
                   model->vertices[poly->v[2]].z);
        
        Vector3f edge1 = v2 - v1;
        Vector3f edge2 = v3 - v1;
        Vector3f normal = edge1.Cross(edge2);
        normals[i] = normal.Normalize();
    }
    
    return normals;
}


void calculateVertexNormals(OffModel* model) {
    Vector3f* faceNormals = calculateFaceNormals(model);
    
    for(int i = 0; i < model->numberOfVertices; i++) {
        model->vertices[i].normal = {0.0f, 0.0f, 0.0f};
        model->vertices[i].numIcidentTri = 0;
    }
    
    for(int i = 0; i < model->numberOfPolygons; i++) {
        Polygon* poly = &model->polygons[i];
        
        for(int j = 0; j < poly->noSides; j++) {
            int vertexIndex = poly->v[j];
            model->vertices[vertexIndex].normal.x += faceNormals[i].x;
            model->vertices[vertexIndex].normal.y += faceNormals[i].y;
            model->vertices[vertexIndex].normal.z += faceNormals[i].z;
            model->vertices[vertexIndex].numIcidentTri++;
        }
    }
    
    for(int i = 0; i < model->numberOfVertices; i++) {
        if(model->vertices[i].numIcidentTri > 0) {
            model->vertices[i].normal.x /= model->vertices[i].numIcidentTri;
            model->vertices[i].normal.y /= model->vertices[i].numIcidentTri;
            model->vertices[i].normal.z /= model->vertices[i].numIcidentTri;
            normalizeVector(model->vertices[i].normal);
        }
    }
    
    free(faceNormals);
}

void calculateFaceCenters(OffModel* model, Vector3f* centers) {
    for(int i = 0; i < model->numberOfPolygons; i++) {
        Polygon* poly = &model->polygons[i];
        Vector3f center(0.0f, 0.0f, 0.0f);
        
        for(int j = 0; j < poly->noSides; j++) {
            Vertex* v = &model->vertices[poly->v[j]];
            center.x += v->x;
            center.y += v->y;
            center.z += v->z;
        }
        
        center.x /= poly->noSides;
        center.y /= poly->noSides;
        center.z /= poly->noSides;
        
        centers[i] = center;
    }
}

void updateMeshExplosion(OffModel* model, float explosionFactor, 
    const std::vector<Vector3f>& originalVertices,
    Vector3f* faceNormals, Vector3f* faceCenters, 
    GLuint VBO, int& numVertices) {

    if (explosionFactor == 0.0f) {
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, model->numberOfVertices * sizeof(Vertex), 
    model->vertices, GL_STATIC_DRAW);
    numVertices = 0;
    return;
    }

    Vector3f modelCenter(0.0f, 0.0f, 0.0f);
    for (int i = 0; i < model->numberOfVertices; i++) {
    modelCenter.x += model->vertices[i].x;
    modelCenter.y += model->vertices[i].y;
    modelCenter.z += model->vertices[i].z;
    }
    modelCenter = modelCenter * (1.0f / model->numberOfVertices);

    std::vector<Vertex> explodedVertices;
    explodedVertices.reserve(model->numberOfPolygons * 3);

    for (int i = 0; i < model->numberOfPolygons; i++) {
    if (model->polygons[i].noSides != 3) {
    printf("Warning: Found non-triangle polygon (%d sides) at index %d!\n", 
    model->polygons[i].noSides, i);
    continue;
    }

    Vector3f triangleCenter(0.0f, 0.0f, 0.0f);
    for (int j = 0; j < 3; j++) {
    int vertIdx = model->polygons[i].v[j];
    triangleCenter.x += model->vertices[vertIdx].x;
    triangleCenter.y += model->vertices[vertIdx].y;
    triangleCenter.z += model->vertices[vertIdx].z;
    }
    triangleCenter = triangleCenter * (1.0f / 3.0f);

    Vector3f explosionDir = (triangleCenter - modelCenter).Normalize();
    float explosionDistance = explosionFactor * (model->extent / 10.0f);
    Vector3f displacement = explosionDir * explosionDistance;

    for (int j = 0; j < 3; j++) {
    int vertIdx = model->polygons[i].v[j];
    Vertex v = model->vertices[vertIdx];

    v.x += displacement.x;
    v.y += displacement.y;
    v.z += displacement.z;

    explodedVertices.push_back(v);
    }
    }

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, explodedVertices.size() * sizeof(Vertex), 
    explodedVertices.data(), GL_STATIC_DRAW);

    numVertices = explodedVertices.size();

    printf("Exploded mesh into %d triangles (%d vertices)\n", 
    model->numberOfPolygons, numVertices);
}


#endif