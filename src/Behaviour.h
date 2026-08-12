//=============================================================================//
//                                                                             //
//   The Living Commonwealth — Fallout 4 adapter for the Living Commonwealth   //
//   Engine (LCE).                                                             //
//                                                                             //
//   A dog knows where the food is; it just doesn't call it trade.              //
//                                                                             //
//=============================================================================//

#pragma once

#include "LCE/Simulation/Mind/Goals.h"
#include "LCE/Simulation/Mind/Memory.h"   // InteractionKind, MemoryEvent
#include "LCE/Simulation/Mind/Needs.h"
#include "LCE/Simulation/Decision/Outcome.h"
#include "LCE/Simulation/Mind/Relationships.h"
#include "LCE/Simulation/Society/Traits.h"   // JitteredTraits — the temperament

#include <algorithm>
#include <cstdint>
#include <optional>

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
    // NeedRates — the seeded decay rates, per need (the tuning stone).
    // The core decays need.Value -= need.DecayRate * dt, so these set the
    // rhythm of the sim: 0.1/s empties a need in ~10 s; "a few meals a
    // day" is roughly 0.001–0.005/s (the math is in Tuning.md). The
    // defaults are the pre-tuning stone's constants; the INI overrides
    // them (sim.hunger.decay = ...). They apply to every species' mind;
    // the species profile still decides which needs exist at all.
    //-------------------------------------------------------------------------
    struct NeedRates
    {
        float Hunger = 0.1f;
        float Fatigue = 0.1f;
        float Safety = 0.1f;
        float Social = 0.1f;
        float Comfort = 0.1f;
    };

    //-------------------------------------------------------------------------
    // A fresh mind of a given species: every seeded need satisfied, decay
    // rates from a_rates (defaults unless the tuning file says otherwise).
    // Hunger, Fatigue, and Safety are universal; Social and Comfort
    // belong to species that can use them.
    //-------------------------------------------------------------------------
    [[nodiscard]]
    LCE::Simulation::Needs SeededNeeds(
        Species a_species, const NeedRates& a_rates = {});

    //-------------------------------------------------------------------------
    // The deterministic per-mind jitter behind VaryNeeds: a value in
    // [-a_span, +a_span) derived from the entity id (FNV-1a of the id
    // scaled). Same id, same jitter, forever.
    //-------------------------------------------------------------------------
    inline float IdJitter(
        LCE::Simulation::EntityId a_id, float a_span) noexcept
    {
        auto hash = std::uint64_t{ 14695981039346656037ull };
        auto value = a_id.Value();

        for (int i = 0; i < 8; ++i)
        {
            hash ^= (value & 0xFF);
            hash *= 1099511628211ull;
            value >>= 8;
        }

        const auto unit =
            static_cast<float>(hash & 0xFFFFFF) / static_cast<float>(0x1000000);

        return (unit - 0.5f) * 2.0f * a_span;
    }

    //-------------------------------------------------------------------------
    // TemperOf — the conflict source's personality (0.7.0 Stone 2). The
    // engine's JitteredTraits substrate (stone 09) varies a per-mind
    // temperament around 1.0 (spread 0.2 → roughly 0.8–1.2), derived
    // deterministically from the entity id — the same id draws the same
    // temper every run, so a saved mind re-derives its own. The adapter's
    // behaviour table decides what the number means: a mind at or above
    // the sim.slight.temper line (a churlish one) blames the stall-keeper
    // when a hungry arrival finds the market shut; a warm mind forgives.
    // The core holds the substrate; the meaning is the world's — exactly
    // the boundary the Traits stone declared.
    //-------------------------------------------------------------------------
    inline float TemperOf(LCE::Simulation::EntityId a_id) noexcept
    {
        const LCE::Simulation::Traits base{
            { LCE::Simulation::TraitValue{ "temper", 1.0f } } };

        const auto traits =
            LCE::Simulation::JitteredTraits(base, a_id, nullptr, 0.2f);

        return traits.List.empty() ? 1.0f : traits.List[0].Value;
    }

    //-------------------------------------------------------------------------
    // VaryNeeds — the desync stone (0.5.x). Every mind's needs are born
    // slightly different: a small deterministic jitter on each need's
    // initial value and decay rate. Hunger therefore arrives at different
    // times for different minds — the settlement stops marching to the
    // market in lockstep. The decay-rate jitter is a *metabolism*: the
    // stagger persists after every feed and grows as the session runs.
    // The core decays need.Value -= need.DecayRate * dt, and the co-save
    // serializes the rate, so a mind's rhythm survives restore. The
    // jitter is shared across a mind's needs (one id, one temperament),
    // which keeps each mind's internal urgency ordering sensible.
    //-------------------------------------------------------------------------
    inline void VaryNeeds(
        LCE::Simulation::Needs& a_needs,
        LCE::Simulation::EntityId a_id) noexcept
    {
        const auto valueJitter = IdJitter(a_id, 0.10f);
        const auto rateFactor = 1.0f + IdJitter(a_id, 0.40f);

        for (auto& need : a_needs.List)
        {
            need.Value = std::clamp(need.Value + valueJitter, 0.0f, 1.0f);
            need.DecayRate = std::max(0.0f, need.DecayRate * rateFactor);
        }
    }

    //-------------------------------------------------------------------------
    // The arrival outcome (0.5.0 — the trade stone): what reaching the
    // food source means, per species. A human who found a trader to deal
    // with gets a real Trade — Success, the exchange lands (the core
    // serves AcquireFood and earns trust). A human who found no one (the
    // stall-keeper setting up, or a defensive miss) gets Trade, Partial —
    // arrived, but nothing changed hands. A child or an animal is fed —
    // Aid, Success: fed, gives nothing in return; a_traded is ignored for
    // them, they never barter. a_other is whoever resolved as the
    // counterparty (the trader, or the market for the stall-keeper; the
    // owner, or the settlement, for the fed).
    //-------------------------------------------------------------------------
    [[nodiscard]]
    LCE::Simulation::Outcome ArrivalOutcome(
        Species a_species,
        LCE::Simulation::EntityId a_other,
        bool a_traded);

    //-------------------------------------------------------------------------
    // RecordSale — the trader's half of the exchange (the trade stone).
    // A sale is a game fact at the edge: the stall-keeper remembers the
    // customer (a Trade memory, day-stamped like every arrival memory)
    // and warms toward them — repeat customers are good for business.
    // Pure and testable; the caller passes the stall-keeper's own Memory
    // and Relationships.
    //-------------------------------------------------------------------------
    inline void RecordSale(
        LCE::Simulation::Memory& a_memory,
        LCE::Simulation::Relationships& a_relationships,
        LCE::Simulation::EntityId a_customer,
        float a_warmth)
    {
        a_memory.Events.push_back(LCE::Simulation::MemoryEvent{
            a_customer, LCE::Simulation::InteractionKind::Trade, 1.0f });

        if (a_warmth != 0.0f)
        {
            a_relationships.ByEntity[a_customer].Disposition += a_warmth;
        }
    }

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
    // The sleep cycle (0.6.0): a resting mind recovers the needs a nap
    // fixes — Fatigue, Safety, and Comfort — each at a_rate per second
    // of simulation time, capped at 1.0 (fully rested). Safety is in
    // there because a drained Safety need with no remembered threat
    // makes the engine's Decide return nullopt (you can't flee from
    // nothing) — the mind parks, invisible to any intent-keyed pass;
    // rest is the recovery side for that silence too. Social stays
    // decay-only: it is the future Socialize stone's job, not a nap's.
    // Returns the Fatigue value (or -1 when the mind has no Fatigue
    // need — a defensive marker, all seeded minds have one).
    //-------------------------------------------------------------------------
    [[nodiscard]]
    float RestRecovery(
        LCE::Simulation::Needs& a_needs,
        float a_rate,
        float a_delta);

    //-------------------------------------------------------------------------
    // The most urgent need — the one with the lowest value, first in
    // the list wins ties, exactly the rule the engine's internal
    // MostUrgent answers (the engine keeps it private; the adapter
    // needs the same question to find rest-silenced minds). nullopt
    // when the mind has no needs at all.
    //-------------------------------------------------------------------------
    [[nodiscard]]
    std::optional<LCE::Simulation::NeedType> MostUrgentNeed(
        const LCE::Simulation::Needs& a_needs);

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
