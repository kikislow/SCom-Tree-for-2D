#include <glad/glad.h>
#include <iostream>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>


// Window parameters
const int windowSizeX = 640;
const int windowSizeY = 480;

// Shaders; executed on the GPU
const char* vertexShaderSource = "#version 460 core\n"
    "layout (location = 0) in vec3 aPos;\n"
    "uniform mat4 model;\n"
    "uniform mat4 view;\n"
    "uniform mat4 projection;\n"
    "void main()\n"
    "{\n"
    "   gl_Position = projection * view * model * vec4(aPos, 1.0);\n"
    "}\0";

const char* fragmentShaderSource = "#version 460 core\n"
    "out vec4 FragColor;\n"
    "void main()\n"
    "{\n"
    "   FragColor = vec4(1.0f, 0.5f, 0.2f, 1.0f);\n"
    "}\0";

// Camera Parameters
glm::vec3 cameraPos = glm::vec3(0.0f, 0.0f, 3.0f); // Initial camera position
glm::vec3 cameraFront = glm::vec3(0.0f, 0.0f, -1.0f); // Direction of view
glm::vec3 cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);
float cameraSpeed = 0.05f;
float cameraRotationSpeed = 1.0f;
float horizontalAngle = -90.0f;
float verticalAngle = 0.0f;

// Window closing function
void glfwClose(GLFWwindow* pWindow, int key, int scancode, int action, int mode){
    if(key == GLFW_KEY_ESCAPE){
        glfwSetWindowShouldClose(pWindow, GL_TRUE);
    }
}

/*
    Converts the Euler angles (horizontalAngle/verticalAngle) to a direction vector
    Spherical coordinates are used to calculate
*/
void calculateCameraDirection() {
    glm::vec3 front;
    front.x = cos(glm::radians(horizontalAngle)) * cos(glm::radians(verticalAngle));
    front.y = sin(glm::radians(verticalAngle));
    front.z = sin(glm::radians(horizontalAngle)) * cos(glm::radians(verticalAngle));
    cameraFront = glm::normalize(front);
}

/*
    Processing keyboard input to control the camera
*/
void processInput(GLFWwindow* window){
    // Movement to the left and to the right
    if(glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        cameraPos += cameraSpeed * cameraFront;
    if(glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        cameraPos -= cameraSpeed * cameraFront;

    // Movement forward and backward
    if(glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        cameraPos -= glm::normalize(glm::cross(cameraFront, cameraUp)) * cameraSpeed;
    if(glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        cameraPos += glm::normalize(glm::cross(cameraFront, cameraUp)) * cameraSpeed;
    
    // Movement up and down
    if(glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS)
        cameraPos += cameraSpeed * cameraUp;
    if(glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS)
        cameraPos -= cameraSpeed * cameraUp;
    
    // Camera rotation using the arrows
    if(glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS) {
        horizontalAngle -= cameraRotationSpeed;
        calculateCameraDirection();
    }
    if(glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS) {
        horizontalAngle += cameraRotationSpeed;
        calculateCameraDirection();
    }
    if(glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS) {
        verticalAngle += cameraRotationSpeed;
        if(verticalAngle > 89.0f) // To avoid overturning the camera
            verticalAngle = 89.0f;
        calculateCameraDirection();
    }
    if(glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS) {
        verticalAngle -= cameraRotationSpeed;
        if(verticalAngle < -89.0f)
            verticalAngle = -89.0f;
        calculateCameraDirection();
    }

    // Reset to the original camera position
    if(glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS) {
        cameraPos = glm::vec3(0.0f, 0.0f, 3.0f);
        horizontalAngle = -90.0f;
        verticalAngle = 0.0f;
        calculateCameraDirection();
    }
}

void drawTriangle(GLuint shaderProgram, GLuint VAO, const glm::mat4& model, 
                 const glm::mat4& view, const glm::mat4& projection){
    glUseProgram(shaderProgram);

    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));

    glBindVertexArray(VAO);
    glDrawArrays(GL_TRIANGLES, 0, 3);
}


/*
    Initializing a triangle, creating shaders, and uploading data to the GPU
*/
void initTriangle(GLuint& shaderProgram, GLuint& VAO, GLuint& VBO){
    // 3D coordinates for the triangle
    GLfloat verticesTriangle[] = {
        -0.5f, -0.5f, 0.0f,
        0.5f, -0.5f, 0.0f,
        0.0f, 0.5f, 0.0f
    };

    // Creating and compiling shaders
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
    glCompileShader(vertexShader);

    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
    glCompileShader(fragmentShader);

    // Creating a shader program and linking
    shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    // Creating buffers on the GPU
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verticesTriangle), verticesTriangle, GL_STATIC_DRAW);

    // Configuring the Vertex position attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*) 0);
    glEnableVertexAttribArray(0);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

void windowDisplay(){
    if(!glfwInit()){
        std::cout << "glfwInit failed!" << std::endl;
        return;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* pWindow = glfwCreateWindow(windowSizeX, windowSizeY, "SCom Tree", NULL, NULL);
    if(!pWindow){
        std::cout << "pWindow failed!" << std::endl;
        glfwTerminate();
        return;
    }

    glfwSetKeyCallback(pWindow, glfwClose);
    glfwMakeContextCurrent(pWindow);

    if(!gladLoadGL()){
        std::cout << "Can'tload GLAD!" << std::endl;
        return;
    }

    std::cout << "Renderer: " << glGetString(GL_RENDERER) << std::endl;
    std::cout << "OpenGL version:" << glGetString(GL_VERSION) << std::endl;

    GLuint shaderProgram;
    GLuint VAO, VBO;    
    initTriangle(shaderProgram, VAO, VBO);

    glEnable(GL_DEPTH_TEST);
    glClearColor(0, 1, 0, 1);

    std::cout << "--Camera control---" << std::endl; 
    std::cout << "W/S - forward/backward" << std::endl; 
    std::cout << "A/D - left/right" << std::endl;
    std::cout << "Q/E - up/down" << std::endl;
    std::cout << "Arrows - camera rotation" << std::endl;
    std::cout << "R - reset to the original camera position" << std::endl;
    std::cout << "ESC - exit (completion of the program)" << std::endl;


    while(!glfwWindowShouldClose(pWindow)){
        processInput(pWindow);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Calculating transformation matrices for the current frame
        glm::mat4 projection = glm::perspective(glm::radians(45.0f), 
                                              (float)windowSizeX/(float)windowSizeY, 
                                              0.1f, 100.0f); // Projection matrix - camera view
        glm::mat4 view = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp); // View matrix - сamera position and direction
        glm::mat4 model = glm::mat4(1.0f); // Model matrix - object position and direction (the object is in place)

        drawTriangle(shaderProgram, VAO, model, view, projection);

        glfwSwapBuffers(pWindow); // Double buffering
        glfwPollEvents();
    }

    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteProgram(shaderProgram);

    glfwDestroyWindow(pWindow);

    glfwTerminate();
}