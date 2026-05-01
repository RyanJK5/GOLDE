#ifndef ErrorWindow_hpp_
#define ErrorWindow_hpp_

#include <optional>
#include <string_view>

#include "PopupWindow.hpp"

namespace Golde {
class ErrorWindow : public PopupWindow {
  public:
    ErrorWindow(std::string_view title,
                std::function<void(PopupWindowState)> onUpdate)
        : PopupWindow(title, onUpdate) {}

  protected:
    virtual std::optional<PopupWindowState> ShowButtons() const override final;
};
} // namespace Golde

#endif
