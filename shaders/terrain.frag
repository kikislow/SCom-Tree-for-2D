#version 330 core
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
}