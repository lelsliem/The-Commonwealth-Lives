//=============================================================================//
//                                                                             //
//   The Living Commonwealth — Fallout 4 adapter for the Living Commonwealth   //
//   Engine (LCE).                                                             //
//                                                                             //
//   First the belly, then the Commonwealth.                                    //
//                                                                             //
//=============================================================================//

#pragma once

#include "Names.h"
#include "Behaviour.h"

#include <cstdint>

namespace TLC
{
    //-------------------------------------------------------------------------
    // FormRef — the entity knows its game form. This is the adapter's half
    // of the translation (ADR-0024): the core reasons over EntityIds, and
    // this component is the edge's memory of which game form that is. The
    // FormID is a plain uint32 here so the component (and its serializer)
    // stay testable without the game — the edge casts at the boundary.
    //-------------------------------------------------------------------------
    struct FormRef
    {
        std::uint32_t FormId = 0;
    };

    //-------------------------------------------------------------------------
    // SpeciesTag — what kind of mind this entity is (Behaviour.h). Set once
    // at translation from the actor's race; persisted in the co-save so a
    // restored world keeps its dogs dogs. A missing tag reads as Human —
    // the default for a workshop population.
    //-------------------------------------------------------------------------
    struct SpeciesTag
    {
        Species Value = Species::Human;
    };

    //-------------------------------------------------------------------------
    // CompanionTag — a mind that has ever been a companion (0.7.5
    // field find). Set from the actor's HasBeenCompanionFaction (the
    // faction the game applies permanently the moment a companion is
    // recruited) and re-derived each second like the species, so a
    // pre-fix save heals. A companion stays a full mind — fed, trading,
    // befriending, feuding — but is never in the dating pool: the bond
    // gates refuse it a sweetheart or spouse (the kin flag).
    //-------------------------------------------------------------------------
    struct CompanionTag
    {
    };

    //-------------------------------------------------------------------------
    // CapPouch — the economy stone (0.5.x): a mind's few caps. Humans are
    // born with a small pouch (SeedPouch) and pay for their meals at the
    // market (PayForMeal); the seller's pouch grows. A missing pouch reads
    // as 0 caps — a pre-economy save restores broke and is fed on the
    // settlement's credit. Children and animals never carry one (they
    // never barter — the species profile decides who trades). Persisted
    // in the co-save like FormRef and SpeciesTag.
    //-------------------------------------------------------------------------
    struct CapPouch
    {
        std::uint32_t Caps = 0;
    };

    //-------------------------------------------------------------------------
    // Name is defined in Names.h — the identity stone (0.7.0): every
    // mind carries one, persisted in the co-save, the game's name first
    // and a procedural Commonwealth name for the nameless. It lives in
    // its own header with the name lists and generation; this include
    // keeps the component visible wherever the adapter's components are.
    //-------------------------------------------------------------------------

    //-------------------------------------------------------------------------
    // BirthDay — the sim-day a child was born. Used by the growth
    // system (0.7.7) to track how old a child is and when it should
    // grow into an adult. Carried by every sim-only child; adults have
    // no need for it.
    //-------------------------------------------------------------------------
    struct BirthDay
    {
        std::uint64_t Day = 0;
    };

    //-------------------------------------------------------------------------
    // Pregnancy (0.7.7 — the birth lifecycle). A spouse household's
    // journey from conception to birth. The adapter owns this component;
    // the engine never sees it. Stored on the mother entity and
    // persisted in the co-save like any named component. A child without
    // a Pregnancy record is a pre-0.7.7 birth (sim-only, already alive).
    //-------------------------------------------------------------------------
    struct Pregnancy
    {
        // The sim-day the couple conceived (the world-day stamp at
        // conception). Used to compute the due day and the gestation
        // progress bar for the news feed.
        std::uint64_t ConceptionDay = 0;

        // The sim-day the child is due (conception + gestation days).
        // On this day the birth event fires.
        std::uint64_t DueDay = 0;

        // The parents' entity IDs — the mother carries this component;
        // the father is the other spouse. Both are needed so the child
        // can bond to both and the household can be identified.
        std::uint64_t ParentA = 0;
        std::uint64_t ParentB = 0;
    };

    //-------------------------------------------------------------------------
    // SeedPouch — a human mind's born wealth: a deterministic pouch from
    // the entity id (the IdJitter span pattern), so a saved mind's purse
    // survives restore exactly. 40 ± 20 caps: a few meals, not a fortune.
    //-------------------------------------------------------------------------
    inline std::uint32_t SeedPouch(LCE::Simulation::EntityId a_id) noexcept
    {
        const auto caps = 40.0f + IdJitter(a_id, 20.0f);
        return static_cast<std::uint32_t>(caps < 0.0f ? 0.0f : caps);
    }

    //-------------------------------------------------------------------------
    // PayForMeal — the physical exchange (the economy stone). The buyer
    // pays what they can afford up to the price; the seller receives it.
    // Returns the caps that changed hands — 0 means the settlement
    // covered the meal (broke customers are still fed, never starved).
    // Pure: the caller resolves the two pouches at the edge.
    //-------------------------------------------------------------------------
    inline std::uint32_t PayForMeal(
        CapPouch& a_buyer, CapPouch& a_seller, std::uint32_t a_price) noexcept
    {
        const auto paid = a_buyer.Caps < a_price ? a_buyer.Caps : a_price;

        a_buyer.Caps -= paid;
        a_seller.Caps += paid;

        return paid;
    }
}
