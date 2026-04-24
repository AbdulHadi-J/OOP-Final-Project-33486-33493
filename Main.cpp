#include <iostream>
#include <vector>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "FastNoiseLite.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

// GLOBALS
static float lastX = 600, lastY = 400;
static bool firstMouse = true;
static float deltaTime = 0.0f, lastFrame = 0.0f;

// SHADERS
static const char* vertexShaderSource = R"glsl(
    #version 330 core
    layout (location = 0) in vec3 aPos;
    layout (location = 1) in vec3 aNormal;
    out vec3 Normal;
    out vec3 FragPos;
    uniform mat4 model; uniform mat4 view; uniform mat4 projection;
    void main() {
        FragPos = vec3(model * vec4(aPos, 1.0));
        Normal = mat3(transpose(inverse(model))) * aNormal;
        gl_Position = projection * view * vec4(FragPos, 1.0);
    }
)glsl";

static const char* fragmentShaderSource = R"glsl(
    #version 330 core
    out vec4 FragColor;
    in vec3 Normal;
    in vec3 FragPos;
    uniform vec3 lightDir;
    uniform float planetRadius;

    void main() {
        vec3 norm = normalize(Normal);
        float diff = max(dot(norm, normalize(lightDir)), 0.15);
        
        float altitude = length(FragPos);
        vec3 color;

        if (altitude < planetRadius + 0.3) color = vec3(0.05, 0.2, 0.5);      // Water
        else if (altitude < planetRadius + 0.8) color = vec3(0.7, 0.6, 0.4); // Sand
        else if (altitude < planetRadius + 3.5) color = vec3(0.1, 0.4, 0.1); // Grass
        else color = vec3(0.9, 0.9, 1.0);                                   // Snow

        FragColor = vec4(color * diff, 1.0);
    }
)glsl";

// CAMERA CLASS
enum Camera_Movement { FORWARD, BACKWARD, LEFT, RIGHT };
class Camera 
{
public:
    glm::vec3 Position;
    glm::vec3 Front = glm::vec3(0.0f, 0.0f, -1.0f);
    glm::vec3 Up; glm::vec3 Right;
    glm::vec3 WorldUp = glm::vec3(0.0f, 1.0f, 0.0f);
    float Yaw = -90.0f, Pitch = 0.0f, MovementSpeed = 60.0f;

    Camera(glm::vec3 pos) : Position(pos) 
        { updateVectors(); }
    glm::mat4 GetViewMatrix() 
        { return glm::lookAt(Position, Position + Front, Up); }
    void ProcessKeyboard(Camera_Movement dir, float dt) 
    {
        float v = MovementSpeed * dt;
        if (dir == FORWARD) Position += Front * v; if (dir == BACKWARD) Position -= Front * v;
        if (dir == LEFT) Position -= Right * v; if (dir == RIGHT) Position += Right * v;
    }
    void ProcessMouse(float xoff, float yoff) 
    {
        Yaw += xoff * 0.1f; Pitch += yoff * 0.1f;
        if (Pitch > 89.0f) Pitch = 89.0f; if (Pitch < -89.0f) Pitch = -89.0f;
        updateVectors();
    }
private:
    void updateVectors() 
    {
        glm::vec3 f;
        f.x = cos(glm::radians(Yaw)) * cos(glm::radians(Pitch));
        f.y = sin(glm::radians(Pitch));
        f.z = sin(glm::radians(Yaw)) * cos(glm::radians(Pitch));
        Front = glm::normalize(f);
        Right = glm::normalize(glm::cross(Front, WorldUp));
        Up = glm::normalize(glm::cross(Right, Front));
    }
};

static Camera camera(glm::vec3(0.0f, 0.0f, 180.0f));

// PLANET FACE CLASS
struct Vertex 
{
    glm::vec3 Position;
    glm::vec3 Normal;
};

class PlanetFace 
{
public:
    unsigned int VAO, VBO, EBO;
    int indexCount;
    int resolution;
    glm::vec3 localUp;

    PlanetFace(int res, glm::vec3 up) : resolution(res), localUp(up) 
    {
        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        glGenBuffers(1, &EBO);
    }

    void constructMesh(float radius, FastNoiseLite& noise, float heightMult) 
    {
        std::vector<Vertex> verts;
        std::vector<unsigned int> indices;
        glm::vec3 axisA = glm::vec3(localUp.y, localUp.z, localUp.x);
        glm::vec3 axisB = glm::cross(localUp, axisA);

        for (int y = 0; y < resolution; y++) 
        {
            for (int x = 0; x < resolution; x++) 
            {
                int i = x + y * resolution;
                glm::vec2 percent = glm::vec2(x, y) / (float)(resolution - 1);
                glm::vec3 pointOnUnitCube = localUp + (percent.x - 0.5f) * 2.0f * axisA + (percent.y - 0.5f) * 2.0f * axisB;
                glm::vec3 pointOnUnitSphere = glm::normalize(pointOnUnitCube);

                float n = noise.GetNoise(pointOnUnitSphere.x * 100.0f, pointOnUnitSphere.y * 100.0f, pointOnUnitSphere.z * 100.0f);
                float elevation = radius + (n * heightMult);

                verts.push_back({ pointOnUnitSphere * elevation, pointOnUnitSphere });

                if (x < resolution - 1 && y < resolution - 1) 
                {
                    indices.push_back(i); indices.push_back(i + resolution + 1); indices.push_back(i + resolution);
                    indices.push_back(i); indices.push_back(i + 1); indices.push_back(i + resolution + 1);
                }
            }
        }
        indexCount = (int)indices.size();
        glBindVertexArray(VAO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(Vertex), verts.data(), GL_DYNAMIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_DYNAMIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0); glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)sizeof(glm::vec3)); glEnableVertexAttribArray(1);
    }

    void draw() 
    {
        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, 0);
    }
};

