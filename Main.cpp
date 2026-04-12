#include <iostream>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <vector>
#include <cmath>
#include <glm/gtc/constants.hpp>

float halfPi = glm::half_pi<float>();
float pi = glm::pi<float>();       

const char* vertexShaderSource = "#version 330 core\n"
"layout (location = 0) in vec3 aPos;\n"
"layout (location = 1) in vec3 aNormal;\n"
"out vec3 Normal;\n"
"out vec3 FragPos;\n"
"uniform mat4 model;\n"
"uniform mat4 view;\n"
"uniform mat4 projection;\n"
"void main() {\n"
"   FragPos = vec3(model * vec4(aPos, 1.0));\n"
"   Normal = mat3(transpose(inverse(model))) * aNormal;\n"
"   gl_Position = projection * view * model * vec4(aPos, 1.0);\n"
"}\0";

const char* fragmentShaderSource = "#version 330 core\n"
"out vec4 FragColor;\n"
"in vec3 Normal;\n"
"in vec3 FragPos;\n"
"void main() {\n"
"   // Basic Ambient Light\n"
"   float ambientStrength = 0.2;\n"
"   vec3 lightColor = vec3(1.0, 1.0, 1.0);\n"
"   vec3 objectColor = vec4(1.0f, 0.5f, 0.2f, 1.0f).rgb; // Your orange\n"
"   \n"
"   // Diffuse Light (The part that adds depth)\n"
"   vec3 norm = normalize(Normal);\n"
"   vec3 lightDir = normalize(vec3(1.0, 1.0, 1.0)); // Light coming from a corner\n"
"   float diff = max(dot(norm, lightDir), 0.0);\n"
"   vec3 diffuse = diff * lightColor;\n"
"   \n"
"   vec3 result = (ambientStrength + diffuse) * objectColor;\n"
"   FragColor = vec4(result, 1.0);\n"
"}\n\0";

struct Vertex 
{
    glm::vec3 Position;
    glm::vec3 Normal;
    glm::vec2 TexCoords;
};

class Mesh
{
private:
    unsigned int VAO, VBO, EBO;
    void setupMesh()
    {
        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        glGenBuffers(1, &EBO);

        glBindVertexArray(VAO);

        // copies vertex data from RAM to GPU 
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), &vertices[0], GL_STATIC_DRAW);

        // uploads the order in which the vertices should be drawn
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), &indices[0], GL_STATIC_DRAW);

        // tells GPU how to interpret the vertex data
        glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);

        // same as the previous line but with position data instead
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, Normal));

		// defines how to interpret the texture coordinate data
		glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, TexCoords));

		glBindVertexArray(0);
    }

public:
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;

	// mesh constructor to initialize the vertex and index data, and set up the buffers
    Mesh(std::vector<Vertex> vertices, std::vector<unsigned int> indices)
    {
        this->vertices = vertices;
        this->indices = indices;
		setupMesh();
    }

    // mesh destructor to clean up buffers
    ~Mesh()
    {
        glDeleteVertexArrays(1, &VAO);
        glDeleteBuffers(1, &VBO);
		glDeleteBuffers(1, &EBO);
    }

	// this function draws the mesh using the vertex array and index data
    void Draw()
    {
		glBindVertexArray(VAO);
		glDrawElements(GL_TRIANGLES, static_cast<unsigned int>(indices.size()), GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
    }

	// this function updates the vertex buffer with new vertex data
    void updateBuffers()
    {
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0, vertices.size() * sizeof(Vertex), &vertices[0]);
    }
};

class Planet
{
public:
    Mesh* mesh;
    float radius;

    Planet(float radius, int rings, int sectors)
    {
        std::vector<Vertex> verts;
        std::vector<unsigned int> inds;

        // Sphere generation logic using latitude and longitude [cite: 6]
        for (int r = 0; r < rings; ++r) 
        {
            float phi = -halfPi + pi * (float)r / (rings - 1);
            for (int s = 0; s < sectors; ++s) 
            {
                float theta = 2.0f * pi * (float)s / (sectors - 1);

                Vertex v;
                v.Position.x = cos(phi) * cos(theta) * radius;
                v.Position.y = sin(phi) * radius;
                v.Position.z = cos(phi) * sin(theta) * radius;
                v.Normal = glm::normalize(v.Position);
                verts.push_back(v);
            }
    }

        for (int r = 0; r < rings - 1; ++r) 
        {
            for (int s = 0; s < sectors - 1; ++s) 
            {
                inds.push_back(r * sectors + s);
                inds.push_back((r + 1) * sectors + s);
                inds.push_back((r + 1) * sectors + (s + 1));
                inds.push_back(r * sectors + s);
                inds.push_back((r + 1) * sectors + (s + 1));
                inds.push_back(r * sectors + (s + 1));
            }
        }
        mesh = new Mesh(verts, inds);
    }

    void Draw() { mesh->Draw(); }
};

int main() 
{
    glfwInit();
    GLFWwindow* window = glfwCreateWindow(800, 600, "Planet Generation", NULL, NULL);
    glfwMakeContextCurrent(window);
    gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
    glEnable(GL_DEPTH_TEST); // Needed for 3D

    // Shader compilation
    unsigned int vShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vShader, 1, &vertexShaderSource, NULL);
    glCompileShader(vShader);
    unsigned int fShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fShader, 1, &fragmentShaderSource, NULL);
    glCompileShader(fShader);
    unsigned int shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vShader);
    glAttachShader(shaderProgram, fShader);
    glLinkProgram(shaderProgram);

    Planet myPlanet(1.0f, 50, 50);

    while (!glfwWindowShouldClose(window)) {
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(shaderProgram);

        // Setup MVP Matrices for 3D view 
        glm::mat4 model = glm::rotate(glm::mat4(1.0f), (float)glfwGetTime(), glm::vec3(0, 1, 0));
        glm::mat4 view = glm::translate(glm::mat4(1.0f), glm::vec3(0, 0, -3.0f));
        glm::mat4 projection = glm::perspective(glm::radians(45.0f), 800.0f / 600.0f, 0.1f, 100.0f);

        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));

        float time = (float)glfwGetTime();
        model = glm::rotate(model, time, glm::vec3(0.0f, 1.0f, 0.0f)); // Rotate around Y axis
        model = glm::rotate(model, time * 0.5f, glm::vec3(1.0f, 0.0f, 0.0f)); // Rotate around X axis

        myPlanet.Draw();

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}