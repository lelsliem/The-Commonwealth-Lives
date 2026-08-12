//=============================================================================//
//                                                                             //
//   The Living Commonwealth — Fallout 4 adapter for the Living Commonwealth   //
//   Engine (LCE).                                                             //
//                                                                             //
//   Show your WorkshopNPCFaction card at the door.                                      //
//                                                                             //
//=============================================================================//

#include "SimRelevant.h"

// CommonLibF4's headers rely on its PCH for standard headers (concepts,
// type_traits, ...) — include it first, as the library's own sources do.
#include <F4SE/Impl/PCH.h>

#include <RE/A/Actor.h>
#include <RE/P/PlayerCharacter.h>
#include <RE/T/TESFaction.h>
#include <RE/T/TESForm.h>
#include <RE/T/TESFormUtil.h>   // the header-only definition of TESForm::As<T>
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

        if (faction == nullptr || !a_actor->IsInFaction(faction))
        {
            return false;
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
