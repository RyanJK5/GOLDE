#ifndef StyleLoader_hpp_
#define StyleLoader_hpp_

#include <algorithm>
#include <cctype>
#include <concepts>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <expected>
#include <filesystem>
#include <fstream>
#include <functional>
#include <imgui.h>
#include <iterator>
#include <optional>
#include <print>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "GameEnums.hpp"

namespace Golde::ConfigLoader {
template <typename T>
concept Vector4 =
    std::floating_point<decltype(T::x)> &&
    std::floating_point<decltype(T::y)> &&
    std::floating_point<decltype(T::z)> &&
    std::floating_point<decltype(T::w)> && std::default_initializable<T>;

enum class StyleColor { Transparent, Background, Contrast, Hover, Text };

enum class SectionType { StyleColors, ImGUIStyle, Shortcuts };

enum class YAMLErrorType { FileOpenError, ParseError, InvalidArguments };

struct YAMLError {
    YAMLErrorType Type;
    std::string Description;
};

class StyleLoaderException : public std::exception {
  public:
    StyleLoaderException(std::string_view error) : m_Error(error) {}

    virtual const char* what() const noexcept override final {
        return m_Error.c_str();
    }

  private:
    std::string m_Error;
};

template <Vector4 Vec>
struct StyleInfo {
    std::filesystem::path OriginPath{};
    std::unordered_map<StyleColor, Vec> StyleColors{};
    std::unordered_map<ImGuiCol_, StyleColor> AttributeColors{};
    std::unordered_map<ActionVariant, std::vector<ImGuiKeyChord>> Shortcuts{};
};

static const std::unordered_map<std::string_view, StyleColor> ColorDefinitions =
    {{"transparent", StyleColor::Transparent},
     {"background", StyleColor::Background},
     {"contrast", StyleColor::Contrast},
     {"hover", StyleColor::Hover},
     {"text", StyleColor::Text}};

static const std::unordered_map<std::string_view, ImGuiCol_>
    AttributeDefinitions = {
        {"WindowBg", ImGuiCol_WindowBg},
        {"Border", ImGuiCol_Border},
        {"Text", ImGuiCol_Text},
        {"Button", ImGuiCol_Button},
        {"ButtonHovered", ImGuiCol_ButtonHovered},
        {"Header", ImGuiCol_Header},
        {"HeaderActive", ImGuiCol_HeaderActive},
        {"TitleBg", ImGuiCol_TitleBg},
        {"TitleBgActive", ImGuiCol_TitleBgActive},
        {"Tab", ImGuiCol_Tab},
        {"TabSelectedOverline", ImGuiCol_TabSelectedOverline},
        {"TabDimmedSelected", ImGuiCol_TabDimmedSelected},
        {"TabHovered", ImGuiCol_TabHovered},
        {"TabUnfocused", ImGuiCol_TabUnfocused},
        {"TabDimmed", ImGuiCol_TabDimmed},
        {"TabSelected", ImGuiCol_TabSelected},
        {"SliderGrab", ImGuiCol_SliderGrab},
        {"FrameBg", ImGuiCol_FrameBg},
        {"CheckMark", ImGuiCol_CheckMark},
        {"FrameBgHovered", ImGuiCol_FrameBgHovered}};

static const std::unordered_map<std::string_view, SectionType>
    SectionDefinitions = {{"colors", SectionType::StyleColors},
                          {"imgui_style", SectionType::ImGUIStyle},
                          {"shortcuts", SectionType::Shortcuts}};

static const std::unordered_map<std::string_view, ImGuiKey>
    ShortcutDefinitions = {{"Ctrl", ImGuiMod_Ctrl},
                           {"Shift", ImGuiMod_Shift},
                           {"Alt", ImGuiMod_Alt},
                           {"Super", ImGuiMod_Super},

                           {"A", ImGuiKey_A},
                           {"B", ImGuiKey_B},
                           {"C", ImGuiKey_C},
                           {"D", ImGuiKey_D},
                           {"E", ImGuiKey_E},
                           {"F", ImGuiKey_F},
                           {"G", ImGuiKey_G},
                           {"H", ImGuiKey_H},
                           {"I", ImGuiKey_I},
                           {"J", ImGuiKey_J},
                           {"K", ImGuiKey_K},
                           {"L", ImGuiKey_L},
                           {"M", ImGuiKey_M},
                           {"N", ImGuiKey_N},
                           {"O", ImGuiKey_O},
                           {"P", ImGuiKey_P},
                           {"Q", ImGuiKey_Q},
                           {"R", ImGuiKey_R},
                           {"S", ImGuiKey_S},
                           {"T", ImGuiKey_T},
                           {"U", ImGuiKey_U},
                           {"V", ImGuiKey_V},
                           {"W", ImGuiKey_W},
                           {"X", ImGuiKey_X},
                           {"Y", ImGuiKey_Y},
                           {"Z", ImGuiKey_Z},

                           {"0", ImGuiKey_0},
                           {"1", ImGuiKey_1},
                           {"2", ImGuiKey_2},
                           {"3", ImGuiKey_3},
                           {"4", ImGuiKey_4},
                           {"5", ImGuiKey_5},
                           {"6", ImGuiKey_6},
                           {"7", ImGuiKey_7},
                           {"8", ImGuiKey_8},
                           {"9", ImGuiKey_9},

                           {"F1", ImGuiKey_F1},
                           {"F2", ImGuiKey_F2},
                           {"F3", ImGuiKey_F3},
                           {"F4", ImGuiKey_F4},
                           {"F5", ImGuiKey_F5},
                           {"F6", ImGuiKey_F6},
                           {"F7", ImGuiKey_F7},
                           {"F8", ImGuiKey_F8},
                           {"F9", ImGuiKey_F9},
                           {"F10", ImGuiKey_F10},
                           {"F11", ImGuiKey_F11},
                           {"F12", ImGuiKey_F12},

                           {"Enter", ImGuiKey_Enter},
                           {"Space", ImGuiKey_Space},
                           {"Esc", ImGuiKey_Escape},
                           {"Tab", ImGuiKey_Tab},
                           {"Backspace", ImGuiKey_Backspace},
                           {"Delete", ImGuiKey_Delete},
                           {"Insert", ImGuiKey_Insert},

                           {"Left", ImGuiKey_LeftArrow},
                           {"Right", ImGuiKey_RightArrow},
                           {"Up", ImGuiKey_UpArrow},
                           {"Down", ImGuiKey_DownArrow},
                           {"Home", ImGuiKey_Home},
                           {"End", ImGuiKey_End},
                           {"PageUp", ImGuiKey_PageUp},
                           {"PageDown", ImGuiKey_PageDown},

                           {"Keypad0", ImGuiKey_Keypad0},
                           {"Keypad1", ImGuiKey_Keypad1},
                           {"Keypad2", ImGuiKey_Keypad2},
                           {"Keypad3", ImGuiKey_Keypad3},
                           {"Keypad4", ImGuiKey_Keypad4},
                           {"Keypad5", ImGuiKey_Keypad5},
                           {"Keypad6", ImGuiKey_Keypad6},
                           {"Keypad7", ImGuiKey_Keypad7},
                           {"Keypad8", ImGuiKey_Keypad8},
                           {"Keypad9", ImGuiKey_Keypad9},

                           {"KeypadDecimal", ImGuiKey_KeypadDecimal},
                           {"KeypadDivide", ImGuiKey_KeypadDivide},
                           {"KeypadMultiply", ImGuiKey_KeypadMultiply},
                           {"KeypadSubtract", ImGuiKey_KeypadSubtract},
                           {"KeypadAdd", ImGuiKey_KeypadAdd},
                           {"KeypadEnter", ImGuiKey_KeypadEnter},
                           {"KeypadEqual", ImGuiKey_KeypadEqual},

                           {"'", ImGuiKey_Apostrophe},
                           {",", ImGuiKey_Comma},
                           {"-", ImGuiKey_Minus},
                           {".", ImGuiKey_Period},
                           {"/", ImGuiKey_Slash},
                           {";", ImGuiKey_Semicolon},
                           {"=", ImGuiKey_Equal},
                           {"LeftBracket", ImGuiKey_LeftBracket},
                           {"\\", ImGuiKey_Backslash},
                           {"RightBracket", ImGuiKey_RightBracket},
                           {"`", ImGuiKey_GraveAccent},

                           {"CapsLock", ImGuiKey_CapsLock},
                           {"ScrollLock", ImGuiKey_ScrollLock},
                           {"NumLock", ImGuiKey_NumLock},
                           {"PrintScreen", ImGuiKey_PrintScreen},
                           {"Pause", ImGuiKey_Pause},

                           {"Menu", ImGuiKey_Menu}};

template <typename T>
using StringConverter = std::function<std::optional<T>(std::string_view)>;

template <typename T>
static StringConverter<T>
MakeConverter(const std::unordered_map<std::string_view, T>& map) {
    return StringConverter<T>{[map](std::string_view str) {
        if (map.count(str) == 0)
            return std::optional<T>(std::nullopt);
        return std::make_optional<T>(map.at(str));
    }};
}

template <typename Chord, typename KeyStroke>
concept KeyChord = requires(Chord a, KeyStroke b) {
    a | b;
    a |= b;
};

template <typename KeyStroke, KeyChord<KeyStroke> Chord>
static StringConverter<Chord> MakeChordConverter(
    const std::unordered_map<std::string_view, KeyStroke>& keyMap) {
    return StringConverter<Chord>{[keyMap](std::string_view str) {
        std::optional<Chord> chord;
        std::string token = "";
        for (char c : str) {
            if (c == '+') {
                if (keyMap.count(token) == 0)
                    return std::optional<Chord>(std::nullopt);

                chord = !chord ? keyMap.at(token) : (*chord | keyMap.at(token));
                token = "";
                continue;
            }
            token += c;
        }
        if (keyMap.count(token) == 0)
            return std::optional<Chord>(std::nullopt);
        chord = !chord ? keyMap.at(token) : (*chord | keyMap.at(token));
        return chord;
    }};
}

template <typename T>
static std::expected<T, YAMLError>
ReadKey(int lineNum, const std::string& line,
        const StringConverter<T>& conversion) {
    auto firstLetter = std::find_if(line.begin(), line.end(),
                                    [](char c) { return std::isalpha(c); });
    auto seperator = std::find(firstLetter, line.end(), ':');

    if (seperator == line.end()) {
        return std::unexpected(
            YAMLError{YAMLErrorType::ParseError,
                      std::format("Could not find ':' in line {}:\n    {}",
                                  lineNum, line)});
    }

    auto keyStr = std::string(firstLetter, seperator);
    auto key = conversion(keyStr);
    if (!key) {
        return std::unexpected(
            YAMLError{YAMLErrorType::ParseError,
                      std::format("'{}' is not a valid key in line {}:\n    {}",
                                  keyStr, lineNum, line)});
    }
    return *key;
}

template <typename T>
static std::expected<std::vector<T>, YAMLError>
ReadList(int lineNum, const std::string& line, std::string_view values,
         const StringConverter<T>& conversion, int numElements = -1) {
    std::vector<T> result = {};
    if (numElements >= 0)
        result.reserve(numElements);

    std::string token = "";
    int index = 0;
    for (char c : values) {
        if (c == '[' || std::isspace(c))
            continue;
        if ((c == ',' || c == ']') && token.length() > 0) {
            auto converted = conversion(token);
            if (!converted) {
                return std::unexpected(YAMLError{
                    YAMLErrorType::InvalidArguments,
                    std::format("{} is not a valid value in line {} o:\n    {}",
                                token, lineNum, line)});
            }
            result.push_back(*converted);
            index++;
            token = "";
            continue;
        }
        token += c;
    }

    if (numElements >= 0 && index != numElements) {
        return std::unexpected(YAMLError{
            YAMLErrorType::InvalidArguments,
            std::format(
                "Expected {} parameters, received {} in line {}:\n    {}",
                numElements, index, lineNum, line)});
    }

    return result;
}

template <Vector4 Vec>
static Vec CreateVector4(const std::vector<float>& elements) {
    auto result = Vec{};
    for (size_t i = 0; i < elements.size(); i++) {
        switch (i) {
        case 0:
            result.x = elements[i];
            break;
        case 1:
            result.y = elements[i];
            break;
        case 2:
            result.z = elements[i];
            break;
        case 3:
            result.w = elements[i];
            break;
        }
    }
    return result;
}

template <typename Key, typename Value>
static std::expected<std::pair<Key, std::vector<Value>>, YAMLError>
ReadListPair(int lineNum, const std::string& line,
             const std::string::const_iterator& firstLetter,
             const StringConverter<Key>& keyConversion,
             const StringConverter<Value>& valueConversion,
             int numElements = -1) {
    auto seperator = std::find(firstLetter, line.end(), ':');

    auto key = ReadKey<Key>(lineNum, line, keyConversion);
    if (!key)
        return std::unexpected(key.error());

    auto values = std::string_view(seperator + 1, line.end());
    auto valueList =
        ReadList<Value>(lineNum, line, values, valueConversion, numElements);
    if (!valueList)
        return std::unexpected(valueList.error());

    return std::pair<Key, std::vector<Value>>{*key, *valueList};
}

template <Vector4 Vec>
static std::expected<std::pair<StyleColor, Vec>, YAMLError>
ReadColorPair(int lineNum, const std::string& line,
              const std::string::const_iterator& firstLetter) {
    StringConverter<float> toF = [](auto str) {
        return std::make_optional<float>(std::strtof(str.data(), nullptr));
    };
    StringConverter<StyleColor> toC = MakeConverter(ColorDefinitions);
    auto pair = ReadListPair<StyleColor, float>(lineNum, line, firstLetter, toC,
                                                toF, 4);
    if (!pair)
        return std::unexpected(pair.error());
    return std::pair<StyleColor, Vec>{pair->first,
                                      CreateVector4<Vec>(pair->second)};
}

template <typename Key, typename Value>
static std::expected<std::pair<Key, Value>, YAMLError>
ReadPair(int lineNum, const std::string& line,
         const std::string::const_iterator& firstLetter,
         const StringConverter<Key>& keyConverter,
         const StringConverter<Value>& valueConverter) {
    auto seperator = std::find(firstLetter, line.end(), ':');

    auto key = ReadKey<Key>(lineNum, line, keyConverter);
    if (!key)
        return std::unexpected(key.error());

    std::string valueStr = "";
    for (auto it = seperator + 1; it != line.end(); it++) {
        if (!std::isspace(*it))
            valueStr += *it;
    }

    auto value = valueConverter(valueStr);
    if (!value) {
        return std::unexpected(YAMLError{
            YAMLErrorType::ParseError,
            std::format("'{}' is not a valid color in line {}:\n    {}",
                        valueStr, lineNum, line)});
    }

    return std::pair<Key, Value>{*key, *value};
}

template <Vector4 Vec>
std::expected<StyleInfo<Vec>, YAMLError>
TryLoadYAML(const std::filesystem::path& styleInfoPath) {
    std::ifstream input(styleInfoPath);
    if (!input.is_open()) {
        return std::unexpected(
            YAMLError{YAMLErrorType::FileOpenError,
                      std::format("Could not open file '{}'",
                                  styleInfoPath.generic_string())});
    }

    std::string line = "";
    std::ptrdiff_t indentWidth = 0;
    int32_t lineNum = 0;

    std::optional<SectionType> section{};
    StyleInfo<Vec> output{.OriginPath = styleInfoPath};

    const auto zeroPossibleEntries =
        [&](const std::ranges::input_range auto& stringMap) {
            for (auto&& [key, value] : stringMap) {
                output.Shortcuts[value] = {};
            }
        };
    zeroPossibleEntries(Actions::GameActionDefinitions);
    zeroPossibleEntries(Actions::EditorActionDefinitions);
    zeroPossibleEntries(Actions::SelectionActionDefinitions);

    while (std::getline(input, line)) {
        lineNum++;

        auto start =
            std::ranges::find_if(line, [](char c) { return std::isalpha(c); });
        if (start == line.end())
            continue;
        if (std::ranges::find(line, '#') < start)
            continue;

        if (indentWidth == 0)
            indentWidth = std::distance(line.begin(), start);
        auto depth = indentWidth != 0
                         ? (std::distance(line.begin(), start) / indentWidth)
                         : 0;

        if (depth == 0)
            section = std::nullopt;

        if (section) {
            switch (*section) {
                using enum SectionType;
            case StyleColors: {
                auto result = ReadColorPair<Vec>(lineNum, line, start);
                if (!result)
                    break;
                output.StyleColors[result->first] = result->second;
            } break;
            case ImGUIStyle: {
                auto result = ReadPair<ImGuiCol_, StyleColor>(
                    lineNum, line, start, MakeConverter(AttributeDefinitions),
                    MakeConverter(ColorDefinitions));
                if (!result)
                    break;
                output.AttributeColors[result->first] = result->second;
            } break;
            case Shortcuts: {
                {
                    auto result = ReadListPair<GameAction, ImGuiKeyChord>(
                        lineNum, line, start,
                        MakeConverter(Actions::GameActionDefinitions),
                        MakeChordConverter<ImGuiKey, ImGuiKeyChord>(
                            ShortcutDefinitions));
                    if (result) {
                        output.Shortcuts[result->first] = result->second;
                        break;
                    }
                }
                {
                    auto result = ReadListPair<EditorAction, ImGuiKeyChord>(
                        lineNum, line, start,
                        MakeConverter(Actions::EditorActionDefinitions),
                        MakeChordConverter<ImGuiKey, ImGuiKeyChord>(
                            ShortcutDefinitions));
                    if (result) {
                        output.Shortcuts[result->first] = result->second;
                        break;
                    }
                }
                {
                    auto result = ReadListPair<SelectionAction, ImGuiKeyChord>(
                        lineNum, line, start,
                        MakeConverter(Actions::SelectionActionDefinitions),
                        MakeChordConverter<ImGuiKey, ImGuiKeyChord>(
                            ShortcutDefinitions));
                    if (result) {
                        output.Shortcuts[result->first] = result->second;
                        break;
                    }
                }
            }
            }
        }

        auto end = std::find_if(line.rbegin(), line.rend(),
                                [](char c) { return !std::isspace(c); });
        if (*end == ':') {
            auto sectionHeader = std::string_view(start, --end.base());
            if (SectionDefinitions.count(sectionHeader) == 0) {
                return std::unexpected(YAMLError{
                    YAMLErrorType::InvalidArguments,
                    std::format(
                        "'{}' is not a valid section type in line {}:\n    {}",
                        sectionHeader, lineNum, line)});
            }

            section = SectionDefinitions.at(sectionHeader);
        }
    }

    return output;
}

template <Vector4 Vec>
StyleInfo<Vec> LoadYAML(const std::filesystem::path& styleInfoPath) {
    auto result = TryLoadYAML<Vec>(styleInfoPath);
    if (!result)
        throw StyleLoaderException{result.error().Description};
    return *result;
}
} // namespace Golde::ConfigLoader

#endif
