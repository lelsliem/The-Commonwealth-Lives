//=============================================================================//
//                                                                             //
//   The Living Commonwealth — Fallout 4 adapter for the Living Commonwealth   //
//   Engine (LCE).                                                             //
//                                                                             //
//   Same hunger, different manners.                                            //
//                                                                             //
//=============================================================================//

#include "Behaviour.h"

namespace TLC
{
    const BehaviourProfile& BehaviourFor(Species a_species)
    {
        using enum LCE::Simulation::InteractionKind;

        static const BehaviourProfile kHuman{
            .MarketKind   = Trade,
            .CanTrade     = true,
            .CanTalk      = true,
            .NeedsSocial  = true,
            .NeedsComfort = true,
        };

        static const BehaviourProfile kChild{
            .MarketKind   = Aid,
            .CanTrade     = false,
            .CanTalk      = true,
            .NeedsSocial  = true,
            .NeedsComfort = true,
        };

        static const BehaviourProfile kAnimal{
            .MarketKind   = Aid,
            .CanTrade     = false,
            .CanTalk      = false,
            .NeedsSocial  = false,
            .NeedsComfort = false,
        };

        switch (a_species)
        {
        case Species::Animal:
            return kAnimal;
        case Species::Child:
            return kChild;
        case Species::Human:
            break;
        }

        return kHuman;
    }

    LCE::Simulation::Needs SeededNeeds(
        Species a_species, const NeedRates& a_rates)
    {
        using namespace LCE::Simulation;

        Needs needs;
        needs.List.push_back(
            Need{ NeedType::Hunger, 1.0f, a_rates.Hunger });
        needs.List.push_back(
            Need{ NeedType::Fatigue, 1.0f, a_rates.Fatigue });
        needs.List.push_back(
            Need{ NeedType::Safety, 1.0f, a_rates.Safety });

        const auto& profile = BehaviourFor(a_species);

        if (profile.NeedsSocial)
        {
            needs.List.push_back(
                Need{ NeedType::Social, 1.0f, a_rates.Social });
        }

        if (profile.NeedsComfort)
        {
            needs.List.push_back(
                Need{ NeedType::Comfort, 1.0f, a_rates.Comfort });
        }

        return needs;
    }

    LCE::Simulation::Outcome ArrivalOutcome(
        Species a_species,
        LCE::Simulation::EntityId a_other,
        bool a_traded)
    {
        using namespace LCE::Simulation;

        switch (a_species)
        {
        case Species::Human:
            // A real trade — the exchange landed: the core serves
            // AcquireFood and earns trust in the trader. Without one (the
            // stall-keeper setting up, no customers yet), the honest
            // result is Partial — got there, nothing changed hands.
            return Outcome{
                a_other,
                InteractionKind::Trade,
                a_traded ? OutcomeResult::Success : OutcomeResult::Partial,
                1.0f };

        case Species::Child:
        case Species::Animal:
            // Fed by the feeder — nothing given in return: disposition
            // warms, no trust ledger, no barter. a_traded is ignored:
            // they never trade.
            return Outcome{
                a_other, InteractionKind::Aid, OutcomeResult::Success, 1.0f };
        }

        return Outcome{
            a_other,
            InteractionKind::Trade,
            a_traded ? OutcomeResult::Success : OutcomeResult::Partial,
            1.0f };
    }

    float RestoreHunger(LCE::Simulation::Needs& a_needs)
    {
        using namespace LCE::Simulation;

        for (auto& need : a_needs.List)
        {
            if (need.Type == NeedType::Hunger)
            {
                const auto previous = need.Value;
                need.Value = 1.0f;
                return previous;
            }
        }

        return -1.0f;   // no Hunger need — defensive, all seeds have one
    }

    float RestRecovery(
        LCE::Simulation::Needs& a_needs,
        float a_rate,
        float a_delta)
    {
        using namespace LCE::Simulation;

        for (auto& need : a_needs.List)
        {
            if (need.Type == NeedType::Fatigue)
            {
                need.Value = std::min(1.0f, need.Value + a_rate * a_delta);
                return need.Value;
            }
        }

        return -1.0f;   // no Fatigue need — defensive, all seeds have one
    }

    LCE::Simulation::Goals SeededGoals(Species a_species)
    {
        using namespace LCE::Simulation;

        if (a_species == Species::Human)
        {
            return Goals{ Goal{ GoalType::AcquireFood, 0.0f } };
        }

        return Goals{};   // no ambition yet — fed by the settlement
    }
}
