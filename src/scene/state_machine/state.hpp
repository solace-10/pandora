#pragma once

namespace WingsOfSteel
{

template <typename TStateEnum, typename TContext>
class State
{
public:
    virtual ~State() = default;

    virtual void OnEnter(TContext& context) {}
    virtual void Update(float delta, TContext& context) {}
    virtual void OnExit(TContext& context) {}
    virtual TStateEnum GetStateID() const = 0;
};

} // namespace WingsOfSteel
