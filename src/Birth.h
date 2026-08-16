//=============================================================================
//                                                                             //
//   The Living Commonwealth — Fallout 4 adapter for the Living Commonwealth   //
//   Engine (LCE).                                                             //
//                                                                             //
//   Every mind is born once — even a settlement's.                            //
//                                                                             //
//=============================================================================

#pragma once

#include "Behaviour.h"
#include "Components.h"

#include "LCE/Simulation/Entity/EntityRegistry.h"
#include "LCE/Simulation/Mind/Memory.h"
#include "LCE/Simulation/Mind/Needs.h"
#include "LCE/Simulation/Simulation.h"

namespace TLC
{
    //-------------------------------------------------------------------------
    // Birth (0.6.0 Stone 6 — experimental, INI-gated). A spouse household
    // has a child: a new mind, sim-only — there is no game actor, no
    // form, no translator entry. The child is fed by the household (its
    // hunger is restored directly — protected and fed), bonds to both
    // parents, and lives in the co-save like any mind. The census cannot
    // evict it: Lifecycle::Diff classifies the *census*, never the
    // registry, so a mind with no form is simply never scanned.
    //
    // 0.7.7 adds the pregnancy window: conception → gestation → birth →
    // growth. A bonded spouse pair conceives on a day-roll
    // (sim.birth.chance), the Pregnancy component tracks the journey, the
    // birth fires on the due day, and the child grows into an adult mind
    // after sim.birth.childhood days.
    //
    // 0.7.8 adds visible children: when the external baby mod is loaded
    // and sim.birth.visible is on, a grown child pairs with a real game
    // actor — it walks, trades, and bonds like any mind.
    //-------------------------------------------------------------------------
    namespace Birth
    {
        using namespace LCE::Simulation;

        //---------------------------------------------------------------------
        // Create — a child mind of a_spouseA and a_spouseB. Seeded like
        // any mind (needs, memory, goals) plus a warm relationship to
        // both parents — and the parents warm to the child. No FormRef:
        // the child has no game actor, and the co-save serializes it as
        // a component-carrying entity like any other.
        //---------------------------------------------------------------------
        [[nodiscard]]
        inline EntityId Create(
            EntityRegistry& a_registry,
            EntityId a_parentA,
            EntityId a_parentB,
            const NeedRates& a_rates)
        {
            if (!a_parentA.IsValid() || !a_parentB.IsValid())
            {
                return {};
            }

            const auto id = a_registry.CreateEntity();

            a_registry.AddComponent<SpeciesTag>(id, SpeciesTag{ Species::Child });
            a_registry.AddComponent<Needs>(
                id, SeededNeeds(Species::Child, a_rates));
            a_registry.AddComponent<Goals>(id, SeededGoals(Species::Child));
            a_registry.AddComponent<Memory>(id, Memory{});

            // The illness stone (0.8.0): a child is born healthy like
            // any mind; children fall sicker, faster (the ChildMult).
            a_registry.AddComponent<Health>(id, Health{});

            Relationships relationships;
            relationships.ByEntity[a_parentA] = Relationship{ 0.5f, 0.3f };
            relationships.ByEntity[a_parentB] = Relationship{ 0.5f, 0.3f };
            a_registry.AddComponent<Relationships>(id, std::move(relationships));

            // The parents know their child.
            auto relA = a_registry.GetComponent<Relationships>(a_parentA);
            auto relB = a_registry.GetComponent<Relationships>(a_parentB);

            if (relA)
            {
                relA->ByEntity[id] = Relationship{ 0.6f, 0.4f };
            }

            if (relB)
            {
                relB->ByEntity[id] = Relationship{ 0.6f, 0.4f };
            }

            return id;
        }

        //---------------------------------------------------------------------
        // TryConceive — roll for conception on a bonded spouse pair. If
        // the roll succeeds and neither parent already carries a
        // Pregnancy, create one on the mother (the parent whose entity
        // ID is lower — deterministic). Returns the mother's entity ID
        // if conception occurred, or {}.
        //---------------------------------------------------------------------
        [[nodiscard]]
        inline EntityId TryConceive(
            EntityRegistry& a_registry,
            EntityId a_parentA,
            EntityId a_parentB,
            float a_chance,
            float a_gestation,
            std::uint64_t a_currentDay)
        {
            if (!a_parentA.IsValid() || !a_parentB.IsValid())
            {
                return {};
            }

            // Don't conceive if either parent already has a pregnancy.
            if (a_registry.GetComponent<Pregnancy>(a_parentA) != nullptr
                || a_registry.GetComponent<Pregnancy>(a_parentB) != nullptr)
            {
                return {};
            }

            // Deterministic mother: the lower entity ID.
            const auto mother = a_parentA.Value() <= a_parentB.Value()
                ? a_parentA : a_parentB;
            const auto father = mother == a_parentA
                ? a_parentB : a_parentA;

            const auto dueDay = static_cast<std::uint64_t>(
                a_currentDay + static_cast<std::uint64_t>(a_gestation));

            Pregnancy preg;
            preg.ConceptionDay = a_currentDay;
            preg.DueDay = dueDay;
            preg.ParentA = a_parentA.Value();
            preg.ParentB = a_parentB.Value();

            a_registry.AddComponent<Pregnancy>(mother, std::move(preg));

            return mother;
        }

