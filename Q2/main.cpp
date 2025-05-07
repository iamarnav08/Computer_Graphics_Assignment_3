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
float linePoints[4] = {-0.5f, -0.5f, 0.5f, 0.5f}; // x1, y1, x2, y2
bool drawLine = false;
std::vector<std::pair<int, int>> bressenhamPoints;
GLuint lineVAO, lineVBO;
GLuint ShaderProgram;

void toggleFullScreen(GLFWwindow* window) {
    if (isFullScreen) {
        glfwSetWindowMonitor(window, NULL, theWindowPositionX, theWindowPositionY, theWindowWidth, theWindowHeight, 0);
    } else {
        GLFWmonitor* monitor = glfwGetPrimaryMonitor();
        const GLFWvidmode* mode = glfwGetVideoMode(monitor);

        glfwSetWindowMonitor(window, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
    }

    isFullScreen = !isFullScreen;
}

void generateBressenhamLine(int x0, int y0, int x1, int y1){
    bressenhamPoints.clear();
 
	printf("Generating line from (%d,%d) to (%d,%d)\n", x0, y0, x1, y1);
    
    bool height_gradient = abs(y1 - y0) > abs(x1 - x0);
    if (height_gradient) {
        std::swap(x0, y0);
        std::swap(x1, y1);
    }
    
	// drawing from left to right
    if (x0 > x1) {
        std::swap(x0, x1);
        std::swap(y0, y1);
    }
    
    int dx = x1 - x0;
    int dy = abs(y1 - y0);
    int y_step = (y0 < y1) ? 1 : -1;
    
    int d = 2*dy - dx;
    int y = y0;
    
    // Generating all points for the line
    for (int x = x0; x <= x1; x++) {
        if (height_gradient) {
            bressenhamPoints.push_back(std::make_pair(y, x)); // If steep, we push the reversed coordinates
        } else {
            bressenhamPoints.push_back(std::make_pair(x, y)); // Normal case
        }
        
        // Updating decision parameter and y-coordinate for next step of the algorithm
        if (d > 0) {
            y += y_step;
            d += 2 * (dy - dx);
        } else {
            d += 2 * dy;
        }
    }
    
	printf("Generated %d points\n", bressenhamPoints.size());
}

void createLineBuffer(){
    if (bressenhamPoints.empty()) return;
    
    std::vector<float> lineVertices;
    for (int i = 0; i < bressenhamPoints.size(); i++) {
        int px = bressenhamPoints[i].first;
        // int py = bressenhamPoints[i].second;
        
        float x = (2.0f * px / theWindowWidth) - 1.0f;
        float y = (2.0f * py / theWindowHeight) - 1.0f;
        
        lineVertices.push_back(x);     // x position
        lineVertices.push_back(y);     // y position
        lineVertices.push_back(0.0f);  // z position
        lineVertices.push_back(1.0f);  // r color
        lineVertices.push_back(1.0f);  // g color
        lineVertices.push_back(0.0f);  // b color
    }
    
    if (lineVAO != 0) {
        glDeleteVertexArrays(1, &lineVAO);
        glDeleteBuffers(1, &lineVBO);
    }
    
    glGenVertexArrays(1, &lineVAO);
    glGenBuffers(1, &lineVBO);
    
    glBindVertexArray(lineVAO);
    glBindBuffer(GL_ARRAY_BUFFER, lineVBO);
    
    glBufferData(GL_ARRAY_BUFFER, lineVertices.size() * sizeof(float), lineVertices.data(), GL_STATIC_DRAW);
    
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), 0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    
    glBindVertexArray(0);
    
	printf("Created line buffer with %d points\n", bressenhamPoints.size());
}

void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (key == GLFW_KEY_F && action == GLFW_PRESS) {
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
	lineVAO = 0;
    lineVBO = 0;

	/* set to draw in window based on depth  */
	glEnable(GL_DEPTH_TEST);
}

static void onDisplay()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    if (drawLine && lineVAO != 0 && !bressenhamPoints.empty()) {
        glPointSize(1.0f);
        
        glUseProgram(ShaderProgram);
        
        Matrix4f World;
        for (int i = 0; i < 4; i++) {
            for (int j = 0; j < 4; j++) {
                World.m[i][j] = (i == j) ? 1.0f : 0.0f;
            }
        }
        
        glUniformMatrix4fv(gWorldLocation, 1, GL_TRUE, &World.m[0][0]);
        
        glBindVertexArray(lineVAO);
        
        glDrawArrays(GL_POINTS, 0, bressenhamPoints.size());
        glBindVertexArray(0);
    }

    /* check for any errors when rendering */
    GLenum errorCode = glGetError();
    if (errorCode != GL_NO_ERROR) {
        fprintf(stderr, "OpenGL rendering error %d\n", errorCode);
    }
}

