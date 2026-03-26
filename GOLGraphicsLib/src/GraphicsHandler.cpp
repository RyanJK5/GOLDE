#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <set>
#include <utility>
#include <vector>

#include "GLException.hpp"
#include "Graphics2D.hpp"
#include "GraphicsHandler.hpp"
#include "Logging.hpp"
#include "ShaderManager.hpp"

namespace gol {
FrameBufferBinder::FrameBufferBinder(const GLFrameBuffer& buffer) {
    GL_DEBUG(glBindFramebuffer(GL_FRAMEBUFFER, buffer.ID()));
}

FrameBufferBinder::~FrameBufferBinder() {
    GL_DEBUG(glBindFramebuffer(GL_FRAMEBUFFER, 0));
}

GraphicsHandler::GraphicsHandler(const std::filesystem::path& shaderDirectory,
                                 int32_t windowWidth, int32_t windowHeight,
                                 Color bgColor)
    : m_BgColor(bgColor), m_GridShader(shaderDirectory / "grid.shader"),
      m_SelectionShader(shaderDirectory / "selection.shader") {
    InitGridBuffer();

    GL_DEBUG(glBindFramebuffer(GL_FRAMEBUFFER, m_FrameBuffer.ID()));

    GL_DEBUG(glBindTexture(GL_TEXTURE_2D, m_Texture.ID()));
    GL_DEBUG(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, windowWidth, windowHeight,
                          0, GL_LUMINANCE, GL_UNSIGNED_BYTE, NULL));
    GL_DEBUG(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
    GL_DEBUG(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
    GL_DEBUG(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                    GL_TEXTURE_2D, m_Texture.ID(), 0));

    GL_DEBUG(glBindRenderbuffer(GL_RENDERBUFFER, m_RenderBuffer.ID()));
    GL_DEBUG(glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8,
                                   windowWidth, windowHeight));
    GL_DEBUG(glFramebufferRenderbuffer(GL_FRAMEBUFFER,
                                       GL_DEPTH_STENCIL_ATTACHMENT,
                                       GL_RENDERBUFFER, m_RenderBuffer.ID()));

    GL_DEBUG(if (glCheckFramebufferStatus(GL_FRAMEBUFFER) !=
                 GL_FRAMEBUFFER_COMPLETE) throw GLException("Framebuffer not "
                                                            "properly "
                                                            "initialized"););

    GL_DEBUG(glBindFramebuffer(GL_FRAMEBUFFER, 0));
    GL_DEBUG(glBindTexture(GL_TEXTURE_2D, 0));
    GL_DEBUG(glBindRenderbuffer(GL_RENDERBUFFER, 0));
}

void GraphicsHandler::InitGridBuffer() {
    GL_DEBUG(glBindVertexArray(m_GridVAO.ID()));

    constexpr static std::array quadVertices{0.f, 0.f, 0.f, 1.f,
                                             1.f, 1.f, 1.f, 0.f};

    GL_DEBUG(glBindBuffer(GL_ARRAY_BUFFER, m_CellBuffer.ID()));
    GL_DEBUG(glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices),
                          quadVertices.data(), GL_STATIC_DRAW));
    GL_DEBUG(glEnableVertexAttribArray(0));
    GL_DEBUG(glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 2,
                                   nullptr));

    constexpr static std::array<uint16_t, 6> quadIndices{0, 1, 2, 2, 3, 0};

    GL_DEBUG(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_CellIndexBuffer.ID()));
    GL_DEBUG(glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(quadIndices),
                          quadIndices.data(), GL_STATIC_DRAW));

    GL_DEBUG(glBindBuffer(GL_ARRAY_BUFFER, m_InstanceBuffer.ID()));
    GL_DEBUG(glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW));
    GL_DEBUG(glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 3,
                                   nullptr));
    GL_DEBUG(glEnableVertexAttribArray(1));
    GL_DEBUG(glVertexAttribDivisor(1, 1));

    GL_DEBUG(glVertexAttribPointer(
        2, 1, GL_FLOAT, GL_FALSE, sizeof(float) * 3,
        reinterpret_cast<const void*>(sizeof(float) * 2)));
    GL_DEBUG(glEnableVertexAttribArray(2));
    GL_DEBUG(glVertexAttribDivisor(2, 1));

    GL_DEBUG(glBindVertexArray(0));
}

