#include "clipmap.h"


std::vector<ClipmapLevel> levels;
std::vector<RenderBlock> blocks;
std::vector<RenderBlock> fixupStrips;
std::vector<RenderBlock> interiorTrims;

/*
    Creating a block for rendering
*/
void createRenderBlock(RenderBlock& block, int startX, int startZ, int sizeX, int sizeZ) {
    std::vector<glm::vec2> vertices;
    std::vector<unsigned> indices;
    
    // Creating a grid of vertices of size (sizeX+1) × (sizeZ+1)
    for(int z = 0; z <= sizeZ; z++) {
        for(int x = 0; x <= sizeX; x++) {
            vertices.push_back(glm::vec2(startX + x, startZ + z));
        }
    }
    
    // Creating indexes for triangles - two triangles for each quad
    for(int z = 0; z < sizeZ; z++) {
        for(int x = 0; x < sizeX; x++) {
            int tl = z * (sizeX + 1) + x;
            int tr = tl + 1;
            int bl = (z + 1) * (sizeX + 1) + x;
            int br = bl + 1;

            indices.push_back(tl);
            indices.push_back(bl);
            indices.push_back(tr);
            
            indices.push_back(tr);
            indices.push_back(bl);
            indices.push_back(br);
        }
    }
    
    glGenVertexArrays(1, &block.VAO);
    glGenBuffers(1, &block.VBO);
    glGenBuffers(1, &block.EBO);
    
    glBindVertexArray(block.VAO);
    
    glBindBuffer(GL_ARRAY_BUFFER, block.VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(glm::vec2), 
                 vertices.data(), GL_STATIC_DRAW);
    
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, block.EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int),
                 indices.data(), GL_STATIC_DRAW);
    
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    
    glBindVertexArray(0);
    
    // Save the number of indexes for rendering
    block.indexCount = indices.size();
    block.blockOffset = glm::ivec2(startX, startZ);
}

/*
    Creating all geometric blocks
    Create (instead of creating one large grid for each level) a set of small blocks that can be reused and rendered efficiently.

    Each level is divided into three types of geometry:
        - Main blocks (12 blocks) with an empty center (the empty center is filled with more detailed levels)
        - Fix-up strips (4 strips) to fill the gaps between levels
        - Internal clippings (4 blocks) for smooth transitions between LOD levels.
*/
void createGeometryBlocks() {
    blocks.clear();
    fixupStrips.clear();
    interiorTrims.clear();
    
    // The main blocks are 64x64 (for n = 255)
    // 
    int m = BLOCK_SIZE;
    int blockPositions[12][2] = {
        {0, 0}, {m, 0}, {2*m, 0}, {3*m, 0}, // Top row 
        {0, m},                     {3*m, m}, // Middle row (empty in the middle)
        {0, 2*m},                   {3*m, 2*m}, // Middle row
        {0, 3*m}, {m, 3*m}, {2*m, 3*m}, {3*m, 3*m} // Bottom row
    };
    
    for(int i = 0; i < 12; i++) {
        RenderBlock block;
        // Creating a block of size (m-1)×(m-1) so that there are common edges
        createRenderBlock(block, blockPositions[i][0], blockPositions[i][1], m-1, m-1);
        blocks.push_back(block);
    }
    
    // Fix-up stripes (3x64)
    int fixupPositions[4][2] = {
        {m-1, 0}, {2*m, 0},    // Upper horizontal stripes
        {m-1, 3*m}, {2*m, 3*m} // Lower horizontal stripes
    };
    
    for(int i = 0; i < 4; i++) {
        RenderBlock strip;
        createRenderBlock(strip, fixupPositions[i][0], fixupPositions[i][1], 3, m-1);
        fixupStrips.push_back(strip);
    }
    
    // Internal L-shaped stripes for smooth transitions between detail levels
    int trimPositions[4][2] = {
        {m, m}, {2*m-2, m},     // Left and right vertical
        {m, 2*m-2}, {2*m-2, 2*m-2} // Upper and lower horizontal
    };
    
    for(int i = 0; i < 4; i++) {
        RenderBlock trim;
        createRenderBlock(trim, trimPositions[i][0], trimPositions[i][1], m-2, m-2);
        interiorTrims.push_back(trim);
    }
}