void mouse_callback(GLFWwindow *window, double xpos, double ypos)
{
	// std::cout << "Mouse position: " << xpos << ", " << ypos << std::endl;
	// Handle mouse movement
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

// Render ImGui
void RenderImGui()
{
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300, 400), ImGuiCond_FirstUseEver);
    ImGui::Begin("Bresenham Line Algorithm", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
    
    static int x0 = 50;
    static int y0 = 50;
    static int x1 = theWindowWidth - 50;
    static int y1 = theWindowHeight - 50;
    
    ImGui::Text("Start Point:");
    ImGui::PushItemWidth(100);
    ImGui::InputInt("x0", &x0);
    ImGui::SameLine();
    ImGui::InputInt("y0", &y0);
    
    ImGui::Text("End Point:");
    ImGui::InputInt("x1", &x1);
    ImGui::SameLine();
    ImGui::InputInt("y1", &y1);
    ImGui::PopItemWidth();
    
    x0 = std::max(0, std::min(x0, theWindowWidth-1));
    y0 = std::max(0, std::min(y0, theWindowHeight-1));
    x1 = std::max(0, std::min(x1, theWindowWidth-1));
    y1 = std::max(0, std::min(y1, theWindowHeight-1));
    
    ImGui::Text("Window size: %d x %d", theWindowWidth, theWindowHeight);
    ImGui::Separator();
    
    if (ImGui::Button("Generate Line")) {
        generateBressenhamLine(x0, y0, x1, y1);
        createLineBuffer();
        drawLine = true;
    }
    
    ImGui::SameLine();
    
    if (ImGui::Button("Clear")) {
        drawLine = false;
    }
    
    if (drawLine && !bressenhamPoints.empty()) {
        ImGui::Separator();
        ImGui::Text("Line Information:");
        ImGui::Text("Endpoints: (%d, %d) to (%d, %d)", x0, y0, x1, y1);
        ImGui::Text("Points plotted: %zu", bressenhamPoints.size());
        
        float dx = x1 - x0;
        float dy = y1 - y0;
        ImGui::Text("Slope: %.2f", (dx != 0) ? dy/dx : INFINITY);
        
        ImGui::Text("Quadrant info: ");
        ImGui::SameLine();
        
        int centerX = theWindowWidth / 2;
        int centerY = theWindowHeight / 2;
        
        bool q1 = false, q2 = false, q3 = false, q4 = false;
        
        if ((x0 >= centerX && y0 <= centerY) || (x1 >= centerX && y1 <= centerY))
            q1 = true;
        if ((x0 <= centerX && y0 <= centerY) || (x1 <= centerX && y1 <= centerY))
            q2 = true;
        if ((x0 <= centerX && y0 >= centerY) || (x1 <= centerX && y1 >= centerY))
            q3 = true;
        if ((x0 >= centerX && y0 >= centerY) || (x1 >= centerX && y1 >= centerY))
            q4 = true;
            
        if (q1) ImGui::Text("Q1 "); ImGui::SameLine();
        if (q2) ImGui::Text("Q2 "); ImGui::SameLine();
        if (q3) ImGui::Text("Q3 "); ImGui::SameLine();
        if (q4) ImGui::Text("Q4");
        
        if (ImGui::TreeNode("Sample Points")) {
            int numToShow = std::min(10, (int)bressenhamPoints.size());
            for (int i = 0; i < numToShow; i++) {
                ImGui::Text("Point %d: (%d, %d)", i, bressenhamPoints[i].first, bressenhamPoints[i].second);
            }
            
            if (bressenhamPoints.size() > numToShow) {
                ImGui::Text("... and %zu more points", bressenhamPoints.size() - numToShow);
            }
            
            ImGui::TreePop();
        }
    }
    
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
    glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);

    GLFWwindow *window = glfwCreateWindow(theWindowWidth, theWindowHeight, 
                                         "Bresenham Line Algorithm", NULL, NULL);
    glfwSetWindowPos(window, theWindowPositionX, theWindowPositionY);
    glfwMakeContextCurrent(window);

    if (!window) {
        glfwTerminate();
        return 0;
    }

    glewExperimental = GL_TRUE;
    glewInit();
    printf("GL version: %s\n", glGetString(GL_VERSION));
    onInit(argc, argv);

    InitImGui(window);

    glfwSetKeyCallback(window, key_callback);

    while (!glfwWindowShouldClose(window)) {
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);  // Dark gray background for better contrast
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        onDisplay();
        RenderImGui();

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}