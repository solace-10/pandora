#pragma once

#include "state.hpp"

#include <array>
#include <memory>
#include <vector>

#include <magic_enum.hpp>

namespace WingsOfSteel
{

template <typename TStateEnum, typename TContext>
class StateMachine
{
public:
    using StatePtr = std::unique_ptr<State<TStateEnum, TContext>>;

    StateMachine() = default;
    ~StateMachine() = default;

    void AddState(TStateEnum id, StatePtr pState)
    {
        m_States[static_cast<size_t>(id)] = std::move(pState);
    }

    void AddTransition(TStateEnum from, TStateEnum to)
    {
        m_Transitions[static_cast<size_t>(from)].push_back(to);
    }

    void AddTransitions(TStateEnum from, std::initializer_list<TStateEnum> toStates)
    {
        for (const auto& to : toStates)
        {
            m_Transitions[static_cast<size_t>(from)].push_back(to);
        }
    }

    void SetInitialState(TStateEnum state)
    {
        const size_t index = static_cast<size_t>(state);
        if (m_States[index] != nullptr)
        {
            m_CurrentState = state;
            m_pCurrentStatePtr = m_States[index].get();
        }
    }

    // Returns true if the transition was successful, false if invalid.
    bool TransitionTo(TStateEnum state, TContext& context)
    {
        const size_t currentIndex = static_cast<size_t>(m_CurrentState);
        const size_t targetIndex = static_cast<size_t>(state);

        // Check if this is a valid transition
        const auto& validTransitions = m_Transitions[currentIndex];
        if (!validTransitions.empty())
        {
            bool isValidTransition = false;
            for (const auto& validState : validTransitions)
            {
                if (validState == state)
                {
                    isValidTransition = true;
                    break;
                }
            }

            if (!isValidTransition)
            {
                return false;
            }
        }
        else if (m_CurrentState != state)
        {
            // No transitions defined from current state, and we're not transitioning to self
            return false;
        }

        // Check if target state exists
        if (m_States[targetIndex] == nullptr)
        {
            return false;
        }

        // Exit current state
        if (m_pCurrentStatePtr != nullptr)
        {
            m_pCurrentStatePtr->OnExit(context);
        }

        // Transition to new state
        m_CurrentState = state;
        m_pCurrentStatePtr = m_States[targetIndex].get();

        // Enter new state
        if (m_pCurrentStatePtr != nullptr)
        {
            m_pCurrentStatePtr->OnEnter(context);
        }

        return true;
    }

    void Update(float delta, TContext& context)
    {
        if (m_pCurrentStatePtr != nullptr)
        {
            m_pCurrentStatePtr->Update(delta, context);
        }
    }

    TStateEnum GetCurrentState() const
    {
        return m_CurrentState;
    }

    const char* GetCurrentStateName() const
    {
        return magic_enum::enum_name(m_CurrentState).data();
    }

private:
    static constexpr size_t StateCount = magic_enum::enum_count<TStateEnum>();

    std::array<StatePtr, StateCount> m_States;
    std::array<std::vector<TStateEnum>, StateCount> m_Transitions;
    TStateEnum m_CurrentState{};
    State<TStateEnum, TContext>* m_pCurrentStatePtr = nullptr;
};

} // namespace WingsOfSteel
