#ifndef WarnWindow_hpp_
#define WarnWindow_hpp_

#include <optional>
#include <string>
#include <string_view>

#include "PopupWindow.hpp"

namespace Golde {
class WarnWindow : public PopupWindow {
  protected:
    virtual std::optional<PopupWindowState> ShowButtons() const override final;
};
} // namespace Golde

#endif
