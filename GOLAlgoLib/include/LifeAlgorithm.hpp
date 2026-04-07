#ifndef LifeAlgorithm_hpp_
#define LifeAlgorithm_hpp_

#include "BigInt.hpp"
#include "LifeDataStructure.hpp"

#include <cassert>
#include <functional>
#include <string_view>

namespace gol {
template <typename Derived>
struct AlgorithmRegistrator {
  private:
    static bool s_Registered;

  public:
    AlgorithmRegistrator();
};

class LifeAlgorithm {
  public:
    static std::unique_ptr<LifeAlgorithm>
    MakeAlgorithm(std::string_view identifier);

    virtual ~LifeAlgorithm() = default;

    virtual BigInt Step(LifeDataStructure& data, const BigInt& numSteps,
                        std::stop_token stopToken = {}) = 0;

    std::string_view GetIdentifier() const;

    template <typename Derived>
    friend struct AlgorithmRegistrator;

  protected:
    template <typename Derived>
    LifeAlgorithm(Derived*) : m_Identifier(Derived::Identifier) {}

  private:
    template <typename Func>
    static void Register(std::string_view identifier, Func&& function) {
        s_Constructors[identifier] = Func{std::forward<Func>(function)};
    }

  private:
    std::string_view m_Identifier;

    static std::unordered_map<std::string_view,
                              std::function<std::unique_ptr<LifeAlgorithm>()>>
        s_Constructors;
};

template <typename Derived>
bool AlgorithmRegistrator<Derived>::s_Registered = [] {
    LifeAlgorithm::Register(Derived::Identifier,
                            [] { return std::make_unique<Derived>(); });
}();

template <typename Derived>
AlgorithmRegistrator<Derived>::AlgorithmRegistrator() {
    assert(s_Registered == true);
}
} // namespace gol

#endif