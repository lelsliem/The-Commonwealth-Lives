//=============================================================================//
//                                                                             //
//   The Living Commonwealth — Fallout 4 adapter for the Living Commonwealth   //
//   Engine (LCE).                                                             //
//                                                                             //
//   QUOTE: <the author's line goes here>                                      //
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

namespace TLC
{
    const RE::TESFaction* SettlerFaction()
    {
        static const RE::TESFaction* faction =
            RE::TESForm::GetFormByID<RE::TESFaction>(kSettlerFactionFormId);

        return faction;
    }

    bool IsSimRelevant(const RE::Actor* a_actor)
    {
        if (!a_actor || a_actor == RE::PlayerCharacter::GetSingleton())
        {
            return false;
        }

        const auto* faction = SettlerFaction();

        return faction != nullptr && a_actor->IsInFaction(faction);
    }
}
