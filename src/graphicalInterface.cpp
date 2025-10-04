#include "graphicalInterface.h"
#include "cameraControl.h"
#include "global.h"
#include "clipmap.h"
#include "shaders.h"


void glfwClose(GLFWwindow* pWindow, int key, int scancode, int action, int mode) {
    if(key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        glfwSetWindowShouldClose(pWindow, GL_TRUE);
    }
}

/*
    Main function
*/
void windowDisplay() {
    if(!glfwInit()) {
        std::cout << "GLFW initialization failed!" << std::endl;
        return;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(1200, 800, "GPU Geometry Clipmaps Implementation", NULL, NULL);
    if(!window) {
        std::cout << "Window creation failed!" << std::endl;
        glfwTerminate();
        return;
    }

    glfwSetKeyCallback(window, glfwClose);
    glfwMakeContextCurrent(window);

    if(!gladLoadGL()) {
        std::cout << "GLAD initialization failed!" << std::endl;
        return;
    }

    std::cout << "OpenGL context: " << glGetString(GL_VERSION) << std::endl;
    std::cout << "GLSL version: " << glGetString(GL_SHADING_LANGUAGE_VERSION) << std::endl;

    std::cout << "Renderer: " << glGetString(GL_RENDERER) << std::endl;
    std::cout << "OpenGL version: " << glGetString(GL_VERSION) << std::endl;

    terrainShaderProgram = compileShaderProgram("shaders/terrain.vert", "shaders/terrain.frag");
    if(terrainShaderProgram == 0) {
        std::cout << "SHADER COMPILATION FAILED!" << std::endl;
        return;
    }
    else {
        std::cout << "Shader compiled successfully!" << std::endl;
    }
    
    initClipmapLevels();

    glEnable(GL_DEPTH_TEST);
    glClearColor(0.2f, 0.3f, 0.8f, 1.0f);

    std::cout << "--Camera control---" << std::endl; 
    std::cout << "W/S - forward/backward" << std::endl; 
    std::cout << "A/D - left/right" << std::endl;
    std::cout << "Q/E - up/down" << std::endl;
    std::cout << "Arrows - camera rotation" << std::endl;
    std::cout << "R - reset to the original camera position" << std::endl;
    std::cout << "ESC - exit (completion of the program)" << std::endl;

    // The main rendering loop
    while(!glfwWindowShouldClose(window)) {
        processInput(window);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        updateClipmapLevels();

        // static int debugCounter = 0;
        // if (debugCounter++ % 60 == 0) {
        //     std::cout << "Camera position: " << cameraPos.x << ", " << cameraPos.y << ", " << cameraPos.z << std::endl;
        //     std::cout << "Level 0 offset: " << levels[0].worldOffset.x << ", " << levels[0].worldOffset.y << std::endl;
        // }

        // Calculating transformation matrices for the current frame
        glm::mat4 projection = glm::perspective(glm::radians(60.0f), 1200.0f/800.0f, 0.1f, 10000.0f); // Projection matrix - camera view
        glm::mat4 view = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp); // View matrix - сamera position and direction
        glm::mat4 model = glm::mat4(1.0f); // Model matrix - object position and direction (the object is in place)

        // Debugging matrices
        // static int debugFrame = 0;
        // if (debugFrame++ % 60 == 0) {
        //     std::cout << "--- Camera Debug ---" << std::endl;
        //     std::cout << "Camera Pos: " << cameraPos.x << ", " << cameraPos.y << ", " << cameraPos.z << std::endl;
        //     std::cout << "Camera Front: " << cameraFront.x << ", " << cameraFront.y << ", " << cameraFront.z << std::endl;
        //     std::cout << "View matrix calculated" << std::endl;
        // }

        // Rendering of all levels of the clipmap
        // Rendering from rough to detailed levels - this is important for proper mixing and performance.
        for(int i = L - 1; i >= 0; i--) {
            renderClipmapLevel(i, model, view, projection);
        }

        glfwSwapBuffers(window); // Double buffering
        glfwPollEvents();
    }

    // Cleaning up resources
    // Removing textures of levels
    for(auto& level : levels) {
        glDeleteTextures(1, &level.elevationTexture);
        glDeleteTextures(1, &level.normalTexture);
    }
    
    // Deleting the geometry of the main blocks
    for(auto& block : blocks) {
        glDeleteVertexArrays(1, &block.VAO);
        glDeleteBuffers(1, &block.VBO);
        glDeleteBuffers(1, &block.EBO);
    }
    
    // Removing the geometry of the fix-up strips
    for(auto& strip : fixupStrips) {
        glDeleteVertexArrays(1, &strip.VAO);
        glDeleteBuffers(1, &strip.VBO);
        glDeleteBuffers(1, &strip.EBO);
    }
    
    // Removing the geometry of internal clippings
    for(auto& trim : interiorTrims) {
        glDeleteVertexArrays(1, &trim.VAO);
        glDeleteBuffers(1, &trim.VBO);
        glDeleteBuffers(1, &trim.EBO);
    }
    
    glDeleteProgram(terrainShaderProgram);
    glfwDestroyWindow(window);
    glfwTerminate();
}

int main(int argc, char* argv[]){
    windowDisplay();

    return 0;
}