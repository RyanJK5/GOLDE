#ifndef __EditorWidget_h__
#define __EditorWidget_h__

#include <imgui/imgui.h>
#include <span>
#include <unordered_map>
#include <vector>

#include "ActionButton.hpp"
#include "EditorResult.hpp"
#include "GameEnums.hpp"
#include "Graphics2D.hpp"
#include "Widget.hpp"
#include "WidgetResult.hpp"

namespace gol {
class CopyButton : public ActionButton<SelectionAction, true> {
  public:
    CopyButton(std::span<const ImGuiKeyChord> shortcuts = {});

  protected:
    virtual Size2F Dimensions() const final;
    virtual std::string Label(const EditorResult &) const override final;
    virtual bool Enabled(const EditorResult &state) const final;
};

class CutButton : public ActionButton<SelectionAction, false> {
  public:
    CutButton(std::span<const ImGuiKeyChord> shortcuts = {});

  protected:
    virtual Size2F Dimensions() const final;
    virtual std::string Label(const EditorResult &) const override final;
    virtual bool Enabled(const EditorResult &state) const final;
};

class PasteButton : public ActionButton<SelectionAction, false> {
  public:
    PasteButton(std::span<const ImGuiKeyChord> shortcuts = {});

  protected:
    virtual Size2F Dimensions() const final;
    virtual std::string Label(const EditorResult &) const override final;
    virtual bool Enabled(const EditorResult &state) const final;
};

class DeleteButton : public ActionButton<SelectionAction, false> {
  public:
    DeleteButton(std::span<const ImGuiKeyChord> shortcuts = {});

  protected:
    virtual Size2F Dimensions() const final;
    virtual std::string Label(const EditorResult &) const override final;
    virtual bool Enabled(const EditorResult &state) const final;
};

class DeselectButton : public ActionButton<SelectionAction, true> {
  public:
    DeselectButton(std::span<const ImGuiKeyChord> shortcuts = {});

  protected:
    virtual Size2F Dimensions() const final;
    virtual std::string Label(const EditorResult &) const override final;
    virtual bool Enabled(const EditorResult &state) const final;
};

class RotateButton : public ActionButton<SelectionAction, false> {
  public:
    RotateButton(std::span<const ImGuiKeyChord> shortcuts = {});

  protected:
    virtual Size2F Dimensions() const final;
    virtual std::string Label(const EditorResult &) const override final;
    virtual bool Enabled(const EditorResult &state) const final;
};

class FlipHorizontalButton : public ActionButton<SelectionAction, false> {
  public:
    FlipHorizontalButton(std::span<const ImGuiKeyChord> shortcuts = {});

  protected:
    virtual Size2F Dimensions() const final;
    virtual std::string Label(const EditorResult &) const override final;
    virtual bool Enabled(const EditorResult &state) const final;
};

class FlipVerticalButton : public ActionButton<SelectionAction, false> {
  public:
    FlipVerticalButton(std::span<const ImGuiKeyChord> shortcuts = {});

  protected:
    virtual Size2F Dimensions() const final;
    virtual std::string Label(const EditorResult &) const override final;
    virtual bool Enabled(const EditorResult &state) const final;
};

class SelectAllButton : public ActionButton<SelectionAction, true> {
  public:
    SelectAllButton(std::span<const ImGuiKeyChord> shortcuts = {});

  protected:
    virtual Size2F Dimensions() const final;
    virtual std::string Label(const EditorResult &) const override final;
    virtual bool Enabled(const EditorResult &state) const final;
};

class UndoButton : public ActionButton<EditorAction, false> {
  public:
    UndoButton(std::span<const ImGuiKeyChord> shortcuts = {});

  protected:
    virtual Size2F Dimensions() const final;
    virtual std::string Label(const EditorResult &) const override final;
    virtual bool Enabled(const EditorResult &state) const final;
};

class RedoButton : public ActionButton<EditorAction, false> {
  public:
    RedoButton(std::span<const ImGuiKeyChord> shortcuts = {});

  protected:
    virtual Size2F Dimensions() const final;
    virtual std::string Label(const EditorResult &) const override final;
    virtual bool Enabled(const EditorResult &state) const final;
};

class EditorWidget : public Widget {
  public:
    EditorWidget(
        const ShortcutMap
            &shortcuts);
    friend Widget;

  public:
    WidgetResult UpdateImpl(const EditorResult &state);

    void SetShortcutsImpl(const ShortcutMap &shortcuts);
  private:
    CopyButton m_CopyButton;
    CutButton m_CutButton;
    PasteButton m_PasteButton;
    DeleteButton m_DeleteButton;

    DeselectButton m_DeselectButton;
    RotateButton m_RotateButton;
    FlipHorizontalButton m_FlipHorizontalButton;
    FlipVerticalButton m_FlipVerticalButton;
    SelectAllButton m_SelectAllButton;
    UndoButton m_UndoButton;
    RedoButton m_RedoButton;
};
} // namespace gol

#endif