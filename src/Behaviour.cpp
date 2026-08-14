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

        // A robot is a mind, not a body: it talks (Codsworth holds a
        // conversation) but has no biological needs to seed — no hunger,
        // no fatigue, no illness — so it never walks to the market and
        // never falls sick. The game's own AI moves it; the sim only
        // remembers and names it. The Aid market kind is defensive only
        // (a robot never gets a hunger drive to arrive with).
        static const BehaviourProfile kRobot{
            .MarketKind   = Aid,
            .CanTrade     = false,
            .CanTalk      = true,
            .NeedsSocial  = false,
            .NeedsComfort = false,
        };

        switch (a_species)
        {
        case Species::Animal:
            return kAnimal;
        case Species::Child:
            return kChild;
        case Species::Robot:
            return kRobot;
        case Species::Human:
            break;
        }

        return kHuman;
    }

    LCE::Simulation::Needs SeededNeeds(
        Species a_species, const NeedRates& a_rates)
    {
        using namespace LCE::Simulation;

        // A robot is born with no biological needs at all — nothing to
        // feed, nothing to tire, nothing to get ill from. The empty list
        // means Decide finds no urgent need and the sim never commands
        // it; the game's own AI runs the machine (the design note: no
        // biological needs to seed).
        if (a_species == Species::Robot)
        {
            return Needs{};
        }

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
        case Species::Robot:
            // Fed by the feeder — nothing given in return: disposition
            // warms, no trust ledger, no barter. a_traded is ignored:
            // they never trade. Defensive for a robot — it never gets
            // a hunger drive to arrive with.
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

        auto fatigue = -1.0f;   // marker: no Fatigue need (defensive)

        for (auto& need : a_needs.List)
        {
            if (need.Type == NeedType::Fatigue)
            {
                need.Value = std::min(1.0f, need.Value + a_rate * a_delta);
                fatigue = need.Value;
            }
            else if (need.Type == NeedType::Safety
                || need.Type == NeedType::Comfort)
            {
                // A nap restores the needs it fixes. Safety especially:
                // a drained Safety with no threat parks the mind (the
                // engine returns nullopt — nothing to flee) and no
                // intent-keyed pass can ever see it again. Comfort is a
                // Work decision, never a park, but a rest restores it
                // all the same. Social is deliberately excluded — that
                // is the future Socialize stone's recovery, not a nap's.
                need.Value = std::min(1.0f, need.Value + a_rate * a_delta);
            }
        }

        return fatigue;
    }

    std::optional<LCE::Simulation::NeedType> MostUrgentNeed(
        const LCE::Simulation::Needs& a_needs)
    {
        using namespace LCE::Simulation;

        const Need* best = nullptr;

        for (const auto& need : a_needs.List)
        {
            if (best == nullptr || need.Value < best->Value)
            {
                best = &need;
            }
        }

        if (best == nullptr)
        {
            return std::nullopt;
        }

        return best->Type;
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
