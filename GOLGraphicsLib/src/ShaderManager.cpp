#include <GL/glew.h>
#include <cstdint>
#include <filesystem>
#include <format>
#include <fstream>
#include <glm/glm.hpp>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "GLException.hpp"
#include "Logging.hpp"
#include "ShaderManager.hpp"

ShaderManager::ShaderManager(const std::filesystem::path &shaderFilePath) {
    auto itr = s_Shaders.find(shaderFilePath);
    if (itr != s_Shaders.end()) {
        itr->second.RefCount++;
        m_ControlBlock = &itr->second;
        return;
    }

    auto controlBlock = ShaderControlBlock{.RefCount = 1};
    GL_DEBUG(controlBlock.ProgramID = glCreateProgram());
    try {
        std::optional<IDPair> shaderIds = ParseShader(shaderFilePath);

        if (!shaderIds)
            throw GLException(std::format("File '{}' could not be read",
                                          shaderFilePath.string()));

        CreateShader(controlBlock.ProgramID, shaderIds.value().first);
        CreateShader(controlBlock.ProgramID, shaderIds.value().second);

        GL_DEBUG(glLinkProgram(controlBlock.ProgramID));
        GL_DEBUG(glValidateProgram(controlBlock.ProgramID));
    } catch (GLException e) {
        GL_DEBUG(glDeleteProgram(controlBlock.ProgramID));
        throw e;
    }

    s_Shaders[shaderFilePath] = controlBlock;
    m_ControlBlock = &s_Shaders[shaderFilePath];
}

ShaderManager::ShaderManager(ShaderManager &&other) noexcept {
    m_ControlBlock = std::exchange(other.m_ControlBlock, nullptr);
}

ShaderManager &ShaderManager::operator=(ShaderManager &&other) noexcept {
    if (this != &other) {
        Destroy();
        m_ControlBlock = std::exchange(other.m_ControlBlock, nullptr);
    }
    return *this;
}

ShaderManager::~ShaderManager() { Destroy(); }

void ShaderManager::Destroy() {
    if (!m_ControlBlock)
        return;

    m_ControlBlock->RefCount--;
    if (m_ControlBlock->RefCount != 0)
        return;

    GL_DEBUG(glDeleteProgram(m_ControlBlock->ProgramID));
    for (auto itr = s_Shaders.begin(); itr != s_Shaders.end(); ++itr) {
        if (&itr->second == m_ControlBlock) {
            s_Shaders.erase(itr);
            break;
        }
    }
}

uint32_t ShaderManager::Program() const {
    return !m_ControlBlock ? 0 : m_ControlBlock->ProgramID;
}

uint32_t ShaderManager::CompileShader(uint32_t type,
                                      std::string_view source) const {
    uint32_t id = glCreateShader(type);
    const char *src = source.data();

    GL_DEBUG(glShaderSource(id, 1, &src, nullptr));
    GL_DEBUG(glCompileShader(id));

    int result;
    GL_DEBUG(glGetShaderiv(id, GL_COMPILE_STATUS, &result));
    if (result != GL_FALSE)
        return id;

    int32_t length = 0;
    GL_DEBUG(glGetShaderiv(id, GL_INFO_LOG_LENGTH, &length));
    auto message = new char[length];
    GL_DEBUG(glGetShaderInfoLog(id, length, &length, message));

    auto error =
        std::format("Failed to compile {} shader\n{}",
                    type == GL_VERTEX_SHADER ? "vertex" : "fragment", message);
    delete[] message;
    throw GLException(error);
}

std::optional<ShaderManager::IDPair>
ShaderManager::ParseShader(const std::filesystem::path &shaderFilePath) const {
    std::ifstream stream(shaderFilePath);
    if (!stream.is_open()) {
        return std::nullopt;
    }

    std::string line;
    std::string shader;

    auto setType = [](const std::string &line, std::optional<uint32_t> &type) {
        size_t spaceIndex = line.find(' ');
        std::string typeStr =
            line.substr(spaceIndex + 1, line.length() - spaceIndex);

        if (typeStr == "vertex")
            type = GL_VERTEX_SHADER;
        else if (typeStr == "fragment")
            type = GL_FRAGMENT_SHADER;
        else
            type = std::nullopt;
    };

    std::optional<uint32_t> id1{};
    std::optional<uint32_t> id2{};
    std::optional<uint32_t> type{};
    while (std::getline(stream, line, '\n')) {
        if (line.find("#shader") == std::string::npos) {
            shader += line + '\n';
            continue;
        }

        if (!type) {
            setType(line, type);
            if (!type)
                break;
            continue;
        }

        id1 = CompileShader(type.value(), shader);

        shader = "";
        setType(line, type);
    }

    if (type)
        id2 = CompileShader(type.value(), shader);

    if (!id1 || !id2)
        return std::nullopt;

    return std::make_pair(id1.value(), id2.value());
}

void ShaderManager::AttachUniformVec2(std::string_view label,
                                      const glm::vec2 &vec) {
    auto loc = UniformLocation(label);
    GL_DEBUG(glUniform2f(loc, vec.x, vec.y));
}

void ShaderManager::AttachUniformVec4(std::string_view label,
                                      const glm::vec4 &vec) {
    auto loc = UniformLocation(label);
    GL_DEBUG(glUniform4f(loc, vec.x, vec.y, vec.z, vec.w));
}

void ShaderManager::AttachUniformMatrix4(std::string_view label,
                                         const glm::mat4 &matrix) {
    GL_DEBUG(
        glUniformMatrix4fv(UniformLocation(label), 1, GL_FALSE, &matrix[0][0]));
}

int32_t ShaderManager::UniformLocation(std::string_view label) {
    if (m_ControlBlock->Uniforms.find(label) != m_ControlBlock->Uniforms.end())
        return m_ControlBlock->Uniforms[label];

    GL_DEBUG(int location = glGetUniformLocation(Program(), label.data()));
    if (location == -1)
        throw GLException(
            std::format("Could not locate uniform '{}' in shader file", label));

    m_ControlBlock->Uniforms[label] = location;
    return location;
}

void ShaderManager::CreateShader(uint32_t program, uint32_t shaderId) {
    GL_DEBUG(glAttachShader(program, shaderId));
    GL_DEBUG(glDeleteShader(shaderId));
}
