#include <stdio.h>
#include <iostream>
#include <fstream>
#include <string.h>
#include <stdlib.h>
#include <string>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <GL/glut.h>
#include <vector>
#include <algorithm>

#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include "file_utils.h"
#include "math_utils.h"
#define GL_SILENCE_DEPRECATION

/********************************************************************/
/*   Variables */

char theProgramTitle[] = "Sample";
int theWindowWidth = 700, theWindowHeight = 700;
int theWindowPositionX = 700, theWindowPositionY = 600; // Called in the glfwSetWindowPos function
bool isFullScreen = false;
bool isAnimating = true;
float rotation = 0.0f;
GLuint VBO, VAO, IBO;
GLuint gWorldLocation;
GLuint ShaderProgram;
int k = 0;


typedef struct Polygon
{
    int num_of_vertices;
    std::vector<Vector3f> vertices;
    std::vector<std::pair<int, int>> edges;
    int xmin, xmax, ymin, ymax;
} Polygon;

typedef struct scan_line_edge
{
    int ymax;
    int ymin;
    int xmin;
    int delta_x;
    int delta_y;
    int counter;
} scan_line_edge;

typedef std::vector<std::vector<scan_line_edge>> SET; // Sorted Edge Table
typedef std::vector<scan_line_edge> AET; // ACtive Edge Table

void DrawPolygon(Polygon &poly){
    ImDrawList *draw_list = ImGui::GetWindowDrawList();

    float poly_center_x = (poly.xmin + poly.xmax) / 2.0f;
    float poly_center_y = (poly.ymin + poly.ymax) / 2.0f;

    const float window_center_x = theWindowWidth / 2.0f;
    const float window_center_y = theWindowHeight / 2.0f;

    for (const auto &edge : poly.edges)
    {
        Vector3f &v1 = poly.vertices[edge.first];
        Vector3f &v2 = poly.vertices[edge.second];

        ImVec2 p1(window_center_x + (v1.x - poly_center_x), window_center_y - (v1.y - poly_center_y));
        ImVec2 p2(window_center_x + (v2.x - poly_center_x), window_center_y - (v2.y - poly_center_y));

        draw_list->AddLine(p1, p2, IM_COL32(255, 255, 255, 255), 2.0f);
    }
}

Polygon create_default_polygon(){
    Polygon poly;
    poly.num_of_vertices = 12;
    poly.vertices.push_back(Vector3f(0, 0, 0));
    poly.vertices.push_back(Vector3f(0, 200, 0));
    poly.vertices.push_back(Vector3f(50, 300, 0));
    poly.vertices.push_back(Vector3f(100, 200, 0));
    poly.vertices.push_back(Vector3f(150, 300, 0));
    poly.vertices.push_back(Vector3f(200, 200, 0));
    poly.vertices.push_back(Vector3f(250, 150, 0));
    poly.vertices.push_back(Vector3f(250, 100, 0));
    poly.vertices.push_back(Vector3f(350, 200, 0));
    poly.vertices.push_back(Vector3f(400, 250, 0));
    poly.vertices.push_back(Vector3f(400, 0, 0));
    poly.vertices.push_back(Vector3f(250, 0, 0));

    poly.edges.push_back(make_pair(0, 1));
    poly.edges.push_back(make_pair(1, 2));
    poly.edges.push_back(make_pair(2, 3));
    poly.edges.push_back(make_pair(3, 4));
    poly.edges.push_back(make_pair(4, 5));
    poly.edges.push_back(make_pair(5, 6));
    poly.edges.push_back(make_pair(6, 7));
    poly.edges.push_back(make_pair(7, 8));
    poly.edges.push_back(make_pair(8, 9));
    poly.edges.push_back(make_pair(9, 10));
    poly.edges.push_back(make_pair(10, 11));
    poly.edges.push_back(make_pair(11, 0));

    poly.xmin = 0;
    poly.xmax = 400;
    poly.ymin = 0;
    poly.ymax = 300;

    return poly;
}

SET createSortedEdgeTable(Polygon &poly){
    std::vector<std::vector<scan_line_edge>> SET(poly.ymax + 1);

    for (auto &edge_pair : poly.edges){
        Vector3f &v1 = poly.vertices[edge_pair.first];
        Vector3f &v2 = poly.vertices[edge_pair.second];

        if (v1.y == v2.y)
            continue;

        scan_line_edge edge;

        if (v1.y < v2.y){
            edge.ymin = v1.y;
            edge.ymax = v2.y;
            edge.xmin = v1.x;
            edge.delta_y = v2.y - v1.y;
            edge.delta_x = v2.x - v1.x;
        }
        else{
            edge.ymin = v2.y;
            edge.ymax = v1.y;
            edge.xmin = v2.x;
            edge.delta_y = v1.y - v2.y;
            edge.delta_x = v1.x - v2.x;
        }

        edge.counter = 0;

        SET[edge.ymin].push_back(edge);
    }

    for (auto &bucket : SET){
        std::sort(bucket.begin(), bucket.end(), [](const scan_line_edge &a, const scan_line_edge &b){
            return a.xmin < b.xmin;
        });
    }

    return SET;
}

