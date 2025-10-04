#pragma once

#include "global.h"
#include <fstream>
#include <sstream>

std::string loadShaderFromFile(const std::string& filePath);
GLuint compileShaderProgram(const std::string& vertexPath, const std::string& fragmentPath);
