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
        LCE::Simulation::EntityId a_feeder)
    {
        using namespace LCE::Simulation;

        switch (a_species)
        {
        case Species::Human:
            // Arrived at the market, but no trade happened yet — the
            // honest result per the outcome contract (got there, didn't
            // trade = Partial). The actual trade is the next stone's work.
            return Outcome{
                a_feeder, InteractionKind::Trade, OutcomeResult::Partial, 1.0f };

        case Species::Child:
        case Species::Animal:
            // Fed by the feeder — nothing given in return: disposition
            // warms, no trust ledger, no barter.
            return Outcome{
                a_feeder, InteractionKind::Aid, OutcomeResult::Success, 1.0f };
        }

        return Outcome{
            a_feeder, InteractionKind::Trade, OutcomeResult::Partial, 1.0f };
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
