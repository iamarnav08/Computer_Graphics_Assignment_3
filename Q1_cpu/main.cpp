#include <stdio.h>
#include <iostream>
#include <fstream>
#include <string.h>
#include <stdlib.h>
#include <string>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <vector>

#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include "file_utils.h"
#include "math_utils.h"
#include "OFFReader.h"
#include "normal.h"
#include "camera.h"
#include "light.h"
#include "plane.h"
#include "mesh_slicer.h"

#define GL_SILENCE_DEPRECATION



// Variable for four plane equations
bool plane1Changed, plane2Changed, plane3Changed, plane4Changed;
Plane plane1;
Plane plane2;
Plane plane3;
Plane plane4;

std::vector<Plane> active_planes;

MeshSlicerState g_slicerState;
bool g_meshInitialized = false;
bool meshSliced = false;
GLuint slicedVAO = 0, slicedVBO = 0, slicedIBO = 0;
int slicedVertexCount = 0;
Matrix4f viewMatrix;
bool extremeExplosion = false;


// Plane rendering variables
GLuint planeVAO = 0, planeVBO = 0;
std::vector<float> planeVertices;
bool showPlanes = true;
float planeSize = 10.0f; // Size of the plane visualization
float planeOpacity = 0.3f; // Transparency of the planes

//Variables

string model_name;

const float UI_PANEL_WIDTH = 300.0f;
const float UI_PANEL_SPACING = 10.0f;
char theProgramTitle[] = "3D Mesh Viewer";
int theWindowWidth = 800, theWindowHeight = 600;
int theWindowPositionX = 40, theWindowPositionY = 40;
bool isFullScreen = false;
float rotation = 01.0f;
GLuint VBO, VAO, IBO;
GLuint ShaderProgram;
GLuint gWorldLocation;
GLuint gProjectionLocation; 
GLuint gViewLocation;

Matrix4f ProjectionMatrix; 
Vector3f cameraPos = Vector3f(0.0f, 0.0f, -25.0f);
Vector3f cameraFront = Vector3f(0.0f, 0.0f, -1.0f);
Vector3f cameraUp = Vector3f(0.0f, 1.0f, 0.0f);
float cameraSpeed = 0.5f;
GLuint viewPosLocation;
GLuint lightPosLocation;
Vector3f lightPos = Vector3f(5.0f, 5.0f, 5.0f);
float yaw = -90.0f;   // Initialize to -90 so camera faces -Z direction
float pitch = 0.0f;
float rotationSpeed = 2.0f;
bool autoRotate;
float explosionFactor = 0.0f;
bool isExploded = false;
std::vector<Vector3f> originalVertices;
Vector3f* faceNormals = nullptr;
Vector3f* faceCenters = nullptr;
int explodedVertexCount = 0;
bool isDragging = false;
double lastMouseX, lastMouseY;
float mouseSensitivity = 0.005f;
float rotationX = 0.0f;
float rotationY = 0.0f;
GLFWwindow *window;
GLFWmonitor* primaryMonitor;
const GLFWvidmode* mode;
const int ANIMATION_DELAY = 20; /* milliseconds between rendering */
const char *pVSFileName = "shaders/shader.vs";
const char *pFSFileName = "shaders/shader.fs";


std::vector<Light> lights = {
    // {{5.0f, 5.0f, 5.0f}, {0.0f, 1.0f, 0.0f}, true, 1.0f},  // Green light
    // {{-5.0f, 0.0f, 5.0f}, {1.0f, 0.0f, 0.0f}, true, 1.0f}, // Red light
    // {{0.0f, -5.0f, -5.0f}, {0.0f, 0.0f, 1.0f}, true, 1.0f} // Blue light
    {{5.0f, 5.0f, 5.0f}, {1.0f, 1.0f, 1.0f}, true, 1.0f},  // Three whilte lights
    {{-5.0f, 0.0f, 5.0f}, {1.0f, 1.0f, 1.0f}, false, 1.0f}, 
    {{0.0f, -5.0f, -5.0f}, {1.0f, 1.0f, 1.0f}, false, 1.0f} 
};

// Global OffModel pointer 
OffModel* model = nullptr;

void computeFPS()
{
    static int frameCount = 0;
    static int lastFrameTime = 0;
    static char *title = NULL;
    int currentTime;

    if (!title)
        title = (char *)malloc((strlen(theProgramTitle) + 20) * sizeof(char));
    frameCount++;
    currentTime = 0;
    if (currentTime - lastFrameTime > 1000)
    {
        sprintf(title, "%s [ FPS: %4.2f ]",
                theProgramTitle,
                frameCount * 1000.0 / (currentTime - lastFrameTime));
        lastFrameTime = currentTime;
        frameCount = 0;
    }
}

