#include <algorithm>
#include <cctype>
#include <string>
#include <utility>
#include <variant>

#include "GameEnums.hpp"

namespace Golde {
std::string Actions::ToString(ActionVariant action) {
    std::string result{[action]() {
        if (auto* value = std::get_if<GameAction>(&action))
            return std::find_if(
                       GameActionDefinitions.begin(),
                       GameActionDefinitions.end(),
                       [value](auto&& pair) { return pair.second == *value; })
                ->first;
        if (auto* value = std::get_if<EditorAction>(&action))
            return std::find_if(
                       EditorActionDefinitions.begin(),
                       EditorActionDefinitions.end(),
                       [value](auto&& pair) { return pair.second == *value; })
                ->first;
        if (auto* value = std::get_if<SelectionAction>(&action))
            return std::find_if(
                       SelectionActionDefinitions.begin(),
                       SelectionActionDefinitions.end(),
                       [value](auto&& pair) { return pair.second == *value; })
                ->first;
        std::unreachable();
    }()};

    result[0] = static_cast<char>(std::toupper(result[0]));

    auto underscoreIndex = result.find('_');
    while (underscoreIndex != std::string::npos) {
        result[underscoreIndex] = ' ';
        underscoreIndex++;
        result[underscoreIndex] =
            static_cast<char>(std::toupper(result[underscoreIndex]));
        underscoreIndex = result.find('_', underscoreIndex);
    }

    return result;
}
} // namespace Golde
