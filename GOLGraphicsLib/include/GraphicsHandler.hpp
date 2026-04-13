#ifndef DrawManager_hpp_
#define DrawManager_hpp_

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
#include "HashQuadtree.hpp"
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
    FrameBufferBinder(const GLFrameBuffer& buffer);

    ~FrameBufferBinder();

    FrameBufferBinder(const FrameBufferBinder&) = delete;
    auto& operator=(const FrameBufferBinder&) = delete;

    FrameBufferBinder(FrameBufferBinder&&) = delete;
    auto& operator=(const FrameBufferBinder&&) = delete;
};

class GraphicsHandler {
  public:
    GraphicsCamera Camera;

  public:
    GraphicsHandler(const std::filesystem::path& shaderDirectory,
                    int32_t windowWidth, int32_t windowHeight, Color bgColor);

    GraphicsHandler(const GraphicsHandler& other) = delete;
    GraphicsHandler& operator=(const GraphicsHandler& other) = delete;

    GraphicsHandler(GraphicsHandler&& other) noexcept = default;
    GraphicsHandler& operator=(GraphicsHandler&& other) noexcept = default;
    ~GraphicsHandler() = default;

    void RescaleFrameBuffer(Rect windowBounds, Rect viewportBounds);

    void DrawGrid(Vec2 offset, const std::ranges::input_range auto& grid,
                  const GraphicsHandlerArgs& args);
    void DrawSelection(Rect region, const GraphicsHandlerArgs& info);
    void ClearBackground(const GraphicsHandlerArgs& args);

    void CenterCamera(const GraphicsHandlerArgs& viewportBounds);

    uint32_t TextureID() const { return m_Texture.ID(); }

  private:
    BigRect VisibleBounds(const GraphicsHandlerArgs& args) const;

    void InitGridBuffer();

    void GenerateGLBuffer(Vec2 offset, int32_t minLevel,
                          const std::ranges::input_range auto& grid,
                          const GraphicsHandlerArgs& args);

    RectF GridToScreenBounds(Rect region,
                             const GraphicsHandlerArgs& args) const;

  private:
    struct GridLineInfo {
        Vec2D UpperLeft;
        Vec2D LowerRight;
        Size2 GridSize;
    };
    GridLineInfo CalculateGridLineInfo(Vec2 offset,
                                       const GraphicsHandlerArgs& args) const;

    void DrawGridLines(Vec2 offset, const GraphicsHandlerArgs& args);

  private:
    Color m_BgColor;

    ShaderManager m_GridShader;
    ShaderManager m_SelectionShader;

    std::vector<float> m_GLBuffer;

    GLVertexArray m_GridVAO;
    GLBuffer m_InstanceBuffer;
    GLBuffer m_CellBuffer;
    GLIndexBuffer m_CellIndexBuffer;

    GLBuffer m_GridLineBuffer;

    GLBuffer m_SelectionBuffer;
    GLIndexBuffer m_SelectionIndexBuffer;

    GLFrameBuffer m_FrameBuffer;
    GLTexture m_Texture;
    GLRenderBuffer m_RenderBuffer;
};