static void CreateVertexBuffer()
{
    if (!model)
    {
        std::cerr << "Model is not loaded!" << std::endl;
        return;
    }
	// for (int i = 0; i < model->numberOfVertices; i++) {
	// 	printf("CHecking for Vertex %d: (%f, %f, %f)\n", i, model->vertices[i].x, model->vertices[i].y, model->vertices[i].z);
	// }

    glGenVertexArrays(1, &VAO);
    glBindVertexArray(VAO);

    // Create vertex buffer
    glGenBuffers(1, &VBO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, model->numberOfVertices * sizeof(Vertex), model->vertices, GL_STATIC_DRAW);

    // Enable vertex attributes
    glEnableVertexAttribArray(0); // Position
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *)offsetof(Vertex, x));

	 // Enable position attribute
	glEnableVertexAttribArray(1); // Position
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *)offsetof(Vertex, r));

    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));
 

    // Create index buffer
    // std::vector<unsigned int> indices;
    // for (int i = 0; i < model->numberOfPolygons; i++)
    // {
    //     for (int j = 0; j < model->polygons[i].noSides; j++)
    //     {
    //         indices.push_back(model->polygons[i].v[j]);
    //     }
    // }
    std::vector<unsigned int> indices;
    for (int i = 0; i < model->numberOfPolygons; i++)
    {
        // All polygons have 3 sides now
        indices.push_back(model->polygons[i].v[0]);
        indices.push_back(model->polygons[i].v[1]);
        indices.push_back(model->polygons[i].v[2]);
    }

    glGenBuffers(1, &IBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, IBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

    glBindVertexArray(0);
}

static void AddShader(GLuint ShaderProgram, const char *pShaderText, GLenum ShaderType)
{
    GLuint ShaderObj = glCreateShader(ShaderType);

    if (ShaderObj == 0)
    {
        fprintf(stderr, "Error creating shader type %d\n", ShaderType);
        exit(0);
    }

    const GLchar *p[1];
    p[0] = pShaderText;
    GLint Lengths[1];
    Lengths[0] = strlen(pShaderText);
    glShaderSource(ShaderObj, 1, p, Lengths);
    glCompileShader(ShaderObj);
    GLint success;
    glGetShaderiv(ShaderObj, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        GLchar InfoLog[1024];
        glGetShaderInfoLog(ShaderObj, 1024, NULL, InfoLog);
        fprintf(stderr, "Error compiling shader type %d: '%s'\n", ShaderType, InfoLog);
        exit(1);
    }

    glAttachShader(ShaderProgram, ShaderObj);
}

using namespace std;

static void CompileShaders()
{
    ShaderProgram = glCreateProgram();

    if (ShaderProgram == 0)
    {
        fprintf(stderr, "Error creating shader program\n");
        exit(1);
    }

    string vs, fs;

    if (!ReadFile(pVSFileName, vs))
    {
        exit(1);
    }

    if (!ReadFile(pFSFileName, fs))
    {
        exit(1);
    }

    AddShader(ShaderProgram, vs.c_str(), GL_VERTEX_SHADER);
    AddShader(ShaderProgram, fs.c_str(), GL_FRAGMENT_SHADER);

    GLint Success = 0;
    GLchar ErrorLog[1024] = {0};

    glLinkProgram(ShaderProgram);
    glGetProgramiv(ShaderProgram, GL_LINK_STATUS, &Success);
    if (Success == 0)
    {
        glGetProgramInfoLog(ShaderProgram, sizeof(ErrorLog), NULL, ErrorLog);
        fprintf(stderr, "Error linking shader program: '%s'\n", ErrorLog);
        exit(1);
    }
    glBindVertexArray(VAO);
    glValidateProgram(ShaderProgram);
    glGetProgramiv(ShaderProgram, GL_VALIDATE_STATUS, &Success);
    if (!Success)
    {
        glGetProgramInfoLog(ShaderProgram, sizeof(ErrorLog), NULL, ErrorLog);
        fprintf(stderr, "Invalid shader program1: '%s'\n", ErrorLog);
        exit(1);
    }

    glUseProgram(ShaderProgram);
    gWorldLocation = glGetUniformLocation(ShaderProgram, "gWorld");
    gViewLocation = glGetUniformLocation(ShaderProgram, "gView");  // Add this
    gProjectionLocation = glGetUniformLocation(ShaderProgram, "gProjection");
    viewPosLocation = glGetUniformLocation(ShaderProgram, "viewPos");
    lightPosLocation = glGetUniformLocation(ShaderProgram, "lightPos");

    // Initialize light uniforms
    for (int i = 0; i < NUM_LIGHTS; i++) {
        std::string lightPosName = "lights[" + std::to_string(i) + "].position";
        std::string lightColorName = "lights[" + std::to_string(i) + "].color";
        std::string lightEnabledName = "lights[" + std::to_string(i) + "].enabled";
        std::string lightIntensityName = "lights[" + std::to_string(i) + "].intensity";
        
        glUniform3f(glGetUniformLocation(ShaderProgram, lightPosName.c_str()), lights[i].position.x, lights[i].position.y, lights[i].position.z);
        glUniform3f(glGetUniformLocation(ShaderProgram, lightColorName.c_str()), lights[i].color.x, lights[i].color.y, lights[i].color.z);
        glUniform1i(glGetUniformLocation(ShaderProgram, lightEnabledName.c_str()), lights[i].enabled);
        glUniform1f(glGetUniformLocation(ShaderProgram, lightIntensityName.c_str()), lights[i].intensity);
    }
}