void GraphicsHandler::RescaleFrameBuffer(Rect windowBounds,
                                         Rect viewportBounds) {
    GL_DEBUG(glBindTexture(GL_TEXTURE_2D, m_Texture.ID()));
    GL_DEBUG(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, windowBounds.Width,
                          windowBounds.Height, 0, GL_RGB, GL_UNSIGNED_BYTE,
                          NULL));
    GL_DEBUG(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
    GL_DEBUG(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));

    FrameBufferBinder binder{m_FrameBuffer};

    glViewport(viewportBounds.X - windowBounds.X,
               viewportBounds.Y - windowBounds.Y, viewportBounds.Width,
               viewportBounds.Height);
    GL_DEBUG(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                    GL_TEXTURE_2D, m_Texture.ID(), 0));
    GL_DEBUG(glBindRenderbuffer(GL_RENDERBUFFER, m_RenderBuffer.ID()));
    GL_DEBUG(glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8,
                                   windowBounds.Width, windowBounds.Height));
    GL_DEBUG(glFramebufferRenderbuffer(GL_FRAMEBUFFER,
                                       GL_DEPTH_STENCIL_ATTACHMENT,
                                       GL_RENDERBUFFER, m_RenderBuffer.ID()));
}

void GraphicsHandler::ClearBackground(const GraphicsHandlerArgs& args) {
    FrameBufferBinder binder{m_FrameBuffer};

    GL_DEBUG(glClear(GL_COLOR_BUFFER_BIT));
    if (args.GridSize.Width == 0 || args.GridSize.Height == 0) {
        GL_DEBUG(glClearColor(0.f, 0.f, 0.f, 1.f));
        GL_DEBUG(glClear(GL_COLOR_BUFFER_BIT));
        return;
    }

    GL_DEBUG(glEnable(GL_SCISSOR_TEST));

    GL_DEBUG(
        glScissor(0, 0, args.ViewportBounds.Width, args.ViewportBounds.Height));
    GL_DEBUG(glClearColor(m_BgColor.Red, m_BgColor.Green, m_BgColor.Blue,
                          m_BgColor.Alpha));
    GL_DEBUG(glClear(GL_COLOR_BUFFER_BIT));

    Size2F gridScreenDimensions{
        static_cast<float>(args.GridSize.Width) * args.CellSize.Width,
        static_cast<float>(args.GridSize.Height) * args.CellSize.Height};
    auto origin = Camera.WorldToScreenPos(Vec2D{0.0, 0.0}, args.ViewportBounds,
                                          gridScreenDimensions);
    auto lowerRight = Camera.WorldToScreenPos(
        Vec2D{gridScreenDimensions.Width, gridScreenDimensions.Height},
        args.ViewportBounds, gridScreenDimensions);
    GL_DEBUG(glScissor(static_cast<int32_t>(origin.x),
                       static_cast<int32_t>(origin.y),
                       static_cast<int32_t>(lowerRight.x - origin.x),
                       static_cast<int32_t>(lowerRight.y - origin.y)));

    GL_DEBUG(glClearColor(0.f, 0.f, 0.f, 1.f));
    GL_DEBUG(glClear(GL_COLOR_BUFFER_BIT));

    GL_DEBUG(glDisable(GL_SCISSOR_TEST));
}

