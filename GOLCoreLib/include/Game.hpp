#ifndef Game_h_
#define Game_h_

#include <cstdint>
#include <filesystem>
#include <imgui/imgui.h>
#include <vector>

#include "GameEnums.hpp"
#include "Graphics2D.hpp"
#include "PopupWindow.hpp"
#include "PresetSelection.hpp"
#include "SimulationCommand.hpp"
#include "SimulationControl.hpp"
#include "SimulationEditor.hpp"

namespace gol {
// Simple RAII wrapper for the GLFWwindow handle.
class OpenGLWindow {
  public:
    Rect Bounds;

    OpenGLWindow(int32_t width, int32_t height);
    ~OpenGLWindow();

    OpenGLWindow(const OpenGLWindow &other) = delete;
    OpenGLWindow(OpenGLWindow &&other) noexcept = delete;

    OpenGLWindow &operator=(const OpenGLWindow &other) = delete;
    OpenGLWindow &operator=(OpenGLWindow &&other) noexcept = delete;

    GLFWwindow *Get() const { return m_Underlying; }

  private:
    GLFWwindow *m_Underlying;
};

// A single instance of GOLDE. The core event loop runs through here.
class Game {
  public:
    static constexpr int32_t DefaultWindowWidth = 1920;
    static constexpr int32_t DefaultWindowHeight = 1080;
    static constexpr int32_t DefaultGridWidth = 0; // Unbounded
    static constexpr int32_t DefaultGridHeight = 0; // Unbounded

  public:
    // Takes in a file to be parsed by ConfigLoader.
    Game();
    ~Game();

    Game(const Game &other) = delete;
    Game(const Game &&other) = delete;
    Game &operator=(const Game &other) = delete;
    Game &operator=(const Game &&other) = delete;

    // Starts running the core loop.
    void Begin();

    bool Open() const { return !glfwWindowShouldClose(m_Window.Get()); }

  private:
    // Main update function
    void UpdateEditors(SimulationControlResult &controlResult,
                       const PresetSelectionResult &presetResult);
    
    // Callback for if the user decides to delete a universe without saving
    void HandleUnsaved(PopupWindowState state);

    void BeginFrame(); // Call at start of frame
    void EndFrame(); // Call at end of frame

    void InitImGUI(const std::filesystem::path &stylePath);
    void CreateDockspace();

    bool WindowCanClose();
    void HandleWindowClose(PopupWindowState state);

    bool CheckForNewEditors(const SimulationControlResult &controlResult);
    void CreateEditorDockspace();

    void InitDockspace(uint32_t dockspaceID, ImVec2 windowSize);

  private:
    EditorResult m_State = {.Simulation = {.State = SimulationState::Paint}};
    size_t m_LastActive = 0UZ; // the index of the last active editor
    
    OpenGLWindow m_Window; // Underlying window backend

    std::vector<SimulationEditor> m_Editors; // The various open editors
    uint32_t m_EditorCounter = 0; // Counter for assigning unique IDs to each editor

    WarnWindow m_UnsavedWarning;
    // The editor the user is trying to close without saving. If the delete
    // prompt is available and this is `nullptr`, then the user is trying to
    // close all windows.
    SimulationEditor *m_Unsaved = nullptr; 

    SimulationControl m_Control;
    PresetSelection m_PresetSelection;
    ImFont *m_Font = nullptr; // Standard font for all children

    // Check for establishing the dockspace on startup
    bool m_Startup = true;
};
} // namespace gol

#endif