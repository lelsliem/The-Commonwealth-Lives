//=============================================================================//
//                                                                             //
//   The Living Commonwealth — Fallout 4 adapter for the Living Commonwealth   //
//   Engine (LCE).                                                             //
//                                                                             //
//   Show your WorkshopNPCFaction card at the door.                                      //
//                                                                             //
//=============================================================================//

#include "SimRelevant.h"

#include "Names.h"   // the role words — the road's people carry them

// CommonLibF4's headers rely on its PCH for standard headers (concepts,
// type_traits, ...) — include it first, as the library's own sources do.
#include <F4SE/Impl/PCH.h>

#include <RE/A/Actor.h>
#include <RE/P/PlayerCharacter.h>
#include <RE/T/TESFaction.h>
#include <RE/T/TESForm.h>
#include <RE/T/TESFormUtil.h>   // the header-only definition of TESForm::As<T>
#include <RE/T/TESFullName.h>
#include <RE/T/TESNPC.h>
#include <RE/T/TESRace.h>

namespace TLC
{
    const RE::TESFaction* SettlerFaction()
    {
        static const RE::TESFaction* faction =
            RE::TESForm::GetFormByID<RE::TESFaction>(kSettlerFactionFormId);

        return faction;
    }

    //-------------------------------------------------------------------------
    // The workshop's props are not minds (0.7.2 fix): the game hands the
    // settler faction to anything a settlement can place that counts as
    // population — turrets, spotlights — so the faction gate alone lets
    // them into the sim, where they get needs, walk to market, and feud.
    // Robots are deliberately outside the sim too (the design's note in
    // ClassifySpecies: their own species is a later stone; they have no
    // biological needs to seed). Race FormIDs verified in xEdit from
    // Fallout4.esm (2026-08-10 for the animal table, the devices checked
    // 2026-08-12 when Deacon was found feuding with four missile turrets).
    //-------------------------------------------------------------------------
    bool IsDeviceRace(const RE::TESRace* a_race)
    {
        if (a_race == nullptr)
        {
            return false;
        }

        switch (a_race->GetFormID())
        {
        // Robots — no biological needs to seed.
        case 0x000359F4:   // HandyRace
        case 0x000AE0B7:   // SentryBotRace
        case 0x000D8417:   // AssaultronRace
        case 0x000DFB33:   // ProtectronRace
        case 0x00108019:   // LibertyPrimeRace
        // Turrets — workshop defense, counted as population by the game.
        case 0x000B1F08:   // TurretTripodRace
        case 0x000B28D3:   // TurretBubbleRace
        case 0x001A6D64:   // TurretWorkshopRace
        // The wall-mounted spotlight's race (DLCRobot.esm / Automatron,
        // record 0x2804) — the diagnostic named it 2026-08-12 when the
        // spotlight survived the prune and kept feuding with Deacon.
        case 0x01002804:
            return true;

        default:
            return false;
        }
    }

    bool IsSimRelevant(const RE::Actor* a_actor)
    {
        if (!a_actor || a_actor == RE::PlayerCharacter::GetSingleton())
        {
            return false;
        }

        const auto* faction = SettlerFaction();
        const bool inFaction =
            faction != nullptr && a_actor->IsInFaction(faction);

        // The road's people are minds too (0.7.3 verification): the
        // game's generic "Provisioner" and "Caravan Guard" NPCs roam
        // the Commonwealth with the brahmin caravans and hold no
        // settler faction — yet a hungry settler trades with exactly
        // these people on the road. A base-form name that is a known
        // role word passes the gate on its own. The name read only
        // runs for actors the faction already rejected, so the common
        // case stays a single faction test.
        if (!inFaction)
        {
            const auto* base = a_actor->GetObjectReference();

            if (base == nullptr
                || TLC::Names::IsRoleName(
                    RE::TESFullName::GetFullName(*base))
                    .empty())
            {
                return false;
            }
        }

        // The props hold the faction too, but they are not minds.
        // Exclude device/robot races outright, and anything with no NPC
        // base record (the defensive catch — a placed object with an AI
        // process that is not built on a person).
        if (IsDeviceRace(a_actor->race))
        {
            return false;
        }

        return a_actor->GetNPC() != nullptr;
    }
}
