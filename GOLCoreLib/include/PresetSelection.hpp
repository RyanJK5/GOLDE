#ifndef PresetComponent_hpp_
#define PresetComponent_hpp_

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "EditorResult.hpp"
#include "GameGrid.hpp"
#include "Graphics2D.hpp"
#include "GraphicsHandler.hpp"
#include "PresetSelectionResult.hpp"

namespace Golde {
struct PresetDisplay {
    GameGrid Grid;
    std::string FileName;
    GraphicsHandler Graphics;

    PresetDisplay(const GameGrid& grid, const std::string& fileName,
                  Size2 windowSize);
};

class PresetSelection {
  public:
    PresetSelection(const std::filesystem::path& defaultPath,
                    Size2 initialBufferSize = {
                        300, 300}); // Dynamically resized during layout

    PresetSelectionResult Update(const EditorResult& info);

  private:
    void ReadFiles(const std::filesystem::path& path);

    std::filesystem::path m_DefaultPath;
    Size2 m_WindowSize;

    std::string m_SearchText;

    std::vector<PresetDisplay> m_Library;
    Size2F m_MaxGridDimensions;
};
} // namespace Golde

#endif
