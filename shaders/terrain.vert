#version 330 core
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
};