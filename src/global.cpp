#include "global.h"

// Global variables definitions
GLuint terrainShaderProgram;
GLuint updateShaderProgram;

// Camera and controls
glm::vec3 cameraPos = glm::vec3(0.0f, 500.0f, 300.0f);
glm::vec3 cameraFront = glm::vec3(0.0f, -0.5f, -1.0f);
glm::vec3 cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);
float horizontalAngle = -90.0f;
float verticalAngle = -25.0f;
float cameraSpeed = 50.0f;