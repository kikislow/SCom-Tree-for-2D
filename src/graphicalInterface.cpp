#include <glad/glad.h>
#include <iostream>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <vector>
#include <cmath>
#include <random>

const int L = 8; // Quantity of detail levels (LOD)
const int N = 255; // The size of the clipmap texture (2^8 - 1)
const int BLOCK_SIZE = 64; // Rendering block size (N+1)/4

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

std::vector<ClipmapLevel> levels;
std::vector<RenderBlock> blocks;
std::vector<RenderBlock> fixupStrips;
std::vector<RenderBlock> interiorTrims;

GLuint terrainShaderProgram;
GLuint updateShaderProgram; // Shader update program (not used in this version)

// Camera and controls
glm::vec3 cameraPos = glm::vec3(0.0f, 500.0f, 300.0f);
glm::vec3 cameraFront = glm::vec3(0.0f, -0.5f, -1.0f);
glm::vec3 cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);
float horizontalAngle = -90.0f;
float verticalAngle = -25.0f;
float cameraSpeed = 50.0f;

// Shaders; executed on the GPU
const char* terrainVertexShaderSource = R"(#version 330 core
layout (location = 0) in vec2 aGridPos;

// Uniform variables (passed from the CPU)
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform float levelScale;
uniform vec2 levelOffset;
uniform int levelIndex;

// The output for the fragment shader
out vec3 FragPos;
out vec3 WorldPos;
out float Elevation;
flat out int lodLevel;

// Noise generation functions for terrain

// Fast hash function for pseudorandom numbers
float hash(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

// Perlin noise (simplified version)
// Creates smooth, continuous noise for smooth hills and valleys
float noise(vec2 p) {
    vec2 i = floor(p); // The whole part of the coordinates
    vec2 f = fract(p); // Fractional part of coordinates
    f = f * f * (3.0 - 2.0 * f); // Cubic interpolation for smoothness

    // Interpolation between 4 corner points
    return mix(mix(hash(i), hash(i + vec2(1.0, 0.0)), f.x),
               mix(hash(i + vec2(0.0, 1.0)), hash(i + vec2(1.0, 1.0)), f.x), f.y);
}

// Fractal Brownian noise (FBM) is a combination of noise of different frequencies.
// Creates a complex, multi-layered relief
float fbm(vec2 p, int octaves, float persistence) {
    float value = 0.0;
    float amplitude = 1.0;
    float frequency = 1.0;
    float maxValue = 0.0;
    
    for(int i = 0; i < octaves; i++) {
        value += amplitude * noise(p * frequency);
        maxValue += amplitude;
        amplitude *= persistence;
        frequency *= 2.0;
    }
    
    return value / maxValue;
}

// The main function of height rendering
float getElevation(vec2 worldPos, int level) {
    float height = 0.0;
    
    float mountainRidges = fbm(worldPos * 0.0003, 8, 0.5) * 1200.0;
    float rollingHills = fbm(worldPos * 0.001 + vec2(100.0, 100.0), 6, 0.6) * 300.0;
    float canyons = fbm(worldPos * 0.0008, 4, 0.7) * 400.0;
    float cliffs = fbm(worldPos * 0.01, 3, 0.8) * 100.0;
    
    // Combination of all layers
    height += mountainRidges * 0.7;
    height += rollingHills * 0.4;
    height -= abs(canyons) * 0.3;
    height += cliffs * 0.2;
    
    // The central high mountain
    float distToCenter = length(worldPos);
    float centralMountain = max(0.0, 800.0 - distToCenter * 0.2);
    height += centralMountain * exp(-distToCenter * 0.0005);
    
    // Reservoirs are only far from the center
    if(distToCenter > 500.0) {
        float waterBasins = fbm(worldPos * 0.0002 + vec2(500.0, 500.0), 5, 0.6);
        if(waterBasins > 0.3) {
            height -= 200.0; // Creating deep depressions for lakes
        }
    }
    
    // Riverbeds
    float riverValley = sin(worldPos.x * 0.001) * 100.0;
    riverValley += sin(worldPos.y * 0.0015) * 80.0;
    height -= abs(riverValley) * 0.5; // The absolute value creates V-shaped valleys
    
    // Details for the near levels (only for high LODs)
    if(level < 3) {
        float fineDetails = fbm(worldPos * 0.05, 2, 0.9) * 30.0;
        height += fineDetails;
    }
    
    // The guarantee that the central mountain will be high (without water in the center)
    if(distToCenter < 200.0) {
        height = max(height, 100.0); // Центр всегда выше 100 единиц
    }
    
    return height;
}

/*
    The main function of the vertex shader
*/
void main() {
    // Converting grid coordinates to world coordinates
    // Initial coordinates: [0..255] -> Centering: [-127.5..127.5]
    vec2 centeredGridPos = aGridPos - vec2(127.5);
    
    // We apply the level scale and the world offset
    vec2 localPos = centeredGridPos * levelScale + levelOffset;
    
    // Scaling the world for more diversity
    float worldScale = 2.0; 
    vec2 worldXZ = localPos * worldScale;
    
    float height = getElevation(worldXZ, levelIndex); // using procedural generation
    
    vec3 worldPos = vec3(worldXZ.x, height, worldXZ.y); // Shaping the ultimate position in the world

    // Transferring data to a fragment shader
    FragPos = worldPos;
    WorldPos = worldPos;
    Elevation = height;
    lodLevel = levelIndex;
    
    // Final transformation: local -> world -> view -> projection
    gl_Position = projection * view * model * vec4(worldPos, 1.0);
})";

