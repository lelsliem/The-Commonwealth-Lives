//=============================================================================//
//                                                                             //
//   The Living Commonwealth — Fallout 4 adapter for the Living Commonwealth   //
//   Engine (LCE).                                                             //
//                                                                             //
//   Grief is company that never arrives; feuds are gossip that stayed.       //
//                                                                             //
//=============================================================================//

#pragma once

#include "Bonds.h"
#include "Gossip.h"

#include "LCE/Simulation/Entity/EntityRegistry.h"
#include "LCE/Simulation/Mind/Memory.h"
#include "LCE/Simulation/Mind/Needs.h"
#include "LCE/Simulation/Substrate/Rng.h"
#include "LCE/Simulation/Simulation.h"

#include <cstdint>
#include <utility>
#include <vector>

namespace TLC
{
    //-------------------------------------------------------------------------
    // Arcs (0.6.0 Stone 5) — the stories that happen because life happens.
    // Two arcs live here, pure and tested; the rest are sketched in
    // Life.md. Each arc reads the world's own state — bonds, gossip,
    // deaths — and steers the sim's numbers, so a story is never a
    // script: it is the behaviour the settlement already earned.
    //
    //   The Feud  — a pair at Enemy, heard of by the settlement, draws a
    //               mediator; a well-liked mediator cools the feud.
    //   The Grief — a loved death drains the survivor's social need:
    //               they seek company.
    //-------------------------------------------------------------------------
    namespace Arcs
    {
        using namespace LCE::Simulation;

        //-------------------------------------------------------------------------
        // Grieving — is this mind grieving? It remembers a recent death
        // (a Death memory still heavy) of someone it loved (a
        // relationship at or above the friend line). The grief is
        // derived from persisted components — the death memory and the
        // relationship — so it needs no record of its own and survives
        // save/load for free.
        //-------------------------------------------------------------------------
        [[nodiscard]]
        inline bool Grieving(
            const EntityRegistry& a_registry,
            EntityId a_mind,
            std::uint64_t a_day,
            float a_freshWeight = 0.5f)
        {
            const auto memory = a_registry.GetComponent<Memory>(a_mind);
            const auto relationships =
                a_registry.GetComponent<Relationships>(a_mind);

            if (!memory || !relationships)
            {
                return false;
            }

            for (const auto& event : memory->Events)
            {
                if (event.Kind != InteractionKind::Death
                    || event.Weight < a_freshWeight
                    || !event.Other.IsValid())
                {
                    continue;
                }

                const auto it = relationships->ByEntity.find(event.Other);

                if (it != relationships->ByEntity.end()
                    && it->second.Disposition
                        >= Bonds::BondThresholds{}.Friend)
                {
                    return true;   // loved the dead — grief
                }
            }

            return false;
        }

        //-------------------------------------------------------------------------
        // ApplyGrief — one tick of the grief arc: every grieving mind's
        // Social need decays at an extra a_rate × a_delta (they seek
        // company; the need empties and the core's Decide answers with
        // Socialize). Returns the (mind, dead) pairs whose grief just
        // became *fresh* (weight high enough to announce) — the adapter
        // logs the line once per bereavement, no bookkeeping needed
        // (the memory fades below fresh and the line stops on its own).
        //-------------------------------------------------------------------------
        inline std::vector<std::pair<EntityId, EntityId>> ApplyGrief(
            EntityRegistry& a_registry,
            std::uint64_t a_day,
            float a_rate,
            float a_delta,
            float a_freshWeight = 0.9f)
        {
            std::vector<std::pair<EntityId, EntityId>> fresh;

            a_registry.ForEachWithComponent<Needs>(
                [&](EntityId a_entity, Needs& a_needs)
                {
                    const auto memory =
                        a_registry.GetComponent<Memory>(a_entity);
                    const auto relationships =
                        a_registry.GetComponent<Relationships>(a_entity);

                    if (!memory || !relationships)
                    {
                        return;
                    }

                    for (const auto& event : memory->Events)
                    {
                        if (event.Kind != InteractionKind::Death
                            || !event.Other.IsValid())
                        {
                            continue;
                        }

                        const auto it =
                            relationships->ByEntity.find(event.Other);

                        if (it == relationships->ByEntity.end()
                            || it->second.Disposition
                                < Bonds::BondThresholds{}.Friend)
                        {
                            continue;
                        }

                        if (event.Weight >= a_freshWeight)
                        {
                            fresh.emplace_back(a_entity, event.Other);
                        }

                        for (auto& need : a_needs.List)
                        {
                            if (need.Type == NeedType::Social)
                            {
                                need.Value -= a_rate * a_delta;
                                break;
                            }
                        }

                        break;   // one bereavement per mind per pass
                    }
                });

            return fresh;
        }