void GraphicsHandler::GenerateGLBuffer(
    Vec2 offset, int32_t minLevel, const std::ranges::input_range auto& grid,
    const GraphicsHandlerArgs& args) {
    m_GLBuffer.clear();

    const auto gridInfo = CalculateGridLineInfo(offset, args);
    const auto minCellX = gridInfo.UpperLeft.X / args.CellSize.Width;
    const auto minCellY = gridInfo.UpperLeft.Y / args.CellSize.Height;
    const auto maxCellX =
        std::ceil(gridInfo.LowerRight.X / args.CellSize.Width) - 1.f;
    const auto maxCellY =
        std::ceil(gridInfo.LowerRight.Y / args.CellSize.Height) - 1.f;

    if constexpr (std::ranges::sized_range<decltype(grid)>) {
        const auto visibleCapacity =
            static_cast<size_t>(std::max(gridInfo.GridSize.Width, 0) *
                                std::max(gridInfo.GridSize.Height, 0));
        const auto reserveCount = std::min(grid.size(), visibleCapacity);
        m_GLBuffer.reserve(reserveCount * 3);
    }

    const auto maxPop = std::exp2(static_cast<double>(2 * minLevel));

    const auto pushToBuffer = [&]<typename VecType>(const VecType& vec,
                                                    int64_t population) {
        const auto x = static_cast<double>(vec.X + offset.X);
        const auto y = static_cast<double>(vec.Y + offset.Y);
        if (x < minCellX || x > maxCellX || y < minCellY || y > maxCellY) {
            return;
        }

        const auto opacity =
            std::clamp(static_cast<float>(population / maxPop), 0.f, 1.f);
        const auto cellScale = std::powf(2.f, static_cast<float>(minLevel));
        m_GLBuffer.push_back(
            static_cast<float>(x - Camera.Center.x / args.CellSize.Width) /
            cellScale);
        m_GLBuffer.push_back(
            static_cast<float>(y - Camera.Center.y / args.CellSize.Height) /
            cellScale);
        m_GLBuffer.push_back(opacity);
    };

    if constexpr (std::is_same_v<std::decay_t<decltype(grid)>, HashQuadtree>) {
        const auto visibleWorldBounds = VisibleBounds(args);
        const auto boundsX = visibleWorldBounds.X - BigInt{offset.X};
        const auto boundsY = visibleWorldBounds.Y - BigInt{offset.Y};
        const auto boundsWidth = visibleWorldBounds.Width;
        const auto boundsHeight = visibleWorldBounds.Height;

        const auto fitsInt32 = [&] {
            const BigInt int32Min{std::numeric_limits<int32_t>::min()};
            const BigInt int32Max{std::numeric_limits<int32_t>::max()};

            if (boundsWidth < BigZero || boundsHeight < BigZero) {
                return false;
            }
            if (boundsWidth > int32Max || boundsHeight > int32Max) {
                return false;
            }

            const auto right = boundsX + boundsWidth;
            const auto bottom = boundsY + boundsHeight;

            return boundsX >= int32Min && boundsX <= int32Max &&
                   boundsY >= int32Min && boundsY <= int32Max &&
                   right >= int32Min && right <= int32Max &&
                   bottom >= int32Min && bottom <= int32Max;
        }();

        if (fitsInt32) {
            const Rect localBounds{boundsX.convert_to<int32_t>(),
                                   boundsY.convert_to<int32_t>(),
                                   boundsWidth.convert_to<int32_t>(),
                                   boundsHeight.convert_to<int32_t>()};
            grid.ForEachCell(pushToBuffer, localBounds, minLevel);
            return;
        }

        const BigRect localBounds{boundsX, boundsY, boundsWidth, boundsHeight};
        grid.ForEachCell(pushToBuffer, localBounds, minLevel);
    } else {
        for (const auto vec : grid) {
            pushToBuffer(vec, minLevel);
        }
    }
}

void GraphicsHandler::DrawGrid(Vec2 offset,
                               const std::ranges::input_range auto& grid,
                               const GraphicsHandlerArgs& args) {
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

    const auto minLevel = [&] {
        if constexpr (std::is_same_v<std::decay_t<decltype(grid)>,
                                     HashQuadtree>) {
            const auto scale =
                1.0 / static_cast<double>(args.CellSize.Width * Camera.Zoom);
            if (!std::isfinite(scale) || scale <= 0.0) {
                return 0;
            }
            const auto level = std::floor(std::log2(scale));
            if (!std::isfinite(level)) {
                return 0;
            }
            return std::max(0, static_cast<int32_t>(level));
        } else {
            return 0;
        }
    }();

    m_GridShader.AttachUniformFloat(
        "u_CellScale", std::powf(2.f, static_cast<float>(minLevel)));

    GenerateGLBuffer(offset, minLevel, grid, args);

    GL_DEBUG(glBindBuffer(GL_ARRAY_BUFFER, m_InstanceBuffer.ID()));

    GL_DEBUG(glBufferData(GL_ARRAY_BUFFER, m_GLBuffer.size() * sizeof(float),
                          nullptr, GL_DYNAMIC_DRAW));
    GL_DEBUG(glBufferSubData(GL_ARRAY_BUFFER, 0,
                             m_GLBuffer.size() * sizeof(float),
                             m_GLBuffer.data()));

    GL_DEBUG(
        glDrawElementsInstanced(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, nullptr,
                                static_cast<GLsizei>(m_GLBuffer.size() / 3)));

    GL_DEBUG(glBindVertexArray(0));
}

} // namespace gol

#endif