const char* terrainFragmentShaderSource = R"(#version 330 core
// Input data from the vertex shader
in vec3 FragPos;
in vec3 WorldPos;
in float Elevation;
flat in int lodLevel;

// Output data
out vec4 FragColor; // The final pixel color

// Noise functions for texturing
float hash(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

float noise(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash(i), hash(i + vec2(1.0, 0.0)), f.x),
               mix(hash(i + vec2(0.0, 1.0)), hash(i + vec2(1.0, 1.0)), f.x), f.y);
}

// The function of determining the color by height and relief
vec3 getTerrainColor(float elevation, vec3 worldPos, vec3 normal) {
    vec3 color;
    
    // Water (low height)
    if(elevation < 50.0) {
        float waterDepth = 1.0 - elevation / 50.0;
        color = mix(vec3(0.0, 0.3, 0.8), // Blue surface
                    vec3(0.0, 0.1, 0.5), // Dark blue depth
                    waterDepth);
    }
    // Beaches
    else if(elevation < 70.0) {
        color = vec3(0.76, 0.70, 0.50); // Yellow color
    }

    // Plains and grass
    else if(elevation < 200.0) {
        vec3 grassGreen = vec3(0.2, 0.6, 0.1);
        vec3 dryGrass = vec3(0.5, 0.6, 0.2);
        
        // Variations of grass color using noise
        float grassVariation = noise(worldPos.xz * 0.02);
        color = mix(grassGreen, dryGrass, grassVariation);
        
        // Rocks on the slopes (determined by normal)
        float slope = 1.0 - normal.y; // normal.y = 1 for horizontal, 0 for vertical
        if(slope > 0.3) {
            color = mix(color, vec3(0.5, 0.5, 0.5), slope * 0.8);
        }
    }
    // The woods
    else if(elevation < 400.0) {
        vec3 forestGreen = vec3(0.1, 0.4, 0.1); // Dark green Forest
        vec3 darkGreen = vec3(0.05, 0.3, 0.05); // Very dark green
        
        float forestVariation = noise(worldPos.xz * 0.01);
        color = mix(forestGreen, darkGreen, forestVariation);
        
        // Cliffs in the forest on steep slopes
        float slope = 1.0 - normal.y;
        if(slope > 0.4) {
            float rockAmount = smoothstep(0.4, 0.8, slope); // Smooth transition
            color = mix(color, vec3(0.4, 0.4, 0.4), rockAmount);
        }
    }
    // Mountain slopes
    else if(elevation < 600.0) {
        vec3 rockGray = vec3(0.6, 0.6, 0.6);
        vec3 darkRock = vec3(0.3, 0.3, 0.3);
        
        float rockVariation = noise(worldPos.xz * 0.05);
        color = mix(rockGray, darkRock, rockVariation);
        
        // Snow on the peaks (normal - on horizontal surfaces)
        float snowStart = 0.8 - (elevation - 600.0) * 0.001;
        if(normal.y > snowStart) {
            float snowAmount = smoothstep(snowStart, 1.0, normal.y);
            color = mix(color, vec3(1.0, 1.0, 1.0), snowAmount);
        }
    }
    // Mountain peaks with snow
    else {
        vec3 snowWhite = vec3(1.0, 1.0, 1.0);
        vec3 rockColor = vec3(0.5, 0.5, 0.5);
        
        // Rocks break through the snow (procedural texture)
        float rockPattern = noise(worldPos.xz * 0.1);
        if(rockPattern > 0.7) {
            color = mix(snowWhite, rockColor, 0.3);
        } else {
            color = snowWhite;
        }
    }
    
    return color;
}

