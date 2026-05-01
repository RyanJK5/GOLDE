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
    PopupWindow(std::string_view title,
                std::function<void(PopupWindowState)> onUpdate);

    void Update();

    void Activate() { Active = true; }

    void SetCallback(std::function<void(PopupWindowState)> onUpdate);

    std::string Message;

  protected:
    virtual std::optional<PopupWindowState> ShowButtons() const = 0;

    ~PopupWindow() = default;

  private:
    std::function<void(PopupWindowState)> m_UpdateCallback;

    bool Active = false;
    std::string m_Title;
};
} // namespace Golde

#endif
