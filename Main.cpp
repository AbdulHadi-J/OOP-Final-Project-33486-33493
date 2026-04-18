#include <iostream>
#include <vector>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "FastNoiseLite.h"

//GLOBALS
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
        float diff = max(dot(norm, normalize(lightDir)), 0.15); // Ambient + Diffuse
        
        float altitude = length(FragPos);
        vec3 color;

        if (altitude < planetRadius + 0.3) color = vec3(0.05, 0.2, 0.5);      // Water
        else if (altitude < planetRadius + 0.8) color = vec3(0.7, 0.6, 0.4); // Sand
        else if (altitude < planetRadius + 3.5) color = vec3(0.1, 0.4, 0.1); // Grass
        else color = vec3(0.9, 0.9, 1.0);                                   // Snow

        FragColor = vec4(color * diff, 1.0);
    }
)glsl";

//CAMERA CLASS
enum Camera_Movement { FORWARD, BACKWARD, LEFT, RIGHT };
class Camera 
{
public:
    glm::vec3 Position;
    glm::vec3 Front = glm::vec3(0.0f, 0.0f, -1.0f);
    glm::vec3 Up; glm::vec3 Right;
    glm::vec3 WorldUp = glm::vec3(0.0f, 1.0f, 0.0f);
    float Yaw = -90.0f, Pitch = 0.0f, MovementSpeed = 60.0f;

    Camera(glm::vec3 pos) : Position(pos) { updateVectors(); }
    glm::mat4 GetViewMatrix() { return glm::lookAt(Position, Position + Front, Up); }
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

//PLANET FACE CLASS
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

    PlanetFace(int res, glm::vec3 localUp, float radius, FastNoiseLite& noise) 
    {
        std::vector<Vertex> verts;
        std::vector<unsigned int> indices;
        glm::vec3 axisA = glm::vec3(localUp.y, localUp.z, localUp.x);
        glm::vec3 axisB = glm::cross(localUp, axisA);

        for (int y = 0; y < res; y++) 
        {
            for (int x = 0; x < res; x++) 
            {
                int i = x + y * res;
                glm::vec2 percent = glm::vec2(x, y) / (float)(res - 1);
                glm::vec3 pointOnUnitCube = localUp + (percent.x - 0.5f) * 2.0f * axisA + (percent.y - 0.5f) * 2.0f * axisB;
                glm::vec3 pointOnUnitSphere = glm::normalize(pointOnUnitCube);

                // Sample 3D noise using sphere coordinates
                float n = noise.GetNoise(pointOnUnitSphere.x * 100.0f, pointOnUnitSphere.y * 100.0f, pointOnUnitSphere.z * 100.0f);
                float elevation = radius + (n * 10.0f);

                verts.push_back({ pointOnUnitSphere * elevation, pointOnUnitSphere });

                if (x < res - 1 && y < res - 1) {
                    indices.push_back(i); indices.push_back(i + res + 1); indices.push_back(i + res);
                    indices.push_back(i); indices.push_back(i + 1); indices.push_back(i + res + 1);
                }
            }
        }
        indexCount = (int)indices.size();
        glGenVertexArrays(1, &VAO); glGenBuffers(1, &VBO); glGenBuffers(1, &EBO);
        glBindVertexArray(VAO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(Vertex), verts.data(), GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0); glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)sizeof(glm::vec3)); glEnableVertexAttribArray(1);
    }

    void draw() { glBindVertexArray(VAO); glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, 0); }
};

// MAIN PLANET CLASS
class ProceduralPlanet 
{
public:
    std::vector<PlanetFace*> faces;
    FastNoiseLite noise;
    float radius;

    ProceduralPlanet(float r, int res) : radius(r) 
    {
        noise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
        noise.SetFrequency(0.012f);
        noise.SetFractalType(FastNoiseLite::FractalType_FBm);
        noise.SetFractalOctaves(5);

        // Create the 6 sides of the cube
        glm::vec3 directions[] = { glm::vec3(0,1,0), glm::vec3(0,-1,0), glm::vec3(-1,0,0), glm::vec3(1,0,0), glm::vec3(0,0,1), glm::vec3(0,0,-1) };
        for (int i = 0; i < 6; i++) 
        {
            faces.push_back(new PlanetFace(res, directions[i], radius, noise));
        }
    }
    void draw() { for (auto f : faces) f->draw(); }
};

//CALLBACKS & MAIN
void mouse_callback(GLFWwindow* w, double x, double y) 
{
    if (firstMouse) 
    { 
        lastX = (float)x; lastY = (float)y;
        firstMouse = false; 
    }
    camera.ProcessMouse((float)x - lastX, lastY - (float)y);
    lastX = (float)x; lastY = (float)y;
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

    ProceduralPlanet planet(60.0f, 100); // Radius 60, Resolution 100 per face

    unsigned int vs = glCreateShader(GL_VERTEX_SHADER); glShaderSource(vs, 1, &vertexShaderSource, NULL); glCompileShader(vs);
    unsigned int fs = glCreateShader(GL_FRAGMENT_SHADER); glShaderSource(fs, 1, &fragmentShaderSource, NULL); glCompileShader(fs);
    unsigned int prog = glCreateProgram(); glAttachShader(prog, vs); glAttachShader(prog, fs); glLinkProgram(prog);

    while (!glfwWindowShouldClose(window)) 
    {
        float c = (float)glfwGetTime(); deltaTime = c - lastFrame; lastFrame = c;
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) glfwSetWindowShouldClose(window, true);
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) camera.ProcessKeyboard(FORWARD, deltaTime);
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) camera.ProcessKeyboard(BACKWARD, deltaTime);
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) camera.ProcessKeyboard(LEFT, deltaTime);
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) camera.ProcessKeyboard(RIGHT, deltaTime);

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
        glUniform1f(glGetUniformLocation(prog, "planetRadius"), 60.0f);

        planet.draw();
        glfwSwapBuffers(window); glfwPollEvents();
    }
    glfwTerminate(); return 0;
}