#ifndef DrawManager_hpp_
#define DrawManager_hpp_

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <limits>
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

namespace Golde {
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
    void EnsureGridStateTexture(Size2 size);

    struct GridBlitInfo {
        int64_t MinCoarseX = 0;
        int64_t MinCoarseY = 0;
        int32_t Width = 0;
        int32_t Height = 0;
        float CellScale = 1.f;
    };

    GridBlitInfo GenerateStateBuffer(Vec2 offset, int32_t minLevel,
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

    std::vector<uint8_t> m_StateBuffer;

    GLVertexArray m_GridVAO;
    GLVertexArray m_GridLineVAO;
    GLVertexArray m_SelectionVAO;

    GLBuffer m_CellBuffer;
    GLTexture m_GridStateTexture;
    Size2 m_GridStateTextureSize{};

    GLBuffer m_GridLineBuffer;

    GLBuffer m_SelectionBuffer;
    GLIndexBuffer m_SelectionIndexBuffer;

    GLFrameBuffer m_FrameBuffer;
    GLTexture m_Texture;
    GLRenderBuffer m_RenderBuffer;
};

auto GraphicsHandler::GenerateStateBuffer(
    Vec2 offset, int32_t minLevel, const std::ranges::input_range auto& grid,
    const GraphicsHandlerArgs& args) -> GridBlitInfo {
    m_StateBuffer.clear();

    const auto gridInfo = CalculateGridLineInfo(offset, args);
    const auto minCellX =
        std::floor(gridInfo.UpperLeft.X / args.CellSize.Width);
    const auto minCellY =
        std::floor(gridInfo.UpperLeft.Y / args.CellSize.Height);
    const auto maxCellX =
        std::ceil(gridInfo.LowerRight.X / args.CellSize.Width) - 1.f;
    const auto maxCellY =
        std::ceil(gridInfo.LowerRight.Y / args.CellSize.Height) - 1.f;

    if (maxCellX < minCellX || maxCellY < minCellY) {
        return {};
    }

    const auto cellScale = std::pow(2.0, static_cast<double>(minLevel));
    const auto minCoarseX =
        static_cast<int64_t>(std::floor(minCellX / cellScale));
    const auto minCoarseY =
        static_cast<int64_t>(std::floor(minCellY / cellScale));
    const auto maxCoarseX =
        static_cast<int64_t>(std::floor(maxCellX / cellScale));
    const auto maxCoarseY =
        static_cast<int64_t>(std::floor(maxCellY / cellScale));

    if (maxCoarseX < minCoarseX || maxCoarseY < minCoarseY) {
        return {};
    }

    const auto width64 = maxCoarseX - minCoarseX + 1;
    const auto height64 = maxCoarseY - minCoarseY + 1;
    if (width64 <= 0 || height64 <= 0 ||
        width64 > std::numeric_limits<int32_t>::max() ||
        height64 > std::numeric_limits<int32_t>::max()) {
        return {};
    }

    const auto width = static_cast<int32_t>(width64);
    const auto height = static_cast<int32_t>(height64);

    m_StateBuffer.assign(
        static_cast<size_t>(width) * static_cast<size_t>(height), 0);

    const auto pushToBuffer = [&]<typename VecType>(const VecType& vec) {
        const auto x = static_cast<double>(vec.X + offset.X);
        const auto y = static_cast<double>(vec.Y + offset.Y);
        if (x < minCellX || x > maxCellX || y < minCellY || y > maxCellY) {
            return;
        }

        const auto coarseX = static_cast<int64_t>(std::floor(x / cellScale));
        const auto coarseY = static_cast<int64_t>(std::floor(y / cellScale));
        if (coarseX < minCoarseX || coarseX > maxCoarseX ||
            coarseY < minCoarseY || coarseY > maxCoarseY) {
            return;
        }

        const auto col = static_cast<int32_t>(coarseX - minCoarseX);
        const auto rowFromTop = static_cast<int32_t>(coarseY - minCoarseY);
        const auto row = height - 1 - rowFromTop;
        const auto index =
            static_cast<size_t>(row) * static_cast<size_t>(width) +
            static_cast<size_t>(col);

        m_StateBuffer[index] = 255;
    };

    if constexpr (std::is_same_v<std::decay_t<decltype(grid)>, HashQuadtree>) {
        const auto visibleWorldBounds = VisibleBounds(args);
        const auto boundsX = visibleWorldBounds.X - BigInt{offset.X};
        const auto boundsY = visibleWorldBounds.Y - BigInt{offset.Y};
        const auto boundsWidth = visibleWorldBounds.Width;
        const auto boundsHeight = visibleWorldBounds.Height;

        const auto fitsInt32 = [&] {
            const static BigInt int32Min{std::numeric_limits<int32_t>::min()};
            const static BigInt int32Max{std::numeric_limits<int32_t>::max()};

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
            return {minCoarseX, minCoarseY, width, height,
                    static_cast<float>(cellScale)};
        }
        const BigRect localBounds{boundsX, boundsY, boundsWidth, boundsHeight};
        grid.ForEachCell(pushToBuffer, localBounds, minLevel);
    } else {
        for (const auto vec : grid) {
            pushToBuffer(vec, 1);
        }
    }

    return {minCoarseX, minCoarseY, width, height,
            static_cast<float>(cellScale)};
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
    m_GridShader.AttachUniformMatrix4("u_MVP", matrix);
    m_GridShader.AttachUniformVec4("u_Color", {1.f, 1.f, 1.f, 1.f});
    m_GridShader.AttachUniformInt("u_StateTex", 0);

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

    const auto blitInfo = GenerateStateBuffer(offset, minLevel, grid, args);
    if (blitInfo.Width <= 0 || blitInfo.Height <= 0) {
        GL_DEBUG(glBindVertexArray(0));
        return;
    }

    EnsureGridStateTexture({blitInfo.Width, blitInfo.Height});

    GL_DEBUG(glActiveTexture(GL_TEXTURE0));
    GL_DEBUG(glBindTexture(GL_TEXTURE_2D, m_GridStateTexture.ID()));
    GL_DEBUG(glPixelStorei(GL_UNPACK_ALIGNMENT, 1));
    GL_DEBUG(glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, blitInfo.Width,
                             blitInfo.Height, GL_RED, GL_UNSIGNED_BYTE,
                             m_StateBuffer.data()));