void generatePlaneVertices(std::vector<float>& vertices, const Plane& plane, float size, float r, float g, float b, float a) {
    Vector3f pointOnPlane;
    if (fabs(plane.a) > fabs(plane.b) && fabs(plane.a) > fabs(plane.c)) {
        pointOnPlane.x = -plane.d / plane.a;
        pointOnPlane.y = 0.0f;
        pointOnPlane.z = 0.0f;
    } else if (fabs(plane.b) > fabs(plane.c)) {
        pointOnPlane.x = 0.0f;
        pointOnPlane.y = -plane.d / plane.b;
        pointOnPlane.z = 0.0f;
    } else {
        pointOnPlane.x = 0.0f;
        pointOnPlane.y = 0.0f;
        pointOnPlane.z = -plane.d / plane.c;
    }
    
    Vector3f normal(plane.a, plane.b, plane.c);
    normal = normal.Normalize();
    
    Vector3f v1;
    if (fabs(normal.x) < fabs(normal.y) && fabs(normal.x) < fabs(normal.z)) {
        v1 = Vector3f(1, 0, 0).Cross(normal).Normalize();
    } else if (fabs(normal.y) < fabs(normal.z)) {
        v1 = Vector3f(0, 1, 0).Cross(normal).Normalize();
    } else {
        v1 = Vector3f(0, 0, 1).Cross(normal).Normalize();
    }
    Vector3f v2 = normal.Cross(v1).Normalize();
    
    Vector3f p1 = pointOnPlane - v1 * size - v2 * size;
    Vector3f p2 = pointOnPlane + v1 * size - v2 * size;
    Vector3f p3 = pointOnPlane + v1 * size + v2 * size;
    Vector3f p4 = pointOnPlane - v1 * size + v2 * size;
    
    // First triangle
    // Vertex 1
    vertices.push_back(p1.x); vertices.push_back(p1.y); vertices.push_back(p1.z);
    vertices.push_back(r); vertices.push_back(g); vertices.push_back(b); vertices.push_back(a);
    vertices.push_back(normal.x); vertices.push_back(normal.y); vertices.push_back(normal.z);
    
    // Vertex 2
    vertices.push_back(p2.x); vertices.push_back(p2.y); vertices.push_back(p2.z);
    vertices.push_back(r); vertices.push_back(g); vertices.push_back(b); vertices.push_back(a);
    vertices.push_back(normal.x); vertices.push_back(normal.y); vertices.push_back(normal.z);
    
    // Vertex 3
    vertices.push_back(p3.x); vertices.push_back(p3.y); vertices.push_back(p3.z);
    vertices.push_back(r); vertices.push_back(g); vertices.push_back(b); vertices.push_back(a);
    vertices.push_back(normal.x); vertices.push_back(normal.y); vertices.push_back(normal.z);
    
    // Second triangle
    // Vertex 1
    vertices.push_back(p1.x); vertices.push_back(p1.y); vertices.push_back(p1.z);
    vertices.push_back(r); vertices.push_back(g); vertices.push_back(b); vertices.push_back(a);
    vertices.push_back(normal.x); vertices.push_back(normal.y); vertices.push_back(normal.z);
    
    // Vertex 3
    vertices.push_back(p3.x); vertices.push_back(p3.y); vertices.push_back(p3.z);
    vertices.push_back(r); vertices.push_back(g); vertices.push_back(b); vertices.push_back(a);
    vertices.push_back(normal.x); vertices.push_back(normal.y); vertices.push_back(normal.z);
    
    // Vertex 4
    vertices.push_back(p4.x); vertices.push_back(p4.y); vertices.push_back(p4.z);
    vertices.push_back(r); vertices.push_back(g); vertices.push_back(b); vertices.push_back(a);
    vertices.push_back(normal.x); vertices.push_back(normal.y); vertices.push_back(normal.z);
}

void updatePlaneBuffers(const std::vector<Plane>& planes, float size) {
    planeVertices.clear();
    
    for (const Plane& plane : planes) {
        generatePlaneVertices(planeVertices, plane, size, 1.0f, 0.0f, 0.0f, planeOpacity); // Red planes
    }
    
    if (planeVAO == 0) {
        glGenVertexArrays(1, &planeVAO);
    }
    
    glBindVertexArray(planeVAO);
    
    if (planeVBO == 0) {
        glGenBuffers(1, &planeVBO);
    }
    
    glBindBuffer(GL_ARRAY_BUFFER, planeVBO);
    glBufferData(GL_ARRAY_BUFFER, planeVertices.size() * sizeof(float), planeVertices.data(), GL_STATIC_DRAW);
    
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 10 * sizeof(float), (void*)0);
    
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 10 * sizeof(float), (void*)(3 * sizeof(float)));
    
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 10 * sizeof(float), (void*)(7 * sizeof(float)));
    
    glBindVertexArray(0);
}

 void onInit(int argc, char *argv[])
{
    model = readOffFile(argv[1]);
    if (!model)
    {
        std::cerr << "Failed to load OFF file!" << std::endl;
        exit(1);
    }

    originalVertices.resize(model->numberOfVertices);
    for(int i = 0; i < model->numberOfVertices; i++) {
        originalVertices[i] = Vector3f(
            model->vertices[i].x,
            model->vertices[i].y,
            model->vertices[i].z
        );
    }

    calculateVertexNormals(model);

    faceNormals = calculateFaceNormals(model);
    
    faceCenters = new Vector3f[model->numberOfPolygons];
    for(int i = 0; i < model->numberOfPolygons; i++) {
        Vector3f center(0.0f, 0.0f, 0.0f);
        Polygon* poly = &model->polygons[i];
        
        for(int j = 0; j < poly->noSides; j++) {
            Vertex* v = &model->vertices[poly->v[j]];
            center.x += v->x;
            center.y += v->y;
            center.z += v->z;
        }
        
        center.x /= poly->noSides;
        center.y /= poly->noSides;
        center.z /= poly->noSides;
        faceCenters[i] = center;
    }

    ProjectionMatrix = Matrix4f();

    float centerX = (model->minX + model->maxX) / 2.0f;
    float centerY = (model->minY + model->maxY) / 2.0f;
    float centerZ = (model->minZ + model->maxZ) / 2.0f;
    
    float width = model->maxX - model->minX;
    float height = model->maxY - model->minY;
    float depth = model->maxZ - model->minZ;
    
    model->extent = std::max(std::max(width, height), depth);

    initMeshSlicer(model);
    meshSliced = false;
    explosionFactor = 0.0f;
    isExploded = false;
    extremeExplosion = false;
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    CreateVertexBuffer();
    CompileShaders();
    // CompileDebugShader();
    Vector3f modelCentroid(
        (model->maxX + model->minX) / 2.0f,
        (model->maxY + model->minY) / 2.0f,
        (model->maxZ + model->minZ) / 2.0f
    );

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    float offset = 0.0001f;
    for (size_t i = 0; i < g_slicerState.segments.size(); i++) {
        Vector3f offsetDir = calculateCentroid(g_slicerState.segments[i]) - modelCentroid;
        offsetDir = offsetDir.Normalize();
        
        for (SlicedVertex& v : g_slicerState.segments[i].vertices) {
            v.position = v.position + offsetDir * (offset * i);
        }
    }
}


