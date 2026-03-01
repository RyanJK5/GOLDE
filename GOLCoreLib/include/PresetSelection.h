#define PresetComponent_h_
#define PresetComponent_h_

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "EditorResult.h"
#include "GameGrid.h"
#include "Graphics2D.h"
#include "GraphicsHandler.h"
#include "InputString.h"
#include "PresetSelectionResult.h"

namespace gol {
struct PresetDisplay {
    GameGrid Grid;
    std::string FileName;
    GraphicsHandler Graphics;

    PresetDisplay(const GameGrid &grid, const std::string &fileName,
                  Size2 windowSize);
};

class PresetSelection {
  public:
    static constexpr Size2 TemplateDimensions = {300, 300};

  public:
    PresetSelection(const std::filesystem::path &defaultPath,
                    Size2 templateSize = TemplateDimensions);

    PresetSelectionResult Update(const EditorResult &info);

  private:
    void ReadFiles(const std::filesystem::path &path);

    std::filesystem::path m_DefaultPath;
    Size2 m_WindowSize;

    InputString m_SearchText;
    size_t m_MaxFileName = 0;

    std::vector<PresetDisplay> m_Library;
    Size2F m_MaxGridDimensions;
};
} // namespace gol

#endif