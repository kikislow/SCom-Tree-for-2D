#include "cameraControl.h"


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
void processInput(GLFWwindow* window) {
    // Acceleration when the Shift is pressed
    float currentSpeed = cameraSpeed;
    if(glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
        currentSpeed *= 3.0f;

    // Movement to the left and to the right
    if(glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        cameraPos += currentSpeed * cameraFront;
    if(glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        cameraPos -= currentSpeed * cameraFront;

    // Movement forward and backward
    if(glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        cameraPos -= glm::normalize(glm::cross(cameraFront, cameraUp)) * currentSpeed;
    if(glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        cameraPos += glm::normalize(glm::cross(cameraFront, cameraUp)) * currentSpeed;

    // Movement up and down
    if(glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS)
        cameraPos += currentSpeed * cameraUp;
    if(glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS)
        cameraPos -= currentSpeed * cameraUp;
    
    // Camera rotation using the arrows
    if(glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS) {
        horizontalAngle -= 1.0f;
        calculateCameraDirection();
    }
    if(glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS) {
        horizontalAngle += 1.0f;
        calculateCameraDirection();
    }
    if(glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS) {
        verticalAngle += 1.0f;
        if(verticalAngle > 89.0f) verticalAngle = 89.0f;
        calculateCameraDirection();
    }
    if(glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS) {
        verticalAngle -= 1.0f;
        if(verticalAngle < -89.0f) verticalAngle = -89.0f;
        calculateCameraDirection();
    }
    
    // Reset to the original camera position
    if(glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS) {
        cameraPos = glm::vec3(0.0f, 200.0f, 300.0f);
        horizontalAngle = -90.0f;
        verticalAngle = -15.0f;
        calculateCameraDirection();
    }
}