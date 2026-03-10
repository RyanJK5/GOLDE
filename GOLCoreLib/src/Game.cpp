#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <format>
#include <imgui/imgui.h>
#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_impl_opengl3.h>
#include <imgui/imgui_internal.h>
#include <optional>
#include <utility>

#include "ConfigLoader.hpp"
#include "GLException.hpp"
#include "Game.hpp"
#include "GameEnums.hpp"
#include "Graphics2D.hpp"
#include "Logging.hpp"
#include "PopupWindow.hpp"
#include "PresetSelectionResult.hpp"
#include "SimulationCommand.hpp"
#include "SimulationControlResult.hpp"
#include "SimulationEditor.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <vector>

namespace gol {
OpenGLWindow::OpenGLWindow(int32_t width, int32_t height)
    : Bounds(0, 0, width, height) {
    if (!glfwInit())
        throw GLException("Failed to initialize glfw");

    m_Underlying = glfwCreateWindow(width, height, "GOLDE", NULL, NULL);

    const auto deleteStbiImage = [](unsigned char* pixels) {
        stbi_image_free(pixels);
    };

    auto iconWidth = 0;
    auto iconHeight = 0;
    [[maybe_unused]] auto iconChannels = 0;

    std::unique_ptr<unsigned char, decltype(deleteStbiImage)> pixels{
        stbi_load("resources/icon.png", &iconWidth, &iconHeight, &iconChannels,
                  4),
        deleteStbiImage};
    const GLFWimage icon{
        .width = iconWidth, .height = iconHeight, .pixels = pixels.get()};
    glfwSetWindowIcon(m_Underlying, 1, &icon);

    if (!m_Underlying)
        throw GLException("Failed to create window");

    glfwMakeContextCurrent(m_Underlying);
    GL_DEBUG(glLineWidth(4));
    GL_DEBUG(glfwSwapInterval(1));

    GL_DEBUG(auto result = glewInit());
    if (result != GLEW_OK)
        throw GLException(
            reinterpret_cast<const char*>(glewGetErrorString(result)));
}

OpenGLWindow::~OpenGLWindow() { glfwTerminate(); }

Game::Game()
    : m_Window(DefaultWindowWidth, DefaultWindowHeight),
      m_UnsavedWarning("Unsaved Changes",
                       std::bind_front(&Game::HandleWindowClose, this)),
      m_Control(ConfigLoader::LoadYAML<ImVec4>(std::filesystem::path{"config"} /
                                               "shortcuts.yml")),
      m_PresetSelection(std::filesystem::current_path() / "templates") {
    m_Editors.emplace_back(m_EditorCounter++, std::filesystem::path{},
                           Size2{DefaultWindowWidth, DefaultWindowHeight},
                           Size2{DefaultGridWidth, DefaultGridHeight});
    InitImGUI(std::filesystem::path{"config"} / "style.yml");
}

Game::~Game() {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

void Game::Begin() {
    while (!glfwWindowShouldClose(m_Window.Get())) {
        BeginFrame();

        auto controlResult = m_Control.Update(m_State);
        auto presetResult = m_PresetSelection.Update(m_State);
        UpdateEditors(controlResult, presetResult);

        EndFrame();
        if (glfwWindowShouldClose(m_Window.Get()))
            glfwSetWindowShouldClose(m_Window.Get(), WindowCanClose());
    }
}

void Game::UpdateEditors(SimulationControlResult& controlResult,
                         const PresetSelectionResult& presetResult) {
    ImGui::Begin("###EditorDockspace", nullptr,
                 ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoDecoration);

    ImGuiID subDockspace = ImGui::GetID("###EditorDockspace");
    ImGui::DockSpace(subDockspace, ImGui::GetContentRegionAvail(),
                     ImGuiDockNodeFlags_None);
    if (m_Startup)
        CreateEditorDockspace();

    auto newWindowIndex = m_LastActive;
    bool pastWindowEnabled = CheckForNewEditors(controlResult);

    m_UnsavedWarning.Update();

    if (m_LastActive < m_Editors.size())
        std::swap(m_Editors[m_LastActive], m_Editors.back());
    m_LastActive = m_Editors.size() > 0 ? m_Editors.size() - 1 : 0UZ;
    if (m_Editors.size() == 0)
        m_State.Simulation.State = SimulationState::None;
    for (size_t i = 0; i < m_Editors.size(); i++) {
        auto activeOverride = [&]() -> std::optional<bool> {
            if (newWindowIndex != i && !pastWindowEnabled)
                return false;
            if (m_LastActive == i)
                return true;
            return std::nullopt;
        }();

        auto result =
            m_Editors[i].Update(activeOverride, controlResult, presetResult);

        if (result.Active) {
            m_State = result;
            m_LastActive = i;
        }
        if (result.Closing && !result.File.HasUnsavedChanges) {
            m_Editors.erase(m_Editors.begin() + i--);
        } else if (result.Closing) {
            m_UnsavedWarning.Activate();
            m_UnsavedWarning.Message =
                result.File.CurrentFilePath.empty()
                    ? "This file has not been saved. Are you sure you want to "
                      "close "
                      "it without saving?"
                    : std::format(
                          "{} has unsaved changes. Are you sure you want to "
                          "close it without saving?",
                          result.File.CurrentFilePath.filename().string());
            m_Unsaved = &m_Editors[i];
        }
    }

    ImGui::End();
}

void Game::InitImGUI(const std::filesystem::path& stylePath) {
    const auto styleInfo = ConfigLoader::LoadYAML<ImVec4>(stylePath);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
#ifdef _DEBUG
    io.ConfigDebugHighlightIdConflicts = true;
#else
    io.ConfigDebugHighlightIdConflicts = false;
#endif
    io.IniFilename = nullptr;

    auto path = std::filesystem::path("resources") / "font" / "arial.ttf";
    m_Font = io.Fonts->AddFontFromFileTTF(path.string().c_str(), 30.0f);

    auto iconPath =
        std::filesystem::path("resources") / "font" / "fontawesome7.otf";
    auto config = ImFontConfig{};
    config.MergeMode = true;
    io.Fonts->AddFontFromFileTTF(iconPath.string().c_str(), 30.0f, &config);

    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowMenuButtonPosition = ImGuiDir_None;

    for (auto&& [imguiCol, styleCol] : styleInfo.AttributeColors)
        style.Colors[imguiCol] = styleInfo.StyleColors.at(styleCol);

    ImGui_ImplGlfw_InitForOpenGL(m_Window.Get(), true);
    ImGui_ImplOpenGL3_Init();
}

void Game::HandleUnsaved(PopupWindowState state) {
    if (state == PopupWindowState::Failure) {
        m_Unsaved = nullptr;
        return;
    }

    if (state != PopupWindowState::Success)
        return;

    if (!m_Unsaved) {
        m_Editors.clear();
        glfwSetWindowShouldClose(m_Window.Get(), true);
    } else
        std::erase(m_Editors, *m_Unsaved);
    m_Unsaved = nullptr;
}

void Game::BeginFrame() {
    GL_DEBUG(glfwPollEvents());
    GL_DEBUG(glClear(GL_COLOR_BUFFER_BIT));

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    ImGui::PushFont(m_Font, 30.0f);
    CreateDockspace();
}

void Game::EndFrame() {
    m_Startup = false;
    ImGui::PopFont();
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    GLFWwindow* backup_current_context = glfwGetCurrentContext();
    ImGui::UpdatePlatformWindows();
    ImGui::RenderPlatformWindowsDefault();
    glfwMakeContextCurrent(backup_current_context);

    GL_DEBUG(glfwSwapBuffers(m_Window.Get()));
}

void Game::CreateDockspace() {
    ImGui::SetNextWindowPos({0, 0}, ImGuiCond_Once);

    int32_t windowWidth, windowHeight;
    GL_DEBUG(
        glfwGetFramebufferSize(m_Window.Get(), &windowWidth, &windowHeight));
    const ImVec2 windowSize = {static_cast<float>(windowWidth),
                               static_cast<float>(windowHeight)};
    ImGui::SetNextWindowSize(windowSize);

    ImGuiID dockspaceID = ImGui::GetID("DockSpace");
    ImGui::DockSpaceOverViewport(dockspaceID, ImGui::GetMainViewport());
    if (m_Startup)
        InitDockspace(dockspaceID, windowSize);
}

void Game::InitDockspace(uint32_t dockspaceID, ImVec2 windowSize) {
    ImGui::DockBuilderRemoveNode(dockspaceID);
    ImGui::DockBuilderAddNode(
        dockspaceID,
        ImGuiDockNodeFlags_PassthruCentralNode |
            static_cast<ImGuiDockNodeFlags>(ImGuiDockNodeFlags_DockSpace));
    ImGui::DockBuilderSetNodeSize(dockspaceID, windowSize);

    ImGuiID leftID{}, rightID{};
    ImGui::DockBuilderSplitNode(dockspaceID, ImGuiDir_Left, 0.25f, &leftID,
                                &rightID);
    auto downID = ImGui::DockBuilderSplitNode(rightID, ImGuiDir_Down, 0.25f,
                                              nullptr, &rightID);

    ImGui::DockBuilderDockWindow("Presets", downID);
    ImGui::DockBuilderDockWindow("###EditorDockspace", rightID);
    ImGui::DockBuilderDockWindow("Simulation Control", leftID);
    ImGui::DockBuilderFinish(dockspaceID);
}

void Game::HandleWindowClose(PopupWindowState state) {
    switch (state) {
    case PopupWindowState::Failure:
        m_Unsaved = nullptr;
        break;
    case PopupWindowState::Success:
        if (!m_Unsaved) {
            m_Editors.clear();
            glfwSetWindowShouldClose(m_Window.Get(), true);
        } else
            std::erase(m_Editors, *m_Unsaved);
        m_Unsaved = nullptr;
        break;
    }
}

bool Game::WindowCanClose() {
    if (std::ranges::all_of(
            m_Editors, [](const auto& editor) { return editor.IsSaved(); }))
        return true;

    const auto fileStringRepresentation = [](const SimulationEditor& editor) {
        return editor.CurrentFilePath().empty()
                   ? "(untitled)"
                   : editor.CurrentFilePath().filename().string();
    };

    auto fileNames = m_Editors | std::views::filter([](const auto& editor) {
                         return !editor.IsSaved();
                     }) |
                     std::views::transform(fileStringRepresentation) |
                     std::ranges::to<std::vector>();
    std::ranges::sort(fileNames);

    m_UnsavedWarning.Activate();
    m_UnsavedWarning.Message = "The following files have unsaved changes:";
    for (const auto& fileName : fileNames)
        m_UnsavedWarning.Message += std::format("\n- {}", fileName);
    m_UnsavedWarning.Message +=
        "\nAre you sure you want to close the application without saving?";

    return false;
}

bool Game::CheckForNewEditors(const SimulationControlResult& controlResult) {
    if (!controlResult.Command)
        return true;

    const auto* loadCmd = std::get_if<LoadCommand>(&*controlResult.Command);
    const auto* newFileCmd =
        std::get_if<NewFileCommand>(&*controlResult.Command);

    if (!loadCmd && !newFileCmd)
        return true;

    const auto& filePath = loadCmd ? loadCmd->FilePath : newFileCmd->FilePath;

    const bool fileOpen =
        loadCmd && std::ranges::any_of(
                       m_Editors, [&filePath](const SimulationEditor& editor) {
                           return editor.CurrentFilePath() == filePath;
                       });
    if (fileOpen)
        return false;

    m_Editors.emplace_back(m_EditorCounter++, filePath,
                           Size2{DefaultWindowWidth, DefaultWindowHeight},
                           Size2{DefaultGridWidth, DefaultGridHeight});

    CreateEditorDockspace();

    return false;
}

void Game::CreateEditorDockspace() {
    ImGuiID editorDockspaceID = ImGui::GetID("###EditorDockspace");
    ImGui::DockBuilderAddNode(
        editorDockspaceID,
        ImGuiDockNodeFlags_PassthruCentralNode |
            static_cast<ImGuiDockNodeFlags>(ImGuiDockNodeFlags_DockSpace));

    for (size_t i = 0; i < m_Editors.size(); i++) {
        ImGui::DockBuilderDockWindow(
            std::format("###Simulation{}", m_Editors[i].EditorID()).c_str(),
            editorDockspaceID);
    }
    ImGui::DockBuilderFinish(editorDockspaceID);
}
} // namespace gol