        //---------------------------------------------------------------------
        // CheckBirths — scan for pregnancies that have reached their due
        // day. For each, create the child mind and remove the Pregnancy
        // component from the mother. Returns the newly created child
        // entity IDs (may be empty).
        //
        // Two-pass: collect due mothers first (read-only), then create
        // children and remove pregnancies.  Removing a component inside
        // ForEachWithComponent can invalidate the iterator and crash
        // (the 0.7.7 day-261 CTD).
        //---------------------------------------------------------------------
        inline std::vector<EntityId> CheckBirths(
            EntityRegistry& a_registry,
            const NeedRates& a_rates,
            std::uint64_t a_currentDay)
        {
            // Pass 1: collect due mothers (read-only).
            struct DueMother
            {
                EntityId Mother;
                EntityId ParentA;
                EntityId ParentB;
            };

            std::vector<DueMother> due;

            a_registry.ForEachWithComponent<Pregnancy>(
                [&](EntityId a_mother, Pregnancy& a_preg)
                {
                    if (a_currentDay >= a_preg.DueDay)
                    {
                        due.push_back({
                            a_mother,
                            EntityId{ a_preg.ParentA },
                            EntityId{ a_preg.ParentB }});
                    }
                });

            // Pass 2: create children and consume pregnancies.
            std::vector<EntityId> newborns;

            for (const auto& entry : due)
            {
                // Safety: if either parent was destroyed (e.g. the
                // player killed them), skip the birth — a child with
                // one dead parent is a sad story, not a crash.
                if (a_registry.GetComponent<Needs>(entry.ParentA) == nullptr
                    || a_registry.GetComponent<Needs>(entry.ParentB) == nullptr)
                {
                    a_registry.RemoveComponent<Pregnancy>(
                        entry.Mother);
                    continue;
                }

                const auto child = Create(
                    a_registry, entry.ParentA, entry.ParentB, a_rates);

                if (child.IsValid())
                {
                    a_registry.AddComponent<BirthDay>(
                        child, BirthDay{ a_currentDay });

                    newborns.push_back(child);
                }

                a_registry.RemoveComponent<Pregnancy>(entry.Mother);
            }

            return newborns;
        }

        //---------------------------------------------------------------------
        // GrowChildren — scan for children whose BirthDay has aged past
        // sim.birth.childhood. Upgrade their SpeciesTag from Child to
        // Human so the adapter treats them as full walking minds (they
        // walk to market, trade, bond, feud). Returns how many children
        // grew.
        //---------------------------------------------------------------------
        inline std::size_t GrowChildren(
            EntityRegistry& a_registry,
            float a_childhood,
            std::uint64_t a_currentDay)
        {
            std::size_t count = 0;

            a_registry.ForEachWithComponent<BirthDay>(
                [&](EntityId a_entity, BirthDay& a_birth)
                {
                    const auto age = static_cast<float>(
                        a_currentDay - a_birth.Day);

                    if (age < a_childhood)
                    {
                        return;  // still a child
                    }

                    const auto species =
                        a_registry.GetComponent<SpeciesTag>(a_entity);

                    if (species == nullptr
                        || species->Value != Species::Child)
                    {
                        return;  // already grown or not a child
                    }

                    // Grow up: Child → Human. The child already has
                    // Needs, Goals, Memory, Relationships — the only
                    // change is the species tag so the adapter's
                    // species split treats it as a full mind.
                    species->Value = Species::Human;

                    // Give it a starting pouch so it can trade.
                    if (a_registry.GetComponent<CapPouch>(a_entity)
                        == nullptr)
                    {
                        a_registry.AddComponent<CapPouch>(
                            a_entity, CapPouch{ SeedPouch(a_entity) });
                    }

                    ++count;
                });

            return count;
        }

        //---------------------------------------------------------------------
        // PairVisibleChild — (0.7.8) when the external baby mod is
        // loaded and sim.birth.visible is on, search for an unassigned
        // child actor in the settlement and pair it with the sim-only
        // child. The child gets a FormRef and walks the world like any
        // mind. Returns true if pairing succeeded.
        //
        // The baby mod ("Baby Sim - Babies That Grow Up") provides
        // child actors via a leveled list (Cyber_KidsThatGrewUp). At
        // runtime, we scan ProcessLists for actors with the HumanChildRace
        // or GhoulChildRace that are NOT already in the translator, and
        // pair the first one we find with the sim-only child.
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        // PairVisibleChild — (0.7.8) implemented in Adapter.cpp because
        // it needs game types (RE::ProcessLists, RE::Actor). The adapter
        // calls this after GrowChildren when sim.birth.visible is on and
        // the baby mod is loaded. Returns true if pairing succeeded.
        //---------------------------------------------------------------------

        //---------------------------------------------------------------------
        // FeedChildren — one tick of the child's life: every not-yet-
        // grown child is fed by the household — Hunger recovers toward
        // full at the settlement's pace. Paired or not (0.8.9: a
        // newborn paired with a visible baby actor still cannot walk
        // to a bench; the household feed is the child's meal until
        // GrowChildren makes it a grown mind that walks like anyone).
        // Returns how many children were fed.
        //---------------------------------------------------------------------
        inline std::size_t FeedChildren(
            EntityRegistry& a_registry, float a_delta)
        {
            std::size_t count = 0;

            a_registry.ForEachWithComponent<Needs>(
                [&](EntityId a_entity, Needs& a_needs)
                {
                    const auto species =
                        a_registry.GetComponent<SpeciesTag>(a_entity);

                    if (species == nullptr || species->Value != Species::Child)
                    {
                        return;
                    }

                    for (auto& need : a_needs.List)
                    {
                        if (need.Type == NeedType::Hunger)
                        {
                            need.Value = std::min(1.0f, need.Value + 0.2f * a_delta);
                            ++count;
                            break;
                        }
                    }
                });

            return count;
        }
    }
}