        //-------------------------------------------------------------------------
        // A mediation attempt's outcome — who tried, who feuded, and
        // whether the settlement's pull worked.
        //-------------------------------------------------------------------------
        struct Mediation
        {
            EntityId Mediator{};
            EntityId EnemyA{};
            EntityId EnemyB{};
            bool Cooled = false;
        };

        //-------------------------------------------------------------------------
        // Mediate — one pass over the feud pairs: for each, find a
        // non-participant who has heard of both (the gossip stone made
        // the settlement know the feud) and let them try. A mediator who
        // is liked by both sides cools the feud — the pair's mutual
        // disposition climbs toward 0 (the bond dissolves halfway back)
        // — and earns their trust; a mediator nobody likes is told off.
        // Returns every attempt, in order, for the log. The adapter calls
        // this once per day (a pair is mediated at most daily).
        //-------------------------------------------------------------------------
        inline std::vector<Mediation> Mediate(
            EntityRegistry& a_registry,
            const std::vector<std::pair<EntityId, EntityId>>& a_enemyPairs,
            const Rng* a_rng = nullptr)
        {
            std::vector<Mediation> attempts;

            const auto knows = [&a_registry](
                                   EntityId a_mind, EntityId a_subject)
            {
                const auto memory = a_registry.GetComponent<Memory>(a_mind);

                if (!memory)
                {
                    return false;
                }

                for (const auto& event : memory->Events)
                {
                    if (event.Kind == InteractionKind::Social
                        && event.Other == a_subject)
                    {
                        return true;
                    }
                }

                return false;
            };

            const auto disposition = [&a_registry](EntityId a_from,
                                       EntityId a_to)
            {
                const auto relationships =
                    a_registry.GetComponent<Relationships>(a_from);

                if (!relationships)
                {
                    return 0.0f;
                }

                const auto it = relationships->ByEntity.find(a_to);

                return it != relationships->ByEntity.end()
                    ? it->second.Disposition
                    : 0.0f;
            };

            for (const auto& [enemyA, enemyB] : a_enemyPairs)
            {
                if (!enemyA.IsValid() || !enemyB.IsValid())
                {
                    continue;
                }

                // The mediator: a third mind that has heard of both
                // sides. The settlement knows its own feuds — strangers
                // do not step in.
                EntityId mediator;
                float bestPull = -2.0f;

                a_registry.ForEachWithComponent<Memory>(
                    [&](EntityId a_entity, const Memory&)
                    {
                        if (a_entity == enemyA || a_entity == enemyB)
                        {
                            return;
                        }

                        if (!knows(a_entity, enemyA)
                            || !knows(a_entity, enemyB))
                        {
                            return;
                        }

                        // Pull: how liked the would-be mediator is by the
                        // pair. A stranger to both sides has no pull.
                        const auto pull = disposition(enemyA, a_entity)
                            + disposition(enemyB, a_entity);

                        if (pull > bestPull)
                        {
                            bestPull = pull;
                            mediator = a_entity;
                        }
                    });

                if (!mediator.IsValid())
                {
                    continue;   // nobody heard of this feud yet
                }

                Mediation attempt;
                attempt.Mediator = mediator;
                attempt.EnemyA = enemyA;
                attempt.EnemyB = enemyB;

                if (bestPull > 0.0f)
                {
                    // The feud cools: each side warms a step toward the
                    // other, and the mediator earns their trust.
                    auto relA = a_registry.GetComponent<Relationships>(enemyA);
                    auto relB = a_registry.GetComponent<Relationships>(enemyB);
                    auto relM = a_registry.GetComponent<Relationships>(mediator);

                    if (relA && relB)
                    {
                        relA->ByEntity[enemyB].Disposition =
                            std::min(0.0f, relA->ByEntity[enemyB].Disposition + 0.05f);
                        relB->ByEntity[enemyA].Disposition =
                            std::min(0.0f, relB->ByEntity[enemyA].Disposition + 0.05f);
                    }

                    if (relA)
                    {
                        relA->ByEntity[mediator].Trust += 0.1f;
                    }

                    if (relB)
                    {
                        relB->ByEntity[mediator].Trust += 0.1f;
                    }

                    if (relM)
                    {
                        relM->ByEntity[enemyA].Disposition += 0.05f;
                        relM->ByEntity[enemyB].Disposition += 0.05f;
                    }

                    attempt.Cooled = true;
                }
                else
                {
                    // Nobody likes the meddler — the feud holds and the
                    // pair cools toward the meddler.
                    auto relA = a_registry.GetComponent<Relationships>(enemyA);
                    auto relB = a_registry.GetComponent<Relationships>(enemyB);

                    if (relA)
                    {
                        relA->ByEntity[mediator].Disposition -= 0.05f;
                    }

                    if (relB)
                    {
                        relB->ByEntity[mediator].Disposition -= 0.05f;
                    }
                }

                attempts.push_back(attempt);
            }

            return attempts;
        }
    }
}
