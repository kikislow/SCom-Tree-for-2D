#pragma once

// #include "graphicalInterface.h"
#include "global.h"

#include <iostream>
#include <vector>

struct ClipmapLevel {
    // Textures for data storage
    GLuint elevationTexture; // Height Texture (R32F)
    GLuint normalTexture; // Texture of normals (RGBA8)
    
    // Toroidal coordinates
    glm::ivec2 textureOffset; // Offset in the texture
    
    // Level Parameters
    float scale;
    glm::vec2 worldOffset;
    bool active;
    
    // Statistics for debugging
    int updateCount;
};

struct RenderBlock {
    GLuint VAO, VBO, EBO; // Vertex Array, Vertex Buffer, Element Buffer
    int indexCount; // The number of indexes to draw
    glm::ivec2 blockOffset; // Block offset in the grid
};


void createRenderBlock(RenderBlock& block, int startX, int startZ, int sizeX, int sizeZ);
void createGeometryBlocks();
void createLevelTextures(ClipmapLevel& level, int levelIndex);
void initClipmapLevels();
void updateClipmapLevels();
void renderClipmapLevel(int levelIndex, const glm::mat4& model, 
                       const glm::mat4& view, const glm::mat4& projection);

extern std::vector<ClipmapLevel> levels;
extern std::vector<RenderBlock> blocks;
extern std::vector<RenderBlock> fixupStrips;
extern std::vector<RenderBlock> interiorTrims;