// MAIN PLANET CLASS
class ProceduralPlanet
{
private:
    std::vector<PlanetFace*> faces;
    FastNoiseLite noise;
    int resolution;

public:
    float radius;
    float mountainHeight = 10.0f;
    float mountainFreq = 0.012f;

    ProceduralPlanet(float r, int res) : radius(r), resolution(res) 
    {
        noise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
        noise.SetFrequency(mountainFreq);
        noise.SetFractalType(FastNoiseLite::FractalType_FBm);
        noise.SetFractalOctaves(5);

        glm::vec3 directions[] = { glm::vec3(0,1,0), glm::vec3(0,-1,0), glm::vec3(-1,0,0), glm::vec3(1,0,0), glm::vec3(0,0,1), glm::vec3(0,0,-1) };
        for (int i = 0; i < 6; i++) 
        {
            faces.push_back(new PlanetFace(resolution, directions[i]));
        }
        updateMesh();
    }

    void updateMesh()
    {
        noise.SetFrequency(mountainFreq);
        for (auto f : faces) f->constructMesh(radius, noise, mountainHeight);
    }

    void renderUI() 
    {
        ImGui::Begin("Planet Editor");
        if (ImGui::SliderFloat("Mountain Height", &mountainHeight, 0.0f, 40.0f)) updateMesh();
        if (ImGui::SliderFloat("Noise Frequency", &mountainFreq, 0.001f, 0.05f)) updateMesh();
        ImGui::Text("Hold L-CTRL for UI, L-ALT for Camera");
        ImGui::End();
    }

    void draw() { for (auto f : faces) f->draw(); }
};

// CALLBACKS
void mouse_callback(GLFWwindow* w, double x, double y) 
{
    if (glfwGetInputMode(w, GLFW_CURSOR) == GLFW_CURSOR_DISABLED) 
    {
        if (firstMouse) { lastX = (float)x; lastY = (float)y; firstMouse = false; }
        camera.ProcessMouse((float)x - lastX, lastY - (float)y);
        lastX = (float)x; lastY = (float)y;
    }
}

int main() 
{
    glfwInit();
    GLFWwindow* window = glfwCreateWindow(1200, 800, "Full Procedural Planet", NULL, NULL);
    glfwMakeContextCurrent(window);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    glfwSetCursorPosCallback(window, mouse_callback);
    gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
    glEnable(GL_DEPTH_TEST);

    // IMGUI SETUP
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    ProceduralPlanet planet(60.0f, 100);

    unsigned int vs = glCreateShader(GL_VERTEX_SHADER); glShaderSource(vs, 1, &vertexShaderSource, NULL); glCompileShader(vs);
    unsigned int fs = glCreateShader(GL_FRAGMENT_SHADER); glShaderSource(fs, 1, &fragmentShaderSource, NULL); glCompileShader(fs);
    unsigned int prog = glCreateProgram(); glAttachShader(prog, vs); glAttachShader(prog, fs); glLinkProgram(prog);

    while (!glfwWindowShouldClose(window)) {
        float c = (float)glfwGetTime(); deltaTime = c - lastFrame; lastFrame = c;

        // Input Management
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) glfwSetWindowShouldClose(window, true);
        if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        if (glfwGetKey(window, GLFW_KEY_LEFT_ALT) == GLFW_PRESS) glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

        if (glfwGetInputMode(window, GLFW_CURSOR) == GLFW_CURSOR_DISABLED) 
        {
            if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) camera.ProcessKeyboard(FORWARD, deltaTime);
            if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) camera.ProcessKeyboard(BACKWARD, deltaTime);
            if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) camera.ProcessKeyboard(LEFT, deltaTime);
            if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) camera.ProcessKeyboard(RIGHT, deltaTime);
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        glClearColor(0.02f, 0.02f, 0.04f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(prog);
        glm::mat4 p = glm::perspective(glm::radians(45.0f), 1200.0f / 800.0f, 0.1f, 2000.0f);
        glm::mat4 v = camera.GetViewMatrix();
        glm::mat4 m = glm::rotate(glm::mat4(1.0f), (float)glfwGetTime() * 0.05f, glm::vec3(0, 1, 0));

        glUniformMatrix4fv(glGetUniformLocation(prog, "projection"), 1, GL_FALSE, glm::value_ptr(p));
        glUniformMatrix4fv(glGetUniformLocation(prog, "view"), 1, GL_FALSE, glm::value_ptr(v));
        glUniformMatrix4fv(glGetUniformLocation(prog, "model"), 1, GL_FALSE, glm::value_ptr(m));
        glUniform3f(glGetUniformLocation(prog, "lightDir"), 1.0f, 1.0f, 1.0f);
        glUniform1f(glGetUniformLocation(prog, "planetRadius"), planet.radius);

        planet.draw();

        planet.renderUI();
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window); glfwPollEvents();
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwTerminate();
    return 0;
}