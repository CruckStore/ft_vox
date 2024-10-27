#include <unordered_map>
#include <sstream>
#include <iostream>
#include <fstream>
#include <cassert>
#include <filesystem>
#include <GL/glew.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <classes/Shader.hpp>

#define VRTX 1 << 0
#define FGMT 1 << 1
#define GMTR 1 << 2

Shader *Shader::activeShader = nullptr;

Shader::Shader(const std::string &folderPath) {
    u_char activeShaders = 0;
    ID = glCreateProgram();

    for (const auto& entry : std::filesystem::directory_iterator(folderPath)) {
        if (entry.is_regular_file()) {
            const std::string& filePath = entry.path().string();
            const std::string& fileName = entry.path().filename().string();

            if (fileName.find("vertex_shader") != std::string::npos) {
                GLuint vertexShader = CompileSingleShader(filePath.c_str(), GL_VERTEX_SHADER, "VERTEX");
                glAttachShader(ID, vertexShader);
                glDeleteShader(vertexShader);
                activeShaders |= VRTX;
                parseAttributes(filePath.c_str());
            } else if (fileName.find("fragment_shader") != std::string::npos) {
                GLuint fragmentShader = CompileSingleShader(filePath.c_str(), GL_FRAGMENT_SHADER, "FRAGMENT");
                glAttachShader(ID, fragmentShader);
                glDeleteShader(fragmentShader);
                activeShaders |= FGMT;
            } else if (fileName.find("geometry_shader") != std::string::npos) {
                GLuint geometryShader = CompileSingleShader(filePath.c_str(), GL_GEOMETRY_SHADER, "GEOMETRY");
                glAttachShader(ID, geometryShader);
                glDeleteShader(geometryShader);
                activeShaders |= GMTR;
            }
        }
    }

    if (!(activeShaders & VRTX) || !(activeShaders & FGMT)) {
        std::cerr << "ERROR::SHADER::Minimum shader requirement not met" << std::endl;
        assert(0);
    }

    glLinkProgram(ID);
    CheckCompileErrors(ID, "PROGRAM");
}

GLuint Shader::CompileSingleShader(const char *path, GLenum type, std::string sType) {
    std::string shaderCode;
    try {
        shaderCode = ReadFile(path);
    } catch (const std::exception &e) {
        std::cerr << "ERROR::SHADER::FILE_NOT_SUCCESSFULLY_READ: " << e.what() << std::endl;
        assert(0);
    }

    const char * cShaderCode = shaderCode.c_str();
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &cShaderCode, nullptr);
    glCompileShader(shader);
    CheckCompileErrors(shader, sType);

    return shader;
}

std::string Shader::ReadFile(const char *filePath) {
    std::ifstream shaderFile(filePath);
    std::stringstream shaderStream;
    shaderStream << shaderFile.rdbuf();
    return shaderStream.str();
}

void Shader::parseAttributes(const char *path) {
    std::ifstream file(path);

    if (!file.is_open()) {
        std::cerr << "Error opening file: " << path << std::endl;
        assert(0);
    }

    std::string line;
    while (std::getline(file, line)) {
        std::istringstream lineStream(line);
        std::string token;

        if (line.find("//") != std::string::npos) continue;

        if (lineStream >> token && token == "layout") {
            t_vertexAttribute attribute;
            std::string typeName;
            while (lineStream >> token && token != "=") {}
            lineStream >> attribute.location;

            lineStream >> token;
            lineStream >> token;
            lineStream >> token;

            typeName = token;
            if (typeName == "int") {
                attribute.type = GL_INT;
                attribute.size = 1;
            } else if (typeName == "float") {
                attribute.type = GL_FLOAT;
                attribute.size = 1;
            } else if (typeName.find("vec") != std::string::npos) {
                attribute.type = GL_FLOAT;
                attribute.size = typeName[3] - '0';
            } else {
                std::cerr << "Type not supported by current codebase: " << typeName << std::endl;
                assert(0);
            }
            attributes.push_back(attribute);
        }
    }
}

void Shader::Use() { 
    if (activeShader != this) {
        glUseProgram(ID);
        activeShader = this;
    }
}

void Shader::SetBool(const std::string &name, bool value) const {         
    glUniform1i(GetUniformLocation(name), (int)value); 
}
void Shader::SetInt(const std::string &name, int value) const { 
    glUniform1i(GetUniformLocation(name), value); 
}
void Shader::SetFloat(const std::string &name, float value) const { 
    glUniform1f(GetUniformLocation(name), value); 
}

void Shader::SetFloat4(const std::string &name, float value1, float value2, float value3, float value4) const { 
    glUniform4f(GetUniformLocation(name), value1 , value2, value3, value4); 
}
void Shader::Setmat4(const std::string &name, glm::mat4 value) const { 
    glUniformMatrix4fv(GetUniformLocation(name), 1, GL_FALSE, glm::value_ptr(value));
}

std::vector<t_vertexAttribute> &Shader::GetVertexAttributes() {
    return attributes;
}

Shader *Shader::GetActiveShader() {
    return activeShader;
}

void Shader::CheckCompileErrors(unsigned int shader, std::string type) {
    int success;
    char infoLog[1024];
    if (type != "PROGRAM") {
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success) {
            glGetShaderInfoLog(shader, 1024, NULL, infoLog);
            std::cerr << "ERROR::SHADER_COMPILATION_ERROR of type: " << type << "\n" << infoLog << "\n -- --------------------------------------------------- -- " << std::endl;
            assert(0);
        }
    } else {
        glGetProgramiv(shader, GL_LINK_STATUS, &success);
        if (!success) {
            glGetProgramInfoLog(shader, 1024, NULL, infoLog);
            std::cerr << "ERROR::PROGRAM_LINKING_ERROR of type: " << type << "\n" << infoLog << "\n -- --------------------------------------------------- -- " << std::endl;
            assert(0);
        }
    }
}

GLint Shader::GetUniformLocation(const std::string &name) const {
    if (uniformLocations.find(name) != uniformLocations.end()) {
        return uniformLocations[name];
    }
    GLint location = glGetUniformLocation(ID, name.c_str());
    uniformLocations[name] = location;
    return location;
}
