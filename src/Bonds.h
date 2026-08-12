//=============================================================================//
//                                                                             //
//   The Living Commonwealth — Fallout 4 adapter for the Living Commonwealth   //
//   Engine (LCE).                                                             //
//                                                                             //
//   A bond is a line crossed twice: once to form, once to fade.               //
//                                                                             //
//=============================================================================//

#pragma once

#include "Components.h"
#include "Kin.h"

#include "LCE/Simulation/Entity/EntityId.h"
#include "LCE/Simulation/Entity/EntityRegistry.h"
#include "LCE/Simulation/Mind/Relationships.h"
#include "LCE/Simulation/Simulation.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace TLC::Bonds
{
    using namespace LCE::Simulation;

    //-------------------------------------------------------------------------
    // Bonds (0.6.0 Stone 2 — "Bonds: relationships, good and bad"). The
    // core holds dispositions and trusts — numbers that drift and shift —
    // and publishes RelationshipChangedEvent when a configured line is
    // crossed (stone 08, Request A). The bond is the *named state* the
    // adapter derives from those numbers: friend, sweetheart, spouse,
    // rival, enemy. Pure: no game types here, so the harness tests it
    // without the game, exactly like Lifecycle and WorldFacts.
    //
    // Two channels feed the same derivation:
    //   - the event (immediate): the core crossed a line mid-mutation —
    //     the adapter re-derives that pair right now;
    //   - the 1-second pass (the net): drift is deliberately quiet in the
    //     core — a bond cooling below its line is a dissolve, not an
    //     event — so the adapter re-derives every pair each pass and
    //     catches what the events never announce.
    //
    // The derivation is mutual and sticky:
    //   - Mutual: the pair's shared disposition is the *minimum* of the
    //     two directions — both must feel it. Friendship is mutual
    //     warmth; one-sided admiration is not yet a bond.
    //   - Sticky: formation is immediate at the line; dissolution waits
    //     until the pair falls halfway back (drift is a whisper — a bond
    //     that just formed at +0.31 must not vanish at +0.29 the next
    //     tick). Same-family downgrades are sticky too: sweethearts stay
    //     sweethearts while their warmth merely thins, and only a true
    //     family flip (friends turned rivals) changes the bond outright.
    //-------------------------------------------------------------------------

    // The named states, in serialization order (the co-save writes the
    // ordinal — append-only, never reorder).
    enum class BondKind
    {
        None,        // no bond
        Rival,       // -0.3
        Enemy,       // -0.6 — the feud
        Friend,      // +0.3
        Sweetheart,  // +0.6
        Spouse       // +0.8, mutual — the deepest line
    };

    // The adapter's copy of the world's lines, typed. Defaults are the
    // Life.md vocabulary; the INI (sim.bond.threshold.<name>) overrides
    // them through the core's Configuration service — the same values
    // the core's watch-list uses, so the events and the derivation can
    // never disagree.
    struct BondThresholds
    {
        float Friend = 0.3f;
        float Sweetheart = 0.6f;
        float Spouse = 0.8f;
        float Rival = -0.3f;
        float Enemy = -0.6f;
    };

    inline const char* BondName(BondKind a_kind) noexcept
    {
        switch (a_kind)
        {
        case BondKind::Rival:      return "rival";
        case BondKind::Enemy:      return "enemy";
        case BondKind::Friend:     return "friend";
        case BondKind::Sweetheart: return "sweetheart";
        case BondKind::Spouse:     return "spouse";
        default:                   return "none";
        }
    }

    inline const char* BondPlural(BondKind a_kind) noexcept
    {
        switch (a_kind)
        {
        case BondKind::Rival:      return "rivals";
        case BondKind::Enemy:      return "enemies";
        case BondKind::Friend:     return "friends";
        case BondKind::Sweetheart: return "sweethearts";
        case BondKind::Spouse:     return "spouses";
        default:                   return "none";
        }
    }

    inline bool IsPositive(BondKind a_kind) noexcept
    {
        return a_kind == BondKind::Friend
            || a_kind == BondKind::Sweetheart
            || a_kind == BondKind::Spouse;
    }

    inline bool IsNegative(BondKind a_kind) noexcept
    {
        return a_kind == BondKind::Rival || a_kind == BondKind::Enemy;
    }

    // The line a kind was formed on — for the sticky dissolve (halfway
    // back) and for upgrade comparisons.
    inline float LineFor(BondKind a_kind, const BondThresholds& a_t) noexcept
    {
        switch (a_kind)
        {
        case BondKind::Rival:      return a_t.Rival;
        case BondKind::Enemy:      return a_t.Enemy;
        case BondKind::Friend:     return a_t.Friend;
        case BondKind::Sweetheart: return a_t.Sweetheart;
        case BondKind::Spouse:     return a_t.Spouse;
        default:                   return 0.0f;
        }
    }

    // The fresh classification of a pair's shared disposition (the
    // minimum of the two directions) against the lines. Negative first —
    // a strong dislike names the pair before a weak warmth does; the
    // ranges cannot overlap (enemy < rival < 0 < friend < sweetheart <
    // spouse with the default lines).
    inline BondKind Classify(float a_shared, const BondThresholds& a_t) noexcept
    {
        if (a_shared <= a_t.Enemy)
        {
            return BondKind::Enemy;
        }

        if (a_shared <= a_t.Rival)
        {
            return BondKind::Rival;
        }

        if (a_shared >= a_t.Spouse)
        {
            return BondKind::Spouse;
        }

        if (a_shared >= a_t.Sweetheart)
        {
            return BondKind::Sweetheart;
        }

        if (a_shared >= a_t.Friend)
        {
            return BondKind::Friend;
        }

        return BondKind::None;
    }

    // Derive — the pair's bond, given both directed dispositions and the
    // pair's current bond. Pure; the caller holds the map. Rules:
    //
    //   fresh = Classify(min(a_toB, b_toA))
    //   current None   -> fresh            (formation)
    //   fresh None     -> current, unless the pair fell below half the
    //                     current kind's line (sticky dissolve)
    //   family flip    -> fresh            (a friend turned rival is news)
    //   same family    -> the stronger     (upgrades are immediate,
    //                     downgrades wait for the sticky dissolve)
    inline BondKind Derive(
        float a_toB,
        float b_toA,
        const BondThresholds& a_t,
        BondKind a_current) noexcept
    {
        const auto shared = std::min(a_toB, b_toA);
        const auto fresh = Classify(shared, a_t);

        if (a_current == BondKind::None)
        {
            return fresh;
        }

        if (fresh == BondKind::None)
        {
            // Sticky dissolve: a bond persists until the pair falls
            // halfway back from its line. Friend +0.3 dissolves below
            // +0.15; enemy −0.6 dissolves above −0.3.
            const auto sticky = LineFor(a_current, a_t) * 0.5f;

            if (IsPositive(a_current))
            {
                return shared >= sticky ? a_current : BondKind::None;
            }

            return shared <= sticky ? a_current : BondKind::None;
        }

        // A family flip — positive to negative or back — is a genuine
        // change of heart; follow the fresh state.
        if (IsPositive(a_current) != IsPositive(fresh))
        {
            return fresh;
        }

        // Same family: upgrade only. A bond keeps its kind until the
        // pair clearly outgrows it; thinning warmth is the sticky
        // dissolve's business, not an instant downgrade.
        if (std::abs(LineFor(fresh, a_t)) > std::abs(LineFor(a_current, a_t)))
        {
            return fresh;
        }

        return a_current;
    }

    //-------------------------------------------------------------------------
    // The bond map — the adapter's book, keyed by the unordered pair of
    // entity ids (sorted values, so a pair has one row no matter which
    // side walks it). Session-local ids, like every core-facing handle;
    // the co-save translates to form ids at the edge (BondsForSave /
    // RestoreBonds), exactly like the stall-keepers.
    //-------------------------------------------------------------------------
    using BondKey = std::pair<std::uint64_t, std::uint64_t>;

    inline BondKey PairKey(EntityId a_a, EntityId a_b) noexcept
    {
        const auto va = a_a.Value();
        const auto vb = a_b.Value();

        return va <= vb ? BondKey{ va, vb } : BondKey{ vb, va };
    }

    struct PairBond
    {
        BondKind Kind = BondKind::None;

        // The world day the bond formed. Stamped on formation, cleared
        // on dissolve — a reformed bond is a new bond, its own day.
        std::uint64_t SinceDay = 0;
    };

    using BondMap = std::map<BondKey, PairBond>;

    //-------------------------------------------------------------------------
    // CurrentKind — the bond state the adapter's book currently holds
    // for a pair (the 1-second pass's view; None when the pair is not in
    // the book). The read the Rows crossing uses: a feud partner is
    // someone the book already names.
    //-------------------------------------------------------------------------
    inline BondKind CurrentKind(
        const BondMap& a_bonds, EntityId a_a, EntityId a_b) noexcept
    {
        const auto iterator = a_bonds.find(PairKey(a_a, a_b));
        return iterator != a_bonds.end() ? iterator->second.Kind
                                         : BondKind::None;
    }

    //-------------------------------------------------------------------------
    // The typed thresholds, from the core's watch-list (the values the
    // core is actually watching — never a second opinion). A name the
    // world configured (friend, sweetheart, ...) overrides the default;
    // a name the adapter does not know (the world's own vocabulary) is
    // ignored — the core still publishes events for it, but there is no
    // typed kind to derive.
    //-------------------------------------------------------------------------
    inline BondThresholds ParseBondThresholds(
        const std::vector<BondThreshold>& a_lines)
    {
        BondThresholds thresholds;

        for (const auto& line : a_lines)
        {
            if (line.Name == "friend")
            {
                thresholds.Friend = line.Value;
            }
            else if (line.Name == "sweetheart")
            {
                thresholds.Sweetheart = line.Value;
            }
            else if (line.Name == "spouse")
            {
                thresholds.Spouse = line.Value;
            }
            else if (line.Name == "rival")
            {
                thresholds.Rival = line.Value;
            }
            else if (line.Name == "enemy")
            {
                thresholds.Enemy = line.Value;
            }
        }

        return thresholds;
    }

    // The adapter's defaults for the core's watch-list. The core ships
    // an empty watch-list on purpose — the world must name its own lines
    // — so the adapter names them here when the config file does not.
    inline std::vector<BondThreshold> DefaultBondThresholds()
    {
        return {
            { "friend", 0.3f },
            { "sweetheart", 0.6f },
            { "spouse", 0.8f },
            { "rival", -0.3f },
            { "enemy", -0.6f },
        };
    }

    //-------------------------------------------------------------------------
    // HasSpouseElsewhere — the monogamy cap (0.7.5 field find): does
    // this mind already hold a spouse bond with someone other than the
    // pair being considered? The heart can warm twice; the marriage is
    // one. Pure scan of the book.
    //-------------------------------------------------------------------------
    inline bool HasSpouseElsewhere(
        const BondMap& a_bonds, EntityId a_entity,
        BondKey a_exclude) noexcept
    {
        const auto value = a_entity.Value();

        for (const auto& [key, bond] : a_bonds)
        {
            if (bond.Kind != BondKind::Spouse || key == a_exclude)
            {
                continue;
            }

            if (key.first == value || key.second == value)
            {
                return true;
            }
        }

        return false;
    }

    //-------------------------------------------------------------------------
    // ApplyPair — derive one pair and fold it into the map. Pure aside
    // from the map; a_onChanged fires only when the bond CHANGED this
    // call (formation, dissolve, upgrade, family flip — resting is
    // silent), with the old and new kinds and the since-day (0 when the
    // bond dissolved). a_kin names a pair the world knows must never
    // romance — family (Kin.h — a shared parent, a child) or a
    // companion (the CompanionTag): family can be friends, never
    // lovers, and a companion's dating pool is closed, so a romantic
    // kind is refused — including one already on the books, so a
    // pre-fix save's mistake heals on the next pass.
    //-------------------------------------------------------------------------
    inline void ApplyPair(
        BondMap& a_bonds,
        BondKey a_key,
        float a_dToOther,
        float a_dOtherToMe,
        const BondThresholds& a_t,
        std::uint64_t a_day,
        const std::function<void(
            EntityId, EntityId, BondKind, BondKind, std::uint64_t)>& a_onChanged,
        bool a_kin = false)
    {
        auto& current = a_bonds[a_key];
        const auto old = current.Kind;
        auto fresh = Derive(a_dToOther, a_dOtherToMe, a_t, old);

        // The family gate (0.7.5 field find): kin never romance. A pair
        // the sim knows is family — a child, or a curated kin pair — can
        // be friends, never sweethearts or spouses. Applies to the
        // CURRENT bond too: a pre-fix romance is downgraded to
        // friendship the first pass it is seen, and an old save heals
        // itself.
        if (a_kin
            && (old == BondKind::Sweetheart || old == BondKind::Spouse
                || fresh == BondKind::Sweetheart || fresh == BondKind::Spouse))
        {
            fresh = BondKind::Friend;
        }

        // The monogamy cap (0.7.5 field find): a pair that would cross
        // the spouse line while either side already holds a spouse bond
        // with someone else caps at sweetheart. An existing marriage is
        // never broken by the cap — only new ones are refused, so a
        // married pair stays married and a heart can warm twice without
        // a second marriage forming.
        if (fresh == BondKind::Spouse && old != BondKind::Spouse)
        {
            const auto a = EntityId{ a_key.first };
            const auto b = EntityId{ a_key.second };

            if (HasSpouseElsewhere(a_bonds, a, a_key)
                || HasSpouseElsewhere(a_bonds, b, a_key))
            {
                fresh = BondKind::Sweetheart;
            }
        }

        if (fresh == old)
        {
            return;   // resting — the lines were not crossed
        }

        current.Kind = fresh;

        if (fresh == BondKind::None)
        {
            current.SinceDay = 0;
        }
        else if (old == BondKind::None)
        {
            current.SinceDay = a_day;
        }

        if (a_onChanged)
        {
            a_onChanged(
                EntityId{ a_key.first }, EntityId{ a_key.second },
                old, fresh, current.SinceDay);
        }
    }

    //-------------------------------------------------------------------------
    // Reconcile — the 1-second pass (the dissolve net). Walks every
    // mind's relationships, derives each unordered pair once (the pair's
    // shared disposition is the minimum of the two directions; a missing
    // reverse direction reads as neutral), and reports changes. The
    // event channel and this pass share ApplyPair, so the two can never
    // disagree — the event is instant, this is complete.
    //-------------------------------------------------------------------------
    inline void Reconcile(
        EntityRegistry& a_registry,
        const BondThresholds& a_t,
        BondMap& a_bonds,
        std::uint64_t a_day,
        const std::function<void(
            EntityId, EntityId, BondKind, BondKind, std::uint64_t)>& a_onChanged,
        const Kin::KinSet& a_kin = {})
    {
        std::set<BondKey> visited;

        a_registry.ForEachWithComponent<Relationships>(
            [&](EntityId a_entity, Relationships& a_relationships)
            {
                // The pair's two directed dispositions, read once. The
                // reverse direction may not exist yet — a one-sided
                // feeling reads as neutral (0), which the mutual rule
                // handles (no bond until both feel it).
                for (const auto& [other, relationship] : a_relationships.ByEntity)
                {
                    if (!other.IsValid() || other == a_entity)
                    {
                        continue;
                    }

                    const auto key = PairKey(a_entity, other);

                    if (!visited.insert(key).second)
                    {
                        continue;   // the other side of the pair walked it
                    }

                    // Both must be minds — a workshop is a target, never
                    // a bond partner — and both must be people (0.7.5
                    // field find): an animal is fed, not bonded. The
                    // feud needs two humanoids; a dog never rows, feuds,
                    // or fights, whatever its dispositions.
                    const auto tagA =
                        a_registry.GetComponent<SpeciesTag>(a_entity);
                    const auto tagB =
                        a_registry.GetComponent<SpeciesTag>(other);

                    if (!tagA || !tagB
                        || tagA->Value == Species::Animal
                        || tagB->Value == Species::Animal)
                    {
                        continue;
                    }

                    // The never-romance gate (0.7.5 field finds): a
                    // child never romances anyone (kin by species), a
                    // curated kin pair (the vanilla families) never
                    // romances either, and a companion
                    // (HasBeenCompanionFaction — the CompanionTag) never
                    // romances: friends and feuds are fine, the dating
                    // pool is closed.
                    const bool kin =
                        tagA->Value == Species::Child
                        || tagB->Value == Species::Child
                        || a_kin.contains(key)
                        || a_registry.GetComponent<CompanionTag>(a_entity)
                        || a_registry.GetComponent<CompanionTag>(other);

                    float dOtherToMe = 0.0f;

                    if (const auto reverse =
                            a_registry.GetComponent<Relationships>(other))
                    {
                        const auto iterator = reverse->ByEntity.find(a_entity);

                        if (iterator != reverse->ByEntity.end())
                        {
                            dOtherToMe = iterator->second.Disposition;
                        }
                    }

                    ApplyPair(
                        a_bonds, key,
                        relationship.Disposition, dOtherToMe,
                        a_t, a_day, a_onChanged, kin);
                }
            });
    }
}
