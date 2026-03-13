#ifndef LoadingSpinner_h_
#define LoadingSpinner_h_

#include <imgui.h>

namespace gol {
void LoadingSpinner(const char *label, float radius, float thickness,
                    ImU32 color);
}

#endif