static void onDisplay()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    float FOV = 45.0f;
    float zNear = 1.0f;
    float zFar = 30.0f;

    float aspectRatio = (float)theWindowWidth / (float)theWindowHeight;

    float scale = 2.0f / model->extent; 
    float tanHalfFOV = tanf((FOV / 2.0f) * 3.14159f / 180.0f);
    ProjectionMatrix.m[0][0] = 1.0f / (tanHalfFOV * aspectRatio);
    ProjectionMatrix.m[1][1] = 1.0f / tanHalfFOV;
    ProjectionMatrix.m[2][2] = -(zFar + zNear) / (zFar - zNear);
    ProjectionMatrix.m[2][3] = -1.0f;
    ProjectionMatrix.m[3][2] = -(2.0f * zFar * zNear) / (zFar - zNear);
    ProjectionMatrix.m[3][3] = 0.0f;


    Vector3f forward = cameraFront.Normalize();
    Vector3f right = cameraUp.Cross(forward).Normalize();
    Vector3f up = forward.Cross(right);
    viewMatrix.m[0][0] = right.x;
    viewMatrix.m[0][1] = right.y;
    viewMatrix.m[0][2] = right.z;
    viewMatrix.m[0][3] = -right.Dot(cameraPos);
    
    viewMatrix.m[1][0] = up.x;
    viewMatrix.m[1][1] = up.y;
    viewMatrix.m[1][2] = up.z;
    viewMatrix.m[1][3] = -up.Dot(cameraPos);
    
    viewMatrix.m[2][0] = forward.x;
    viewMatrix.m[2][1] = forward.y;
    viewMatrix.m[2][2] = forward.z;
    viewMatrix.m[2][3] = -forward.Dot(cameraPos);
    
    // Fourth row
    viewMatrix.m[3][0] = 0.0f;
    viewMatrix.m[3][1] = 0.0f;
    viewMatrix.m[3][2] = 0.0f;
    viewMatrix.m[3][3] = 1.0f;
    

    Matrix4f modelMatrix;

    Vector3f modelCenter(
        (model->maxX + model->minX) / 2.0f,
        (model->maxY + model->minY) / 2.0f,
        (model->maxZ + model->minZ) / 2.0f
    );

    Matrix4f translateToOrigin;
    Matrix4f translateBack;
    Matrix4f rotationMatrixY;
    Matrix4f rotationMatrixX;

    translateToOrigin.InitTranslationTransform(-modelCenter.x, -modelCenter.y, -modelCenter.z);
    translateBack.InitTranslationTransform(modelCenter.x, modelCenter.y, modelCenter.z);

    rotationMatrixY.m[0][0] = cosf(rotationY);
    rotationMatrixY.m[0][1] = 0.0f;
    rotationMatrixY.m[0][2] = sinf(rotationY);
    rotationMatrixY.m[0][3] = 0.0f;
    
    rotationMatrixY.m[1][0] = 0.0f;
    rotationMatrixY.m[1][1] = 1.0f;
    rotationMatrixY.m[1][2] = 0.0f;
    rotationMatrixY.m[1][3] = 0.0f;
    
    rotationMatrixY.m[2][0] = -sinf(rotationY);
    rotationMatrixY.m[2][1] = 0.0f;
    rotationMatrixY.m[2][2] = cosf(rotationY);
    rotationMatrixY.m[2][3] = 0.0f;
    
    rotationMatrixY.m[3][0] = 0.0f;
    rotationMatrixY.m[3][1] = 0.0f;
    rotationMatrixY.m[3][2] = 0.0f;
    rotationMatrixY.m[3][3] = 1.0f;

    rotationMatrixX.m[0][0] = 1.0f;
    rotationMatrixX.m[0][1] = 0.0f;
    rotationMatrixX.m[0][2] = 0.0f;
    rotationMatrixX.m[0][3] = 0.0f;
    
    rotationMatrixX.m[1][0] = 0.0f;
    rotationMatrixX.m[1][1] = cosf(rotationX);
    rotationMatrixX.m[1][2] = -sinf(rotationX);
    rotationMatrixX.m[1][3] = 0.0f;
    
    rotationMatrixX.m[2][0] = 0.0f;
    rotationMatrixX.m[2][1] = sinf(rotationX);
    rotationMatrixX.m[2][2] = cosf(rotationX);
    rotationMatrixX.m[2][3] = 0.0f;
    
    rotationMatrixX.m[3][0] = 0.0f;
    rotationMatrixX.m[3][1] = 0.0f;
    rotationMatrixX.m[3][2] = 0.0f;
    rotationMatrixX.m[3][3] = 1.0f;

    modelMatrix = translateBack * rotationMatrixY * rotationMatrixX * translateToOrigin;

    Matrix4f World = viewMatrix * modelMatrix;

    glUseProgram(ShaderProgram);
    glUniformMatrix4fv(gWorldLocation, 1, GL_TRUE, &modelMatrix.m[0][0]);
    glUniformMatrix4fv(gViewLocation, 1, GL_TRUE, &viewMatrix.m[0][0]);    // Add this
    glUniformMatrix4fv(gProjectionLocation, 1, GL_TRUE, &ProjectionMatrix.m[0][0]);
    glUniform3f(viewPosLocation, cameraPos.x, cameraPos.y, cameraPos.z);
    glUniform3f(lightPosLocation, lightPos.x, lightPos.y, lightPos.z);


    glBindVertexArray(VAO);
    
    
    if (meshSliced) {
        glUseProgram(ShaderProgram);
        glUniformMatrix4fv(glGetUniformLocation(ShaderProgram, "gWorld"), 1, GL_TRUE, &modelMatrix.m[0][0]);
        glUniformMatrix4fv(glGetUniformLocation(ShaderProgram, "gView"), 1, GL_TRUE, &viewMatrix.m[0][0]);
        glUniformMatrix4fv(glGetUniformLocation(ShaderProgram, "gProjection"), 1, GL_TRUE, &ProjectionMatrix.m[0][0]);
        
        glBindVertexArray(slicedVAO);
        glDrawElements(GL_TRIANGLES, slicedVertexCount, GL_UNSIGNED_INT, 0);
        
        glUseProgram(ShaderProgram);
    } else {
        if (explosionFactor > 0.0f) {
            glDrawArrays(GL_TRIANGLES, 0, explodedVertexCount);
        } else {
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, IBO);
            glDrawElements(GL_TRIANGLES, model->numberOfPolygons * 3, GL_UNSIGNED_INT, 0);
        }
    }

    if (showPlanes && !active_planes.empty()) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        
        glBindVertexArray(planeVAO);
        glDrawArrays(GL_TRIANGLES, 0, planeVertices.size() / 10); 
        glBindVertexArray(0);
        
        glDisable(GL_BLEND);
    }
    
    glBindVertexArray(0);

    // printf("a1: %f, b1: %f, c1: %f, d1: %f, plane_1_enabled: %d\n", a1, b1, c1, d1, plane_1_enabled);
    // printf("Plane 1: a1: %f, b1: %f, c1: %f, d1: %f, plane_1_enabled: %d\n", plane1.a, plane1.b, plane1.c, plane1.d, plane1.enabled);

    GLenum errorCode = glGetError();
    if (errorCode == GL_NO_ERROR)
    {
    }
    else
    {
        fprintf(stderr, "OpenGL rendering error %d\n", errorCode);
    }
}

