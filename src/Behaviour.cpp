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

    LCE::Simulation::Needs SeededNeeds(Species a_species)
    {
        using namespace LCE::Simulation;

        Needs needs;
        needs.List.push_back(Need{ NeedType::Hunger, 1.0f, 0.1f });
        needs.List.push_back(Need{ NeedType::Fatigue, 1.0f, 0.1f });
        needs.List.push_back(Need{ NeedType::Safety, 1.0f, 0.1f });

        const auto& profile = BehaviourFor(a_species);

        if (profile.NeedsSocial)
        {
            needs.List.push_back(Need{ NeedType::Social, 1.0f, 0.1f });
        }

        if (profile.NeedsComfort)
        {
            needs.List.push_back(Need{ NeedType::Comfort, 1.0f, 0.1f });
        }

        return needs;
    }
}
