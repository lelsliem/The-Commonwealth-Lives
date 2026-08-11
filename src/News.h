//=============================================================================//
//                                                                             //
//   The Living Commonwealth — Fallout 4 adapter for the Living Commonwealth   //
//   Engine (LCE).                                                             //
//                                                                             //
//   Extra! Extra! The Commonwealth lives!                                     //
//                                                                             //
//=============================================================================//

#pragma once

#include <cstddef>
#include <deque>
#include <string>

namespace TLC::News
{
    //-------------------------------------------------------------------------
    // NewsFeed — the world's paper (0.7.0 Stone 3 — the player window).
    // World events — bonds, feuds, births, deaths, the market's hours —
    // become one-line news: "Marcy and Jun became friends", "a child was
    // born to the Vances". The feed is capped (the settlement's memory of
    // its own story is a window, not an archive) and rotated: the radio
    // reads it as captions, newest last, wrapping when it runs out. Pure
    // and testable — no game types.
    //-------------------------------------------------------------------------
    struct NewsFeed
    {
        std::deque<std::string> Lines;
        std::size_t Cap = 64;

        // The radio's reading position. Advances on NextLine, wraps at
        // the end — the settlement tells its story in a loop.
        std::size_t Cursor = 0;

        void Add(std::string a_line)
        {
            if (a_line.empty())
            {
                return;
            }

            if (Lines.size() >= Cap)
            {
                Lines.pop_front();

                if (Cursor != 0)
                {
                    --Cursor;
                }
            }

            Lines.push_back(std::move(a_line));
        }

        // The next caption, in the order the news happened; wraps to the
        // oldest when the feed is exhausted. Empty feed → empty line.
        [[nodiscard]] std::string NextLine()
        {
            if (Lines.empty())
            {
                return {};
            }

            const auto line = Lines[Cursor];
            Cursor = (Cursor + 1) % Lines.size();

            return line;
        }

        void Clear() noexcept
        {
            Lines.clear();
            Cursor = 0;
        }
    };
}
