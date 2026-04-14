#include "Combo.hpp"

#include <imgui.h>

namespace gol {
void Combo::Update() {
    ImGui::Combo(m_Label.data(), &ActiveIndex, m_Data.data());
}
} // namespace gol