GraphicsHandler::GridLineInfo
GraphicsHandler::CalculateGridLineInfo(Vec2,
                                       const GraphicsHandlerArgs& args) const {
    const auto cameraCorner =
        Vec2D{Camera.Center.x - args.ViewportBounds.Width / 2.0 / Camera.Zoom,
              Camera.Center.y - args.ViewportBounds.Height / 2.0 / Camera.Zoom};

    const bool bounded = args.GridSize.Width != 0 && args.GridSize.Height != 0;

    const auto upperLeft = [cameraCorner, bounded, args]() {
        const auto unboundedReturn =
            Vec2D{std::floor(cameraCorner.X / args.CellSize.Width) *
                      args.CellSize.Width,
                  std::floor(cameraCorner.Y / args.CellSize.Height) *
                      args.CellSize.Height};

        if (!bounded)
            return unboundedReturn;
        return Vec2D{std::max(unboundedReturn.X, 0.0),
                     std::max(unboundedReturn.Y, 0.0)};
    }();

    const auto lowerRight = [cameraCorner, bounded, args, this]() {
        const auto unboundedReturn =
            Vec2D{cameraCorner.X + args.ViewportBounds.Width / Camera.Zoom,
                  cameraCorner.Y + args.ViewportBounds.Height / Camera.Zoom};

        if (!bounded)
            return unboundedReturn;
        return Vec2D{std::min(unboundedReturn.X,
                              static_cast<double>(args.GridSize.Width *
                                                  args.CellSize.Width)),
                     std::min(unboundedReturn.Y,
                              static_cast<double>(args.GridSize.Height *
                                                  args.CellSize.Height))};
    }();

    const auto gridSize = [upperLeft, bounded, args, this]() {
        const auto unboundedReturn = Size2{
            static_cast<int32_t>(std::ceil(args.ViewportBounds.Width /
                                           Camera.Zoom / args.CellSize.Width)),
            static_cast<int32_t>(
                std::ceil(args.ViewportBounds.Height / Camera.Zoom /
                          args.CellSize.Height))};

        if (!bounded)
            return unboundedReturn;

        const auto gridDimensions =
            Size2{args.GridSize.Width -
                      static_cast<int32_t>(upperLeft.X / args.CellSize.Width),
                  args.GridSize.Height -
                      static_cast<int32_t>(upperLeft.Y / args.CellSize.Height)};

        return Size2{std::min(gridDimensions.Width, unboundedReturn.Width),
                     std::min(gridDimensions.Height, unboundedReturn.Height)};
    }();

    return GridLineInfo{
        .UpperLeft = upperLeft, .LowerRight = lowerRight, .GridSize = gridSize};
}

void GraphicsHandler::DrawGridLines(Vec2 offset,
                                    const GraphicsHandlerArgs& args) {
    const auto gridInfo = CalculateGridLineInfo(offset, args);

    auto positions = std::vector<float>{};
    for (int32_t i = 0; i <= gridInfo.GridSize.Height; i++) {
        positions.push_back(
            static_cast<float>(gridInfo.UpperLeft.X - Camera.Center.x));
        positions.push_back(static_cast<float>(
            gridInfo.UpperLeft.Y + args.CellSize.Height * i - Camera.Center.y));
        positions.push_back(
            static_cast<float>(gridInfo.LowerRight.X - Camera.Center.x));
        positions.push_back(static_cast<float>(
            gridInfo.UpperLeft.Y + args.CellSize.Height * i - Camera.Center.y));
    }

    for (int32_t i = 0; i <= gridInfo.GridSize.Width; i++) {
        positions.push_back(static_cast<float>(
            gridInfo.UpperLeft.X + args.CellSize.Width * i - Camera.Center.x));
        positions.push_back(
            static_cast<float>(gridInfo.UpperLeft.Y - Camera.Center.y));
        positions.push_back(static_cast<float>(
            gridInfo.UpperLeft.X + args.CellSize.Width * i - Camera.Center.x));
        positions.push_back(
            static_cast<float>(gridInfo.LowerRight.Y - Camera.Center.y));
    }

    GL_DEBUG(glBindBuffer(GL_ARRAY_BUFFER, m_GridLineBuffer.ID()));
    GL_DEBUG(glBufferData(GL_ARRAY_BUFFER, positions.size() * sizeof(float),
                          positions.data(), GL_DYNAMIC_DRAW));
    GL_DEBUG(glEnableVertexAttribArray(0));
    GL_DEBUG(
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 2, 0));
    GL_DEBUG(
        glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(positions.size() / 2)));
}

RectF GraphicsHandler::GridToScreenBounds(
    Rect region, const GraphicsHandlerArgs& args) const {
    return RectF{
        static_cast<float>(static_cast<double>(region.X) * args.CellSize.Width -
                           Camera.Center.x),
        static_cast<float>(static_cast<double>(region.Y) *
                               args.CellSize.Height -
                           Camera.Center.y),
        static_cast<float>(region.Width * args.CellSize.Width),
        static_cast<float>(region.Height * args.CellSize.Height),
    };
}

