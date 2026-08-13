//=============================================================================//
//                                                                             //
//   The Living Commonwealth — Fallout 4 adapter for the Living Commonwealth   //
//   Engine (LCE).                                                             //
//                                                                             //
//   Health is the price of being in the wastes.                               //
//                                                                             //
//=============================================================================//

#pragma once

#include "Components.h"

#include <algorithm>
#include <cstdint>

namespace TLC
{
    //-------------------------------------------------------------------------
    // IllnessSettings — the tuning for the illness stone (0.8.0). Every
    // number is INI-tunable (sim.illness.*); the defaults here are the
    // living-world rhythm (death must be rare and earned).
    //-------------------------------------------------------------------------
    struct IllnessSettings
    {
        // The hold level: on contraction Health.Value drops to this and
        // holds while the illness runs. 0.4 = visibly sick, not dead.
        float Hold = 0.4f;

        // How much faster the sick tire: Fatigue decay × this while ill.
        // 2.0 = twice as fast — the visible cost is rest, not a stat.
        float FatigueMult = 2.0f;

        // Health per second once the hold ends (recovery starts).
        float Recovery = 0.05f;

        // The hold window, sim seconds. Long enough to feel like an
        // illness; short enough to watch a full cycle in a session.
        float Duration = 120.0f;

        // Contraction chances, per vector. Radstorms expose per radstorm
        // day; food and wounds roll at the event; contagion spreads from
        // the sick to their settlement at this rate per second.
        float RadstormChance = 0.3f;
        float FoodChance = 0.1f;
        float WoundChance = 0.15f;
        float ContagionChance = 0.05f;

        // Caps for a dose of medicine at the stall. 0 = no medicine
        // anywhere — a sick settlement suffers honestly.
        float MedicinePrice = 25.0f;

        // Severity growth per second while untreated. Severity drives
        // the fatigue toll's ease-off and, at the top, death.
        float SeverityRate = 0.01f;

        // Children are more fragile: their severity grows this many
        // times faster.
        float ChildMult = 2.0f;

        // Severity at or above this while untreated can end a mind.
        float DeathSeverity = 1.0f;

        // Health per second a severity-capped illness drains toward
        // death. The window between the cap and the end is the rescue
        // window — medicine or rest can still turn it around.
        float DeathDrain = 0.01f;
    };

    //-------------------------------------------------------------------------
    // Contract — the illness takes hold. Returns true when the mind fell
    // ill (false when already ill or the chance roll missed). The caller
    // decides the vector's chance (radstorm day, food, wound, contagion)
    // and supplies the roll — the Rng lives at the edge, this stays pure.
    // Severity is seeded by the vector (a wound is nastier than a chill).
    //-------------------------------------------------------------------------
    inline bool Contract(
        Health& a_health,
        SicknessKind a_kind,
        float a_severity,
        std::uint64_t a_day,
        const IllnessSettings& a_settings,
        float a_chance,
        float a_roll01)
    {
        if (a_health.Illness.Kind != SicknessKind::None)
        {
            return false;   // already ill — no stacking
        }

        if (a_roll01 >= a_chance)
        {
            return false;
        }

        a_health.Value = a_settings.Hold;
        a_health.Illness = Sickness{
            a_kind, a_severity, a_day, a_settings.Duration };

        return true;
    }

    //-------------------------------------------------------------------------
    // TickIllness — the hold-then-recover curve. Returns:
    //   0  — well (nothing was ill, or the illness finished and healed)
    //   1  — ill (holding or recovering, still sick)
    //   2  — died (severity capped and health drained to zero)
    // Pure: the adapter drives the delta and reads the return.
    //-------------------------------------------------------------------------
    inline int TickIllness(
        Health& a_health,
        float a_deltaSeconds,
        const IllnessSettings& a_settings,
        bool a_isChild)
    {
        if (a_health.Illness.Kind == SicknessKind::None)
        {
            return 0;   // well
        }

        // Severity grows while untreated — children faster.
        const auto mult = a_isChild ? a_settings.ChildMult : 1.0f;
        a_health.Illness.Severity +=
            a_settings.SeverityRate * mult * a_deltaSeconds;

        if (a_health.Illness.Remaining > 0.0f)
        {
            // Holding: the illness runs its course. The hold window
            // counts down; health holds at the reduced amount.
            a_health.Illness.Remaining -= a_deltaSeconds;

            // Untreated severity at the top can end the illness fatally:
            // health drains toward zero — the rescue window. Medicine
            // (or a low enough severity) still turns it around.
            if (a_health.Illness.Severity >= a_settings.DeathSeverity)
            {
                a_health.Value -= a_settings.DeathDrain * a_deltaSeconds;

                if (a_health.Value <= 0.0f)
                {
                    a_health.Value = 0.0f;
                    a_health.Illness = Sickness{};   // dead, not sick
                    return 2;
                }
            }

            return 1;
        }

        // Recovering: the hold ended (or medicine was taken) — health
        // climbs at the recovery rate until whole. The fatigue toll
        // eases off linearly as health returns.
        a_health.Value += a_settings.Recovery * a_deltaSeconds;

        if (a_health.Value >= 1.0f)
        {
            a_health.Value = 1.0f;
            a_health.Illness = Sickness{};
            return 0;
        }

        return 1;
    }

    //-------------------------------------------------------------------------
    // FatigueMultiplier — the visible cost of being sick. 1.0 when well;
    // FatigueMult while ill; easing off linearly toward 1.0 as health
    // recovers. The adapter scales the mind's Fatigue decay by this.
    //-------------------------------------------------------------------------
    inline float FatigueMultiplier(
        const Health& a_health, const IllnessSettings& a_settings)
    {
        if (a_health.Illness.Kind == SicknessKind::None)
        {
            return 1.0f;
        }

        // While holding: full toll. While recovering: ease off.
        const auto healthFraction =
            (a_health.Value - a_settings.Hold) / (1.0f - a_settings.Hold);

        const auto eased = std::clamp(
            1.0f - healthFraction, 0.0f, 1.0f);

        return 1.0f + (a_settings.FatigueMult - 1.0f) * eased;
    }

    //-------------------------------------------------------------------------
    // TakeMedicine — a dose ends the hold: recovery starts now (the
    // severity cap no longer drains toward death — the curve's Rescue
    // path). Returns true when the mind was actually ill (the dose was
    // used).
    //-------------------------------------------------------------------------
    inline bool TakeMedicine(
        Health& a_health, const IllnessSettings& a_settings)
    {
        if (a_health.Illness.Kind == SicknessKind::None)
        {
            return false;   // nothing to treat
        }

        a_health.Illness.Remaining = 0.0f;
        return true;
    }
}
