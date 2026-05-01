#ifndef ShaderManager_hpp_
#define ShaderManager_hpp_

#include <cstdint>
#include <filesystem>
#include <glm/glm.hpp>
#include <optional>
#include <string_view>
#include <unordered_map>
#include <utility>

namespace Golde {

class ShaderManager {
    using IDPair = typename std::pair<unsigned int, unsigned int>;

  public:
    explicit ShaderManager(const std::filesystem::path& shaderFilePath);

    ShaderManager(const ShaderManager& other) = delete;

    ShaderManager(ShaderManager&& other) noexcept;

    ShaderManager& operator=(const ShaderManager& other) = delete;

    ShaderManager& operator=(ShaderManager&& other) noexcept;

    ~ShaderManager();

    uint32_t Program() const;

    void AttachUniformVec2(std::string_view label, glm::vec2 vec);
    void AttachUniformVec4(std::string_view label, const glm::vec4& vec);
    void AttachUniformMatrix4(std::string_view label, const glm::mat4& matrix);
    void AttachUniformFloat(std::string_view label, float value);
    void AttachUniformInt(std::string_view label, int32_t value);

  private:
    uint32_t CompileShader(uint32_t type, std::string_view source) const;

    std::optional<IDPair>
    ParseShader(const std::filesystem::path& filePath) const;

    void CreateShader(uint32_t program, uint32_t shaderId);

    int32_t UniformLocation(std::string_view label);

    void Destroy();

  private:
    struct ShaderControlBlock {
        int32_t RefCount = 0;
        uint32_t ProgramID = 0;
        std::unordered_map<std::string_view, int32_t> Uniforms{};
    };

    inline static std::unordered_map<std::filesystem::path, ShaderControlBlock>
        s_Shaders = {};

  private:
    ShaderControlBlock* m_ControlBlock;
};
} // namespace Golde

#endif
