#pragma once

#include "graphicalInterface.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>


struct ClipmapLevel;
struct RenderBlock;

// Constants
inline constexpr int L = 8; // Quantity of detail levels (LOD)
inline constexpr int N = 255; // The size of the clipmap texture (2^8 - 1)
inline constexpr int BLOCK_SIZE = 64; // Rendering block size (N+1)/4

// Global variables declarations (defined in graphicalInterface.cpp)
extern GLuint terrainShaderProgram;
extern GLuint updateShaderProgram;

// Camera and controls
extern glm::vec3 cameraPos;
extern glm::vec3 cameraFront;
extern glm::vec3 cameraUp;
extern float horizontalAngle;
extern float verticalAngle;
extern float cameraSpeed;

// Clipmap levels and geometry
extern std::vector<ClipmapLevel> levels;
extern std::vector<RenderBlock> blocks;
extern std::vector<RenderBlock> fixupStrips;
extern std::vector<RenderBlock> interiorTrims;
