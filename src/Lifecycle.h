//=============================================================================//
//                                                                             //
//   The Living Commonwealth — Fallout 4 adapter for the Living Commonwealth   //
//   Engine (LCE).                                                             //
//                                                                             //
//   Every mind has a chapter; the world keeps the book.                       //
//                                                                             //
//=============================================================================//

#pragma once

#include <cstdint>
#include <unordered_set>
#include <vector>

namespace TLC
{
    //-------------------------------------------------------------------------
    // Lifecycle — the world's bookkeeping (0.6.0 Stone 1, "The world
    // keeps its books"). Pure: given the known minds and a census of
    // loaded actors, classify what the world owes the sim — arrivals,
    // deaths, departures. No game types here; the edge read
    // (Adapter.cpp) fills the Scan from the game, and the adapter acts
    // on the Events. Testable without the game, like WorldFacts.
    //-------------------------------------------------------------------------
    namespace Lifecycle
    {
        // One loaded actor's census reading: who it is, whether it is
        // still a settler, whether it is dead. The game read happens at
        // the edge; this is the data the classifier reasons over.
        struct Scan
        {
            std::uint32_t FormId = 0;
            bool Relevant = false;   // still in the settler faction
            bool Dead = false;
        };

        enum class EventKind
        {
            Arrival,   // a new settler is loaded and relevant
            Death,     // a known mind's actor is dead
            Departure  // a known mind's actor is alive but left the faction
        };

        struct Event
        {
            EventKind Kind = EventKind::Arrival;
            std::uint32_t FormId = 0;
        };

        // The diff. a_known is the set of live minds' form ids (the
        // translator's minds — workshops are targets, never minds);
        // a_scans is this pass's loaded-actor census. Rules:
        //
        //   unknown + relevant + alive  -> Arrival
        //   unknown + dead              -> nothing (never a mind)
        //   known + dead                -> Death (a corpse is a death,
        //                                    not a departure)
        //   known + alive + irrelevant  -> Departure (left the faction)
        //
        // A form id is classified at most once per pass — the process
        // lists can hold an actor in more than one list, and the
        // second sighting must not double-book.
        inline std::vector<Event> Diff(
            const std::unordered_set<std::uint32_t>& a_known,
            const std::vector<Scan>& a_scans)
        {
            std::vector<Event> events;
            std::unordered_set<std::uint32_t> done;

            for (const auto& scan : a_scans)
            {
                if (!done.insert(scan.FormId).second)
                {
                    continue;
                }

                const bool known = a_known.contains(scan.FormId);

                if (!known)
                {
                    if (scan.Relevant && !scan.Dead)
                    {
                        events.push_back({ EventKind::Arrival, scan.FormId });
                    }

                    continue;
                }

                if (scan.Dead)
                {
                    events.push_back({ EventKind::Death, scan.FormId });
                }
                else if (!scan.Relevant)
                {
                    events.push_back({ EventKind::Departure, scan.FormId });
                }
            }

            return events;
        }
    }
}