    const auto scaledCellWidth =
        static_cast<double>(args.CellSize.Width) * blitInfo.CellScale;
    const auto scaledCellHeight =
        static_cast<double>(args.CellSize.Height) * blitInfo.CellScale;

    const auto left = static_cast<float>(
        static_cast<double>(blitInfo.MinCoarseX) * scaledCellWidth -
        Camera.Center.x);
    const auto top = static_cast<float>(
        static_cast<double>(blitInfo.MinCoarseY) * scaledCellHeight -
        Camera.Center.y);
    const auto right =
        left + static_cast<float>(scaledCellWidth * blitInfo.Width);
    const auto bottom =
        top + static_cast<float>(scaledCellHeight * blitInfo.Height);

    const std::array<float, 24> quadVertices = {
        left,  top,    0.f, 1.f, left,  bottom, 0.f, 0.f,
        right, bottom, 1.f, 0.f, right, bottom, 1.f, 0.f,
        right, top,    1.f, 1.f, left,  top,    0.f, 1.f,
    };

    GL_DEBUG(glBindBuffer(GL_ARRAY_BUFFER, m_CellBuffer.ID()));
    GL_DEBUG(glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices),
                          quadVertices.data(), GL_DYNAMIC_DRAW));

    GL_DEBUG(glDrawArrays(GL_TRIANGLES, 0, 6));

    GL_DEBUG(glBindVertexArray(0));
}

} // namespace Golde

#endif
