#include "shaders.h"


std::string loadShaderFromFile(const std::string& filePath) {
    std::string shaderCode;
    std::ifstream shaderFile;
    
    // Убеждаемся, что ifstream может выбросить исключения
    shaderFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);
    
    try {
        // Открываем файл
        shaderFile.open(filePath);
        std::stringstream shaderStream;
        
        // Читаем содержимое файла в поток
        shaderStream << shaderFile.rdbuf();
        
        // Закрываем файл
        shaderFile.close();
        
        // Преобразуем поток в строку
        shaderCode = shaderStream.str();
    }
    catch(std::ifstream::failure& e) {
        std::cout << "ERROR::SHADER::FILE_NOT_SUCCESSFULLY_READ: " << filePath << std::endl;
        std::cout << "Exception: " << e.what() << std::endl;
        return "";
    }
    
    return shaderCode;
}

/*
    Shader program compilation function
*/
GLuint compileShaderProgram(const std::string& vertexPath, const std::string& fragmentPath) {
    int success;
    char infoLog[1024]; // Buffer for error messages

    // Загрузка шейдеров из файлов
    std::string vertexCode = loadShaderFromFile(vertexPath);
    std::string fragmentCode = loadShaderFromFile(fragmentPath);
    
    if(vertexCode.empty() || fragmentCode.empty()) {
        std::cout << "ERROR: Failed to load shader files" << std::endl;
        return 0;
    }
    
    const char* vertexSource = vertexCode.c_str();
    const char* fragmentSource = fragmentCode.c_str();
    
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