void GraphicsHandler::DrawSelection(Rect region,
                                    const GraphicsHandlerArgs& args) {
    FrameBufferBinder binder{m_FrameBuffer};
    GL_DEBUG(glUseProgram(m_SelectionShader.Program()));

    auto matrix = Camera.OrthographicProjection(args.ViewportBounds.Size());
    m_SelectionShader.AttachUniformMatrix4("u_MVP", matrix);

    const auto rect = GridToScreenBounds(region, args);
    const std::array positions{rect.UpperLeft().X,  rect.UpperLeft().Y,
                               rect.LowerLeft().X,  rect.LowerLeft().Y,
                               rect.LowerRight().X, rect.LowerRight().Y,
                               rect.UpperRight().X, rect.UpperRight().Y};

    GL_DEBUG(glBindBuffer(GL_ARRAY_BUFFER, m_SelectionBuffer.ID()));
    GL_DEBUG(glBufferData(GL_ARRAY_BUFFER, 2 * 4 * sizeof(float),
                          positions.data(), GL_DYNAMIC_DRAW));
    GL_DEBUG(glEnableVertexAttribArray(0));
    GL_DEBUG(
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 2, 0));

    GL_DEBUG(glEnable(GL_BLEND));
    GL_DEBUG(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));

    GL_DEBUG(
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_SelectionIndexBuffer.ID()));

    // Draw translucent blue fill
    constexpr static std::array<uint8_t, 6> fillIndices{0, 1, 2, 0, 2, 3};
    m_SelectionShader.AttachUniformVec4("u_Color", {0.2f, 0.5f, 1.0f, 0.25f});
    GL_DEBUG(glBufferData(GL_ELEMENT_ARRAY_BUFFER, 6 * sizeof(uint8_t),
                          fillIndices.data(), GL_DYNAMIC_DRAW));
    GL_DEBUG(glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, nullptr));

    // Draw opaque blue border
    constexpr static std::array<uint8_t, 8> borderIndices{0, 1, 1, 2,
                                                          2, 3, 3, 0};
    m_SelectionShader.AttachUniformVec4("u_Color", {0.2f, 0.5f, 1.0f, 1.0f});
    GL_DEBUG(glBufferData(GL_ELEMENT_ARRAY_BUFFER, 8 * sizeof(uint8_t),
                          borderIndices.data(), GL_DYNAMIC_DRAW));
    GL_DEBUG(glDrawElements(GL_LINES, 8, GL_UNSIGNED_BYTE, nullptr));
    GL_DEBUG(glDisable(GL_BLEND));
}

void GraphicsHandler::CenterCamera(const GraphicsHandlerArgs& args) {
    Camera.Center = {args.GridSize.Width / 2.0 * args.CellSize.Width,
                     args.GridSize.Height / 2.0 * args.CellSize.Height};
}

Rect GraphicsHandler::VisibleBounds(const GraphicsHandlerArgs& args) const {
    const auto viewBounds = args.ViewportBounds;
    const auto topLeftWorld =
        Camera.ScreenToWorldPos(Vec2F{static_cast<float>(viewBounds.X),
                                      static_cast<float>(viewBounds.Y)},
                                viewBounds);
    const auto bottomRightWorld = Camera.ScreenToWorldPos(
        Vec2F{static_cast<float>(viewBounds.X + viewBounds.Width),
              static_cast<float>(viewBounds.Y + viewBounds.Height)},
        viewBounds);
    const auto [minWorldX, maxWorldX] =
        std::minmax(topLeftWorld.x, bottomRightWorld.x);
    const auto [minWorldY, maxWorldY] =
        std::minmax(topLeftWorld.y, bottomRightWorld.y);
    const auto minCellX =
        static_cast<int32_t>(std::floor(minWorldX / args.CellSize.Width));
    const auto minCellY =
        static_cast<int32_t>(std::floor(minWorldY / args.CellSize.Height));
    const auto maxCellX =
        static_cast<int32_t>(std::ceil(maxWorldX / args.CellSize.Width));
    const auto maxCellY =
        static_cast<int32_t>(std::ceil(maxWorldY / args.CellSize.Height));

    return Rect{minCellX, minCellY, std::max(0, maxCellX - minCellX),
                std::max(0, maxCellY - minCellY)};
}
} // namespace gol