// Calculating the normal from the height gradient
vec3 calculateNormal(vec3 fragPos) {
    vec3 dx = dFdx(fragPos);
    vec3 dy = dFdy(fragPos);
    return normalize(cross(dx, dy));
}

// Function lighting (procedural lighting)
vec3 applyLighting(vec3 color, vec3 normal, vec3 fragPos) {
    // Directional light (sun)
    vec3 lightDir = normalize(vec3(0.8, 1.0, 0.6)); // Light from above-right-front
    vec3 viewDir = normalize(-fragPos); // Direction to the camera
    vec3 reflectDir = reflect(-lightDir, normal);
    
    // Diffuse illumination (depends on the angle between the light and the normal)
    float diffuse = max(dot(normal, lightDir), 0.1);
    
    // Mirror image (glare)
    float specular = pow(max(dot(viewDir, reflectDir), 0.0), 32.0) * 0.3;
    
    // Ambient lighting
    float ambient = 0.4;
    
    // Shadows from the mountains
    float shadow = 1.0;
    if(diffuse < 0.3) {
        shadow = 0.7;
    }
    
    // Combine all the lighting components
    return color * (ambient + diffuse * shadow) + vec3(1.0) * specular;
}

// The main function of the fragment shader
void main() {
    // Calculating the normal for lighting and texturing
    vec3 normal = calculateNormal(FragPos);
    
    // Getting the color of a landscape based on height and terrain
    vec3 terrainColor = getTerrainColor(Elevation, WorldPos, normal);
    
    // Lighting Application
    terrainColor = applyLighting(terrainColor, normal, FragPos);
    
    FragColor = vec4(terrainColor, 1.0); // Setting the final pixel color
})";

/*
    Shader program compilation function
*/
GLuint compileShaderProgram(const char* vertexSource, const char* fragmentSource) {
    int success;
    char infoLog[1024]; // Buffer for error messages
    
    // Compilation of the vertex shader
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexSource, NULL);
    glCompileShader(vertexShader);
    
    // Checking for vertex shader compilation errors
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if(!success) {
        glGetShaderInfoLog(vertexShader, 1024, NULL, infoLog);
        std::cout << "ERROR::VERTEX_SHADER_COMPILATION:\n" << infoLog << std::endl;
        return 0;
    }
    
    // Compilation of the fragment shader
    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentSource, NULL);
    glCompileShader(fragmentShader);
    
    // Checking for fragment shader compilation errors
    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if(!success) {
        glGetShaderInfoLog(fragmentShader, 1024, NULL, infoLog);
        std::cout << "ERROR::FRAGMENT_SHADER_COMPILATION:\n" << infoLog << std::endl;
        glDeleteShader(vertexShader);
        return 0;
    }
    
    // Creating and composing a shader program
    GLuint program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);
    
    // Checking for layout errors
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if(!success) {
        glGetProgramInfoLog(program, 1024, NULL, infoLog);
        std::cout << "ERROR::SHADER_PROGRAM_LINKING:\n" << infoLog << std::endl;
        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
        return 0;
    }
    
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    
    return program;
}

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
        if(level.worldOffset != newWorldOffset) {
            level.worldOffset = newWorldOffset;
            level.updateCount++;

            static int updateCounter = 0;
            if(updateCounter++ % 30 == 0 && i == 0) {
                std::cout << "🔄 Level " << i << " updated to: " 
                          << newWorldOffset.x << ", " << newWorldOffset.y << std::endl;
            }
        }
        
        level.active = true;
    }
}

