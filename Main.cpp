#include <iostream>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <vector>

const char* vertexShaderSource = "#version 330 core\n"
"layout (location = 0) in vec3 aPos;\n"
"void main()\n"
"{\n"
"   gl_Position = vec4(aPos.x, aPos.y, aPos.z, 1.0);\n"
"}\0";

const char* fragmentShaderSource = "#version 330 core\n"
"out vec4 FragColor;\n"
"void main()\n"
"{\n"
"   FragColor = vec4(1.0f, 0.5f, 0.2f, 1.0f); // Bright Orange\n"
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

int main()
{
	// this is to initialize the window and OpenGL context
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(800, 600, "Mesh Test", NULL, NULL);
    if (!window) { glfwTerminate(); return -1; }
    glfwMakeContextCurrent(window);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) return -1;

    // this compiles the shaders to create the shader program
    unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
    glCompileShader(vertexShader);

    unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
    glCompileShader(fragmentShader);

    unsigned int shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    
    std::vector<Vertex> vertices = {
        {{-0.5f, -0.5f, 0.0f}, {0,0,1}, {0,0}}, // Bottom Left
        {{ 0.5f, -0.5f, 0.0f}, {0,0,1}, {1,0}}, // Bottom Right
        {{ 0.0f,  0.5f, 0.0f}, {0,0,1}, {0.5, 1}} // Top Center
    };
    std::vector<unsigned int> indices = { 0, 1, 2 };

    // this creates the mesh
    Mesh testMesh(vertices, indices);

    // this creates the window
    while (!glfwWindowShouldClose(window))
    {
        // pressing escape closes window
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            glfwSetWindowShouldClose(window, true);

        // Background Color
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        // DRAWING
        glUseProgram(shaderProgram);
        testMesh.Draw();

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // this is to cleanup the shaders and terminate the window
    glDeleteProgram(shaderProgram);
    glfwTerminate();
    return 0;
}