void update_edge_x_coord(scan_line_edge& edge) {
    edge.counter += abs(edge.delta_x);
    
    while (edge.counter >= edge.delta_y) {
        edge.counter -= edge.delta_y;
        edge.xmin += (edge.delta_x >= 0) ? 1 : -1;
    }
}

bool isLocalMaxOrMin(const scan_line_edge& edge1, const scan_line_edge& edge2, int y){
    bool edge1_upward = (edge1.ymax > y);
    bool edge2_upward = (edge2.ymax > y);
    return (edge1_upward && edge2_upward) || (!edge1_upward && !edge2_upward);
}

void ScanLineFill(Polygon& poly, ImDrawList* draw_list){
    SET edgeTable = createSortedEdgeTable(poly);
    AET activeEdges;
    
    int y = poly.ymin;
    while (y <= poly.ymax && edgeTable[y].empty()) {
        y++;
    }
    
    if (y > poly.ymax) return;
    
    float poly_center_x = (poly.xmin + poly.xmax) / 2.0f;
    float poly_center_y = (poly.ymin + poly.ymax) / 2.0f;
    const float window_center_x = theWindowWidth / 2.0f;
    const float window_center_y = theWindowHeight / 2.0f;
    
    while (y <= poly.ymax-k && (!activeEdges.empty() || !edgeTable[y].empty())) {
        if (y <= poly.ymax && !edgeTable[y].empty()) {
            activeEdges.insert(activeEdges.end(), edgeTable[y].begin(), edgeTable[y].end());
        }
        
        std::sort(activeEdges.begin(), activeEdges.end(), [](const scan_line_edge& a, const scan_line_edge& b){
            return a.xmin < b.xmin;
        });
        
        for (size_t i = 0; i < activeEdges.size() - 1; i++) {
            if (std::round(activeEdges[i].xmin) == std::round(activeEdges[i + 1].xmin)) {
                if (!isLocalMaxOrMin(activeEdges[i], activeEdges[i + 1], y)) {
                    activeEdges.erase(activeEdges.begin() + i + 1);
                    i--; 
                }
            }
        }
        
        for (size_t i = 0; i + 1 < activeEdges.size(); i += 2) {
            int x1 = activeEdges[i].xmin;
            int x2 = activeEdges[i+1].xmin;
            
            float sx1 = window_center_x + (x1 - poly_center_x);
            float sx2 = window_center_x + (x2 - poly_center_x);
            float sy = window_center_y - (y - poly_center_y);
            
            draw_list->AddLine(
                ImVec2(sx1, sy),
                ImVec2(sx2, sy),
                IM_COL32(255, 0, 0, 255),  
                1.0f
            );
        }
        
        y++;
        
        activeEdges.erase(
            std::remove_if(activeEdges.begin(), activeEdges.end(),
                          [y](const scan_line_edge& edge) {
                              return edge.ymax <= y;
                          }),
            activeEdges.end()
        );
        
        for (auto& edge : activeEdges) {
            update_edge_x_coord(edge);
        }
    }
}

void toggleFullScreen(GLFWwindow *window){
    if (isFullScreen)
    {
        glfwSetWindowMonitor(window, NULL, theWindowPositionX, theWindowPositionY, theWindowWidth, theWindowHeight, 0);
    }
    else
    {
        GLFWmonitor *monitor = glfwGetPrimaryMonitor();
        const GLFWvidmode *mode = glfwGetVideoMode(monitor);

        glfwGetWindowSize(window, &theWindowWidth, &theWindowHeight);

        glfwSetWindowMonitor(window, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);

        theWindowWidth = mode->width;
        theWindowHeight = mode->height;
    }

    isFullScreen = !isFullScreen;
}

void keyCallback(GLFWwindow *window, int key, int scancode, int action, int mods)
{
    if (key == GLFW_KEY_F && action == GLFW_PRESS)
    {
        toggleFullScreen(window); // Toggle full-screen mode when 'F' is pressed
    }
}

/* Constants */
const int ANIMATION_DELAY = 400; /* milliseconds between rendering */
const char *pVSFileName = "shaders/shader.vs";
const char *pFSFileName = "shaders/shader.fs";