void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
    ImGui_ImplGlfw_MouseButtonCallback(window, button, action, mods);
    
    if (ImGui::GetIO().WantCaptureMouse) {
        return;
    }

    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        if (action == GLFW_PRESS) {
            isDragging = true;
            glfwGetCursorPos(window, &lastMouseX, &lastMouseY);
        } else if (action == GLFW_RELEASE) {
            isDragging = false;
        }
    }
}

void cursor_position_callback(GLFWwindow* window, double xpos, double ypos) {
    ImGui_ImplGlfw_CursorPosCallback(window, xpos, ypos);
    if (ImGui::GetIO().WantCaptureMouse) {
        return;
    }

    if (isDragging) {
        double deltaX = xpos - lastMouseX;
        double deltaY = ypos - lastMouseY;
        
        rotationY += deltaX * mouseSensitivity;
        rotationX += deltaY * mouseSensitivity;
        
        const float maxAngle = static_cast<float>(M_PI/2.0f - 0.1f);
        rotationX = std::max(-maxAngle, std::min(maxAngle, rotationX));
        
        while (rotationY >= 2 * M_PI) rotationY -= 2 * M_PI;
        while (rotationY < 0) rotationY += 2 * M_PI;

        lastMouseX = xpos;
        lastMouseY = ypos;
    }
}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (ImGui::GetIO().WantCaptureKeyboard) {
        return;
    }

    if (action == GLFW_PRESS || action == GLFW_REPEAT) {
        float moveSpeed = cameraSpeed;
        
        // Fixed directions
        Vector3f north(0.0f, 0.0f, -1.0f); // North 
        Vector3f south(0.0f, 0.0f, 1.0f);  // South 
        Vector3f west(-1.0f, 0.0f, 0.0f);  // West 
        Vector3f east(1.0f, 0.0f, 0.0f);   // East 

        switch (key) {
            case GLFW_KEY_R: // Reset view
                cameraPos = Vector3f(0.0f, 0.0f, 5.0f);
                cameraFront = Vector3f(0.0f, 0.0f, -1.0f);
                cameraUp = Vector3f(0.0f, 1.0f, 0.0f);
                
                rotationX = 0.0f;
                rotationY = 0.0f;
                rotation = 0.0f;
                
                autoRotate = false;
                rotationSpeed = 2.0f;
                
                explosionFactor = 0.0f;
                isExploded = false;
                
                glBindBuffer(GL_ARRAY_BUFFER, VBO);
                glBufferData(GL_ARRAY_BUFFER, model->numberOfVertices * sizeof(Vertex), 
                            model->vertices, GL_STATIC_DRAW);
                explodedVertexCount = 0;
                break;
            case GLFW_KEY_W: // Move North
                cameraPos = cameraPos + (north * moveSpeed);
                break;
            case GLFW_KEY_S: // Move South
                cameraPos = cameraPos + (south * moveSpeed);
                break;
            case GLFW_KEY_A: // Move West
                cameraPos = cameraPos + (west * moveSpeed);
                break;
            case GLFW_KEY_D: // Move East
                cameraPos = cameraPos + (east * moveSpeed);
                break;
            case GLFW_KEY_SPACE: // Move up
                cameraPos = cameraPos + (cameraUp * moveSpeed);
                break;
            case GLFW_KEY_Z: // Move down
                cameraPos = cameraPos - (cameraUp * moveSpeed);
                break;
            case GLFW_KEY_F: // Toggle fullscreen
                if (action == GLFW_PRESS) {  
                    isFullScreen = !isFullScreen;
                    if (isFullScreen) {
                        glfwGetWindowPos(window, &theWindowPositionX, &theWindowPositionY);
                        glfwGetWindowSize(window, &theWindowWidth, &theWindowHeight);
                        
                        glfwSetWindowMonitor(window, primaryMonitor, 0, 0, 
                                           mode->width, mode->height, 
                                           mode->refreshRate);
                    } else {
                        glfwSetWindowMonitor(window, nullptr, 
                                           theWindowPositionX, theWindowPositionY,
                                           theWindowWidth, theWindowHeight, 
                                           GLFW_DONT_CARE);
                    }
                    
                    int width, height;
                    glfwGetFramebufferSize(window, &width, &height);
                    glViewport(0, 0, width, height);
                }
                break;
            case GLFW_KEY_ESCAPE:
                glfwSetWindowShouldClose(window, true);
                break;
        }
    }
    // printf("Camera Position: x=%.2f, y=%.2f, z=%.2f\n", 
    //     cameraPos.x, cameraPos.y, cameraPos.z);
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    theWindowWidth = width;
    theWindowHeight = height;
    glViewport(0, 0, width, height);
}


