#ifndef PopupWindow_hpp_
#define PopupWindow_hpp_

#include <functional>
#include <optional>
#include <string>
#include <string_view>

namespace Golde {
enum class PopupWindowState { Success, Failure };

class PopupWindow {
  public:
    void Update();

    void Activate(std::string_view title, std::string_view message);

    void SetCallback(std::function<void(PopupWindowState)> onUpdate);

    std::string Message{};
    std::string Title{};

  protected:
    virtual std::optional<PopupWindowState> ShowButtons() const = 0;

    ~PopupWindow() = default;

  private:
    std::function<void(PopupWindowState)> m_UpdateCallback = [](auto) {};

    bool Active = false;
};
} // namespace Golde

#endif
