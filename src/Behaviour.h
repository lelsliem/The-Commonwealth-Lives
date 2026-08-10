//=============================================================================//
//                                                                             //
//   The Living Commonwealth — Fallout 4 adapter for the Living Commonwealth   //
//   Engine (LCE).                                                             //
//                                                                             //
//   A dog knows where the food is; it just doesn't call it trade.              //
//                                                                             //
//=============================================================================//

#pragma once

#include "LCE/Simulation/Goals.h"
#include "LCE/Simulation/Memory.h"   // InteractionKind
#include "LCE/Simulation/Needs.h"
#include "LCE/Simulation/Outcome.h"

namespace TLC
{
    //-------------------------------------------------------------------------
    // Species — what kind of mind this is. The core is deliberately
    // species-agnostic: Decide reasons over needs and memory, never game
    // facts. The behavior split is the adapter's, because "who can trade"
    // is game knowledge (ADR-0024: game knowledge at the edge). A junkyard
    // dog is a mind like a settler — it just cannot buy, sell, or talk;
    // a child can talk but does not run a stall.
    //-------------------------------------------------------------------------
    enum class Species
    {
        Human,   // adult settlers — trade, talk, work
        Child,   // can talk, cannot trade — fed by the settlement
        Animal   // cannot trade, buy, or talk — fed by the settlement
    };

    //-------------------------------------------------------------------------
    // BehaviourProfile — what a species may do and how its mind is seeded.
    //
    // The 0.5.0 arrival leg consults this: a human's market interaction is
    // Trade with a trader; an animal's is Aid with the settlement — fed,
    // never bartering. CanTrade / CanTalk gate the execution side; the
    // needs flags decide which drives a mind is born with (no Social need,
    // no Socialize intent — an animal never wanders off to talk).
    //
    // Deliberately free of game types: the whole table is testable.
    //-------------------------------------------------------------------------
    struct BehaviourProfile
    {
        LCE::Simulation::InteractionKind MarketKind;
        bool CanTrade;
        bool CanTalk;
        bool NeedsSocial;
        bool NeedsComfort;
    };

    //-------------------------------------------------------------------------
    // The profile for a species. Unknown species fall back to Human — a
    // misclassified mind only behaves human until the table grows.
    //-------------------------------------------------------------------------
    [[nodiscard]]
    const BehaviourProfile& BehaviourFor(Species a_species);

    //-------------------------------------------------------------------------
    // A fresh mind of a given species: every seeded need satisfied, decay
    // rates at the seed defaults. Hunger, Fatigue, and Safety are
    // universal; Social and Comfort belong to species that can use them.
    //-------------------------------------------------------------------------
    [[nodiscard]]
    LCE::Simulation::Needs SeededNeeds(Species a_species);

    //-------------------------------------------------------------------------
    // The arrival outcome (0.5.0): what reaching the food source means,
    // per species. A human arrived at the market but traded nothing yet —
    // Partial (the actual trade is the next stone's work). A child or an
    // animal is fed — Aid, Success: fed, gives nothing in return. The
    // feeder is whoever resolved as the food source (the owner, or the
    // settlement).
    //-------------------------------------------------------------------------
    [[nodiscard]]
    LCE::Simulation::Outcome ArrivalOutcome(
        Species a_species,
        LCE::Simulation::EntityId a_feeder);

    //-------------------------------------------------------------------------
    // The hunger loop's payoff (0.5.0, the real test): arriving at the
    // market means food — the settlement's stores feed arrivals. Restores
    // the Hunger need to full and returns its previous value (or -1 when
    // the mind has no Hunger need — a defensive marker, all seeded minds
    // have one). Game fact at the edge: the dog ate; the sim just records
    // it.
    //-------------------------------------------------------------------------
    [[nodiscard]]
    float RestoreHunger(LCE::Simulation::Needs& a_needs);

    //-------------------------------------------------------------------------
    // The ambition a fresh mind of a species is born with. Humans carry
    // the AcquireFood ambition (served when trading lands; Partial halves
    // it per arrival). Children and animals carry none yet — their loop
    // closes on the feed alone, and the core's Aid kind does not serve
    // AcquireFood (the Feed-kind engine ask).
    //-------------------------------------------------------------------------
    [[nodiscard]]
    LCE::Simulation::Goals SeededGoals(Species a_species);
}
