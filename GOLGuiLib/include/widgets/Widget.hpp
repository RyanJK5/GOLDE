#ifndef Widget_hpp_
#define Widget_hpp_

#include <concepts>

#include "ActionButton.hpp"
#include "EditorResult.hpp"
#include "GameEnums.hpp"
#include "SimulationCommand.hpp"
#include "WidgetResult.hpp"

namespace Golde {
class Widget;

class Widget {
  public:
    static float DefaultInputPadding() { return ImGui::GetFontSize() / 2.0f; }

    WidgetResult Update(this auto&& self, const EditorResult& state) {
        return self.UpdateImpl(state);
    }

    void SetShortcuts(this auto&& self, const ShortcutMap& shortcuts) {
        return self.SetShortcutsImpl(shortcuts);
    }

  protected:
    template <ActionType ActType>
    inline static void UpdateResult(WidgetResult& result,
                                    const ActionButtonResult<ActType>& update) {
        if (!result.Command && update.Action)
            result.Command = ToCommand(*update.Action);
        if (!result.FromShortcut)
            result.FromShortcut = update.FromShortcut;
    }
};
} // namespace Golde

#endif
