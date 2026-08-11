//=============================================================================//
//                                                                             //
//   The Living Commonwealth — Fallout 4 adapter for the Living Commonwealth   //
//   Engine (LCE).                                                             //
//                                                                             //
//   Two hearts, one pouch.                                                     //
//                                                                             //
//=============================================================================//

#pragma once

#include "Bonds.h"
#include "Components.h"

#include "LCE/Simulation/EntityId.h"
#include "LCE/Simulation/EntityRegistry.h"

#include <cstdint>
#include <unordered_set>

namespace TLC::Households
{
    using namespace LCE::Simulation;

    //-------------------------------------------------------------------------
    // Households (0.6.0 Stone 3 — "couples share one pouch, one stall,
    // the same bench"). A household is the deepest bond made concrete: the
    // moment a pair's bond becomes Spouse (the +0.8 mutual line), their
    // two pouches merge into one shared wallet; the family bench is home
    // for both (no exchange between the pair); and when the marriage
    // dissolves, the wallet splits. Pure — no game types, tested without
    // the game, exactly like Bonds and Lifecycle.
    //
    // Households are DERIVED state, never a record of their own: the
    // marriage rides the bond map (co-save v5), and the shared pouch rides
    // the CapPouch component on one member. The invariant — one pouch per
    // married pair, one per unmarried human — is what makes the world
    // round-trip: a saved merged pouch restores as one pouch, and Enforce
    // repairs any drift silently. ADR-0013.
    //-------------------------------------------------------------------------

    // The spouse in a marriage, from the bond map. The map is keyed by
    // the sorted pair; scan for the Spouse row containing a_entity
    // (bonds are few — a settlement's handful of marriages).
    inline EntityId SpouseOf(
        const Bonds::BondMap& a_bonds,
        EntityId a_entity) noexcept
    {
        const auto value = a_entity.Value();

        for (const auto& [key, bond] : a_bonds)
        {
            if (bond.Kind != Bonds::BondKind::Spouse)
            {
                continue;
            }

            if (key.first == value)
            {
                return EntityId{ key.second };
            }

            if (key.second == value)
            {
                return EntityId{ key.first };
            }
        }

        return {};
    }

    // The pouch an entity trades with: its own, else the household's —
    // the spouse's pouch (a married member may be the one without the
    // physical pouch; the wallet is shared either way). nullptr when the
    // entity has no pouch at all (a broke, unmarried mind — fed on the
    // settlement's credit).
    inline CapPouch* PouchOf(
        EntityRegistry& a_registry,
        const Bonds::BondMap& a_bonds,
        EntityId a_entity) noexcept
    {
        if (auto pouch = a_registry.GetComponent<CapPouch>(a_entity))
        {
            return pouch.get();
        }

        const auto spouse = SpouseOf(a_bonds, a_entity);

        if (spouse.IsValid())
        {
            if (auto pouch = a_registry.GetComponent<CapPouch>(spouse))
            {
                return pouch.get();
            }
        }

        return nullptr;
    }

    // Is a_entity already part of a shared household? Derived from the
    // components — the merged wallet lives on one member, the other has
    // no pouch — plus the bond book: scan EVERY spouse bond a_entity
    // holds (a beloved settler can honestly read Spouse to two minds;
    // SpouseOf alone would miss the second marriage) and ask, for each
    // pair, whether exactly one pouch exists — shared. Order-independent
    // and restore-proof: the merged pouch is a component, so the answer
    // is true no matter which marriage formed first.
    //
    // This is the one-wallet-per-mind guard (the 2026-08-11 polygamy
    // edge): the bond layer may honestly read Spouse to two minds at
    // once (bonds are pure derived disposition), but the household layer
    // must stay monogamous — a second FormHousehold would fold a third
    // pouch into the shared wallet, and a later 2-way split would
    // silently vanish the third member's caps.
    inline bool InHousehold(
        const EntityRegistry& a_registry,
        const Bonds::BondMap& a_bonds,
        EntityId a_entity) noexcept
    {
        const auto value = a_entity.Value();
        const auto pouch = a_registry.GetComponent<CapPouch>(a_entity);

        for (const auto& [key, bond] : a_bonds)
        {
            if (bond.Kind != Bonds::BondKind::Spouse)
            {
                continue;
            }

            EntityId spouse;

            if (key.first == value)
            {
                spouse = EntityId{ key.second };
            }
            else if (key.second == value)
            {
                spouse = EntityId{ key.first };
            }
            else
            {
                continue;
            }

            const auto spousePouch =
                a_registry.GetComponent<CapPouch>(spouse);

            // Shared: exactly one of the pair carries the pouch. Two
            // pouches = both personal (not yet merged); none = defensive
            // (nothing shared to guard).
            if ((pouch != nullptr) != (spousePouch != nullptr))
            {
                return true;
            }
        }

        return false;
    }

