#include "Combo.hpp"

#include <imgui.h>

namespace Golde {
void Combo::Update() {
    ImGui::Combo(m_Label.data(), &ActiveIndex, m_Data.data());
}
} // namespace Golde