#include "LifeAlgorithm.hpp"

namespace gol {
std::unordered_map<std::string_view,
                   std::function<std::unique_ptr<LifeAlgorithm>()>>
    LifeAlgorithm::s_Constructors{};

std::unique_ptr<LifeAlgorithm>
LifeAlgorithm::MakeAlgorithm(std::string_view identifier) {
    if (!s_Constructors.contains(identifier)) {
        return nullptr;
    }

    return s_Constructors.at(identifier)();
}

std::string_view LifeAlgorithm::GetIdentifier() const { return m_Identifier; }
} // namespace gol