/*
void renderClipmapLevel1(int levelIndex, const glm::mat4& model, 
                       const glm::mat4& view, const glm::mat4& projection) {
    if(levelIndex == 7)
        return;

    if(levelIndex >= L || !levels[levelIndex].active) return;
    
    ClipmapLevel& level = levels[levelIndex];

    static int lastRenderedFrame = -1;
    int currentFrame = glfwGetTime() * 60; // примерный номер кадра
    if(currentFrame != lastRenderedFrame) {
        std::cout << "Rendering level " << levelIndex 
                  << " | Offset: " << level.worldOffset.x << ", " << level.worldOffset.y
                  << " | Scale: " << level.scale << std::endl;
        lastRenderedFrame = currentFrame;
    }
    
    glUseProgram(terrainShaderProgram);
    
    glUniformMatrix4fv(glGetUniformLocation(terrainShaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));
    glUniformMatrix4fv(glGetUniformLocation(terrainShaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(glGetUniformLocation(terrainShaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
    
    glUniform1f(glGetUniformLocation(terrainShaderProgram, "levelScale"), 1.0f);
    glUniform2fv(glGetUniformLocation(terrainShaderProgram, "levelOffset"), 1, glm::value_ptr(level.worldOffset));
    glUniform1i(glGetUniformLocation(terrainShaderProgram, "levelIndex"), levelIndex);
    glUniform1i(glGetUniformLocation(terrainShaderProgram, "isFinestLevel"), levelIndex == 0);
    
    glm::vec2 viewerLocalPos = glm::vec2(N/2, N/2); // всегда в центре для теста
    glUniform2fv(glGetUniformLocation(terrainShaderProgram, "viewerPos"), 1, glm::value_ptr(viewerLocalPos));
    
    // Привязываем текстуру высот
    // glActiveTexture(GL_TEXTURE0);
    // glBindTexture(GL_TEXTURE_2D, level.elevationTexture);
    // glUniform1i(glGetUniformLocation(terrainShaderProgram, "elevationMap"), 0);
    
    for(auto& block : blocks) {
        glBindVertexArray(block.VAO);
        glDrawElements(GL_TRIANGLES, block.indexCount, GL_UNSIGNED_INT, 0);
    }
    
    for(auto& strip : fixupStrips) {
        glBindVertexArray(strip.VAO);
        glDrawElements(GL_TRIANGLES, strip.indexCount, GL_UNSIGNED_INT, 0);
    }
    
    if(levelIndex < L - 1) {
        for(auto& trim : interiorTrims) {
            glBindVertexArray(trim.VAO);
            glDrawElements(GL_TRIANGLES, trim.indexCount, GL_UNSIGNED_INT, 0);
        }
    }
    
    glBindVertexArray(0);
}
*/

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

    terrainShaderProgram = compileShaderProgram(terrainVertexShaderSource, terrainFragmentShaderSource);
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

        static int debugCounter = 0;
        if (debugCounter++ % 60 == 0) {
            std::cout << "📍 Camera position: " << cameraPos.x << ", " << cameraPos.y << ", " << cameraPos.z << std::endl;
            std::cout << "🎯 Level 0 offset: " << levels[0].worldOffset.x << ", " << levels[0].worldOffset.y << std::endl;
        }

        // Calculating transformation matrices for the current frame
        glm::mat4 projection = glm::perspective(glm::radians(60.0f), 1200.0f/800.0f, 0.1f, 10000.0f); // Projection matrix - camera view
        glm::mat4 view = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp); // View matrix - сamera position and direction
        glm::mat4 model = glm::mat4(1.0f); // Model matrix - object position and direction (the object is in place)

        // Debugging matrices
        static int debugFrame = 0;
        if (debugFrame++ % 60 == 0) {
            std::cout << "--- Camera Debug ---" << std::endl;
            std::cout << "Camera Pos: " << cameraPos.x << ", " << cameraPos.y << ", " << cameraPos.z << std::endl;
            std::cout << "Camera Front: " << cameraFront.x << ", " << cameraFront.y << ", " << cameraFront.z << std::endl;
            std::cout << "View matrix calculated" << std::endl;
        }

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