#ifndef DisabledScope_hpp_
#define DisabledScope_hpp_

namespace Golde {
class DisabledScope {
  public:
    DisabledScope(bool disabled);
    ~DisabledScope();

    DisabledScope(const DisabledScope&) = delete;
    DisabledScope(DisabledScope&&) = delete;

    DisabledScope& operator=(const DisabledScope&) = delete;
    DisabledScope& operator=(DisabledScope&&) = delete;

  private:
    bool m_Disabled;
};
} // namespace Golde

#endif