/*
    Creating textures for the clipmap level

    Creates two specialized textures for each LOD level:
        1. Elevation texture - stores information about elevation heights
        2. The texture of the normals (normal texture) - stores information about the slopes of the surface
*/
void createLevelTextures(ClipmapLevel& level, int levelIndex) {
    // Height texture (single channel, 32-bit float)
    glGenTextures(1, &level.elevationTexture);
    glBindTexture(GL_TEXTURE_2D, level.elevationTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, N, N, 0, GL_RED, GL_FLOAT, nullptr);

    // Adjusting texture filtering and repetition
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT); // Toroidal addressing
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    
    // Texture of normals (4-channel, 8-bit per channel)
    glGenTextures(1, &level.normalTexture);
    glBindTexture(GL_TEXTURE_2D, level.normalTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, N, N, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    // Initializing the toroidal displacement
    level.textureOffset = glm::ivec2(0, 0);
}

/*
    Initialization of all levels of the clipmap
*/
void initClipmapLevels() {
    levels.resize(L);
    createGeometryBlocks();
    
    for(int i = 0; i < L; i++) {
        ClipmapLevel& level = levels[i];
        level.scale = pow(2.0f, i); // Level scale: 1, 2, 4, 8, 16, 32, 64, 128
        
        level.worldOffset = glm::vec2(0.0f, 0.0f); // The initial shift is in the center of the world
        
        level.active = true;
        level.updateCount = 0;
        
        createLevelTextures(level, i);
    }
    
    std::cout << "Camera starts at world center (0,0)" << std::endl;
    std::cout << "Initialized " << L << " clipmap levels with " << 
                blocks.size() << " render blocks" << std::endl;
}

/*
    Updating clipmap levels with triple addressing
*/
void updateClipmapLevels() {
    glm::vec2 viewerXZ = glm::vec2(cameraPos.x, cameraPos.z); // The observer's position in the XZ plane
    
    for(int i = 0; i < L; i++) {
        ClipmapLevel& level = levels[i];
        
        // Calculating the new offset as an integer to avoid artifacts
        // So that the geometry does not "shake" at the subpixel level
        float gridSpacing = 5.0f * level.scale; // The distance between the vertices of the grid
        glm::ivec2 gridCoords = glm::ivec2(
            floor(viewerXZ.x / gridSpacing),
            floor(viewerXZ.y / gridSpacing)
        );
        
        // New level shift in world coordinates
        glm::vec2 newWorldOffset = glm::vec2(gridCoords) * gridSpacing;
        
        // If the offset has changed, update the level
        // if(level.worldOffse<t != newWorldOffset) {
        //     level.worldOffset = newWorldOffset;
        //     level.updateCount++;

        //     static int updateCounter = 0;
        //     if(updateCounter++ % 30 == 0 && i == 0) {
        //         std::cout << "Level " << i << " updated to: " 
        //                   << newWorldOffset.x << ", " << newWorldOffset.y << std::endl;
        //     }
        // }>
        
        level.active = true;
    }
}

/*
    The rendering of only one level function
    The function is responsible for rendering all geometric components of the same LOD level with the correct scale and offset parameters.
*/
void renderClipmapLevel(int levelIndex, const glm::mat4& model, 
                       const glm::mat4& view, const glm::mat4& projection) {
    // Verification: the level exists and is active
    if(levelIndex >= L || !levels[levelIndex].active)
        return;
    
    ClipmapLevel& level = levels[levelIndex];
    
    glUseProgram(terrainShaderProgram);
    
    // Passing uniform variables to the shader
    // Setting uniform variables
    glUniformMatrix4fv(glGetUniformLocation(terrainShaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));
    glUniformMatrix4fv(glGetUniformLocation(terrainShaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(glGetUniformLocation(terrainShaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
    
    // Level Parameters
    float renderScale = 5.0f * level.scale; // Scale with a base multiplier
    glm::vec2 renderOffset = level.worldOffset; // Level shift
    
    glUniform1f(glGetUniformLocation(terrainShaderProgram, "levelScale"), renderScale);
    glUniform2fv(glGetUniformLocation(terrainShaderProgram, "levelOffset"), 1, glm::value_ptr(renderOffset));
    glUniform1i(glGetUniformLocation(terrainShaderProgram, "levelIndex"), levelIndex);
    
    // Rendering blocks
    for(auto& block : blocks) {
        glBindVertexArray(block.VAO);
        glDrawElements(GL_TRIANGLES, block.indexCount, GL_UNSIGNED_INT, 0);
    }
    
    // Rendering of fix strips
    for(auto& strip : fixupStrips) {
        glBindVertexArray(strip.VAO);
        glDrawElements(GL_TRIANGLES, strip.indexCount, GL_UNSIGNED_INT, 0);
    }
    
    // Rendering of internal Trimmings
    for(auto& trim : interiorTrims) {
        glBindVertexArray(trim.VAO);
        glDrawElements(GL_TRIANGLES, trim.indexCount, GL_UNSIGNED_INT, 0);
    }
    
    glBindVertexArray(0);
}