    //-------------------------------------------------------------------------
    // FormHousehold — merge the pair's pouches into one shared wallet on
    // the lower-id member. Returns true when a merge (or a defensive
    // seed) actually happened — a new shared pouch; false when the pair
    // already shares one (idempotent, so the caller may call it freely).
    //-------------------------------------------------------------------------
    inline bool FormHousehold(
        EntityRegistry& a_registry,
        EntityId a_a,
        EntityId a_b)
    {
        const auto holder = a_a.Value() <= a_b.Value() ? a_a : a_b;
        const auto other = a_a.Value() <= a_b.Value() ? a_b : a_a;

        auto holderPouch = a_registry.GetComponent<CapPouch>(holder);
        auto otherPouch = a_registry.GetComponent<CapPouch>(other);

        if (holderPouch && otherPouch)
        {
            // The marriage: the wallets become one.
            holderPouch->Caps += otherPouch->Caps;
            a_registry.RemoveComponent<CapPouch>(other);
            return true;
        }

        if (holderPouch)
        {
            return false;   // already one shared pouch
        }

        if (otherPouch)
        {
            // The pouch happened to live on the other member — move it to
            // the deterministic holder, so a restored world always puts
            // the wallet in the same hands.
            a_registry.AddComponent<CapPouch>(
                holder, CapPouch{ otherPouch->Caps });
            a_registry.RemoveComponent<CapPouch>(other);
            return true;
        }

        // Neither has a pouch — defensive (a marriage should never be
        // broke by construction): seed the holder.
        a_registry.AddComponent<CapPouch>(
            holder, CapPouch{ SeedPouch(holder) });
        return true;
    }

    //-------------------------------------------------------------------------
    // DissolveHousehold — split the shared pouch. The member who still
    // holds the wallet keeps the remainder; the other member receives a
    // pouch with half. Returns true when a split actually happened (a
    // shared wallet existed); false when both already carry their own
    // (idempotent). The split (holder's share, other's share) is written
    // back for the caller's log line.
    //-------------------------------------------------------------------------
    inline bool DissolveHousehold(
        EntityRegistry& a_registry,
        EntityId a_a,
        EntityId a_b,
        std::uint32_t& a_holderShare,
        std::uint32_t& a_otherShare)
    {
        const auto pouchA = a_registry.GetComponent<CapPouch>(a_a);
        const auto pouchB = a_registry.GetComponent<CapPouch>(a_b);

        if (pouchA && pouchB)
        {
            return false;   // already split — two personal pouches
        }

        const auto holder = pouchA ? a_a : (pouchB ? a_b : EntityId{});

        if (!holder.IsValid())
        {
            return false;   // nothing to split
        }

        const auto other = holder == a_a ? a_b : a_a;
        const auto caps = (holder == a_a ? pouchA : pouchB)->Caps;
        const auto half = caps / 2;

        auto holderPouch = a_registry.GetComponent<CapPouch>(holder);
        auto otherPouch = a_registry.GetComponent<CapPouch>(other);

        holderPouch->Caps = caps - half;

        if (otherPouch)
        {
            otherPouch->Caps = half;
        }
        else
        {
            a_registry.AddComponent<CapPouch>(other, CapPouch{ half });
        }

        a_holderShare = caps - half;
        a_otherShare = half;

        return true;
    }

    //-------------------------------------------------------------------------
    // Enforce — the household invariants, silently (never logs, never
    // reports): every married pair shares exactly one pouch, and every
    // unmarried human carries their own. Restores call this after the
    // bond book returns (a saved marriage is already merged — no-op); the
    // 1-second pass calls it as a defensive repair after the loud bond
    // events have had their say. FormHousehold is idempotent, so the
    // repeated call is free.
    //-------------------------------------------------------------------------
    inline void Enforce(
        EntityRegistry& a_registry,
        const Bonds::BondMap& a_bonds)
    {
        std::unordered_set<EntityId> married;

        for (const auto& [key, bond] : a_bonds)
        {
            if (bond.Kind != Bonds::BondKind::Spouse)
            {
                continue;
            }

            const auto a = EntityId{ key.first };
            const auto b = EntityId{ key.second };

            married.insert(a);
            married.insert(b);

            // The one-wallet-per-mind guard (the polygamy edge): a pair
            // where either side already shares a pouch must not merge a
            // third one in — the first household stands.
            if (InHousehold(a_registry, a_bonds, a)
                || InHousehold(a_registry, a_bonds, b))
            {
                continue;
            }

            FormHousehold(a_registry, a, b);
        }

        // Unmarried humans keep a personal pouch (a widow or a fresh
        // divorcee never goes broke by construction).
        a_registry.ForEachWithComponent<SpeciesTag>(
            [&](EntityId a_entity, const SpeciesTag& a_tag)
            {
                if (a_tag.Value != Species::Human || married.contains(a_entity))
                {
                    return;
                }

                if (!a_registry.GetComponent<CapPouch>(a_entity))
                {
                    a_registry.AddComponent<CapPouch>(
                        a_entity, CapPouch{ SeedPouch(a_entity) });
                }
            });
    }
}
