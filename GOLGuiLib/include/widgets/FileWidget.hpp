#ifndef FileWidget_hpp_
#define FileWidget_hpp_

#include <filesystem>
#include <imgui.h>
#include <span>
#include <string>

#include "ActionButton.hpp"
#include "EditorResult.hpp"
#include "ErrorWindow.hpp"
#include "GameEnums.hpp"
#include "Graphics2D.hpp"
#include "Widget.hpp"
#include "WidgetResult.hpp"

namespace Golde {
class NewFileButton : public ActionButton<EditorAction, true> {
  public:
    NewFileButton(std::span<const ImGuiKeyChord> shortcuts);

  protected:
    virtual Size2F Dimensions() const override final;
    virtual std::string Label(const EditorResult& state) const override final;
    virtual bool Enabled(const EditorResult& state) const override final;
};

class UpdateFileButton : public ActionButton<EditorAction, false> {
  public:
    UpdateFileButton(std::span<const ImGuiKeyChord> shortcuts);

  protected:
    virtual Size2F Dimensions() const override final;
    virtual std::string Label(const EditorResult& state) const override final;
    virtual bool Enabled(const EditorResult& state) const override final;
};

class SaveButton : public ActionButton<EditorAction, false> {
  public:
    SaveButton(std::span<const ImGuiKeyChord> shortcuts);

  protected:
    virtual Size2F Dimensions() const override final;
    virtual std::string Label(const EditorResult& state) const override final;
    virtual bool Enabled(const EditorResult& state) const override final;
};

class LoadButton : public ActionButton<EditorAction, false> {
  public:
    LoadButton(std::span<const ImGuiKeyChord> shortcuts);

  protected:
    virtual Size2F Dimensions() const override final;
    virtual std::string Label(const EditorResult& state) const override final;
    virtual bool Enabled(const EditorResult& state) const override final;
};

class FileWidget : public Widget {
  public:
    FileWidget(const ShortcutMap& shortcutInfo);
    friend Widget;

  private:
    WidgetResult UpdateImpl(const EditorResult& state);
    void SetShortcutsImpl(const ShortcutMap& shortcutInfo);

  private:
    NewFileButton m_NewFileButton;
    UpdateFileButton m_UpdateFileButton;
    SaveButton m_SaveButton;
    LoadButton m_LoadButton;

    ErrorWindow m_FileNotOpened;
};
} // namespace Golde

#endif