int main(int argc, char *argv[])
{
    if (argc > 1) {
        // Store the first CLI argument in the global variable
        model_name = argv[1];

    } else {
        std::cout << "Input formal- ./sample meshes/<mesh_name>" << std::endl;
    }
    glfwInit();

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_RESIZABLE, GL_TRUE);

    window = glfwCreateWindow(theWindowWidth, theWindowHeight, theProgramTitle, NULL, NULL);
    glfwMakeContextCurrent(window);

    primaryMonitor = glfwGetPrimaryMonitor();
    mode = glfwGetVideoMode(primaryMonitor);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    if (!window)
    {
        glfwTerminate();
        return 0;
    }

    glewExperimental = GL_TRUE;
    glewInit();
    printf("GL version: %s\n",
           glGetString(GL_VERSION));
    onInit(argc, argv);

    glfwSetKeyCallback(window, key_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetCursorPosCallback(window, cursor_position_callback);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    while (!glfwWindowShouldClose(window))
    {

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        plane1Changed = false;
        plane2Changed = false;
        plane3Changed = false;
        plane4Changed = false;

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);

        ImGui::SetNextWindowPos(ImVec2(UI_PANEL_SPACING, UI_PANEL_SPACING), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(UI_PANEL_WIDTH, 0), ImGuiCond_Always);
        ImGui::Begin("Light Controls", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);
        
        glUseProgram(ShaderProgram); 
        glUniform3f(viewPosLocation, cameraPos.x, cameraPos.y, cameraPos.z);
        
        for (int i = 0; i < NUM_LIGHTS; i++) {
            ImGui::PushID(i);
            std::string label = "Light " + std::to_string(i + 1);
            
            if (ImGui::CollapsingHeader(label.c_str())) {
                bool changed = false;
                changed |= ImGui::Checkbox("Enabled", &lights[i].enabled);
                changed |= ImGui::ColorEdit3("Color", &lights[i].color.x);
                changed |= ImGui::SliderFloat("Intensity", &lights[i].intensity, 0.0f, 2.0f);
                changed |= ImGui::SliderFloat3("Position", &lights[i].position.x, -10.0f, 10.0f);
                
                if (changed) {
                    std::string lightPosName = "lights[" + std::to_string(i) + "].position";
                    std::string lightColorName = "lights[" + std::to_string(i) + "].color";
                    std::string lightEnabledName = "lights[" + std::to_string(i) + "].enabled";
                    std::string lightIntensityName = "lights[" + std::to_string(i) + "].intensity";
                    
                    glUniform3f(glGetUniformLocation(ShaderProgram, lightPosName.c_str()), 
                                lights[i].position.x, lights[i].position.y, lights[i].position.z);
                    glUniform3f(glGetUniformLocation(ShaderProgram, lightColorName.c_str()), 
                                lights[i].color.x, lights[i].color.y, lights[i].color.z);
                    glUniform1i(glGetUniformLocation(ShaderProgram, lightEnabledName.c_str()), 
                                lights[i].enabled);
                    glUniform1f(glGetUniformLocation(ShaderProgram, lightIntensityName.c_str()), 
                                lights[i].intensity);
                }
            }
            ImGui::PopID();
        }
        
        ImGui::End();
        ImVec2 windowPos = ImVec2(theWindowWidth - 150, 10);
        ImGui::SetNextWindowPos(windowPos, ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.7f); // Semi-transparent background
        ImGui::Begin("Directions", nullptr, 
            ImGuiWindowFlags_NoMove | 
            ImGuiWindowFlags_NoResize | 
            ImGuiWindowFlags_AlwaysAutoResize |
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoSavedSettings);
        ImGui::Text("W: North (Forward)");
        ImGui::Text("S: South (Backward)");
        ImGui::Text("A: West (Left)");
        ImGui::Text("D: East (Right)");
        ImGui::End();


        // Set positions for the next panels to stack vertically
        float nextPanelY = UI_PANEL_SPACING + 300; 


        ImGui::SetNextWindowPos(ImVec2(UI_PANEL_SPACING, nextPanelY), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(UI_PANEL_WIDTH, 0), ImGuiCond_Always);
        ImGui::Begin("Plane Equation Controls", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);

        // Plane 1
        ImGui::PushID("plane1");
        if (ImGui::CollapsingHeader("Plane 1")) {
            plane1Changed |= ImGui::Checkbox("Enabled", &plane1.enabled);
            
            ImGui::Text("a:"); ImGui::SameLine(); 
            plane1Changed |= ImGui::InputFloat("##a1", &plane1.a, 0.0f, 0.0f, "%.2f");
            
            ImGui::Text("b:"); ImGui::SameLine(); 
            plane1Changed |= ImGui::InputFloat("##b1", &plane1.b, 0.0f, 0.0f, "%.2f");
            
            ImGui::Text("c:"); ImGui::SameLine(); 
            plane1Changed |= ImGui::InputFloat("##c1", &plane1.c, 0.0f, 0.0f, "%.2f");
            
            ImGui::Text("d:"); ImGui::SameLine(); 
            plane1Changed |= ImGui::InputFloat("##d1", &plane1.d, 0.0f, 0.0f, "%.2f");
            
        }
        ImGui::PopID();

        // Plane 2
        ImGui::PushID("plane2");
        if (ImGui::CollapsingHeader("Plane 2")) {
            plane2Changed |= ImGui::Checkbox("Enabled", &plane2.enabled);

            ImGui::Text("a:"); ImGui::SameLine(); 
            plane2Changed |= ImGui::InputFloat("##a2", &plane2.a, 0.0f, 0.0f, "%.2f");
            
            ImGui::Text("b:"); ImGui::SameLine(); 
            plane2Changed |= ImGui::InputFloat("##b2", &plane2.b, 0.0f, 0.0f, "%.2f");
            
            ImGui::Text("c:"); ImGui::SameLine(); 
            plane2Changed |= ImGui::InputFloat("##c2", &plane2.c, 0.0f, 0.0f, "%.2f");
            
            ImGui::Text("d:"); ImGui::SameLine(); 
            plane2Changed |= ImGui::InputFloat("##d2", &plane2.d, 0.0f, 0.0f, "%.2f");
            
        
        }
        ImGui::PopID();

        // Plane 3
        ImGui::PushID("plane3");
        if (ImGui::CollapsingHeader("Plane 3")) {
            plane3Changed |= ImGui::Checkbox("Enabled", &plane3.enabled);
            
            ImGui::Text("a:"); ImGui::SameLine(); 
            plane3Changed |= ImGui::InputFloat("##a3", &plane3.a, 0.0f, 0.0f, "%.2f");
            
            ImGui::Text("b:"); ImGui::SameLine(); 
            plane3Changed |= ImGui::InputFloat("##b3", &plane3.b, 0.0f, 0.0f, "%.2f");
            
            ImGui::Text("c:"); ImGui::SameLine(); 
            plane3Changed |= ImGui::InputFloat("##c3", &plane3.c, 0.0f, 0.0f, "%.2f");
            
            ImGui::Text("d:"); ImGui::SameLine(); 
            plane3Changed |= ImGui::InputFloat("##d3", &plane3.d, 0.0f, 0.0f, "%.2f");
       
        }
        ImGui::PopID();

        // Plane 4
        ImGui::PushID("plane4");
        if (ImGui::CollapsingHeader("Plane 4")) {
            plane4Changed |= ImGui::Checkbox("Enabled", &plane4.enabled);
            
            ImGui::Text("a:"); ImGui::SameLine(); 
            plane4Changed |= ImGui::InputFloat("##a4", &plane4.a, 0.0f, 0.0f, "%.2f");
            
            ImGui::Text("b:"); ImGui::SameLine(); 
            plane4Changed |= ImGui::InputFloat("##b4", &plane4.b, 0.0f, 0.0f, "%.2f");
            
            ImGui::Text("c:"); ImGui::SameLine(); 
            plane4Changed |= ImGui::InputFloat("##c4", &plane4.c, 0.0f, 0.0f, "%.2f");
            
            ImGui::Text("d:"); ImGui::SameLine(); 
            plane4Changed |= ImGui::InputFloat("##d4", &plane4.d, 0.0f, 0.0f, "%.2f");
   
        }
        ImGui::PopID();

        if (plane1Changed || plane2Changed || plane3Changed || plane4Changed) {
            printf("Changing plane data\n");
            updateActivePlanes(plane1, plane2, plane3, plane4, active_planes);
            updatePlaneBuffers(active_planes, planeSize);
        }

        ImGui::End();

        nextPanelY+=300;

        ImGui::SetNextWindowPos(ImVec2(UI_PANEL_SPACING, nextPanelY), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(UI_PANEL_WIDTH, 0), ImGuiCond_Always);
                
        ImGui::Begin("Mesh Controls", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);

        if (meshSliced) {
            ImGui::Checkbox("Extreme Triangle Explosion", &extremeExplosion);
        }

        if (ImGui::SliderFloat("Explosion Factor", &explosionFactor, 0.0f, 2.0f)) {
            if (meshSliced) {
                if (extremeExplosion) {
                    updateSlicedMeshExtremeExplosion(explosionFactor, model);
                } else {
                    updateSlicedMeshExplosion(explosionFactor, model);
                }
                uploadToGPU(slicedVAO, slicedVBO, slicedIBO, slicedVertexCount);
            } else {
                glBindVertexArray(VAO);
                updateMeshExplosion(model, explosionFactor, originalVertices, faceNormals, faceCenters, VBO, explodedVertexCount);
                glBindVertexArray(0);
            }
        }

        if (ImGui::Button("Toggle Explosion")) {
            isExploded = !isExploded;
            explosionFactor = isExploded ? 2.0f : 0.0f;
            
            if (meshSliced) {
                if (!isExploded) {
                    sliceWithPlanes(active_planes);  
                } else {
                    if (extremeExplosion) {
                        updateSlicedMeshExtremeExplosion(explosionFactor, model);
                    } else {
                        updateSlicedMeshExplosion(explosionFactor, model);
                    }
                }
                uploadToGPU(slicedVAO, slicedVBO, slicedIBO, slicedVertexCount);
            } else {
                if (!isExploded) {
                    glBindBuffer(GL_ARRAY_BUFFER, VBO);
                    glBufferData(GL_ARRAY_BUFFER, model->numberOfVertices * sizeof(Vertex), 
                                model->vertices, GL_STATIC_DRAW);
                    explodedVertexCount = 0;
                } else {
                    updateMeshExplosion(model, explosionFactor, originalVertices, 
                                    faceNormals, faceCenters, VBO, explodedVertexCount);
                }
                glBindVertexArray(0);
            }
        }
        ImGui::End();
                
        ImGui::SetNextWindowPos(ImVec2(UI_PANEL_SPACING, nextPanelY + 250), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(UI_PANEL_WIDTH, 0), ImGuiCond_Always);
        ImGui::Begin("Mesh Slicing Controls", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);

        if (ImGui::Button("Slice Mesh")) {
            sliceWithPlanes(active_planes);
            uploadToGPU(slicedVAO, slicedVBO, slicedIBO, slicedVertexCount);
            meshSliced = true;
            
            printf("Mesh sliced into %zu segments with different colors\n", getSegmentCount());
        }

        if (ImGui::Button("Reset Mesh")) {
            meshSliced = false;
        }

        ImGui::Text("Active planes: %zu", active_planes.size());
        ImGui::Text("Mesh segments: %zu", meshSliced ? getSegments().size() : 0);

        ImGui::End();

        nextPanelY += 150;

        ImGui::SetNextWindowPos(ImVec2(UI_PANEL_SPACING, nextPanelY), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(UI_PANEL_WIDTH, 0), ImGuiCond_Always);
        ImGui::Begin("Rotation Controls", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);
        ImGui::Checkbox("Auto Rotate", &autoRotate);
        ImGui::SliderFloat("Rotation Speed", &rotationSpeed, -2.0f, 2.0f);
        if(ImGui::Button("Reset Rotation")) {
            rotation = 0.0f;
            rotationX = 0.0f;
            rotationY = 0.0f;
        }
        ImGui::End();

        if(autoRotate) {
            rotationY += rotationSpeed * 0.01f;
        
        if(rotationY > 2 * M_PI) rotationY -= 2 * M_PI;
        if(rotationY < 0) rotationY += 2 * M_PI;
        
        }

        onDisplay();

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    FreeOffModel(model);

    cleanupMeshSlicer();
    if (slicedVAO != 0) {
        glDeleteVertexArrays(1, &slicedVAO);
        glDeleteBuffers(1, &slicedVBO);
        glDeleteBuffers(1, &slicedIBO);
    }
    if (planeVAO != 0) {
        glDeleteVertexArrays(1, &planeVAO);
        glDeleteBuffers(1, &planeVBO);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    delete[] faceNormals;
    delete[] faceCenters;
    glfwTerminate();
    return 0;
}