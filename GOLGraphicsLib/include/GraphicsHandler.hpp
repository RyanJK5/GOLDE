#ifndef DrawManager_h_
#define DrawManager_h_

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <ranges>
#include <unordered_set>
#include <vector>

#include "Camera.hpp"
#include "GLBuffer.hpp"
#include "Graphics2D.hpp"
#include "Logging.hpp"
#include "ShaderManager.hpp"

namespace gol {
struct GraphicsHandlerArgs {
    Rect ViewportBounds;
    Size2 GridSize;
    Size2F CellSize;
    bool ShowGridLines = false;
};

struct FrameBufferBinder {
    FrameBufferBinder(const GLFrameBuffer &buffer);

    ~FrameBufferBinder();

    FrameBufferBinder(const FrameBufferBinder &) = delete;
    auto &operator=(const FrameBufferBinder &) = delete;

    FrameBufferBinder(FrameBufferBinder &&) = delete;
    auto &operator=(const FrameBufferBinder &&) = delete;
};

class GraphicsHandler {
  public:
    GraphicsCamera Camera;

  public:
    GraphicsHandler(const std::filesystem::path &shaderDirectory,
                    int32_t windowWidth, int32_t windowHeight, Color bgColor);

    GraphicsHandler(const GraphicsHandler &other) = delete;
    GraphicsHandler &operator=(const GraphicsHandler &other) = delete;

    GraphicsHandler(GraphicsHandler &&other) noexcept = default;
    GraphicsHandler &operator=(GraphicsHandler &&other) noexcept = default;
    ~GraphicsHandler() = default;

    void RescaleFrameBuffer(const Rect &windowBounds,
                            const Rect &viewportBounds);

    void DrawGrid(Vec2 offset, std::ranges::input_range auto &&grid,
                  const GraphicsHandlerArgs &args);
    void DrawSelection(const Rect &region, const GraphicsHandlerArgs &info);
    void ClearBackground(const GraphicsHandlerArgs &args);

    void CenterCamera(const GraphicsHandlerArgs &viewportBounds);

    uint32_t TextureID() const { return m_Texture.ID(); }

  private:
    void InitGridBuffer();

    std::vector<float> GenerateGLBuffer(Vec2 offset,
                                        std::ranges::input_range auto &&grid,
                                        const GraphicsHandlerArgs &args) const;

    RectF GridToScreenBounds(const Rect &region,
                             const GraphicsHandlerArgs &args) const;

  private:
    struct GridLineInfo {
        Vec2F UpperLeft;
        Vec2F LowerRight;
        Size2 GridSize;
    };
    GridLineInfo CalculateGridLineInfo(Vec2 offset,
                                       const GraphicsHandlerArgs &args) const;

    void DrawGridLines(Vec2 offset, const GraphicsHandlerArgs &args);

  private:
    Color m_BgColor;

    ShaderManager m_GridShader;
    ShaderManager m_SelectionShader;

    GLVertexArray m_GridVAO;
    GLBuffer m_InstanceBuffer;
    GLBuffer m_CellBuffer;
    GLIndexBuffer m_CellIndexBuffer;

    GLBuffer m_GridLineBuffer;

    GLBuffer m_SelectionBuffer;
    GLIndexBuffer m_SelectionIndexBuffer;

    GLFrameBuffer m_FrameBuffer;
    GLTexture m_Texture;
    GLRenderBuffer m_renderBuffer;
};

template <typename T>
concept HasSize = requires(T a) { a.size(); };

std::vector<float>
GraphicsHandler::GenerateGLBuffer(Vec2 offset,
                                       std::ranges::input_range auto &&grid,
                                       const GraphicsHandlerArgs &args) const {
    std::vector<float> result{};

    const auto gridInfo = CalculateGridLineInfo(offset, args);
    const auto minCellX = gridInfo.UpperLeft.X / args.CellSize.Width;
    const auto minCellY = gridInfo.UpperLeft.Y / args.CellSize.Height;
    const auto maxCellX =
        std::ceil(gridInfo.LowerRight.X / args.CellSize.Width) - 1.f;
    const auto maxCellY =
        std::ceil(gridInfo.LowerRight.Y / args.CellSize.Height) - 1.f;

    if constexpr (HasSize<decltype(grid)>) {
        const auto visibleCapacity =
            static_cast<size_t>(std::max<int32_t>(gridInfo.GridSize.Width, 0) *
                                std::max<int32_t>(gridInfo.GridSize.Height, 0));
        const auto reserveCount =
            std::min(static_cast<size_t>(grid.size()), visibleCapacity);
        result.reserve(reserveCount * 2);
    }

    for (const auto vec : grid) {
        const auto x = vec.X + offset.X;
        const auto y = vec.Y + offset.Y;
        if (x < minCellX || x > maxCellX || y < minCellY || y > maxCellY)
            continue;
        result.push_back(static_cast<float>(x));
        result.push_back(static_cast<float>(y));
    }

    return result;
}

void GraphicsHandler::DrawGrid(Vec2 offset,
                                    std::ranges::input_range auto &&grid,
                                    const GraphicsHandlerArgs &args) {
    FrameBufferBinder binder{m_FrameBuffer};

    auto matrix = Camera.OrthographicProjection(args.ViewportBounds.Size());

    if (args.ShowGridLines && offset.X == 0 && offset.Y == 0) {
        GL_DEBUG(glUseProgram(m_SelectionShader.Program()));
        m_SelectionShader.AttachUniformMatrix4("u_MVP", matrix);
        m_SelectionShader.AttachUniformVec4("u_Color", {0.2f, 0.2f, 0.2f, 1.f});
        DrawGridLines(offset, args);
    }

    GL_DEBUG(glUseProgram(m_GridShader.Program()));
    GL_DEBUG(glBindVertexArray(m_GridVAO.ID()));
    m_GridShader.AttachUniformVec2("u_CellSize",
                                   {args.CellSize.Width, args.CellSize.Height});
    m_GridShader.AttachUniformMatrix4("u_MVP", matrix);
    m_GridShader.AttachUniformVec4("u_Color", {1.f, 1.f, 1.f, 1.f});

    const auto positions = GenerateGLBuffer(offset, grid, args);

    GL_DEBUG(glBindBuffer(GL_ARRAY_BUFFER, m_InstanceBuffer.ID()));

    GL_DEBUG(glBufferData(GL_ARRAY_BUFFER, positions.size() * sizeof(float),
                          nullptr, GL_DYNAMIC_DRAW));
    GL_DEBUG(glBufferSubData(GL_ARRAY_BUFFER, 0,
                             positions.size() * sizeof(float),
                             positions.data()));

    GL_DEBUG(
        glDrawElementsInstanced(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, nullptr,
                                static_cast<GLsizei>(positions.size() / 2)));

    GL_DEBUG(glBindVertexArray(0));
}

} // namespace gol

#endif