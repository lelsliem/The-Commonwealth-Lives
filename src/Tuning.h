//=============================================================================//
//                                                                             //
//   The Living Commonwealth — Fallout 4 adapter for the Living Commonwealth   //
//   Engine (LCE).                                                             //
//                                                                             //
//   Open at eight, closed at eight — unless you say otherwise.                //
//                                                                             //
//=============================================================================//

#pragma once

#include "WorldFacts.h"

#include "LCE/Config/Configuration.h"

#include <cctype>
#include <exception>
#include <string>
#include <string_view>

namespace TLC::Tuning
{
    //-------------------------------------------------------------------------
    // Tuning — the modder's knob (0.5.0). One text file next to the DLL,
    // `Data\F4SE\Plugins\TheLivingCommonwealth.ini`:
    //
    //     ; The Living Commonwealth tuning
    //     sim.memory.fade = 0.2        ; feeds the core (FromConfiguration)
    //     market.open.hour = 8         ; the adapter's own keys ride along
    //     market.close.hour = 20
    //
    // Unknown keys are ignored (the core's rule) so one file serves both
    // sides. Missing, empty, or unparsable values keep the default — a
    // broken line must never break the world. Pure here: parsing and the
    // settings read are free of the game; only the file IO is the edge's.
    //-------------------------------------------------------------------------

    //-------------------------------------------------------------------------
    // ParseConfig
    //
    // Parses the file text into the core's Configuration service. One
    // `key = value` per line; blank lines and lines starting with `;` or
    // `#` are skipped; surrounding whitespace is trimmed. A malformed
    // line (no `=`) is skipped, not fatal.
    //-------------------------------------------------------------------------
    inline LCE::Config::Configuration ParseConfig(std::string_view a_text)
    {
        LCE::Config::Configuration config;

        std::size_t start = 0;

        while (start <= a_text.size())
        {
            auto end = a_text.find('\n', start);

            if (end == std::string_view::npos)
            {
                end = a_text.size();
            }

            auto line = a_text.substr(start, end - start);

            if (!line.empty() && line.back() == '\r')
            {
                line.remove_suffix(1);
            }

            const auto trim = [](std::string_view s)
            {
                while (!s.empty() && std::isspace(
                    static_cast<unsigned char>(s.front())))
                {
                    s.remove_prefix(1);
                }

                while (!s.empty() && std::isspace(
                    static_cast<unsigned char>(s.back())))
                {
                    s.remove_suffix(1);
                }

                return s;
            };

            line = trim(line);

            if (!line.empty() && line.front() != ';' && line.front() != '#')
            {
                const auto equals = line.find('=');

                if (equals != std::string_view::npos)
                {
                    const auto key = trim(line.substr(0, equals));
                    const auto value = trim(line.substr(equals + 1));

                    if (!key.empty())
                    {
                        config.Set(key, value);
                    }
                }
            }

            if (end == a_text.size())
            {
                break;
            }

            start = end + 1;
        }

        return config;
    }

    //-------------------------------------------------------------------------
    // AdapterSettings
    //
    // The adapter's own keys — the ones the core's FromConfiguration does
    // not know. Defaults are the WorldFacts constants; the config file
    // overrides them.
    //-------------------------------------------------------------------------
    struct AdapterSettings
    {
        float MarketOpenHour = WorldFacts::kMarketOpenHour;
        float MarketCloseHour = WorldFacts::kMarketCloseHour;
    };

    inline AdapterSettings AdapterSettingsFrom(
        const LCE::Config::Configuration& a_config)
    {
        AdapterSettings settings;

        const auto read = [&a_config](std::string_view key, float fallback)
        {
            const auto raw = a_config.Get(key);

            if (raw.empty())
            {
                return fallback;
            }

            try
            {
                return std::stof(std::string(raw));
            }
            catch (const std::exception&)
            {
                return fallback;
            }
        };

        settings.MarketOpenHour =
            read("market.open.hour", settings.MarketOpenHour);
        settings.MarketCloseHour =
            read("market.close.hour", settings.MarketCloseHour);

        return settings;
    }
}
