#ifndef PresetComponent_hpp_
#define PresetComponent_hpp_

#include <cstdint>
#include <filesystem>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>

#include "EditorResult.hpp"
#include "GameGrid.hpp"
#include "Graphics2D.hpp"
#include "GraphicsHandler.hpp"
#include "PresetSelectionResult.hpp"

namespace Golde {
struct LoadedPreset {
    GameGrid Grid;
    std::string FileName;
    std::filesystem::path NormalizedFolder;
};

struct PresetDisplay {
    GameGrid Grid;
    std::string FileName;
    std::filesystem::path RelativeFolder;
    GraphicsHandler Graphics;
    bool WasHovered = false;

    PresetDisplay(LoadedPreset&& preset, Size2 windowSize);
};

struct PresetFolderContents {
    std::unordered_set<std::filesystem::path> ChildFolders;
    std::vector<size_t> Files;
};

class PresetSelection {
  public:
    PresetSelection(const std::filesystem::path& defaultPath,
                    Size2 initialBufferSize = {
                        300, 300}); // Dynamically resized during layout

    PresetSelectionResult Update(const EditorResult& info);

  private:
    void DrainLoadQueue();

    void ReadFiles(const std::filesystem::path& path);

    void RedrawPreset(PresetDisplay& preset, RectF windowBounds, bool hovered);

    std::string CurrentFolderName() const;

  private:
    std::filesystem::path m_DefaultPath;
    std::filesystem::path m_CurrentPath;
    Size2 m_WindowSize;

    HashLifeCache m_Cache;

    std::string m_SearchText;

    std::vector<PresetDisplay> m_Library;
    std::unordered_map<std::filesystem::path, PresetFolderContents>
        m_DirectoryContents;
    Size2F m_MaxGridDimensions;

    RectF m_LastWindowBounds;

    // For concurrently reading from files
    std::mutex m_QueueMutex;
    std::atomic<bool> m_FinishedReading;
    std::queue<LoadedPreset> m_LoadQueue;
    std::jthread m_LoadThread;
};
} // namespace Golde

#endif