/********************************************************************
  Utility functions
 */

/* post: compute frames per second and display in window's title bar */
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

    glGenVertexArrays(1, &VAO);
    cout << "VAO: " << VAO << endl;
    glBindVertexArray(VAO);

    float Vertices[] = {
        -1.0f, -1.0f, 0.0f, 1.0f, 0.0f, 0.0f,
        1.0f, -1.0f, 0.0f, 0.0f, 1.0f, 0.0f,
        1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f,
        -1.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f};

    glGenBuffers(1, &VBO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(Vertices), Vertices, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    // glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), 0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void *)(3 * sizeof(float)));

    unsigned int indices[] = {0, 1, 2, 0, 2, 3};
    glGenBuffers(1, &IBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, IBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    // glEnable(GL_CULL_FACE);
    // glCullFace(GL_FRONT);
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
    // glBindVertexArray(0);
}

/********************************************************************
 Callback Functions
 */

void onInit(int argc, char *argv[])
{
    /* by default the back ground color is black */
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    CreateVertexBuffer();
    CompileShaders();

    /* set to draw in window based on depth  */
    glEnable(GL_DEPTH_TEST);
}

static void onDisplay()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glViewport(0, 0, theWindowWidth, theWindowHeight);

    /* check for any errors when rendering */
    GLenum errorCode = glGetError();
    if (errorCode != GL_NO_ERROR)
    {
        fprintf(stderr, "OpenGL rendering error %d\n", errorCode);
    }
}

void mouse_callback(GLFWwindow *window, double xpos, double ypos)
{
    // std::cout << "Mouse position: " << xpos << ", " << ypos << std::endl;
    // Handle mouse movement
}

void framebuffer_size_callback(GLFWwindow *window, int width, int height)
{
    theWindowWidth = width;
    theWindowHeight = height;

    glViewport(0, 0, width, height);
}

void mouse_button_callback(GLFWwindow *window, int button, int action, int mods)
{
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS)
    {
        // Left button pressed
    }
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_RELEASE)
    {
        // Left button released
    }
}

void key_callback(GLFWwindow *window, int key, int scancode, int action, int mods)
{
    if (action == GLFW_PRESS) // Only react on key press
    {
        switch (key)
        {
        case GLFW_KEY_R:
            rotation = 0;
            break;
        case GLFW_KEY_Q:
        case GLFW_KEY_ESCAPE:
            glfwSetWindowShouldClose(window, true);
            break;
        case GLFW_KEY_F:
            toggleFullScreen(window);
        }
    }
}

// Initialize ImGui
void InitImGui(GLFWwindow *window)
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    (void)io;
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");
}

void RenderImGui()
{
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                                    ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar |
                                    ImGuiWindowFlags_NoScrollWithMouse;

    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(theWindowWidth, theWindowHeight));

    ImGui::Begin("Canvas", nullptr, window_flags);

    static Polygon polygon = create_default_polygon();
    static bool fillPolygon = true;
    
    // Fill the polygon first if enabled
    if (fillPolygon) {
        ScanLineFill(polygon, ImGui::GetWindowDrawList());
        // ScanLineFill(polygon, ImGui::GetBackgroundDrawList());
    }
    
    // Then draw the outline
    DrawPolygon(polygon);

    ImGui::End();

    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(350, 150));
    ImGui::Begin("Control Panel");
    ImGui::Text("Press 'F' to toggle full screen");
    ImGui::Text("Press 'Esc' to exit");
    ImGui::Checkbox("Fill Polygon", &fillPolygon);
    ImGui::SliderInt("Scanline Limit", &k, 0, 290, "%d");
    ImGui::Text("Higher values show fewer scanlines");
    ImGui::End();

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

int main(int argc, char *argv[])
{
    glfwInit();

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_RESIZABLE, GL_TRUE);

    GLFWwindow *window = glfwCreateWindow(theWindowWidth, theWindowHeight,
                                          "Bresenham Line Algorithm", NULL, NULL);
    glfwSetWindowPos(window, theWindowPositionX, theWindowPositionY);
    glfwMakeContextCurrent(window);

    if (!window)
    {
        glfwTerminate();
        return 0;
    }

    glewExperimental = GL_TRUE;
    glewInit();
    printf("GL version: %s\n", glGetString(GL_VERSION));
    onInit(argc, argv);

    InitImGui(window);

    glfwSetKeyCallback(window, key_callback);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    while (!glfwWindowShouldClose(window))
    {
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f); // Dark gray background for better contrast
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        onDisplay();
        RenderImGui();

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}