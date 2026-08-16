//=============================================================================//
//                                                                             //
//   The Living Commonwealth — Fallout 4 adapter for the Living Commonwealth   //
//   Engine (LCE).                                                             //
//                                                                             //
//   The game acts, the sim thinks, the adapter translates.                                      //
//                                                                             //
//=============================================================================//

#include "Adapter.h"

#include "Arcs.h"
#include "Behaviour.h"
#include "Birth.h"
#include "Components.h"
#include "Gossip.h"
#include "Fights.h"
#include "Rows.h"
#include "Market.h"
#include "Tuning.h"
#include "WorldFacts.h"
#include "Movement.h"
#include "Serialization.h"
#include "SimRelevant.h"

// CommonLibF4's headers rely on its PCH for standard headers (concepts,
// type_traits, ...) — include it first, as the library's own sources do.
#include <F4SE/Impl/PCH.h>

#include <RE/A/AIProcess.h>   // the game's knockback — the punch's shove
#include <RE/A/Actor.h>
#include <RE/B/BSContainer.h>
#include <RE/B/BSScript_IStackCallbackFunctor.h>   // the audio probe — Say's null callback
#include <RE/B/BSScriptUtil.h>   // BSScript::GetVMTypeID — the OwnedByPlayer ownership read
#include <RE/C/Console.h>        // the holding flavor — PRID + ChangeAnimFlavor, the game's own parser
#include <RE/D/DamageImpactData.h>
#include <RE/D/DIALOGUE_SUBTYPE.h>   // the audio probe — ProcessGreet's greeting sub-type
#include <RE/D/DIALOGUE_TYPE.h>      // the audio probe — ProcessGreet's dialogue type
#include <RE/D/DialogueItem.h>       // the audio probe — greetTopic's info read-back
#include <RE/H/HitData.h>      // the melee hit-reaction — the standing shove
#include <RE/Fallout.h>              // the audio probe — HighProcessData (nextGreeting seed); the fork's umbrella includes it with its transitive deps in order
#include <RE/K/KNOCK_STATE_ENUM.h>   // the standing guard — a beat never fires into a down actor
#include <RE/T/TESIdleForm.h>   // the paired push — the game's real shove animation
#include <RE/S/STAGGER_MAGNITUDE.h>
#include <RE/C/Calendar.h>
#include <RE/G/GameScript.h>   // GameVM — the OwnedByPlayer ownership read
#include <RE/N/NiAVObject.h>
#include <RE/P/PlayerCharacter.h>
#include <RE/P/ProcessLists.h>
#include <RE/S/SendHUDMessage.h>
#include <RE/S/Setting.h>          // GameSettingCollection — the Realistic Conversations compatibility applies GMST overrides to the game's own settings
#include <RE/S/Sky.h>
#include <RE/T/TESDataHandler.h>
#include <RE/T/TESFullName.h>
#include <RE/T/TESTopicInfo.h>   // the audio probe — INFO -> parentTopic -> Say
#include <RE/T/TESForm.h>
#include <RE/T/TESObjectCELL.h>
#include <RE/T/TESWorldSpace.h>
#include <RE/T/TESFormUtil.h>   // the header-only definition of TESForm::As<T>
#include <RE/T/TESObjectREFR.h>
#include <RE/B/BGSOutfit.h>   // the child's clothes — the ChildOutfit* records are OTFT bundles, not ARMOs
#include <RE/T/TESObjectARMO.h>   // the ARMOs inside those bundles — the runtime dress fallback
#include <RE/T/TESNPC.h>   // the child base — defOutfit (WNAM) is the game's own child-dressing path
#include <RE/T/TESRace.h>
#include <RE/T/TESWeather.h>

#include <LCE/Logging/Logger.h>

#include "LCE/Simulation/Decision/Behaviour.h"
#include "LCE/Simulation/Society/Groups.h"
#include "LCE/Simulation/Decision/Legacy.h"
#include "LCE/Simulation/Mind/Memory.h"
#include "LCE/Simulation/Mind/Relationships.h"
#include "LCE/Simulation/Simulation.h"

#include <REX/LOG.h>

#include <algorithm>
#include <cctype>   // std::tolower — the AVIF candidate dump
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <typeindex>
#include <typeinfo>

// Windows.h LAST: its macros (min/max, MEM_*, ...) collide with
// commonlibf4's tokens (REX::W32::MEM_RELEASE, std::numeric_limits::max)
// if it is included before them. NOMINMAX alone is not enough — the
// MEM_* collisions are the reason for the ordering.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>   // GetModuleHandleExW/GetModuleFileNameW (config path)

namespace TLC
{
    namespace
    {
        //-------------------------------------------------------------------------
        // The standing shove (ADR-0042: the punch's flinch). KnockExplosion
        // — the game's knock-over — collapses the victim in place with no
        // visible push: the loop tests proved that force alone, at any
        // magnitude, never reads as a shove. The game's actual shove is the
        // melee hit-reaction: when a punch lands, the victim staggers back
        // (Actor::DoHitMe, the same path a bare-fisted brawl uses). Play
        // that first — a zero-damage HitData carrying a stagger magnitude
        // and a modest push-back — so the victim stumbles, and then the
        // knock fall reads as the punch's consequence. Best-effort: a
        // missing actor or process skips it (the fight is booked either
        // way); sim.fight.stagger = 0 turns it off (fall only).
        //-------------------------------------------------------------------------
        void FireHitReaction(
            RE::Actor* a_victim,
            RE::Actor* a_aggressor,
            int a_magnitude,
            float a_pushBack)
        {
            if (a_victim == nullptr || a_aggressor == nullptr
                || a_magnitude <= 0)
            {
                return;
            }

            // HitData has no default constructor (its weapon member
            // needs a real weapon), so it is built on zeroed raw
            // storage — the same shape the game's explosion-stagger
            // path uses: weapon null, zero damage, a stagger magnitude
            // and a push-back. The melee hit-reaction reads the
            // stagger and plays the standing flinch; nothing is ever
            // flagged as an attack (no combat escalation, no damage —
            // the scuffle stays a scuffle). DoHitMe is synchronous;
            // the block is freed right after.
            auto* hit = static_cast<RE::HitData*>(
                std::malloc(sizeof(RE::HitData)));
            std::memset(hit, 0, sizeof(RE::HitData));
            hit->aggressor =
                RE::BSPointerHandleManagerInterface<RE::Actor>::GetHandle(
                    a_aggressor);
            hit->target =
                RE::BSPointerHandleManagerInterface<RE::Actor>::GetHandle(
                    a_victim);
            hit->sourceRef =
                RE::BSPointerHandleManagerInterface<
                    RE::TESObjectREFR>::GetHandle(a_aggressor);
            hit->stagger =
                static_cast<RE::STAGGER_MAGNITUDE>(a_magnitude);
            hit->pushBack = a_pushBack;

            a_victim->DoHitMe(hit);
            std::free(hit);
        }

        //-------------------------------------------------------------------------
        // The paired shove (ADR-0045): the game's real push animation.
        // The crowd mod's visible push is the vanilla paired idles —
        // PairedFrontPushKick (0x47FC3) on the attacker, its _Human
        // half (0x6571F) on the victim — both on the game's
        // MeleeBehavior.hkx paired-attack sync (the same records the
        // game's own shove/kill-move system plays). The crowd mod's
        // ESP ships no animation files; it merely re-exposes these
        // records unconditionally. We play the vanilla pairs directly:
        // PlayIdle with the other actor as target, and the engine
        // aligns and syncs the two. Best-effort: a missing form, a
        // missing process, or a failed record condition returns false
        // and the caller falls back to the melee flinch — the fight is
        // booked either way (never crash, never teleport).
        //-------------------------------------------------------------------------
        bool FirePairedPush(
            RE::Actor* a_attacker,
            RE::Actor* a_victim)
        {
            if (a_attacker == nullptr || a_victim == nullptr
                || a_attacker->currentProcess == nullptr
                || a_victim->currentProcess == nullptr)
            {
                return false;
            }

            // The kick (0.7.6, ADR-0051): our own unconditional copy of
            // the vanilla paired kick — the crowd mod's decoded recipe
            // (MeleeBehavior.hkx + dyn_Activation + the vanilla
            // PairedFrontPushKick_AttackerLead animation, no CTDA
            // conditions). The vanilla PairedFrontPushKick records carry
            // conditions and RaiderRootBehavior, so PlayIdle refuses them
            // outside combat — that is why every receipt read "flinch
            // fallback" and the "kick" was really the flinch. The ESP
            // ships as data/TheLivingCommonwealthAnims.esp; LookupFormID
            // resolves it load-order independent, and a missing plugin
            // falls through to the vanilla pair, then the flinch.
            auto* kick = []() -> RE::TESIdleForm*
            {
                static RE::TESIdleForm* resolved = nullptr;
                static bool tried = false;

                if (!tried)
                {
                    tried = true;

                    if (auto* handler =
                            RE::TESDataHandler::GetSingleton();
                        handler != nullptr)
                    {
                        const auto formId = handler->LookupFormID(
                            0x00000800, "TheLivingCommonwealthAnims.esp");

                        if (formId != 0)
                        {
                            resolved =
                                RE::TESForm::GetFormByID<RE::TESIdleForm>(
                                    formId);
                        }
                    }
                }

                return resolved;
            }();

            if (kick != nullptr)
            {
                const auto played = a_attacker->currentProcess->PlayIdle(
                    *a_attacker, kick, a_victim);

                if (played)
                {
                    return true;
                }
            }

            // Fallback: the vanilla paired halves, cross-targeted — the
            // engine's paired-attack sync. Usually refused outside
            // combat (the CTDA conditions); kept so the old path still
            // exists when our ESP is absent.
            auto* lead = RE::TESForm::GetFormByID<RE::TESIdleForm>(
                0x00047FC3);   // PairedFrontPushKick — the shove's lead
            auto* human = RE::TESForm::GetFormByID<RE::TESIdleForm>(
                0x0006571F);   // PairedFrontPushKick_Human — the victim's half

            if (lead == nullptr || human == nullptr)
            {
                return false;
            }

            const auto leadPlayed = a_attacker->currentProcess->PlayIdle(
                *a_attacker, lead, a_victim);
            const auto humanPlayed = a_victim->currentProcess->PlayIdle(
                *a_victim, human, a_attacker);

            return leadPlayed && humanPlayed;
        }

        //-------------------------------------------------------------------------
        // The standing guard (0.7.6, ADR-0050): an actor mid-knock —
        // exploding, out, down, getting up, or with a knock queued — is
        // not available for the next beat. The both-fall look happened
        // because a beat could fire while the other actor was still on
        // the ground, and a fall could re-fire into a victim already
        // down (the ground-slide). A beat that finds its actor down
        // waits a beat and re-checks instead of firing into the state.
        //-------------------------------------------------------------------------
        bool IsDown(RE::Actor* a_actor)
        {
            return a_actor != nullptr
                && static_cast<RE::KNOCK_STATE_ENUM>(a_actor->knockState)
                    != RE::KNOCK_STATE_ENUM::kNormal;
        }

        //-------------------------------------------------------------------------
        // Species classification (ADR-0024: game knowledge at the edge). The
        // core never knows a race; the adapter decides which minds trade,
        // which are fed, and which are children. Race FormIDs verified in
        // xEdit 2026-08-10 from Fallout4.esm. Anything outside the lists
        // defaults to Human — a workshop population is usually people, and a
        // misclassified mind only behaves human until the table grows
        // (Behaviour.h).
        //
        // Enemies (feral ghouls, super mutants, ...) never reach this
        // table: sim-relevance is WorkshopNPCFaction membership, and
        // hostiles do not hold it. The wild-animal entries below are
        // future-proofing — if a mod ever makes one a settler, it is fed,
        // not trading. Robots and synths are deliberately absent: a synth
        // settler is a person (Human is right); a robot is its own species
        // for a later stone (no biological needs to seed).
        //-------------------------------------------------------------------------
        Species ClassifySpecies(const RE::TESRace* a_race)
        {
            if (a_race == nullptr)
            {
                return Species::Human;
            }

            switch (a_race->GetFormID())
            {
            // Children — they play and talk; they don't run stalls.
            case 0x0011D83F:   // HumanChildRace
            case 0x0011EB96:   // GhoulChildRace
                return Species::Child;

            // Animals — fed at the settlement, never bartering.
            case 0x0001D698:   // DogmeatRace (junkyard dog)
            case 0x0001D810:   // MoleratRace
            case 0x0001DB4A:   // DeathclawRace
            case 0x0002047E:   // BrahminRace (pack brahmin)
            case 0x00023FFC:   // MirelurkRace
            case 0x0002456D:   // BloodbugRace
            case 0x00029463:   // BloatflyRace
            case 0x0003578A:   // ViciousDogRace
            case 0x0004716C:   // RadRoachRace
            case 0x0005FBB1:   // StingwingRace
            case 0x000636AB:   // RadScorpionRace
            case 0x00064C60:   // MirelurkHunterRace
            case 0x0006B4EC:   // FeralGhoulRace
            case 0x0007ED1D:   // RadStagRace
            case 0x00090C33:   // FEVHoundRace
            case 0x000A0F2F:   // YaoGuaiRace
            case 0x000A96BF:   // FeralGhoulGlowingRace
            case 0x000BB7D9:   // SupermutantBehemothRace — an animal,
                               // not a person: fed at the settlement,
                               // never bartering, feuding, or fighting
                               // (the 0.7.5 field find — the missing
                               // race let behemoths default to Human
                               // and brawl at the market)
            case 0x000B7F91:   // MirelurkKingRace
            case 0x000C9ACF:   // CatRace
            case 0x000D77E3:   // VertibirdRace
            case 0x000D9804:   // GorillaRace (settlement gorillas)
            case 0x000E12A6:   // MirelurkQueenRace
            case 0x00187AF9:   // RaiderDogRace
                return Species::Animal;

            // Robots — machines, not bodies (0.8.6b field find: Buddy
            // the Mr. Handy seeded as Human and fell ill). A robot is a
            // mind that talks but has no biological needs: never
            // hungry, never tired, never sick, never at the market.
            case 0x000359F4:   // HandyRace (Buddy, Codsworth)
            case 0x000DFB33:   // ProtectronRace
            case 0x000AE0B7:   // SentryBotRace
            case 0x000D8417:   // AssaultronRace
            case 0x000A563A:   // EyeBotRace
                return Species::Robot;

            default:
                break;
            }

            return Species::Human;
        }

        // The species table's complement: every race a person can be.
        // ClassifySpecies defaults anything unknown to Human (a workshop
        // population is usually people — ADR-0024), and the device table
        // in SimRelevant.cpp catches what it knows. A Human-classified
        // mind whose race is neither here nor a known device is either a
        // modded race (fine — default Human is the point) or a prop that
        // slipped the table; the prune announces it once so the table can
        // grow. Races verified in xEdit 2026-08-10 from Fallout4.esm.
        bool IsKnownOrganicRace(const RE::TESRace* a_race)
        {
            if (a_race == nullptr)
            {
                return false;
            }

            switch (a_race->GetFormID())
            {
            // People, in every form the sim seeds as Human.
            case 0x00013746:   // HumanRace
            case 0x000EAFB6:   // GhoulRace
            case 0x0001A009:   // SuperMutantRace
            case 0x0001D31E:   // PowerArmorRace (an armored settler)
            case 0x000E8D09:   // SynthGen1Race
            case 0x0010BD65:   // SynthGen2Race
            case 0x002261A4:   // SynthGen2RaceValentine
            // Children and the animal table (species-tagged, never
            // classified Human — listed so the complement is total).
            case 0x0011D83F:   // HumanChildRace
            case 0x0011EB96:   // GhoulChildRace
            case 0x0001D698:   // DogmeatRace
            case 0x0001D810:   // MoleratRace
            case 0x0001DB4A:   // DeathclawRace
            case 0x0002047E:   // BrahminRace
            case 0x00023FFC:   // MirelurkRace
            case 0x0002456D:   // BloodbugRace
            case 0x00029463:   // BloatflyRace
            case 0x0003578A:   // ViciousDogRace
            case 0x0004716C:   // RadRoachRace
            case 0x0005FBB1:   // StingwingRace
            case 0x000636AB:   // RadScorpionRace
            case 0x00064C60:   // MirelurkHunterRace
            case 0x0006B4EC:   // FeralGhoulRace
            case 0x0007ED1D:   // RadStagRace
            case 0x00090C33:   // FEVHoundRace
            case 0x000A0F2F:   // YaoGuaiRace
            case 0x000A96BF:   // FeralGhoulGlowingRace
            case 0x000BB7D9:   // SupermutantBehemothRace — an animal,
                               // not a person: fed at the settlement,
                               // never bartering, feuding, or fighting
                               // (the 0.7.5 field find — the missing
                               // race let behemoths default to Human
                               // and brawl at the market)
            case 0x000B7F91:   // MirelurkKingRace
            case 0x000C9ACF:   // CatRace
            case 0x000D77E3:   // VertibirdRace
            case 0x000D9804:   // GorillaRace
            case 0x000E12A6:   // MirelurkQueenRace
            case 0x00187AF9:   // RaiderDogRace
                return true;

            default:
                return false;
            }
        }

        // The world's voice for a mind — the same labels the arrival
        // logging uses, read from the mind's tag (a missing tag reads as
        // a settler, the workshop default).
        const char* SpeciesLabel(
            const SpeciesTag* a_tag)
        {
            if (a_tag == nullptr)
            {
                return "settler";
            }

            switch (a_tag->Value)
            {
            case Species::Child:
                return "child";
            case Species::Animal:
                return "animal";
            case Species::Robot:
                return "robot";
            default:
                return "settler";
            }
        }

        //-------------------------------------------------------------------------
        // The per-second census sweep (0.6.0 Stone 1). ForEachLoadedActor
        // visits every actor the game is simulating near the player — the
        // same four process lists the wake seed reads; the classification
        // (arrival / death / departure) is Lifecycle::Diff, pure.
        //-------------------------------------------------------------------------
        template <class Fn>
        void ForEachLoadedActor(Fn&& a_fn)
        {
            const auto* processLists = RE::ProcessLists::GetSingleton();

            if (!processLists)
            {
                return;
            }

            for (const auto* list : processLists->allProcesss)
            {
                if (!list)
                {
                    continue;
                }

                for (const auto& handle : *list)
                {
                    // handle.get() is a NiPointer<Actor>; .get() is the
                    // raw pointer the callers want.
                    auto* actor = handle.get().get();

                    if (actor == nullptr || actor->IsPlayerRef())
                    {
                        continue;
                    }

                    a_fn(actor);
                }
            }
        }

        // IsActorDead — the game's own death markers, read only on a
        // fully streamed-in actor. A killer handle is set the moment an
        // actor is killed, a corpse-cleanup timer runs while the body
        // awaits cleanup, and a deleted ref is removed from the world;
        // any of the three means the mind is gone. The 3D gate matters:
        // after a load, actors enter the process lists BEFORE their
        // members are initialized — reading myKiller / the corpse timer /
        // the form flags on a partially-initialized actor is garbage, and
        // in-game that read 11 false deaths in one frame, 3s after a
        // 665-mind restore (2026-08-10). A fully-loaded actor has a 3D
        // node, and a corpse keeps it — so the raw reads only happen once
        // the actor is really there.
        inline bool IsActorDead(RE::Actor* a_actor)
        {
            if (a_actor == nullptr)
            {
                return true;
            }

            // Not fully streamed in yet — treat as alive; the census
            // re-reads next second, when the members are real.
            if (a_actor->Get3D() == nullptr)
            {
                return false;
            }

            return a_actor->IsDeleted()
                || a_actor->myKiller.get().get() != nullptr
                || a_actor->checkMyDeadBodyTimer > 0.0f;
        }

        //-------------------------------------------------------------------------
        // The executor's game answers (ADR-0024: every RE:: touch at the
        // edge). The pure plan builder asks "loaded? available?" — these
        // functions answer with the real game.
        //-------------------------------------------------------------------------
        const char* ActionName(LCE::Simulation::ActionType a_action)
        {
            using enum LCE::Simulation::ActionType;

            switch (a_action)
            {
            case MoveTo:
                return "MoveTo";
            case Rest:
                return "Rest";
            case Socialize:
                return "Socialize";
            case Explore:
                return "Explore";
            case Work:
                return "Work";
            case Flee:
                return "Flee";
            }

            return "?";
        }

        std::string FormatHex8(std::uint32_t a_value)
        {
            char buffer[9];
            std::snprintf(buffer, sizeof(buffer), "%08X", a_value);
            return buffer;
        }

        // A settler reaching the market counts as arrived. The game's own
        // command-mode arrival stop leaves walkers ~1 m short of the
        // marker (probes bottom out around 50 units ≈ 0.7 m), so the
        // radius must cover the stop distance — 200 units ≈ 2.8 m — not
        // hug the marker (4 units ≈ 6 cm never fired: walkers stood at
        // the bench outside a 6 cm circle).
        constexpr float kArrivalRadius = 200.0f;

        // (The walk cap itself is tuning, not a constant: sim.walk.cap
        // in the INI — see AdapterSettings::WalkCap. Arrival ends a
        // session and frees its slot immediately, so the default 16 is
        // generous; big saves can raise it without a rebuild.)

        // The entity's form, or null when the form is unknown.
        RE::TESForm* FormFor(const Translator& a_translator, LCE::Simulation::EntityId a_entity)
        {
            const auto formId = a_translator.FormFor(a_entity);
            return formId != 0 ? RE::TESForm::GetFormByID(formId) : nullptr;
        }

        bool IsActorLoaded(const Translator& a_translator, LCE::Simulation::EntityId a_entity)
        {
            const auto formId = a_translator.FormFor(a_entity);
            return formId != 0 && RE::TESForm::GetFormByID<RE::Actor>(formId) != nullptr;
        }

        bool IsTargetLoaded(const Translator& a_translator, LCE::Simulation::EntityId a_entity)
        {
            if (!a_entity.IsValid())
            {
                return false;
            }

            const auto formId = a_translator.FormFor(a_entity);
            return formId != 0 && RE::TESForm::GetFormByID<RE::TESObjectREFR>(formId) != nullptr;
        }

        //-------------------------------------------------------------------------
        // The config file's anchor: its own code address locates this
        // module (the DLL) so the INI is found next to it, regardless of
        // how F4SE loaded us. A free function — a member function pointer
        // cannot cast to LPCWSTR.
        void ModuleAnchor() {}

        //-------------------------------------------------------------------------
        // The game clock, read once at the edge (the world-facts stone).
        // Sky::currentGameHour is the running hour 0–24 the game itself
        // shows in the HUD clock. Formatted as HH:MM for the log lines
        // the player reads.
        //-------------------------------------------------------------------------
        float CurrentGameHour()
        {
            const auto* sky = RE::Sky::GetSingleton();
            return sky != nullptr ? sky->currentGameHour : 0.0f;
        }

        std::string FormatGameHour(float a_hour)
        {
            const auto whole = static_cast<int>(a_hour);
            const auto minutes = static_cast<int>((a_hour - static_cast<float>(whole)) * 60.0f);

            char buffer[8];
            std::snprintf(buffer, sizeof(buffer), "%02d:%02d", whole, minutes);
            return buffer;
        }

        // The visible child (0.8.9, the deferred-spawn find): when the
        // bundle comes off, a real child actor is spawned at the
        // mother's feet — but DELIBERATELY left un-initialized (no
        // clearStillLoadingFlag, no forced Load3D). A ref created like
        // this is invisible while the cell holds it; the game's own
        // save/load routine completes the actor — facegen, AI process,
        // animation — the next time the world reloads, and the child
        // steps out fully real (the 2026-08-16 field find: exactly this
        // happened; the invisible child "became real" on load, with
        // only the clothes missing). The alternative — forcing the
        // init immediately — produces the half-born headless, T-posing,
        // immobile child that even survives saves; and the game's own
        // PlaceAtMe dispatch is the documented PackVariables CTD. So:
        // deferred, real after a load. The per-tick pass dresses the
        // child the moment it reads fully initialized (3D + AI
        // process). Base and outfit form ids verified in Fallout4.esm
        // 2026-08-16 (child-race NPCs; ChildOutfit* OTFT bundles).
        //
        // The safe bases: the farm children — generic child NPCs with
        // no quest strings attached (the Vault 81 and Diamond City
        // school kids are quest actors; spawning copies of them risks
        // script interference).
        constexpr std::uint32_t kFemaleChildBases[] = {
            0x00156a23,   // Farm05FemaleChild
        };
        constexpr std::uint32_t kMaleChildBases[] = {
            0x00156a21,   // Farm05MaleChild
            0x00156a13,   // Farm02MaleChild
        };

        // The child's clothes — the game's own child-sized outfit
        // bundles. FIELD FIND (2026-08-16): these "ChildOutfit*"
        // records are OTFT bundles (BGSOutfit), NOT ARMOs — a cast to
        // TESBoundObject always fails, which is exactly why the first
        // dressing attempt silently equipped nothing while the log
        // still claimed "dressed and fully real". Each bundle carries
        // the real child ARMOs (ClothesKids01..04 on slot 0x8, plus a
        // hat on the RagsWithHat bundle). The game applies such a
        // bundle through the NPC base's default outfit (WNAM /
        // defOutfit) at actor init — the same path that dresses the
        // game's own children; the bundles are orphans (no NPC record
        // references them), so we reuse that mechanism. ChildNatOutfit
        // (ClothesNat) is Nat Wright's unique dress and is excluded;
        // the dress (ChildOutfitDress) is female-only.
        constexpr std::uint32_t kChildOutfitMale[] = {
            0x001681b5,   // ChildOutfitStripedShirtAndJeans (ClothesKids01)
            0x001681b7,   // ChildOutfitStreetUrchinRags (ClothesKids02)
            0x001681b8,   // ChildOutfitStreetUrchinRagsWithHat (+hat)
            0x00164164,   // ChildOutfitPajamas (ClothesKids04)
        };
        constexpr std::uint32_t kChildOutfitFemale[] = {
            0x001681b5,   // ChildOutfitStripedShirtAndJeans (ClothesKids01)
            0x001681b7,   // ChildOutfitStreetUrchinRags (ClothesKids02)
            0x001681b8,   // ChildOutfitStreetUrchinRagsWithHat (+hat)
            0x00164164,   // ChildOutfitPajamas (ClothesKids04)
            0x0015d310,   // ChildOutfitDress (ClothesKids03)
        };

        // The materialize pass's cadence — a couple of checks a second
        // is plenty; the child stays invisible until a load completes it.
        constexpr double kVisualChildCheckSeconds = 0.5;

        //-------------------------------------------------------------------------
        // The visible baby (0.8.9 field find): the swaddled bundle the
        // mother carries is NOT the armor's mesh — babybundled and the
        // baby mod's Cyber_* bundles all ship an empty MODL. The visible
        // baby is the AnimObject (Objects\BabyBundled.nif) that the
        // AnimFlavorHoldingBaby anim flavor attaches. The baby mod's
        // bundles carry a script (Cyber_BabyOnEquip, decompiled
        // 2026-08-15) that calls Actor.ChangeAnimFlavor(AnimFlavorHoldingBaby)
        // on equip — that is what makes the bundle visible; the vanilla
        // fallback (babybundled) has no script, so equipping it shows
        // nothing. This applies the flavor ourselves through the game's
        // own console parser (PRID selects the ref, ChangeAnimFlavor
        // sets or clears the flavor) — the identical native the script
        // calls, without the VM: the papyrus-dispatch route is the
        // documented PackVariables crasher, while the console parser is
        // the game's own stable seam (a bad arg at worst does nothing).
        // Idempotent: next to the mod's own OnEquipped call it is a
        // harmless duplicate.
        //-------------------------------------------------------------------------
        void ApplyHoldingFlavor(RE::Actor* a_actor, bool a_set)
        {
            if (a_actor == nullptr)
            {
                return;
            }

            RE::Console::ExecuteCommand(
                std::format("PRID {:08x}", a_actor->GetFormID()).c_str());

            if (a_set)
            {
                // AnimFlavorHoldingBaby (0x000E52D9, Fallout4.esm) —
                // the flavor whose anim object is the swaddled baby.
                RE::Console::ExecuteCommand("ChangeAnimFlavor 000E52D9");
            }
            else
            {
                // No keyword = the papyrus default (None): the flavor
                // drops and the anim object detaches.
                RE::Console::ExecuteCommand("ChangeAnimFlavor");
            }
        }
    }

    Adapter::Adapter()
    {
        // Once, before any world exists; survives Clear() so Restore (the
        // co-save stone) always has its serializers.
        RegisterAllSerializers(m_Registry);

        // The observation bus (Request A — stone 08): the core publishes
        // RelationshipChangedEvent when a configured disposition line is
        // crossed. Subscribed once for the adapter's lifetime; the
        // handler reads the live relationships and applies the same
        // derivation the 1-second pass uses, so the two channels never
        // disagree. No logging here — the constructor runs before the
        // logger attaches.
        m_Bus.Subscribe(
            std::type_index(typeid(LCE::Simulation::RelationshipChangedEvent)),
            [this](const LCE::Events::Event& a_event)
            {
                OnRelationshipChanged(
                    static_cast<const LCE::Simulation::RelationshipChangedEvent&>(
                        a_event));
            });

        // Tuning is loaded from GameLoaded (not here): the constructor
        // runs before the logger attaches, so its confirmation lines
        // would be silently dropped — and the modder should see them.
    }

    void Adapter::LoadConfiguration()
    {
        // The file lives next to the DLL — Data\F4SE\Plugins\
        // TheLivingCommonwealth.ini. Located via the module's own path so
        // it works regardless of how F4SE loaded us.
        wchar_t modulePath[MAX_PATH]{};
        HMODULE module = nullptr;

        if (!GetModuleHandleExW(
                GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
                reinterpret_cast<LPCWSTR>(&ModuleAnchor),
                &module)
            || GetModuleFileNameW(module, modulePath, MAX_PATH) == 0)
        {
            REX::WARN("tuning: could not locate the plugin module — defaults.");
            return;
        }

        std::filesystem::path ini{ modulePath };
        ini.replace_extension(".ini");

        std::error_code error;

        if (!std::filesystem::exists(ini, error))
        {
            REX::INFO(
                "tuning: no config file ({} expected) — defaults. "
                "Create it to change the sim's feel.",
                ini.string());
            return;
        }

        std::ifstream stream(ini);
        std::stringstream buffer;
        buffer << stream.rdbuf();

        m_Config = Tuning::ParseConfig(buffer.str());
        m_ConfigPath = ini;

        // The MCM override (0.8.5): the MCM page's changes live in
        // Data\MCM\Settings\TheLivingCommonwealth.ini (MCM's own
        // storage — the page is pure JSON, no Papyrus surface). The
        // adapter overlays that file on top of its own INI, the MCM
        // layer winning, so a slider change persists here and survives
        // a restart. The last-write stamp drives the per-second
        // hot-apply check.
        m_McmOverridePath =
            std::filesystem::current_path() / "Data" / "MCM" / "Settings"
            / "TheLivingCommonwealth.ini";

        std::error_code mcmError;

        if (std::filesystem::exists(m_McmOverridePath, mcmError))
        {
            std::ifstream mcmStream(m_McmOverridePath);
            std::stringstream mcmBuffer;
            mcmBuffer << mcmStream.rdbuf();

            const auto mcmConfig =
                Tuning::ParseConfig(mcmBuffer.str());

            auto overlaid = 0U;

            mcmConfig.ForEach(
                [this, &overlaid](
                    std::string_view a_key, std::string_view a_value)
                {
                    // MCM settings carry a one-char type prefix
                    // (b/i/f/s — the MCM convention, e.g. "fHunger"),
                    // but the sim's own keys are bare. Strip the
                    // prefix so fsim.hunger.decay overlays
                    // sim.hunger.decay.
                    auto key = std::string(a_key);

                    if (!key.empty()
                        && (key[0] == 'b' || key[0] == 'i'
                            || key[0] == 'f' || key[0] == 's'))
                    {
                        key.erase(0, 1);
                    }

                    m_Config.Set(key, a_value);
                    ++overlaid;
                });

            m_McmOverrideStamp =
                std::filesystem::last_write_time(m_McmOverridePath, mcmError);

            REX::INFO(
                "tuning: MCM override applied ({} keys).", overlaid);
        }

        ApplyConfig(m_Config, ini);

        // The Realistic Conversations compatibility (0.8.7): applied
        // here, after the sim tuning reads, so the game's settings land
        // before the first world starts — the NPC social behavior is
        // in place from the first frame.
        ApplyRealisticConversations();
    }

    void Adapter::ApplyRealisticConversations()
    {
        // The Realistic Conversations compatibility (0.8.7): the 2018
        // ESP's 33 GMST overrides, re-delivered as a tuning file next
        // to the DLL — applied here to the game's own setting
        // collection, so the mod's social behavior (NPC-to-NPC voiced
        // chatter, natural greeting distances and timers, dinner
        // hours) lands on the Next-Gen build with no xedit patch, no
        // ESP, no load-order slot. A missing file is the off switch.
        wchar_t modulePath[MAX_PATH]{};
        HMODULE module = nullptr;

        if (!GetModuleHandleExW(
                GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
                reinterpret_cast<LPCWSTR>(&ModuleAnchor),
                &module)
            || GetModuleFileNameW(module, modulePath, MAX_PATH) == 0)
        {
            return;
        }

        std::filesystem::path ini{ modulePath };
        ini.replace_filename(L"Realistic Conversations.ini");

        std::error_code error;

        if (!std::filesystem::exists(ini, error))
        {
            return;   // not installed — nothing to apply
        }

        std::ifstream stream(ini);
        std::stringstream buffer;
        buffer << stream.rdbuf();

        const auto config = Tuning::ParseConfig(buffer.str());

        auto* collection = RE::GameSettingCollection::GetSingleton();

        if (collection == nullptr)
        {
            REX::WARN(
                "rc: game settings unavailable — overrides not applied.");
            return;
        }

        // The Setting's own type (derived from the key's prefix letter
        // — f/i/b — the game's convention) decides how the value is
        // written back, so the file needs no type annotations: the
        // setting itself says what it is.
        auto applied = 0U;
        auto missing = 0U;

        config.ForEach(
            [&](std::string_view a_key, std::string_view a_value)
            {
                auto* setting = collection->GetSetting(a_key);

                if (setting == nullptr)
                {
                    REX::INFO(
                        "rc: setting '{}' not found in this build — skipped.",
                        a_key);
                    ++missing;
                    return;
                }

                switch (setting->GetType())
                {
                    case RE::Setting::SETTING_TYPE::kFloat:
                    {
                        const float value =
                            std::strtof(std::string(a_value).c_str(), nullptr);
                        setting->SetFloat(value);
                        ++applied;
                        break;
                    }
                    case RE::Setting::SETTING_TYPE::kInt:
                    {
                        const int value =
                            std::strtol(std::string(a_value).c_str(), nullptr, 10);
                        setting->SetInt(value);
                        ++applied;
                        break;
                    }
                    case RE::Setting::SETTING_TYPE::kBinary:
                    {
                        const auto value =
                            a_value == "1" || a_value == "true"
                            || a_value == "yes";
                        setting->SetBinary(value);
                        ++applied;
                        break;
                    }
                    default:
                        REX::INFO(
                            "rc: setting '{}' has an unsupported type — skipped.",
                            a_key);
                        ++missing;
                        break;
                }
            });

        REX::INFO(
            "rc: Realistic Conversations overrides applied ({} settings, "
            "{} skipped).",
            applied, missing);
    }

    void Adapter::ApplyConfig(
        const LCE::Config::Configuration& a_config,
        const std::filesystem::path& a_iniPath)
    {
        m_CoreTuning =
            LCE::Simulation::SimulationTuning::FromConfiguration(a_config);
        m_Settings = Tuning::AdapterSettingsFrom(a_config);

        // The crash-hunt bisect gate (0.8.7, sim.diag.noWalks): every
        // Movement command refuses without touching the game, so the
        // sim never issues a command-mode travel package. Applied at
        // every config load — the gate flips live on INI reload.
        TLC::Movement::SetCommandsEnabled(!m_Settings.NoWalks);

        // The identity stone's pool (0.7.0 Stone 1): the author's own
        // name lists — names.first.male / .female / .animal, names.last
        // — comma-separated, each overriding its default list. The
        // defaults are the world's fallback, never a broken line.
        m_Names = TLC::Names::PoolFrom(a_config);

        // The dialogue pools (0.7.1 Talk): the author's one-liners,
        // overridable per list in the INI (dialogue.* keys), defaults
        // otherwise — a missing or broken line never breaks the world.
        m_Dialogue = TLC::Dialogue::PoolFrom(a_config);

        // The feeling rhythm (0.6.0 Stone 2): the core's drift default
        // (0.05/s — half-life ~14 s) was tuned for a fast demo and
        // erases a shared meal's warmth between meals (minutes apart),
        // so no relationship can ever accumulate. The adapter's world
        // runs the same slow clock the shipped INI sets (sim.drift.rate)
        // when the config names none — the adapter's defaults ARE the
        // living-world defaults, exactly like the bond lines below.
        const auto driftInjected =
            a_config.Get("sim.drift.rate").empty();

        if (driftInjected)
        {
            m_CoreTuning.DriftRate = Tuning::kLivingDriftRate;
        }

        // Bonds (0.6.0 Stone 2): the core's watch-list is empty unless
        // the world names its lines — the adapter names them here, so a
        // world without a config file still bonds (friend +0.3 through
        // enemy −0.6). The INI's sim.bond.threshold.<name> keys override
        // whatever they set; the same values drive the core's events and
        // the adapter's derivation.
        const auto bondDefaultsInjected =
            m_CoreTuning.BondThresholds.empty();

        if (bondDefaultsInjected)
        {
            m_CoreTuning.BondThresholds = Bonds::DefaultBondThresholds();
        }

        // The sleep cycle (0.6.0): the recovery rate a resting mind
        // refills Fatigue at. The adapter's own default (0.2/s — a full
        // nap in ~5 s) applies when the config names none, so a world
        // without the key still sleeps.
        const auto restInjected =
            a_config.Get("sim.rest.recovery").empty();

        // The seeded need rhythm — the five sim.*.decay rates. Printed
        // so the log always says which rhythm actually runs (the
        // 2026-08-11 hunt: a 0.1/s hunger INI that read as if it were
        // 0.002/s — the banner only showed market hours, drift, and rest).
        const auto needsDefaultsInjected =
            a_config.Get("sim.hunger.decay").empty()
            && a_config.Get("sim.fatigue.decay").empty()
            && a_config.Get("sim.safety.decay").empty()
            && a_config.Get("sim.social.decay").empty()
            && a_config.Get("sim.comfort.decay").empty();

        m_BondThresholds =
            Bonds::ParseBondThresholds(m_CoreTuning.BondThresholds);

        REX::INFO(
            "tuning: applied {} — market {:02.0f}:00–{:02.0f}:00, drift {}{}, rest {:.3f}/s{}.",
            a_iniPath.string(),
            m_Settings.MarketOpenHour,
            m_Settings.MarketCloseHour,
            m_CoreTuning.DriftRate,
            driftInjected ? " (defaults)" : "",
            m_Settings.RestRecovery,
            restInjected ? " (defaults)" : "");
        REX::INFO(
            "bonds: friend {:+.2f}, sweetheart {:+.2f}, spouse {:+.2f}, "
            "rival {:+.2f}, enemy {:+.2f}{}.",
            m_BondThresholds.Friend,
            m_BondThresholds.Sweetheart,
            m_BondThresholds.Spouse,
            m_BondThresholds.Rival,
            m_BondThresholds.Enemy,
            bondDefaultsInjected ? " (defaults)" : "");
        REX::INFO(
            "tuning: needs — hunger {:.3f}/s, fatigue {:.3f}/s, "
            "safety {:.3f}/s, social {:.3f}/s, comfort {:.3f}/s, "
            "walk cap {}{}.",
            m_Settings.Rates.Hunger,
            m_Settings.Rates.Fatigue,
            m_Settings.Rates.Safety,
            m_Settings.Rates.Social,
            m_Settings.Rates.Comfort,
            m_Settings.WalkCap,
            needsDefaultsInjected ? " (defaults)" : "");
    }

    void Adapter::CheckMcmOverride()
    {
        if (m_McmOverridePath.empty())
        {
            return;   // LoadConfiguration never ran — no override known
        }

        std::error_code error;

        if (!std::filesystem::exists(m_McmOverridePath, error))
        {
            return;   // no MCM page installed — the sim INI alone rules
        }

        const auto stamp =
            std::filesystem::last_write_time(m_McmOverridePath, error);

        if (stamp == m_McmOverrideStamp)
        {
            return;   // unchanged since the last apply
        }

        m_McmOverrideStamp = stamp;

        // The player changed something in the MCM page — re-overlay and
        // re-apply live, mid-session. A stat is cheap; this only fires
        // when MCM actually wrote the file.
        std::ifstream stream(m_McmOverridePath);
        std::stringstream buffer;
        buffer << stream.rdbuf();

        const auto mcmConfig = Tuning::ParseConfig(buffer.str());

        mcmConfig.ForEach(
            [this](std::string_view a_key, std::string_view a_value)
            {
                // Strip the MCM type prefix (see LoadConfiguration).
                auto key = std::string(a_key);

                if (!key.empty()
                    && (key[0] == 'b' || key[0] == 'i'
                        || key[0] == 'f' || key[0] == 's'))
                {
                    key.erase(0, 1);
                }

                m_Config.Set(key, a_value);
            });

        REX::INFO("tuning: MCM override changed — hot-applied.");
        ApplyConfig(m_Config, m_ConfigPath);
    }

    void Adapter::GameLoaded()
    {
        // One load can complete with both kPostLoadGame and kGameLoaded —
        // the first completion applies the restore; the second must not
        // wipe it with a fresh start. Reset by PreLoadGame; the startup
        // wake (no preceding load) also starts false and is handled once.
        if (m_LoadCompleted)
        {
            return;
        }

        m_LoadCompleted = true;

        // The modder's knob, loaded once per session — from here, after
        // the logger is attached, so the tuning confirmation lines
        // actually appear. Before the first world: StartWorld and
        // ApplyRestore below both consume m_Settings/m_CoreTuning.
        if (!m_TuningLoaded)
        {
            LoadConfiguration();
            m_TuningLoaded = true;
        }

        // Every completed load is a fresh world — but if the co-save held
        // a world for this save, restore it instead of translating anew:
        // the sim remembers (0.4.0). An empty pending restore (a save made
        // while the sim was not running) falls through to a fresh world.
        if (m_PendingRestore && !m_PendingRestore->Entities.empty())
        {
            ApplyRestore(std::move(*m_PendingRestore));
        }
        else
        {
            EndWorld();
            StartWorld();
        }

        m_PendingRestore.reset();
        m_AwaitingLoad = false;
    }

    LCE::Simulation::RegistrySnapshot Adapter::CaptureWorld() const
    {
        return m_Registry.Capture();
    }

    std::uint64_t Adapter::RngState() const noexcept
    {
        return m_Rng.State();
    }

    void Adapter::QueueRestore(
        LCE::Simulation::RegistrySnapshot a_snapshot,
        std::uint64_t a_rngState,
        std::vector<TLC::CoSave::StallKeeperPair> a_stallKeepers,
        std::vector<TLC::CoSave::BondPair> a_bonds,
        std::vector<TLC::CoSave::ConflictGatePair> a_gates,
        std::vector<TLC::CoSave::BurialEntry> a_burials,
        std::vector<TLC::CoSave::MedicineStockPair> a_medicineStock,
        std::vector<TLC::CoSave::BabyHold> a_babyHolds,
        std::vector<TLC::CoSave::VisualChild> a_visualChildren)
    {
        m_PendingRestore = std::move(a_snapshot);
        m_PendingRngState = a_rngState;
        m_PendingStallKeepers = std::move(a_stallKeepers);
        m_PendingBonds = std::move(a_bonds);
        m_PendingGates = std::move(a_gates);
        m_PendingBurials = std::move(a_burials);
        m_PendingMedicineStock = std::move(a_medicineStock);
        m_PendingBabyHolds = std::move(a_babyHolds);
        m_PendingVisualChildren = std::move(a_visualChildren);
    }

    std::vector<TLC::CoSave::StallKeeperPair> Adapter::StallKeepersForSave() const
    {
        // Entity ids are session-local; the durable form is form ids.
        // A keeper or market whose form the translator cannot resolve is
        // skipped — it was never a real entity this world.
        std::vector<TLC::CoSave::StallKeeperPair> result;
        result.reserve(m_StallKeepers.size());

        for (const auto& [market, keeper] : m_StallKeepers)
        {
            const auto marketFormId = m_Translator.FormFor(market);
            const auto keeperFormId = m_Translator.FormFor(keeper);

            if (marketFormId == 0 || keeperFormId == 0)
            {
                continue;
            }

            result.emplace_back(marketFormId, keeperFormId);
        }

        return result;
    }

    void Adapter::RestoreStallKeepers(
        const std::vector<TLC::CoSave::StallKeeperPair>& a_stallKeepers)
    {
        for (const auto& [marketFormId, keeperFormId] : a_stallKeepers)
        {
            const auto market = m_Translator.EntityFor(marketFormId);
            const auto keeper = m_Translator.EntityFor(keeperFormId);

            // Both ride the snapshot (the market owns a FormRef, the
            // keeper is a mind) — if either is missing, that market's
            // stall re-derives on the first arrival, like a fresh world.
            if (!market.IsValid() || !keeper.IsValid())
            {
                continue;
            }

            // An animal cannot run a stall (the election is human-only
            // too) — a keeper saved before the species fix is skipped
            // and the stall re-derives under a person on the first
            // arrival.
            const auto keeperTag =
                m_Registry.GetComponent<SpeciesTag>(keeper);

            if (keeperTag
                && keeperTag->Value == Species::Animal)
            {
                continue;
            }

            m_StallKeepers[market] = keeper;

            REX::INFO(
                "LCE: stall restored — market {:#x} reopens under keeper {:#x}.",
                marketFormId, keeperFormId);
        }
    }

    std::vector<TLC::CoSave::BondPair> Adapter::BondsForSave() const
    {
        // Entity ids are session-local; the durable form is form ids.
        // A pair whose form the translator cannot resolve is skipped — it
        // was never a real pair this world. A resting (None) row is never
        // written.
        std::vector<TLC::CoSave::BondPair> result;
        result.reserve(m_Bonds.size());

        for (const auto& [key, bond] : m_Bonds)
        {
            if (bond.Kind == Bonds::BondKind::None)
            {
                continue;
            }

            const auto formA = m_Translator.FormFor(
                LCE::Simulation::EntityId{ key.first });
            const auto formB = m_Translator.FormFor(
                LCE::Simulation::EntityId{ key.second });

            if (formA == 0 || formB == 0)
            {
                continue;
            }

            result.push_back(TLC::CoSave::BondPair{
                formA, formB,
                static_cast<std::uint32_t>(bond.Kind),
                bond.SinceDay });
        }

        return result;
    }

    std::vector<TLC::CoSave::ConflictGatePair>
    Adapter::ConflictGatesForSave() const
    {
        // Entity ids are session-local; the durable form is form ids.
        // A pair whose form the translator cannot resolve is skipped —
        // it was never a real pair this world.
        std::vector<TLC::CoSave::ConflictGatePair> result;
        result.reserve(m_ConflictGates.size());

        for (const auto& [key, gate] : m_ConflictGates)
        {
            if (gate.RowDay == 0 && gate.FightDay == 0)
            {
                continue;   // never rowed, never fought — nothing to keep
            }

            const auto formA = m_Translator.FormFor(
                LCE::Simulation::EntityId{ key.first });
            const auto formB = m_Translator.FormFor(
                LCE::Simulation::EntityId{ key.second });

            if (formA == 0 || formB == 0)
            {
                continue;
            }

            result.push_back(TLC::CoSave::ConflictGatePair{
                formA, formB, gate.RowDay, gate.FightDay });
        }

        return result;
    }

    void Adapter::RestoreConflictGates(
        const std::vector<TLC::CoSave::ConflictGatePair>& a_gates)
    {
        m_ConflictGates.clear();

        for (const auto& gate : a_gates)
        {
            const auto a = m_Translator.EntityFor(gate.FormA);
            const auto b = m_Translator.EntityFor(gate.FormB);

            // Both ride the snapshot (both are minds with FormRefs) — a
            // pair whose actor is missing is simply absent, and the
            // pair's gate is free again (they may row or fight once
            // more today, which is honest for a dead world).
            if (!a.IsValid() || !b.IsValid())
            {
                continue;
            }

            m_ConflictGates[ConflictGates::PairKey(a, b)] =
                ConflictGates::Gate{ gate.RowDay, gate.FightDay };
        }

        if (!m_ConflictGates.empty())
        {
            REX::INFO(
                "gates: {} feud gate{} restored from the co-save.",
                m_ConflictGates.size(),
                m_ConflictGates.size() == 1 ? "" : "s");
        }
    }

    std::vector<TLC::CoSave::BurialEntry>
    Adapter::BurialsForSave() const
    {
        // The ledger is already durable: form ids are stable across
        // sessions (entity ids are session-local, but a burial's key is
        // the dead actor's game form id — the thing the corpse ref
        // resolves by). Nothing to translate; the co-save carries it as
        // (form id, day died) pairs (v8).
        std::vector<TLC::CoSave::BurialEntry> result;
        result.reserve(m_Burials.size());

        for (const auto& [formId, day] : m_Burials)
        {
            result.push_back(TLC::CoSave::BurialEntry{ formId, day });
        }

        return result;
    }

    void Adapter::RestoreBurials(
        const std::vector<TLC::CoSave::BurialEntry>& a_burials)
    {
        m_Burials.clear();

        for (const auto& burial : a_burials)
        {
            m_Burials[burial.FormId] = burial.DiedDay;
        }

        if (!m_Burials.empty())
        {
            REX::INFO(
                "burials: {} corpse{} in the co-save's burial book — the window keeps ticking.",
                m_Burials.size(),
                m_Burials.size() == 1 ? " is" : "s are");
        }
    }

    void Adapter::BurialSweep()
    {
        const auto day = CurrentDay();

        for (auto it = m_Burials.begin(); it != m_Burials.end();)
        {
            const auto [formId, diedDay] = *it;

            // The mourning window: the settlement leaves the body where
            // it fell until the dead are given their due. Still inside
            // it — wait.
            if (day < diedDay
                + static_cast<std::uint64_t>(
                    m_Settings.BurialDays))
            {
                ++it;
                continue;
            }

            // The corpse ref: the game's own actor, still in the cell.
            // The census's IsActorDead reads the death markers the same
            // way; here the ref itself is the body — disable it, and the
            // cell no longer holds the dead.
            auto* corpse = RE::TESForm::GetFormByID<RE::TESObjectREFR>(formId);

            if (corpse == nullptr)
            {
                // The body is gone — cleaned up by the game, or its
                // cell never loaded. Either way there is nothing to
                // bury; the book is settled.
                REX::INFO(
                    "burials: {} is already gone — nothing to lay to rest.",
                    MindLabelForm(formId));
                it = m_Burials.erase(it);
                continue;
            }

            // The name, before the disable: the corpse ref still speaks
            // its override name (the identity stone wrote it), and the
            // translator has already forgotten the dead — so the
            // display name is the only honest source left. Wrapped in a
            // std::string: GetDisplayFullName returns a const char* —
            // null when the ref has no name — and the empty check
            // below needs a real string.
            const auto deadName =
                std::string(corpse->GetDisplayFullName()
                                ? corpse->GetDisplayFullName()
                                : "");

            corpse->Disable();

            const auto graveLabel =
                deadName.empty() ? MindLabelForm(formId) : deadName;

            REX::INFO(
                "burials: the settlement laid {} to rest.", graveLabel);

            PushNews(
                graveLabel + " was laid to rest.",
                Tuning::AdapterSettings::NewsCategory::Death);

            it = m_Burials.erase(it);
        }
    }

    void Adapter::PayStipends()
    {
        using namespace LCE::Simulation;

        const auto stipend =
            static_cast<std::uint32_t>(m_Settings.Stipend);

        if (stipend == 0)
        {
            return;   // the stipend is off (the default) — nothing to pay
        }

        const auto day = CurrentDay();

        if (day == 0)
        {
            return;   // the calendar is not up yet
        }

        std::vector<TLC::StipendReceipt> receipts;

        const auto ownedGate =
            m_Settings.StipendRequireOwned
            ? std::function<bool(std::uint32_t)>(
                  [this](std::uint32_t a_marketFormId)
                  {
                      // The census's ownership read. A market with no
                      // record (never censused, or the wastes — market
                      // 0) reads unowned: the gate's safe default. A
                      // mind in the wastes has no settlement to pay it
                      // anyway.
                      const auto it = m_WorkshopOwned.find(a_marketFormId);

                      return it != m_WorkshopOwned.end() && it->second;
                  })
            : std::function<bool(std::uint32_t)>();

        TLC::PayStipends(
            m_Registry, stipend, day,
            [this](LCE::Simulation::EntityId a_entity)
            {
                // The mind's home market, from its own memory first:
                // the per-settlement seed wrote a Trade-kind event whose
                // Other is its market — co-save state, available even
                // before the actor streams in. The 0.7.4 vendor seed
                // also writes Trade events, but their Other is a person
                // (an actor, not a workshop), so a memory's Other that
                // is a known workshop is unambiguously the home market.
                if (const auto memory =
                        m_Registry.GetComponent<Memory>(a_entity))
                {
                    for (const auto& event : memory->Events)
                    {
                        if (event.Kind
                            != InteractionKind::Trade)
                        {
                            continue;
                        }

                        const auto marketFormId =
                            m_Translator.FormFor(event.Other);

                        if (IsWorkshopForm(marketFormId))
                        {
                            return marketFormId;
                        }
                    }
                }

                // No remembered market (a mind that never traded, or a
                // fresh world): the nearest workshop to the actor — the
                // same spatial rule the seed uses. An actor not loaded
                // yet has no position; it is paid under the wastes this
                // day and its mark holds, so tomorrow's sweep (actor
                // loaded, memory seeded) books it under the right
                // settlement.
                const auto formId = m_Translator.FormFor(a_entity);
                const auto* actor =
                    RE::TESForm::GetFormByID<RE::Actor>(formId);

                if (actor == nullptr)
                {
                    return 0u;
                }

                const auto pos = actor->GetPosition();

                return NearestWorkshop(
                    pos.x, pos.y, m_Workshops, kMarketRadius);
            },
            receipts,
            ownedGate);

        std::uint64_t totalOut = 0;

        for (const auto& receipt : receipts)
        {
            totalOut += receipt.Caps;

            const auto settlement =
                receipt.MarketFormId != 0
                ? MarketLabel(receipt.MarketFormId)
                : "the wastes";

            REX::INFO(
                "LCE: economy — {} paid its {} people {} caps each ({} caps out).",
                settlement, receipt.Paid, stipend, receipt.Caps);
        }

        // The player-pays leg (0.8.6b): when the source is the player,
        // the wage bill comes out of the player's own caps — the game's
        // gold API, read and written at the edge (game knowledge,
        // ADR-0024). A player who can't cover the bill still pays what
        // the pouches received (the settlement covers the shortfall in
        // spirit); the show never runs a deficit.
        if (m_Settings.StipendSourcePlayer && totalOut > 0)
        {
            auto* player = RE::PlayerCharacter::GetSingleton();
            auto* caps = RE::TESForm::GetFormByID(0x0000000Fu);

            if (player != nullptr && caps != nullptr)
            {
                const auto before = player->GetGoldAmount();

                // The wage bill leaves the player's inventory: the
                // game's own remove path, caps [ITEM:0000000F]. The
                // count is POSITIVE — the unified inventory native
                // removes on a positive count and adds on a negative
                // one (the 0.8.6b field failure: the bill line fired
                // but the caps never moved because the count was
                // negated; the sign was the bug). The selling reason
                // is the game's own trade-money flow.
                RE::TESObjectREFR::RemoveItemData data{
                    caps, static_cast<std::int32_t>(totalOut) };
                data.reason = RE::ITEM_REMOVE_REASON::kSelling;
                player->RemoveItem(data);

                const auto after = player->GetGoldAmount();

                REX::INFO(
                    "LCE: economy — the wage bill of {} caps came from the player's purse ({} -> {}).",
                    totalOut, before, after);
            }
        }
    }

    std::uint32_t Adapter::MedicineStockOf(
        std::uint32_t a_marketFormId) const noexcept
    {
        const auto it = m_MedicineStock.find(a_marketFormId);

        if (it == m_MedicineStock.end())
        {
            return m_Settings.Illness.Stock;   // a fresh shelf
        }

        return it->second;
    }

    void Adapter::ConsumeMedicine(std::uint32_t a_marketFormId) noexcept
    {
        const auto full = m_Settings.Illness.Stock;
        const auto left = MedicineStockOf(a_marketFormId);

        m_MedicineStock[a_marketFormId] = left > 0 ? left - 1 : 0;
    }

    void Adapter::RestoreBabyHolds(
        const std::vector<TLC::CoSave::BabyHold>& a_babyHolds)
    {
        m_BabyHolds.clear();

        for (const auto& entry : a_babyHolds)
        {
            m_BabyHolds[entry.MotherFormId] = entry;
        }

        // The game save keeps the equipped bundle, but the holding
        // flavor is runtime state — re-apply it so a restored carry is
        // visible again (best-effort: a streamed-out mother is skipped
        // and re-applied on her next shed-day pass).
        for (const auto& [motherFormId, hold] : m_BabyHolds)
        {
            ApplyHoldingFlavor(
                RE::TESForm::GetFormByID<RE::Actor>(motherFormId),
                true);
        }

        if (!m_BabyHolds.empty())
        {
            REX::INFO(
                "birth: {} newborn hold{} restored from the co-save.",
                m_BabyHolds.size(),
                m_BabyHolds.size() == 1 ? "" : "s");
        }
    }

    bool Adapter::BabyModLoaded()
    {
        if (m_BabyModChecked)
        {
            return m_BabyModLoaded;
        }

        m_BabyModChecked = true;

        // The soft dependency (0.8.9): resolve the baby mod's plugin by
        // name — the load order never changes mid-session, so the read
        // is cached. Loaded once, the sim knows the world provides
        // babies: the pairing, the crib walk, and the visible journey
        // all gate on it, and everything degrades to sim-only children
        // without it.
        auto* handler = RE::TESDataHandler::GetSingleton();

        if (handler == nullptr)
        {
            return false;
        }

        m_BabyModLoaded = handler->GetLoadedModIndex(
            "Baby Sim - Babies That Grow Up.esp").has_value();

        REX::INFO(
            "birth: baby mod {} — the visible journey {}.",
            m_BabyModLoaded ? "found" : "not found",
            m_BabyModLoaded ? "enabled" : "stays sim-only");

        return m_BabyModLoaded;
    }

    std::uint32_t Adapter::BabyForm(std::uint32_t a_recordId)
    {
        // The plugin's form ids carry its load index in the top byte.
        // 0 when the mod is absent — callers check BabyModLoaded first.
        if (!m_BabyModLoaded)
        {
            return 0;
        }

        auto* handler = RE::TESDataHandler::GetSingleton();

        if (handler == nullptr)
        {
            return 0;
        }

        const auto index = handler->GetLoadedModIndex(
            "Baby Sim - Babies That Grow Up.esp");

        if (!index.has_value())
        {
            return 0;
        }

        return (static_cast<std::uint32_t>(*index) << 24)
            | (a_recordId & 0xFFFFFFu);
    }

    void Adapter::ReplenishMedicineStock() noexcept
    {
        const auto full = m_Settings.Illness.Stock;

        // Every shelf the world knows refills; a shelf nobody has
        // touched yet stays missing (MedicineStockOf reads it as full).
        for (auto& [market, stock] : m_MedicineStock)
        {
            stock = full;
        }

        REX::INFO(
            "illness: the market day turns — {} {} refill{} to full stock.",
            m_MedicineStock.size(),
            m_MedicineStock.size() == 1 ? "shelf" : "shelves",
            m_MedicineStock.size() == 1 ? "s" : "");
    }

    void Adapter::RestoreMedicineStock(
        const std::vector<TLC::CoSave::MedicineStockPair>& a_medicineStock)
    {
        m_MedicineStock.clear();

        for (const auto& entry : a_medicineStock)
        {
            m_MedicineStock[entry.MarketFormId] = entry.Stock;
        }

        if (!m_MedicineStock.empty())
        {
            REX::INFO(
                "illness: {} market shelf{} from the co-save — sold-out stalls stay sold out until the market day turns.",
                m_MedicineStock.size(),
                m_MedicineStock.size() == 1 ? " restored" : "s restored");
        }
    }

    std::vector<TLC::CoSave::MedicineStockPair>
    Adapter::MedicineStockForSave() const
    {
        // The map is already durable: form ids are stable across
        // sessions (entity ids are session-local, but a shelf's key is
        // the market's game form id — the thing walks resolve by).
        // Nothing to translate; the co-save carries it as (market form
        // id, doses left) pairs (v9).
        std::vector<TLC::CoSave::MedicineStockPair> result;
        result.reserve(m_MedicineStock.size());

        for (const auto& [marketFormId, stock] : m_MedicineStock)
        {
            result.push_back(
                TLC::CoSave::MedicineStockPair{ marketFormId, stock });
        }

        return result;
    }

    std::vector<TLC::CoSave::BabyHold>
    Adapter::BabyHoldsForSave() const
    {
        // The held newborns (0.8.9 birth journey): the map is already
        // durable — (mother form id, bundle form id, born day) holds,
        // co-save v10 — nothing to translate.
        std::vector<TLC::CoSave::BabyHold> result;
        result.reserve(m_BabyHolds.size());

        for (const auto& [motherFormId, hold] : m_BabyHolds)
        {
            result.push_back(hold);
        }

        return result;
    }

    void Adapter::RestoreBonds(
        const std::vector<TLC::CoSave::BondPair>& a_bonds)
    {
        m_Bonds.clear();

        for (const auto& bond : a_bonds)
        {
            const auto a = m_Translator.EntityFor(bond.FormA);
            const auto b = m_Translator.EntityFor(bond.FormB);

            // Both ride the snapshot (both are minds with FormRefs) — a
            // pair whose actor is missing is simply absent, and the 1s
            // reconcile pass re-derives what it can from the restored
            // relationships.
            if (!a.IsValid() || !b.IsValid())
            {
                continue;
            }

            // The animal gate: a bond saved before the species fix may
            // name an animal — a behemoth's old feud. It is not
            // restored; the 1-second reconcile never re-forms it (the
            // same gate), so old animal bonds dissolve quietly out of
            // the world instead of persisting as stale feuds.
            const auto speciesA =
                m_Registry.GetComponent<SpeciesTag>(a);
            const auto speciesB =
                m_Registry.GetComponent<SpeciesTag>(b);

            if ((speciesA && speciesA->Value == Species::Animal)
                || (speciesB && speciesB->Value == Species::Animal))
            {
                continue;
            }

            m_Bonds[Bonds::PairKey(a, b)] =
                Bonds::PairBond{
                    static_cast<Bonds::BondKind>(bond.Kind),
                    bond.SinceDay };
        }

        if (!m_Bonds.empty())
        {
            REX::INFO(
                "bonds: {} bond{} restored from the co-save.",
                m_Bonds.size(), m_Bonds.size() == 1 ? "" : "s");
        }
    }

    void Adapter::ReconcileBonds()
    {
        // The 1-second pass (the dissolve net): re-derive every pair
        // from the live relationships. The event channel is instant; this
        // is complete — quiet drift, restores, and anything the bus
        // missed all surface here. OnBondChange fires only on change,
        // so a pair the event already settled is silent here.
        Bonds::Reconcile(
            m_Registry,
            m_BondThresholds,
            m_Bonds,
            CurrentDay(),
            [this](
                LCE::Simulation::EntityId a_entityA,
                LCE::Simulation::EntityId a_entityB,
                Bonds::BondKind a_old,
                Bonds::BondKind a_new,
                std::uint64_t a_sinceDay)
            {
                OnBondChange(a_entityA, a_entityB, a_old, a_new, a_sinceDay);
            },
            m_Kin);

        // The household invariant (0.6.0 Stone 3): one pouch per married
        // pair, one per unmarried human. The loud events fire in
        // OnBondChange; this is the silent repair — a restored marriage,
        // a defensive seed, anything the events missed.
        Households::Enforce(m_Registry, m_Bonds);
    }

    void Adapter::OnRelationshipChanged(
        const LCE::Simulation::RelationshipChangedEvent& a_event)
    {
        // The immediate channel (Request A — stone 08): the core crossed
        // a bond line mid-mutation. Re-derive that pair now, the same
        // rule the 1-second pass applies — the pass then finds the pair
        // resting and stays silent. The event's day is the crossing day,
        // the honest birthdate of a fresh bond.
        if (!a_event.Subject.IsValid() || !a_event.Other.IsValid())
        {
            return;
        }

        // Both must be minds — a workshop is a target, never a bond
        // partner (the same gate the reconcile pass applies) — and both
        // must be people (the animal gate, same pass): an animal is
        // fed, not bonded.
        const auto eventTagA =
            m_Registry.GetComponent<SpeciesTag>(a_event.Subject);
        const auto eventTagB =
            m_Registry.GetComponent<SpeciesTag>(a_event.Other);

        if (!eventTagA || !eventTagB
            || eventTagA->Value == Species::Animal
            || eventTagB->Value == Species::Animal)
        {
            return;
        }

        const auto dToOther = DispositionOf(a_event.Subject, a_event.Other);
        const auto dOtherToMe = DispositionOf(a_event.Other, a_event.Subject);

        // The family gate (0.7.5 field find), the same rule the
        // reconcile pass applies: a child never romances anyone (kin by
        // species), a curated kin pair (the vanilla families) never
        // romances either — family can be friends, never lovers — and
        // a companion (HasBeenCompanionFaction) never romances either:
        // friends and feuds are fine, the dating pool is closed.
        const bool kin =
            eventTagA->Value == Species::Child
            || eventTagB->Value == Species::Child
            || m_Kin.contains(
                Bonds::PairKey(a_event.Subject, a_event.Other))
            || m_Registry.GetComponent<CompanionTag>(a_event.Subject)
            || m_Registry.GetComponent<CompanionTag>(a_event.Other);

        Bonds::ApplyPair(
            m_Bonds,
            Bonds::PairKey(a_event.Subject, a_event.Other),
            dToOther, dOtherToMe,
            m_BondThresholds,
            a_event.Day,
            [this](
                LCE::Simulation::EntityId a_entityA,
                LCE::Simulation::EntityId a_entityB,
                Bonds::BondKind a_old,
                Bonds::BondKind a_new,
                std::uint64_t a_sinceDay)
            {
                OnBondChange(a_entityA, a_entityB, a_old, a_new, a_sinceDay);
            },
            kin);
    }

    float Adapter::DispositionOf(
        LCE::Simulation::EntityId a_from,
        LCE::Simulation::EntityId a_to)
    {
        const auto relationships =
            m_Registry.GetComponent<LCE::Simulation::Relationships>(a_from);

        if (!relationships)
        {
            return 0.0f;
        }

        const auto iterator = relationships->ByEntity.find(a_to);

        return iterator != relationships->ByEntity.end()
            ? iterator->second.Disposition
            : 0.0f;
    }

    void Adapter::EquipBabyBundle(
        LCE::Simulation::EntityId a_mother,
        LCE::Simulation::EntityId a_child)
    {
        // The 0.8.9 birth journey: when a birth fires, the mother
        // visibly carries her newborn — a swaddled-bundle armor in the
        // body slot. With the baby mod loaded, the bundle is one of
        // its ethnicity variants (random per household); without it,
        // the game's own Shaun bundle (babybundled, Fallout4.esm)
        // fills in — the very item the mod's bundles are copies of
        // (same body slot, same isPlayerChild keyword, surveyed
        // 2026-08-15). The sim never touches the mod's scripts; it
        // equips the item like any armor, and the holding flavor plays
        // on the player while the item itself is the NPC's visible
        // carry. The gate is BirthVisible alone — everyone gets a
        // visible bundle, mod or not.
        const auto motherForm = m_Translator.FormFor(a_mother);

        if (motherForm == 0)
        {
            return;
        }

        if (!m_Settings.BirthVisible)
        {
            return;
        }

        auto* motherActor =
            RE::TESForm::GetFormByID<RE::Actor>(motherForm);

        if (motherActor == nullptr || motherActor->Get3D() == nullptr)
        {
            return;   // not loaded — the bundle can't be seen anyway
        }

        std::uint32_t bundleForm = 0;

        if (BabyModLoaded())
        {
            // The bundle variants the baby mod provides (surveyed from
            // the plugin, 2026-08-15): five ethnicities, clean and
            // wasteland. One is picked at random so two households
            // don't carry the same-looking baby.
            const std::uint32_t bundleRecordIds[] = {
                0x004c95,   // Cyber_babybundled_Asian
                0x004c93,   // Cyber_babybundled_Asian_Wastelander
                0x001734,   // Cyber_babybundled_Black
                0x004c89,   // Cyber_babybundled_Black2
                0x004c87,   // Cyber_babybundled_Black2_Wastelander
                0x0035a1,   // Cyber_babybundled_Black_Wastelander
                0x004c8d,   // Cyber_babybundled_Caucasian
                0x004c8b,   // Cyber_babybundled_Caucasian_Wastelander
                0x004c91,   // Cyber_babybundled_Hispanic
                0x004c8f,   // Cyber_babybundled_Hispanic_Wastelander
            };

            const auto& pick = bundleRecordIds[
                static_cast<std::size_t>(m_Rng.Next())
                % (sizeof(bundleRecordIds)
                   / sizeof(bundleRecordIds[0]))];
            bundleForm = BabyForm(pick);
        }
        else
        {
            // The vanilla fallback (no baby mod): Shaun's own bundle
            // from Fallout4.esm — the item you carry in the intro. The
            // mod's bundles are copies of this exact armor, so the
            // equip path is identical; every household's bundle just
            // looks the same (they all carry a Shaun).
            bundleForm = 0x000f468e;   // babybundled
        }

        if (bundleForm == 0)
        {
            return;   // the mod vanished mid-session — sim-only child
        }

        // Equip the bundle: the game's own equip path. The item first
        // enters the mother's inventory (AddObjectToContainer), then
        // ActorEquipManager equips it in the body slot. Force equip so
        // the carry wins even if she holds something.
        auto* bundle = RE::TESForm::GetFormByID<RE::TESBoundObject>(
            bundleForm);

        if (bundle == nullptr)
        {
            return;
        }

        motherActor->AddObjectToContainer(
            bundle, nullptr, 1, nullptr,
            RE::ITEM_REMOVE_REASON::kNone);

        RE::BGSObjectInstance object{ bundle, nullptr };

        RE::ActorEquipManager::GetSingleton()->EquipObject(
            motherActor, object, 0, 1, nullptr,
            false, true, true, true, false);

        // The holding flavor (0.8.9 field find): the bundle's armor
        // mesh is empty — the visible baby is the AnimObject the
        // AnimFlavorHoldingBaby flavor attaches. The baby mod's
        // bundles do this via their own OnEquipped script; the vanilla
        // fallback has none, so the flavor is applied here for every
        // carry — idempotent next to the mod's own call.
        ApplyHoldingFlavor(motherActor, true);

        // The hold rides the co-save (v10): mother form id, bundle
        // form id, born day — so a mid-carry survives save/load, and
        // the advance can shed the bundle and spawn the child later.
        m_BabyHolds[motherForm] = TLC::CoSave::BabyHold{
            motherForm, bundleForm, CurrentDay() };

        REX::INFO(
            "birth: {} carries her newborn {} — the bundle is visible.",
            MindLabel(a_mother), MindLabel(a_child));
    }

    void Adapter::AdvanceBabyHolds()
    {
        // The 0.8.9 birth journey, day two: once a hold has aged past
        // sim.baby.holdDays, the bundle comes off — the holding flavor
        // clears with it (no invisible cradling) — and the sim child
        // goes on growing inside the household. The child stays
        // sim-only: the visible child is the baby mod's own (the mod
        // grows its placed babies; it never spawns children from
        // scratch, and the game's low-level actor spawn cannot run the
        // full actor init — see the shed body's field findings). The
        // sim's journey is untouched by the baby mod's own growth
        // scripts; this is the mod working as the author intended (the
        // baby ages at its crib), just with the sim's child as its
        // memory.
        std::vector<std::uint32_t> due;

        for (const auto& [motherForm, hold] : m_BabyHolds)
        {
            // The clock never runs backwards — but a restored save
            // could carry a born day ahead of the current day (a
            // reverted save, a mod rolled back); the underflow guard
            // keeps such a hold young rather than instantly due.
            if (hold.BornDay > CurrentDay())
            {
                continue;
            }

            const auto age = CurrentDay() - hold.BornDay;

            if (age >= static_cast<std::uint64_t>(
                    m_Settings.BabyHoldDays))
            {
                due.push_back(motherForm);
            }
        }

        for (const auto& motherForm : due)
        {
            const auto it = m_BabyHolds.find(motherForm);

            if (it == m_BabyHolds.end())
            {
                continue;
            }

            const auto hold = it->second;
            m_BabyHolds.erase(it);

            auto* motherActor =
                RE::TESForm::GetFormByID<RE::Actor>(motherForm);

            if (motherActor != nullptr)
            {
                // The bundle comes off — the game's own unequip path.
                auto* bundle = RE::TESForm::GetFormByID<RE::TESBoundObject>(
                    hold.BundleFormId);

                if (bundle != nullptr)
                {
                    RE::BGSObjectInstance object{ bundle, nullptr };

                    RE::ActorEquipManager::GetSingleton()->UnequipObject(
                        motherActor, &object, 1, nullptr, 0,
                        false, true, true, true, nullptr);

                    RE::TESObjectREFR::RemoveItemData data{
                        bundle, 1 };
                    motherActor->RemoveItem(data);

                    // The hold ends — the holding flavor comes off
                    // with the bundle, so the mother doesn't keep
                    // cradling an invisible baby beside the child.
                    ApplyHoldingFlavor(motherActor, false);
                }
            }

            // Find the sim-only child of THIS mother born on this day.
            // The 0.8.9 field find: matching on the birth day alone
            // grabbed the FIRST child born that day, so two mothers
            // whose births landed on the same day both paired with the
            // same child mind — Samuel King's newborn and Ivy
            // Rodriguez's newborn both became "Aria Parker", one child
            // mis-named and the other left without its mind. The child
            // stores both parents in its Relationships component
            // (Birth::Create), so the match must require the mother's
            // relationship, not just the day.
            const auto motherEntity = m_Translator.EntityFor(motherForm);

            LCE::Simulation::EntityId simChild;

            m_Registry.ForEachWithComponent<BirthDay>(
                [&](LCE::Simulation::EntityId a_entity,
                    BirthDay& a_birth)
                {
                    if (simChild.IsValid() || a_birth.Day != hold.BornDay)
                    {
                        return;
                    }

                    const auto species =
                        m_Registry.GetComponent<SpeciesTag>(a_entity);

                    if (species == nullptr
                        || species->Value != Species::Child)
                    {
                        return;
                    }

                    // The mother's own child: the child's Relationships
                    // name both parents (0.6.0 Stone 3 pair machinery),
                    // so a birth-day match that is not this mother's
                    // child is skipped.
                    const auto rels = m_Registry
                        .GetComponent<LCE::Simulation::Relationships>(
                            a_entity);

                    if (rels == nullptr
                        || !rels->ByEntity.contains(motherEntity))
                    {
                        return;
                    }

                    simChild = a_entity;
                });

            if (!simChild.IsValid())
            {
                continue;
            }

            // The child's journey (0.8.9, the deferred-spawn find): the
            // visible child is a real child actor spawned at the
            // mother's feet — DELIBERATELY left un-initialized. A ref
            // created this way is invisible while its cell holds it;
            // the game's own save/load routine completes the actor
            // (facegen, AI process, animation) the next time the world
            // reloads, and the child steps out fully real — the
            // 2026-08-16 field find: exactly this happened, the
            // invisible child "became real" on load, with only the
            // clothes missing. Forcing the init immediately (the
            // clearStillLoadingFlag/Load3D route) instead produces the
            // half-born headless, T-posing, immobile child that even
            // survives saves; the game's own PlaceAtMe dispatch is the
            // documented PackVariables CTD. So: deferred, real after a
            // load. The per-tick pass dresses the child the moment it
            // reads fully initialized. The sim child keeps its name,
            // mind, and household — the actor is its body only; it
            // never joins the settler faction, so the census never
            // seeds a duplicate mind for it.
            REX::INFO(
                "birth: {} carries her child no longer — {} is born. "
                "She will appear in the world after the next load "
                "(the deferred spawn; the sim child grows meanwhile).",
                MindLabelForm(motherForm), MindLabel(simChild));

            PushNews(
                MindLabelForm(motherForm)
                + "\u2019s child is growing up.",
                Tuning::AdapterSettings::NewsCategory::Birth);

            // The visible child (0.8.9, deferred): gated on
            // sim.baby.visualChild; skipped when the mother is not
            // loaded (her position is where the child must appear).
            if (m_Settings.VisualChild
                && !m_VisualChildren.contains(motherForm))
            {
                auto* mother = RE::TESForm::GetFormByID<RE::Actor>(
                    motherForm);

                if (mother != nullptr)
                {
                    // Gender-matched base: the sim child's own
                    // deterministic gender (Names::GenderOf — the same
                    // draw that named it) picks the pool, so a son gets
                    // a male base and a daughter a female one.
                    const bool male =
                        TLC::Names::GenderOf(simChild)
                        == TLC::Names::Gender::Male;

                    const auto& pool = male
                        ? kMaleChildBases : kFemaleChildBases;
                    const auto basePick = pool[
                        static_cast<std::size_t>(m_Rng.Next())
                        % (sizeof(pool) / sizeof(pool[0]))];

                    auto* childBase = RE::TESForm::GetFormByID<RE::TESNPC>(
                        basePick);

                    if (childBase != nullptr)
                    {
                        // Dress the child at the base — the game's own
                        // child-clothing path. The deferred spawn reads
                        // the base NPC's default outfit (WNAM /
                        // defOutfit) when the child materializes, so
                        // setting it here means the game itself applies
                        // the clothes at init: no runtime-equip
                        // reversion risk, and it survives every reload.
                        // (The bundles are the orphaned ChildOutfit*
                        // OTFTs — see the pool comment above.)
                        const auto& outfitPool = male
                            ? kChildOutfitMale : kChildOutfitFemale;

                        auto* outfit = RE::TESForm::GetFormByID<RE::BGSOutfit>(
                            outfitPool[
                                static_cast<std::size_t>(m_Rng.Next())
                                % (sizeof(outfitPool) / sizeof(outfitPool[0]))]);

                        if (outfit != nullptr)
                        {
                            childBase->defOutfit = outfit;
                        }

                        auto* cell = mother->GetParentCell();

                        // The child appears a step ahead of the mother,
                        // facing the same way.
                        RE::NEW_REFR_DATA spawn;
                        spawn.location = mother->GetPosition();
                        spawn.direction = mother->data.angle;
                        spawn.object = childBase;
                        spawn.interior = cell;

                        // The cell's own world — the game's full spawn
                        // data (an exterior cell carries its worldspace;
                        // an interior's is null). Without it the child
                        // can land in the wrong worldspace.
                        spawn.world = cell != nullptr
                            ? cell->worldSpace : nullptr;
                        spawn.forcePersist = true;

                        // THE deferred bit: clearStillLoadingFlag stays
                        // FALSE. A freshly-created ref flagged as still
                        // loading defers its full initialization; the
                        // game's own load routine completes it — the
                        // invisible-child find. Forcing it (true) is the
                        // headless/T-pose path.
                        spawn.clearStillLoadingFlag = false;

                        const auto handle =
                            RE::TESDataHandler::GetSingleton()
                                ->CreateReferenceAtLocation(spawn);

                        if (const auto child = handle.get().get();
                            child != nullptr)
                        {
                            // The name rides the extra data — it needs
                            // no 3D or AI, so the sim child's name is
                            // on the actor from the first moment (the
                            // earlier field test: names were right on
                            // the materialized children).
                            const auto name =
                                m_Registry.GetComponent<Name>(simChild);

                            if (name != nullptr && !name->Full.empty())
                            {
                                ApplyActorName(
                                    child->GetFormID(), name->Full);
                            }

                            TLC::CoSave::VisualChild entry;
                            entry.MotherFormId = motherForm;
                            entry.FigureFormId = child->GetFormID();
                            entry.BornDay = hold.BornDay;
                            m_VisualChildren[motherForm] = entry;

                            REX::INFO(
                                "birth: {} is born at {}'s feet — the "
                                "visible child (deferred; appears after "
                                "a load).",
                                MindLabel(simChild),
                                MindLabelForm(motherForm));
                        }
                    }
                }
            }
        }
    }

    void Adapter::AdvanceVisualChildren()
    {
        // The materialize pass (0.8.9, deferred-spawn find): each
        // spawned child is invisible until the game's own load routine
        // completes it. This pass waits for that moment — 3D loaded AND
        // an AI process (the exact things the half-born child lacked) —
        // and dresses the child from the game's child-outfit pool. The
        // equip only sticks on a fully-initialized actor, which is why
        // the earlier attempts left children in their pants: they
        // dressed half-born actors. Once dressed, the record retires.
        // A child whose actor is gone (a cell reset, a mod) is dropped
        // and its record retired; a child that never materializes stays
        // invisible but keeps its sim life.
        if (m_VisualChildren.empty())
        {
            return;
        }

        const auto now = std::chrono::steady_clock::now();

        if (now - m_LastVisualChildFollow < std::chrono::milliseconds(
                static_cast<std::int64_t>(kVisualChildCheckSeconds * 1000.0)))
        {
            return;
        }

        m_LastVisualChildFollow = now;

        for (auto it = m_VisualChildren.begin(); it != m_VisualChildren.end();)
        {
            auto* child = RE::TESForm::GetFormByID<RE::Actor>(
                it->second.FigureFormId);

            if (child == nullptr)
            {
                ++it;   // not loaded yet — keep waiting
                continue;
            }

            // Fully initialized: 3D loaded AND an AI process. The
            // half-born child had neither (no head, no movement); a
            // real, loaded NPC has both. Until then the child is still
            // invisible — wait.
            if (child->Get3D() == nullptr
                || child->currentProcess == nullptr)
            {
                ++it;
                continue;
            }

            // Dress the child. The base's defOutfit (set at spawn) is
            // the game's own path — the materialized child is usually
            // already dressed by it, and this pass only confirms. The
            // fallback (a child whose base predates the fix, or a base
            // reloaded from the ESM after a restart) equips the real
            // ARMOs inside the bundle here — the equip lands on a
            // fully-initialized actor. The bundle is carried onto the
            // base so later inits (cell reloads) stay dressed too.
            auto* childBase = child->GetNPC();

            RE::BGSOutfit* outfit = nullptr;

            if (childBase != nullptr && childBase->defOutfit != nullptr)
            {
                outfit = childBase->defOutfit;
            }
            else
            {
                const bool male = childBase != nullptr
                    && childBase->GetSex() == RE::SEX::kMale;

                const auto& pool = male
                    ? kChildOutfitMale : kChildOutfitFemale;

                outfit = RE::TESForm::GetFormByID<RE::BGSOutfit>(
                    pool[static_cast<std::size_t>(m_Rng.Next())
                         % (sizeof(pool) / sizeof(pool[0]))]);

                if (outfit != nullptr && childBase != nullptr)
                {
                    childBase->defOutfit = outfit;
                }
            }

            if (outfit != nullptr)
            {
                for (auto* item : outfit->outfitItems)
                {
                    auto* armor = item != nullptr
                        ? item->As<RE::TESObjectARMO>() : nullptr;

                    if (armor == nullptr)
                    {
                        continue;   // not a wearable — skip it
                    }

                    RE::BGSObjectInstance object{ armor, nullptr };

                    RE::ActorEquipManager::GetSingleton()->EquipObject(
                        child, object, 0, 1, nullptr,
                        false, true, true, true, false);
                }
            }

            REX::INFO(
                "birth: the child appears in the world — dressed and "
                "fully real (the deferred spawn completed).");

            PushNews(
                "A child appears in the world.",
                Tuning::AdapterSettings::NewsCategory::Birth);

            it = m_VisualChildren.erase(it);
        }
    }

    void Adapter::RestoreVisualChildren(
        const std::vector<TLC::CoSave::VisualChild>& a_visualChildren)
    {
        // The visual children (v11): restore by form ids — the per-tick
        // follow re-engages each figure as its mother streams in.
        m_VisualChildren.clear();

        for (const auto& entry : a_visualChildren)
        {
            if (entry.MotherFormId == 0 || entry.FigureFormId == 0)
            {
                continue;
            }

            m_VisualChildren[entry.MotherFormId] = entry;
        }

        if (!m_VisualChildren.empty())
        {
            REX::INFO(
                "birth: {} visual child figure{} restored — each follows "
                "its mother again.",
                m_VisualChildren.size(),
                m_VisualChildren.size() == 1 ? "" : "s");
        }
    }

    std::vector<TLC::CoSave::VisualChild>
    Adapter::VisualChildrenForSave() const
    {
        std::vector<TLC::CoSave::VisualChild> result;
        result.reserve(m_VisualChildren.size());

        for (const auto& [motherForm, entry] : m_VisualChildren)
        {
            result.push_back(entry);
        }

        return result;
    }

    void Adapter::RunMediation()
    {
        AttemptMediation();
    }

    void Adapter::AttemptMediation()
    {
        // The feud pairs the settlement knows: every pair in the bond
        // book at the enemy line. A feud is a story the world heard —
        // strangers do not step in, but the settlement can try.
        std::vector<std::pair<LCE::Simulation::EntityId,
            LCE::Simulation::EntityId>> feuds;

        for (const auto& [key, bond] : m_Bonds)
        {
            if (bond.Kind != Bonds::BondKind::Enemy)
            {
                continue;
            }

            feuds.emplace_back(
                LCE::Simulation::EntityId{ key.first },
                LCE::Simulation::EntityId{ key.second });
        }

        if (feuds.empty())
        {
            return;
        }

        const auto attempts = Arcs::Mediate(m_Registry, feuds, &m_Rng);

        for (const auto& attempt : attempts)
        {
            const auto mediator = m_Translator.FormFor(attempt.Mediator);
            const auto enemyA = m_Translator.FormFor(attempt.EnemyA);
            const auto enemyB = m_Translator.FormFor(attempt.EnemyB);

            if (mediator == 0 || enemyA == 0 || enemyB == 0)
            {
                continue;
            }

            if (attempt.Cooled)
            {
                REX::INFO(
                    "arcs: {} cooled the feud between {} and {} "
                    "— the settlement pulls its own apart.",
                    MindLabelForm(mediator),
                    MindLabelForm(enemyA), MindLabelForm(enemyB));

                PushNews(
                    MindLabelForm(mediator) + " cooled the feud between "
                    + MindLabelForm(enemyA) + " and "
                    + MindLabelForm(enemyB) + ".",
                    Tuning::AdapterSettings::NewsCategory::Bonds);
            }
            else
            {
                REX::INFO(
                    "arcs: {} tried to cool the feud between {} "
                    "and {} — nobody listened.",
                    MindLabelForm(mediator),
                    MindLabelForm(enemyA), MindLabelForm(enemyB));
            }
        }
    }

    void Adapter::RunBirth()
    {
        // Phase 1: check for due births — pregnancies that have reached
        // their due day. Each produces a child mind and a news line.
        const auto newborns = Birth::CheckBirths(
            m_Registry, m_Settings.Rates, CurrentDay());

        for (const auto& child : newborns)
        {
            // Name the child (0.7.0 identity stone).
            const auto mother = FindMother(child);
            const auto father = FindFather(child);

            std::string family;

            if (const auto parentName =
                    m_Registry.GetComponent<Name>(mother))
            {
                family = std::string(TLC::Names::FamilyOf(parentName->Full));
            }

            const auto childGender = TLC::Names::GenderOf(child);

            const auto childName = family.empty()
                ? TLC::Names::GenerateUnique(
                    m_UsedNames, child, m_Names, childGender)
                : TLC::Names::ChildName(
                    family, child, m_Names, childGender);

            m_Registry.AddComponent<Name>(child, Name{ childName });
            m_UsedNames.insert(childName);

            // Inherit memories from both parents (0.7.0 legacy).
            const auto accept =
                [](const LCE::Simulation::MemoryEvent& a_event)
                {
                    return a_event.Other.IsValid();
                };

            LCE::Simulation::InheritMemory(
                m_Registry, child, mother, m_CoreTuning,
                LCE::Simulation::WorldTime{ CurrentDay() }, accept);
            LCE::Simulation::InheritMemory(
                m_Registry, child, father, m_CoreTuning,
                LCE::Simulation::WorldTime{ CurrentDay() }, accept);

            REX::INFO(
                "birth: a child is born to {} and {} — {}, a new mind, "
                "fed by the household.",
                MindLabel(mother), MindLabel(father),
                MindLabel(child));

            PushNews(
                "a child was born to " + MindLabel(mother) + " and "
                + MindLabel(father) + " — " + childName + ".",
                Tuning::AdapterSettings::NewsCategory::Birth);

            // The visible journey (0.8.9 birth journey): when the
            // flag is on, the mother visibly carries her newborn — the
            // mod's swaddled bundle when loaded, the game's own Shaun
            // bundle otherwise (everyone gets a carry). The hold
            // advances two days later: the bundle comes off and a
            // child from the game's own pool takes its place.
            EquipBabyBundle(mother, child);
        }

        // Phase 2: roll for new conceptions among eligible couples.
        // One roll per eligible pair per day; the chance is tunable.
        std::vector<std::pair<LCE::Simulation::EntityId,
            LCE::Simulation::EntityId>> couples;

        for (const auto& [key, bond] : m_Bonds)
        {
            if (bond.Kind != Bonds::BondKind::Spouse)
            {
                continue;
            }

            const auto a = LCE::Simulation::EntityId{ key.first };
            const auto b = LCE::Simulation::EntityId{ key.second };

            if (m_Translator.FormFor(a) == 0
                || m_Translator.FormFor(b) == 0)
            {
                continue;  // one or both not in the world
            }

            // Species gate: only humans conceive. Animals, robots,
            // super mutants, ghouls — they live, they bond, but
            // they don't add to the population.
            const auto spA = m_Registry.GetComponent<SpeciesTag>(a);
            const auto spB = m_Registry.GetComponent<SpeciesTag>(b);

            if ((spA == nullptr || spA->Value != Species::Human)
                || (spB == nullptr || spB->Value != Species::Human))
            {
                continue;
            }

            couples.emplace_back(a, b);
        }

        for (const auto& [parentA, parentB] : couples)
        {
            // Roll: the LCG provides a uniform [0,1) float.
            const auto roll =
                static_cast<float>(m_Rng.Next() & 0xFFFFFF)
                / static_cast<float>(0xFFFFFF);

            if (roll >= m_Settings.BirthChance)
            {
                continue;  // no conception this day
            }

            const auto mother = Birth::TryConceive(
                m_Registry, parentA, parentB,
                m_Settings.BirthChance,
                m_Settings.BirthGestation,
                CurrentDay());

            if (mother.IsValid())
            {
                REX::INFO(
                    "birth: {} and {} are expecting — due on day {}.",
                    MindLabel(parentA), MindLabel(parentB),
                    CurrentDay()
                        + static_cast<std::uint64_t>(
                            m_Settings.BirthGestation));

                PushNews(
                    MindLabel(parentA) + " and "
                    + MindLabel(parentB) + " are expecting a child.",
                    Tuning::AdapterSettings::NewsCategory::Birth);
            }
        }
    }

    //-------------------------------------------------------------------------
    // FindMother / FindFather — given a child entity, look up its
    // parents from the Pregnancy record that created it (or from the
    // Relationships component if the Pregnancy was already consumed).
    //-------------------------------------------------------------------------
    LCE::Simulation::EntityId Adapter::FindMother(
        LCE::Simulation::EntityId a_child) const
    {
        // The Relationships component stores both parents.
        const auto rels =
            m_Registry.GetComponent<LCE::Simulation::Relationships>(a_child);

        if (rels != nullptr)
        {
            // The parent with the stronger bond is the mother
            // (convention — both are equal in the sim).
            LCE::Simulation::EntityId best;
            float bestStrength = -1.0f;

            for (const auto& [eid, rel] : rels->ByEntity)
            {
                if (rel.Disposition > bestStrength)
                {
                    bestStrength = rel.Disposition;
                    best = eid;
                }
            }

            return best;
        }

        return {};
    }

    LCE::Simulation::EntityId Adapter::FindFather(
        LCE::Simulation::EntityId a_child) const
    {
        const auto mother = FindMother(a_child);
        const auto rels =
            m_Registry.GetComponent<LCE::Simulation::Relationships>(a_child);

        if (rels != nullptr)
        {
            for (const auto& [eid, rel] : rels->ByEntity)
            {
                if (eid != mother)
                {
                    return eid;
                }
            }
        }

        return {};
    }

    void Adapter::OnBondChange(
        LCE::Simulation::EntityId a_entityA,
        LCE::Simulation::EntityId a_entityB,
        Bonds::BondKind a_old,
        Bonds::BondKind a_new,
        std::uint64_t)
    {
        const auto formA = m_Translator.FormFor(a_entityA);
        const auto formB = m_Translator.FormFor(a_entityB);

        if (formA == 0 || formB == 0)
        {
            return;   // defensive — a translated pair names two minds
        }

        // The identity stone's voice (0.7.0 Stone 1): the world speaks
        // in people now — the name with its console hex beside it. The
        // species label stays for the household section below.
        const auto labelA = SpeciesLabel(
            m_Registry.GetComponent<SpeciesTag>(a_entityA).get());
        const auto labelB = SpeciesLabel(
            m_Registry.GetComponent<SpeciesTag>(a_entityB).get());
        const auto nameA = MindLabel(a_entityA);
        const auto nameB = MindLabel(a_entityB);

        // The world's voice: formation, change, and dissolution each get
        // their line. The feud line is Life.md's own: "X is feuding
        // with Y." — only the crossing into Enemy says it; the rest use
        // the pair's plural.
        if (a_new == Bonds::BondKind::None)
        {
            if (Bonds::IsNegative(a_old))
            {
                REX::INFO(
                    "bonds: {} and {} made peace.",
                    nameA, nameB);

                PushNews(
                    nameA + " and " + nameB + " made peace.",
                    Tuning::AdapterSettings::NewsCategory::Bonds);
            }
            else
            {
                REX::INFO(
                    "bonds: {} and {} are no longer {}.",
                    nameA, nameB, Bonds::BondPlural(a_old));
            }
        }
        else if (a_new == Bonds::BondKind::Enemy)
        {
            // Any crossing into Enemy is a feud — whether it jumped the
            // whole book in one blow (None -> Enemy) or cooled down the
            // rivalry (Rival -> Enemy, the normal shut-stall path).
            REX::INFO(
                "bonds: {} is feuding with {}.",
                nameA, nameB);
        }
        else if (a_old == Bonds::BondKind::None)
        {
            REX::INFO(
                "bonds: {} and {} became {}.",
                nameA, nameB, Bonds::BondPlural(a_new));
        }
        else
        {
            REX::INFO(
                "bonds: {} and {} are now {}.",
                nameA, nameB, Bonds::BondPlural(a_new));
        }

        // The household follows the deepest bond (0.6.0 Stone 3): the
        // moment a pair becomes spouses, their pouches become one shared
        // wallet; when the marriage dissolves, the wallet splits. Formed
        // exactly once — the pair rests at Spouse afterwards, so neither
        // the event channel nor the 1-second pass says it again.
        //
        // The one-wallet-per-mind guard (the polygamy edge, 2026-08-11):
        // the bond layer may honestly read Spouse to two minds at once
        // (pure derived disposition — a beloved settler can cross +0.8
        // with two people), but a second merge would fold a third pouch
        // into the shared wallet and a later 2-way split would vanish
        // the third member's caps. The second marriage stands as a bond
        // (the family bench still feeds both spouses); the wallets stay
        // personal.
        if (a_new == Bonds::BondKind::Spouse
            && a_old != Bonds::BondKind::Spouse)
        {
            if (Households::InHousehold(
                    m_Registry, m_Bonds, a_entityA)
                || Households::InHousehold(
                    m_Registry, m_Bonds, a_entityB))
            {
                REX::INFO(
                    "households: {} and {} are spouses, but one "
                    "already shares a household — the first pouch stands; "
                    "their wallets stay personal.",
                    nameA, nameB);
            }
            else if (Households::FormHousehold(
                    m_Registry, a_entityA, a_entityB))
            {
                REX::INFO(
                    "households: {} and {} are now a household — one pouch, one bench.",
                    nameA, nameB);
            }
        }

        if (a_old == Bonds::BondKind::Spouse
            && a_new != Bonds::BondKind::Spouse)
        {
            std::uint32_t holderShare = 0;
            std::uint32_t otherShare = 0;

            if (Households::DissolveHousehold(
                    m_Registry, a_entityA, a_entityB,
                    holderShare, otherShare))
            {
                REX::INFO(
                    "households: {} and {} are no longer a household — the pouch splits ({} / {} caps).",
                    nameA, nameB, holderShare, otherShare);
            }
        }

        // The settlement hears its own news (0.6.0 Stone 4 — gossip):
        // a bond crossing — friend, sweetheart, spouse, rival, enemy —
        // names both participants to every mind. Strangers and fresh
        // arrivals never hear it (gossip is written once, not replayed).
        // Deaths spread through the same channel in RemoveMind. The
        // player window (0.7.0 Stone 3) reads the same crossings as
        // news: formations are the world's headlines.
        if (a_new != Bonds::BondKind::None
            && a_old == Bonds::BondKind::None)
        {
            Gossip::SpreadBond(
                m_Registry, a_entityA, a_entityB,
                LCE::Simulation::InteractionKind::Social,
                CurrentDay());

            if (a_new == Bonds::BondKind::Rival)
            {
                PushNews(
                    nameA + " and " + nameB + " became rivals.",
                    Tuning::AdapterSettings::NewsCategory::Bonds);
            }
            else if (a_new == Bonds::BondKind::Friend)
            {
                PushNews(
                    nameA + " and " + nameB + " became friends.",
                    Tuning::AdapterSettings::NewsCategory::Bonds);
            }
            else if (a_new == Bonds::BondKind::Sweetheart)
            {
                PushNews(
                    nameA + " and " + nameB + " are sweethearts.",
                    Tuning::AdapterSettings::NewsCategory::Bonds);
            }
            else if (a_new == Bonds::BondKind::Spouse)
            {
                PushNews(
                    nameA + " and " + nameB + " are married.",
                    Tuning::AdapterSettings::NewsCategory::Bonds);
            }
        }

        // The feud headline (0.7.0 Stone 2): any crossing into Enemy
        // makes the papers, not just a direct jump from nothing. The
        // slow rival -> enemy path is how shut-stall feuds actually
        // arrive, and the world should hear them too.
        if (a_new == Bonds::BondKind::Enemy
            && a_old != Bonds::BondKind::Enemy)
        {
            PushNews(
                nameA + " is feuding with " + nameB + ".",
                Tuning::AdapterSettings::NewsCategory::Bonds);

            // The settlement hears the feud (Gossip.h's intent: gossip
            // covers "a feud starts") — the formation gossip from the
            // rival stage has long faded by now (memory fade 0.2/s),
            // and without a fresh spread no third mind could ever know
            // the pair well enough to step in.
            Gossip::SpreadBond(
                m_Registry, a_entityA, a_entityB,
                LCE::Simulation::InteractionKind::Social,
                CurrentDay());

            // And the settlement tries, now — while the news is still
            // alive. The once-per-day pass alone can never reach a feud:
            // by the next day turn the gossip has faded to nothing and
            // no mediator can be found. A feud is mediated the moment it
            // breaks, while everyone still knows.
            AttemptMediation();
        }
    }

    void Adapter::ApplyRestore(LCE::Simulation::RegistrySnapshot a_snapshot)
    {
        // End whatever was running (the pre-load already did, defensively)
        // — Clear keeps the serializers, registered once at init.
        EndWorld();

        m_Registry.Restore(a_snapshot);

        // The decay-jitter wiring (engine stone 07): resume the saved
        // world's randomness — its stream, exactly where it left off.
        // (For a v1 record the decode left the default seed untouched,
        // which is honest: that world never had a saved stream.)
        m_Rng.SetState(m_PendingRngState);

        // The arcs' day gates are session state — a restore must not
        // re-fire today's birth or mediation. The max sentinel says
        // "never ran", which is exactly wrong after a reload: the first
        // tick would birth a child and re-mediate every feud even
        // though the day hasn't turned (the 2026-08-11 restore-birth:
        // a child 1 s after every load). Seeded to today — the world
        // has already had its day's chances; tomorrow turns the gates
        // again.
        m_LastMediationDay = CurrentDay();
        m_LastBirthDay = CurrentDay();

        // Rebuild the edge's memory: which form is which entity, from the
        // restored FormRef components. The translator is adapter state,
        // not core state — it never rides inside the snapshot.
        m_Registry.ForEachWithComponent<FormRef>(
            [this](LCE::Simulation::EntityId a_entity, FormRef& a_formRef)
            {
                m_Translator.Add(a_formRef.FormId, a_entity);
            });

        // The device prune (0.7.2 fix): a polluted co-save holds the
        // workshop's props as minds. Runs now — the translator is up, but
        // pouches, names, and bonds are not rebuilt yet, so a pruned
        // prop never carries a wallet, a name, or a feud.
        PruneDeviceMinds();

        // The species is game truth (ADR-0024): a mind whose stored tag
        // disagrees with its actor's race — a behemoth saved as Human
        // before the 0.7.5 classification fix — is corrected now, before
        // pouches, keepers, and bonds restore. The fix's point: a
        // restored behemoth is fed, not feuding. Actors that load later
        // are corrected by the per-second sweep.
        ReclassifyLoadedMinds();

        // The tuning truth (0.8.x field find): the co-save serializes
        // each mind's DecayRate — a mind's rhythm survives restore — but
        // a save written under old tuning carries those old rates
        // forever, so the INI stops mattering for a restored world (the
        // 0.1/s flood under a 0.002 INI). Re-derive every rate from
        // today's INI * the mind's deterministic jitter: the jitter is
        // id-derived, so the desync survives; the value (where the need
        // sits in its rhythm) stays as saved. The INI is authoritative;
        // a stale save heals on its next load instead of flooding.
        {
            auto reDerived = 0U;

            m_Registry.ForEachWithComponent<LCE::Simulation::Needs>(
                [this, &reDerived](
                    LCE::Simulation::EntityId a_entity,
                    LCE::Simulation::Needs& a_needs)
                {
                    const auto rateFactor = 1.0f + IdJitter(a_entity, 0.40f);

                    for (auto& need : a_needs.List)
                    {
                        float base = 0.0f;

                        switch (need.Type)
                        {
                        case LCE::Simulation::NeedType::Hunger: base = m_Settings.Rates.Hunger; break;
                        case LCE::Simulation::NeedType::Fatigue: base = m_Settings.Rates.Fatigue; break;
                        case LCE::Simulation::NeedType::Safety: base = m_Settings.Rates.Safety; break;
                        case LCE::Simulation::NeedType::Social: base = m_Settings.Rates.Social; break;
                        case LCE::Simulation::NeedType::Comfort: base = m_Settings.Rates.Comfort; break;
                        }

                        need.DecayRate = std::max(0.0f, base * rateFactor);
                    }

                    ++reDerived;
                });

            REX::INFO(
                "restore: re-derived decay rates for {} minds from "
                "today's tuning (hunger base {:.4f}/s) — the INI "
                "governs a restored world",
                reDerived, m_Settings.Rates.Hunger);
        }

        // The economy stone: a restored human without a pouch predates
        // the economy — the record had no caps to carry. Back-fill the
        // seed so a pre-economy save wakes into a living market instead of
        // a world where everyone is broke (a human mind always has a
        // pouch; children and animals never do).
        m_Registry.ForEachWithComponent<SpeciesTag>(
            [this](LCE::Simulation::EntityId a_entity, SpeciesTag& a_tag)
            {
                if (a_tag.Value == Species::Human
                    && !m_Registry.GetComponent<CapPouch>(a_entity))
                {
                    m_Registry.AddComponent<CapPouch>(
                        a_entity, CapPouch{ SeedPouch(a_entity) });
                }
            });

        // The identity stone (0.7.0 Stone 1): a restored mind without a
        // name predates the stone — the record had no name to carry.
        // Back-fill a procedural name (the same deterministic draw the
        // seed uses, deduped against the world's live names) so a
        // pre-0.7 save wakes into a named world instead of a log of hex.
        // Children restored without a name get a plain procedural name;
        // children born after the stone carry their household's family
        // name (the RunBirth path).
        m_UsedNames.clear();

        m_Registry.ForEachWithComponent<Name>(
            [this](LCE::Simulation::EntityId, const Name& a_name)
            {
                m_UsedNames.insert(a_name.Full);
            });

        m_Registry.ForEachWithComponent<SpeciesTag>(
            [this](LCE::Simulation::EntityId a_entity, SpeciesTag& a_tag)
            {
                if (m_Registry.GetComponent<Name>(a_entity) != nullptr)
                {
                    return;
                }

                std::string backfilled;

                if (a_tag.Value == Species::Animal)
                {
                    // The naming rule (the owner stone): only an owned
                    // animal gets a name — a restored stray stays
                    // nameless, its label the species and hex. An owner
                    // is whoever the game assigns (the player, or a
                    // settler); a nameless restored pet is claimed here.
                    const auto formRef =
                        m_Registry.GetComponent<FormRef>(a_entity);
                    auto* actor = formRef
                        ? RE::TESForm::GetFormByID<RE::Actor>(formRef->FormId)
                        : nullptr;

                    if (actor == nullptr || actor->GetOwner() == nullptr)
                    {
                        return;
                    }

                    backfilled = TLC::Names::GenerateUniqueAnimal(
                        m_UsedNames, a_entity, m_Names);
                }
                else
                {
                    // A person's gender: the actor's sex when the form
                    // resolves (a restored world's actors load
                    // gradually), else the id's own draw.
                    auto gender = TLC::Names::GenderOf(a_entity);

                    if (const auto formRef =
                            m_Registry.GetComponent<FormRef>(a_entity))
                    {
                        auto* actor = RE::TESForm::GetFormByID<RE::Actor>(
                            formRef->FormId);

                        if (actor != nullptr)
                        {
                            const auto sex = actor->GetSex();

                            if (sex == RE::SEX::kMale)
                            {
                                gender = TLC::Names::Gender::Male;
                            }
                            else if (sex == RE::SEX::kFemale)
                            {
                                gender = TLC::Names::Gender::Female;
                            }
                        }
                    }

                    backfilled = TLC::Names::GenerateUnique(
                        m_UsedNames, a_entity, m_Names, gender);
                }

                m_Registry.AddComponent<Name>(
                    a_entity, Name{ backfilled });
                m_UsedNames.insert(backfilled);
            });

        // Pet names are unique per world: an old co-save can hold
        // duplicates (the five "Bandit" dogs from earlier builds). The
        // first keeps its name; each later one is re-drawn
        // deterministically against the world's used set — self-heals
        // once, and every future session dedups as it names. The
        // corrected names persist on the next save.
        m_Registry.ForEachWithComponent<Name>(
            [this](LCE::Simulation::EntityId a_entity, Name& a_name)
            {
                const auto tag =
                    m_Registry.GetComponent<SpeciesTag>(a_entity);

                if (tag == nullptr || tag->Value != Species::Animal)
                {
                    return;
                }

                if (!m_UsedNames.insert(a_name.Full).second)
                {
                    a_name.Full = TLC::Names::GenerateUniqueAnimal(
                        m_UsedNames, a_entity, m_Names);
                }
            });

        // The name's visible half after a restore: every mind whose
        // actor is loaded gets its name written onto the actor — the
        // same reconcile-aware rule as the per-second sweep (the base
        // form is the truth; stale generated stamps on game-named NPCs
        // are dropped). The sweep re-applies as the rest stream in.
        ApplyLoadedActorNames();

        // The market was saved with the world (it owns a FormRef); now
        // that the world is back, every mind must remember where to trade
        // again. The seed is a fading memory event (weight 1.0, forgotten
        // in seconds) — a mind saved long after its world woke has already
        // forgotten the market, and without the seed a restored world is
        // market-blind: starving minds Explore instead of walking. The
        // tick's periodic refresh keeps catching minds whose actors load
        // after this instant.
        SeedMarket(true);

        // The conflict source's settlement (0.7.0 Stone 2): every mind
        // with a restored market memory belongs to that settlement's
        // group — the engine's echo spreads a slight (or a warmth)
        // through it, and newcomers inherit the settlement's cold
        // shoulder toward a feud's villain. Derived from the restored
        // memories, never persisted itself.
        AssignSettlementGroups();

        // The stall-keepers stone (v3): a restored market reopens under
        // its saved keeper — the same face behind the bench — instead of
        // whoever happens to arrive first. Runs after the translator
        // rebuild above, so both the market and the keeper resolve.
        RestoreStallKeepers(m_PendingStallKeepers);

        // The bonds stone (v5): a spouse is still a spouse after reload.
        // The book restores by form ids; the 1-second reconcile pass
        // then re-derives — a bond whose relationship drifted below its
        // dissolve line while the game was away dissolves, everything
        // else stands.
        RestoreBonds(m_PendingBonds);

        // The once-per-day conflict gates (v7): a feud is still a
        // once-a-day scene after reload. The gates restore by form ids;
        // a pair whose actor is missing is simply free again.
        RestoreConflictGates(m_PendingGates);

        // The burial book (v8): a death whose mourning window hasn't
        // passed — or one whose window passed while the game was away —
        // is still on the books after reload. The per-second sweep
        // below buries the due ones within a second of the world waking.
        RestoreBurials(m_PendingBurials);

        // The medicine shelves (v9): a stall that sold out before the
        // save stays sold out after the load — the next market day
        // (PushWorldFacts' open transition) refills it.
        RestoreMedicineStock(m_PendingMedicineStock);

        // The newborn holds (v10, 0.8.9 birth journey): a mother
        // mid-carry keeps her bundle after reload — the hold is form
        // ids only, so it restores even before her actor streams in;
        // the daily advance ages it from the restored born day.
        RestoreBabyHolds(m_PendingBabyHolds);

        // The visible children (v11, 0.8.9 deferred-spawn find): the
        // pending child actors restore by form ids — the per-tick
        // materialize pass dresses each the moment it reads fully
        // initialized.
        RestoreVisualChildren(m_PendingVisualChildren);

        // The household stone (v3? no — derived, ADR-0013): a restored
        // marriage re-establishes its shared pouch silently. The loud
        // events fired in OnBondChange when the pair crossed the line in
        // life; here, the invariant is just repaired — one pouch per
        // married pair, one per unmarried human.
        Households::Enforce(m_Registry, m_Bonds);

        REX::INFO(
            "The Commonwealth wakes up: {} minds restored from the co-save.",
            a_snapshot.Entities.size());
        LCE::Logging::Info(
            "The Commonwealth wakes up: " + std::to_string(a_snapshot.Entities.size())
            + " minds restored from the co-save.");

        const auto children = CountSimOnlyChildren();

        if (children > 0)
        {
            REX::INFO(
                "The Commonwealth wakes up: {} sim-only {} restored too — fed by their households.",
                children, children == 1 ? "child" : "children");
            LCE::Logging::Info(
                "The Commonwealth wakes up: " + std::to_string(children)
                + (children == 1 ? " sim-only child restored too — fed by its household."
                                 : " sim-only children restored too — fed by their households."));
        }

        LCE::Logging::Flush();

        m_Started = true;
    }

    void Adapter::PreLoadGame()
    {
        EndWorld();
        m_AwaitingLoad = true;
        m_WorldEndedAt = std::chrono::steady_clock::now();

        // A load is starting — its completion event has not been handled.
        m_LoadCompleted = false;
    }

    void Adapter::DeleteGame()
    {
        // A save FILE was deleted — not a world teardown. The sim must
        // keep running: in-game, an autosave rotation deleted a save
        // mid-world and EndWorld killed the sim with no pending load to
        // revive it (m_AwaitingLoad was false, so the 60s abort-revival
        // never fired) — every later save wrote 0 entities and the world
        // stayed dead until a full restart. A new game or a load goes
        // through PreLoadGame/GameLoaded, which own the world's life.
    }

    void Adapter::SeedMind(const RE::Actor* a_actor)
    {
        using namespace LCE::Simulation;

        if (a_actor == nullptr || !IsSimRelevant(a_actor))
        {
            return;
        }

        const auto formId = a_actor->GetFormID();

        // Already a mind — the wake seed and the per-second bookkeeping
        // share this path, and a form id never names both a mind and a
        // workshop (targets carry a FormRef but no SpeciesTag).
        if (m_Translator.EntityFor(formId).IsValid())
        {
            return;
        }

        const auto species = ClassifySpecies(a_actor->race);
        const auto id = m_Registry.CreateEntity();

        // The desync stone: every mind's needs are born slightly different
        // (VaryNeeds — deterministic per entity id), so hunger arrives at
        // different times and the settlement doesn't march to the market
        // in lockstep.
        auto needs = SeededNeeds(species, m_Settings.Rates);
        VaryNeeds(needs, id);

        m_Registry.AddComponent<FormRef>(id, FormRef{ formId });
        m_Registry.AddComponent<SpeciesTag>(id, SpeciesTag{ species });

        // The companion truth (0.7.5 field find): a mind that has ever
        // been a companion is never in the dating pool. The seed marks
        // it the moment it becomes a mind (a dismissed companion
        // assigned to a settlement); the per-second sweep re-derives it
        // like the species, so a pre-fix save heals too.
        const auto companionFaction = HasBeenCompanionFaction();

        if (companionFaction != nullptr
            && a_actor->IsInFaction(companionFaction))
        {
            m_Registry.AddComponent<CompanionTag>(id, CompanionTag{});
        }

        m_Registry.AddComponent<Needs>(id, std::move(needs));
        m_Registry.AddComponent<Goals>(id, SeededGoals(species));
        m_Registry.AddComponent<Memory>(id, Memory{});
        m_Registry.AddComponent<Relationships>(id, Relationships{});

        // The economy stone: a human is born with a small pouch
        // (deterministic per entity id — a saved purse restores exactly).
        // Children and animals never carry one: they never barter.
        if (species == Species::Human)
        {
            m_Registry.AddComponent<CapPouch>(
                id, CapPouch{ SeedPouch(id) });
        }

        // The identity stone (0.7.0 Stone 1): every mind is born with a
        // name. The game's own name wins — Sturges stays Sturges — and
        // a generic "Settler" gets a procedural Commonwealth name: the
        // actor's sex picks the first-name list (male or female; an
        // unset sex draws from the id), deduped against the world's live
        // names (no two "Vance" in the same room). An owned animal draws
        // from its own pool — a dog is "Rex", not "Rex Hart" — and a
        // stray with no owner stays nameless: the log labels it by
        // species and hex until someone claims it.
        std::string fullName;

        // The name must come from the BASE form, not the reference: the
        // reference's own full-name is the sparse map, which is empty
        // for most actors — reading the ref made EVERYONE look generic
        // and renamed Mama Murphy into "Milo Grey". The base form holds
        // the real name (Sturges, Mama Murphy, even "Dog").
        const auto gameName = a_actor->GetObjectReference()
            ? RE::TESFullName::GetFullName(*a_actor->GetObjectReference())
            : std::string_view{};

        // The role may live on the reference, not the base: the game
        // names a supply-line settler "Provisioner" itself, so the
        // base form is still the generic "Settler". Read the actor's
        // display name first (it falls back to the base form) and
        // prefer it for the role rule — real names and placeholders are
        // still decided by the base form exactly as before; the display
        // read is only ever consulted as a role candidate.
        const auto* shownName = const_cast<RE::Actor*>(a_actor)
            ->GetDisplayFullName();
        const auto displayName = shownName
            ? std::string_view(shownName)
            : std::string_view{};

        // A game name is the truth — unless it is a placeholder or a
        // role label. A placeholder ("Settler") gets a full procedural
        // name; a role label ("Provisioner", "Guard") keeps its
        // title and gains the person — "Provisioner Cole" — so memory
        // can tell two provisioners apart (0.7.3 Stone 1).
        auto role = species == Species::Human
            ? TLC::Names::IsRoleName(displayName)
            : std::string_view{};

        if (role.empty() && species == Species::Human)
        {
            role = TLC::Names::IsRoleName(gameName);
        }

        if (TLC::Names::IsGenericName(gameName, species) || !role.empty())
        {
            if (species == Species::Animal)
            {
                // GetOwner is non-const in the game API; the process
                // lists hand us a live, non-const actor — the cast is
                // contained to the read.
                if (const_cast<RE::Actor*>(a_actor)->GetOwner() != nullptr)
                {
                    fullName = TLC::Names::GenerateUniqueAnimal(
                        m_UsedNames, id, m_Names);
                }
            }
            else
            {
                auto gender = TLC::Names::GenderOf(id);
                const auto sex =
                    const_cast<RE::Actor*>(a_actor)->GetSex();

                if (sex == RE::SEX::kMale)
                {
                    gender = TLC::Names::Gender::Male;
                }
                else if (sex == RE::SEX::kFemale)
                {
                    gender = TLC::Names::Gender::Female;
                }

                fullName = role.empty()
                    ? TLC::Names::GenerateUnique(
                        m_UsedNames, id, m_Names, gender)
                    : TLC::Names::GenerateUniqueRole(
                        m_UsedNames, role, id, m_Names, gender);
            }

            // The name's visible half: write it onto the actor's extra
            // data so the game shows it — the pip-boy, the hover, the
            // workshop. Only for a name the sim generated here; a
            // game-named NPC (Sturges) is never touched.
            ApplyActorName(formId, fullName);
        }
        else
        {
            fullName = std::string(gameName);
        }

        // A stray stays unnamed — no Name component, so the label falls
        // back to species + hex until someone owns it.
        if (!fullName.empty())
        {
            m_Registry.AddComponent<Name>(id, Name{ fullName });
            m_UsedNames.insert(fullName);
        }

        // The illness stone (0.8.0): every mind is born healthy. The
        // component is additive in the co-save — a pre-health save
        // restores with Value 1.0 and no sickness (the safe default).
        // A robot never carries one (0.8.6b field find — Buddy the
        // Mr. Handy seeded as Human and caught the flu): machines have
        // no biology to sicken, and the illness passes only touch minds
        // with a Health component.
        if (species != Species::Robot)
        {
            m_Registry.AddComponent<Health>(id, Health{});
        }

        m_Translator.Add(formId, id);
    }

    std::size_t Adapter::SeedLoadedActors()
    {
        std::size_t count = 0;

        ForEachLoadedActor(
            [this, &count](const RE::Actor* a_actor)
            {
                if (!IsSimRelevant(a_actor)
                    || m_Translator.EntityFor(a_actor->GetFormID()).IsValid())
                {
                    return;
                }

                SeedMind(a_actor);
                ++count;
            });

        return count;
    }

    void Adapter::KeepBooks()
    {
        using namespace LCE::Simulation;

        // The known minds: every live entity with a SpeciesTag. Workshops
        // carry a FormRef but no tag — they are targets, never minds.
        std::unordered_set<std::uint32_t> known;

        m_Registry.ForEachWithComponent<SpeciesTag>(
            [&](EntityId a_entity, const SpeciesTag&)
            {
                if (const auto formId = m_Translator.FormFor(a_entity); formId != 0)
                {
                    known.insert(formId);
                }
            });

        // The loaded-actor census: one read of the game's truth this pass.
        std::vector<Lifecycle::Scan> scans;

        ForEachLoadedActor(
            [&scans](RE::Actor* a_actor)
            {
                scans.push_back(Lifecycle::Scan{
                    a_actor->GetFormID(),
                    IsSimRelevant(a_actor),
                    IsActorDead(a_actor) });
            });

        // Two-pass death confirmation: park a form id read dead this
        // pass; book the death only when the next pass still reads it
        // dead. Stream-in artifacts read dead once — cleared the moment
        // the actor reads alive or leaves the lists. Real corpses stay
        // dead for minutes; two passes a second apart never miss one.
        // The seen-alive rule rides along: an actor that has never read
        // alive cannot die (a death is a transition) — the spawn burst
        // after a big load reads the same actors dead on first sight for
        // ~2s, and only an alive reading un-parks them.
        std::unordered_set<std::uint32_t> deadThisPass;

        for (const auto& scan : scans)
        {
            if (scan.Dead)
            {
                deadThisPass.insert(scan.FormId);
            }
            else
            {
                m_SeenAlive.insert(scan.FormId);
            }
        }

        for (auto it = m_PendingDeaths.begin(); it != m_PendingDeaths.end();)
        {
            if (!deadThisPass.contains(it->first))
            {
                it = m_PendingDeaths.erase(it);
            }
            else
            {
                ++it;
            }
        }

        for (const auto& event : Lifecycle::Diff(known, scans))
        {
            switch (event.Kind)
            {
            case Lifecycle::EventKind::Arrival:
            {
                // The scan's actor, re-resolved: the diff is pure, so the
                // actor pointer comes back through the form id. A loaded
                // arrival always resolves.
                const auto* actor =
                    RE::TESForm::GetFormByID<RE::Actor>(event.FormId);

                if (actor == nullptr)
                {
                    break;
                }

                SeedMind(actor);

                if (m_Translator.EntityFor(event.FormId).IsValid())
                {
                    REX::INFO(
                        "lifecycle: settler {:#x} arrives — a new mind wakes.",
                        event.FormId);
                }

                break;
            }
            case Lifecycle::EventKind::Death:
                if (!m_SeenAlive.contains(event.FormId))
                {
                    // Never read alive — the spawn-burst artifact (or a
                    // corpse that was already gone before the world
                    // woke). A death is a transition; this is not one.
                    // Parked forever, cleared the moment the actor reads
                    // alive; never booked.
                    m_PendingDeaths[event.FormId] =
                        std::chrono::steady_clock::now();

                    REX::DEBUG(
                        "lifecycle: settler {:#x} reads dead before ever "
                        "being seen alive — parked, never booked.",
                        event.FormId);
                    break;
                }

                if (m_PendingDeaths.contains(event.FormId))
                {
                    // Second consecutive dead read — the death is real.
                    m_PendingDeaths.erase(event.FormId);
                    RemoveMind(event.FormId, true);
                }
                else
                {
                    // First dead read — an artifact passes next pass, a
                    // corpse confirms. Nothing booked yet; the debug line
                    // makes the confirmation visible in the log.
                    m_PendingDeaths[event.FormId] =
                        std::chrono::steady_clock::now();

                    REX::DEBUG(
                        "lifecycle: settler {:#x} reads dead — first pass, "
                        "not booked (confirming).",
                        event.FormId);
                }
                break;
            case Lifecycle::EventKind::Departure:
                RemoveMind(event.FormId, false);
                break;
            }
        }
    }

    void Adapter::PruneDeviceMinds()
    {
        // A polluted co-save holds the workshop's props as minds (they
        // hold the settler faction, and before the exclusion they seeded
        // as Human — needs, walks, feuds). The prune runs on restore:
        // a fresh world never seeds them (IsSimRelevant now excludes
        // device/robot races), and the polluted world self-heals here —
        // quietly, one summary line, before pouches, names, or bonds
        // are rebuilt.
        std::vector<std::uint32_t> pruned;

        m_Registry.ForEachWithComponent<FormRef>(
            [&](LCE::Simulation::EntityId a_entity, FormRef& a_form)
            {
                // Targets (workshops) carry a FormRef but no SpeciesTag
                // — they are places to walk to, never minds.
                if (m_Registry.GetComponent<SpeciesTag>(a_entity) == nullptr)
                {
                    return;
                }

                auto* actor =
                    RE::TESForm::GetFormByID<RE::Actor>(a_form.FormId);

                if (actor != nullptr && !IsSimRelevant(actor))
                {
                    pruned.push_back(a_form.FormId);
                }
            });

        for (const auto formId : pruned)
        {
            RemoveMind(formId, false, true);
        }

        if (!pruned.empty())
        {
            REX::INFO(
                "sim: {} workshop device{} pruned — turrets, spotlights, "
                "and robots are not minds.",
                pruned.size(), pruned.size() == 1 ? "" : "s");
        }

        // The table's blind spot (2026-08-12): the wall-mounted spotlight
        // slipped the device list — its race is neither a known device nor
        // a known organic race, so it survived the prune and became a
        // Human mind. Announce any such mind once per session, with its
        // race hex, so the device table can grow. A modded organic race
        // warns once and then stays a settler — default Human is the
        // design (ADR-0024); the line is how the table learns.
        std::unordered_set<std::uint32_t> announced;

        m_Registry.ForEachWithComponent<FormRef>(
            [&](LCE::Simulation::EntityId a_entity, FormRef& a_form)
            {
                const auto tag =
                    m_Registry.GetComponent<SpeciesTag>(a_entity);

                if (tag == nullptr || tag->Value != Species::Human)
                {
                    return;
                }

                auto* actor =
                    RE::TESForm::GetFormByID<RE::Actor>(a_form.FormId);

                if (actor == nullptr || actor->race == nullptr
                    || IsKnownOrganicRace(actor->race))
                {
                    return;
                }

                if (!announced.insert(actor->race->GetFormID()).second)
                {
                    return;
                }

                const auto* npc = actor->GetNPC();

                REX::WARN(
                    "sim: {:#x} ({}) is a Human mind with unknown race "
                    "{:#x} — if this is a device, its race belongs in the "
                    "device table (SimRelevant.cpp).",
                    a_form.FormId,
                    npc != nullptr
                        ? RE::TESFullName::GetFullName(*npc)
                        : "?",
                    actor->race->GetFormID());
            });
    }

    void Adapter::RemoveMind(
        std::uint32_t a_formId, bool a_isDeath, bool a_quiet)
    {
        using namespace LCE::Simulation;

        const auto entity = m_Translator.EntityFor(a_formId);

        if (!entity.IsValid())
        {
            return;
        }

        // The book's other pages: no walk, no last-log, no feeder line,
        // no stall — a keeper's market re-derives its keeper on the next
        // arrival.
        m_Walks.erase(entity);
        m_ArrivedAt.erase(entity);
        m_LastWander.erase(entity);
        m_LastLogged.erase(entity);
        m_FeederLogged.erase(entity);

        for (auto it = m_StallKeepers.begin(); it != m_StallKeepers.end();)
        {
            if (it->second == entity)
            {
                it = m_StallKeepers.erase(it);
            }
            else
            {
                ++it;
            }
        }

        // The household (0.6.0 Stone 3): if the dead held the family
        // pouch, it passes to the widow(er) — the wallet is the
        // household's, not the holder's. Runs before the bond erase
        // below, so the spouse still resolves.
        const auto spouse = Households::SpouseOf(m_Bonds, entity);

        if (spouse.IsValid()
            && m_Registry.GetComponent<CapPouch>(entity)
            && !m_Registry.GetComponent<CapPouch>(spouse))
        {
            const auto pouch = m_Registry.GetComponent<CapPouch>(entity);

            m_Registry.AddComponent<CapPouch>(
                spouse, CapPouch{ pouch->Caps });
        }

        // The bond book closes with the mind: every pair it belonged to
        // dissolves (the survivors' relationship rows still hold the
        // stale id, but the id is dead — the reconcile pass skips pairs
        // whose members are not minds, so no ghost bond lingers).
        for (auto it = m_Bonds.begin(); it != m_Bonds.end();)
        {
            if (it->first.first == entity.Value()
                || it->first.second == entity.Value())
            {
                it = m_Bonds.erase(it);
            }
            else
            {
                ++it;
            }
        }

        if (a_isDeath)
        {
            // The death fact — the settlement's grief news (Stone 1,
            // spread through the gossip channel of Stone 4): every
            // surviving mind remembers who is gone — { the dead, Death,
            // weight, day }. A fact, never a door: Decide gates only
            // Trade and Social, so a death never blocks a walk or a
            // trade. Survivors carry it across the co-save; the dead
            // themselves are simply absent (they do not restore).
            // Grief reads it in Stone 5.
            const auto mourning = Gossip::Spread(
                m_Registry, entity,
                LCE::Simulation::InteractionKind::Death,
                CurrentDay());

            // The grief announce's memory (0.6.0 Stone 5): the dead is
            // about to leave the translator forever — FormFor can no
            // longer answer — but the grief arc needs the form to say
            // who is mourned. Recorded before the destroy, consulted by
            // the announce, cleared on EndWorld.
            m_RecentDeaths[entity.Value()] = a_formId;

            // The burial book (0.8.2): the dead and the day they died.
            // The corpse stays in the settlement cell forever otherwise
            // (no cell reset there), so the adapter lays it to rest once
            // the mourning window passes. The sweep reads the game's own
            // death markers the same way the census does — the ledger is
            // form ids, stable across sessions, so a death whose window
            // expires while the game is away is still buried on the
            // next load.
            m_Burials[a_formId] = CurrentDay();

            // 0.7.0 Legacy (engine stones 10–12): what the dead leaves
            // behind. The household's heir — the spouse — receives the
            // dead's memories at or above the bequest floor, scaled
            // (the story survives its maker, fainter). And the dead's
            // name becomes a registry-level legacy — the world's memory
            // of who they were, permanent until the world forgets it.
            // Both run before the destroy below: the dead must still be
            // alive for the core.
            if (spouse.IsValid())
            {
                LCE::Simulation::Bequeath(
                    m_Registry, entity,
                    std::span<const LCE::Simulation::EntityId>{ &spouse, 1 },
                    m_CoreTuning);
            }

            if (const auto name = m_Registry.GetComponent<Name>(entity))
            {
                m_Registry.LeaveLegacy(LCE::Simulation::LegacyFact{
                    entity, CurrentDay(), name->Full, 1.0f });
            }

            // The gossip stone's observable half: one line per death —
            // how many minds remember who is gone. The fact itself is
            // silent (a memory is not a door); this is the verify. The
            // name is the identity stone's voice; the hex rides along.
            REX::INFO(
                "gossip: {} {} remember {} is gone.",
                mourning, mourning == 1 ? "mind" : "minds",
                MindLabelForm(a_formId));

            PushNews(
                MindLabelForm(a_formId) + " died.",
                Tuning::AdapterSettings::NewsCategory::Death);
        }

        // The label before the remove: the translator forgets the form
        // below, so the farewell line keeps the name it just lost.
        const auto goneLabel = MindLabelForm(a_formId);

        m_Registry.DestroyEntity(entity);
        m_Translator.Remove(a_formId);

        if (!a_quiet)
        {
            REX::INFO(
                "lifecycle: {} {} — the world keeps its books.",
                goneLabel, a_isDeath ? "died" : "left the settlement");
        }
    }

    std::uint64_t Adapter::CurrentDay() const
    {
        const auto* calendar = RE::Calendar::GetSingleton();

        return calendar != nullptr && calendar->gameDaysPassed != nullptr
            ? std::uint64_t(calendar->gameDaysPassed->value)
            : 0;
    }

    std::string Adapter::MindLabel(
        LCE::Simulation::EntityId a_entity) const
    {
        const auto formId = m_Translator.FormFor(a_entity);

        if (const auto name = m_Registry.GetComponent<Name>(a_entity))
        {
            return name->Full + " [" + FormatHex8(formId) + "]";
        }

        // No name — a stray animal (nobody claimed it yet), or a mind
        // predating the identity stone mid-world. The species label
        // keeps the log's voice: "animal [FF0197BF]", not a bare hex.
        return std::string(SpeciesLabel(
                   m_Registry.GetComponent<SpeciesTag>(a_entity).get()))
            + " [" + FormatHex8(formId) + "]";
    }

    std::string Adapter::MindNameOnly(std::uint32_t a_formId) const
    {
        const auto entity = m_Translator.EntityFor(a_formId);

        if (entity.IsValid())
        {
            if (const auto name = m_Registry.GetComponent<Name>(entity))
            {
                return name->Full;
            }

            return std::string(SpeciesLabel(
                m_Registry.GetComponent<SpeciesTag>(entity).get()));
        }

        // Not a mind — a workshop or a form the sim does not know.
        // No name worth subtitling; the caller falls back to the bare
        // line.
        return {};
    }

    std::string Adapter::MindLabelForm(std::uint32_t a_formId) const
    {
        const auto entity = m_Translator.EntityFor(a_formId);

        if (entity.IsValid())
        {
            return MindLabel(entity);
        }

        // No entity — a workshop target, or a form the sim does not
        // know. A workshop is a market; name it from its base form.
        return MarketLabel(a_formId);
    }

    bool Adapter::IsWorkshopForm(std::uint32_t a_formId) const noexcept
    {
        return a_formId != 0
            && std::find_if(
                   m_Workshops.begin(), m_Workshops.end(),
                   [a_formId](const WorkshopPosition& a_workshop)
                   {
                       return a_workshop.FormId == a_formId;
                   })
                != m_Workshops.end();
    }

    std::string Adapter::MarketLabel(std::uint32_t a_formId) const
    {
        // The census-time name first (the persistent-cell read happens
        // before the workshops stream in — a label asked while the form
        // is still unloaded still names the settlement).
        const auto cached = m_WorkshopNames.find(a_formId);

        if (cached != m_WorkshopNames.end())
        {
            return cached->second + " [" + FormatHex8(a_formId) + "]";
        }

        const auto* form = RE::TESForm::GetFormByID(a_formId);

        if (form != nullptr)
        {
            const auto name = RE::TESFullName::GetFullName(*form);

            if (!name.empty())
            {
                return std::string(name) + " [" + FormatHex8(a_formId) + "]";
            }
        }

        return FormatHex8(a_formId);
    }

    void Adapter::ApplyActorName(
        std::uint32_t a_formId, const std::string& a_name) const
    {
        if (a_name.empty())
        {
            return;
        }

        auto* actor = RE::TESForm::GetFormByID<RE::Actor>(a_formId);

        if (actor == nullptr || actor->extraList == nullptr)
        {
            // Not loaded (a restored world's actors stream in slowly) —
            // the restore pass re-applies names as actors load, and the
            // actor's own extra data persists it once written.
            return;
        }

        actor->extraList->SetOverrideName(a_name.c_str());
    }

    void Adapter::ApplyLoadedActorNames()
    {
        ForEachLoadedActor(
            [this](const RE::Actor* a_actor)
            {
                const auto formId = a_actor->GetFormID();
                const auto entity = m_Translator.EntityFor(formId);

                if (!entity.IsValid())
                {
                    return;
                }

                const auto name = m_Registry.GetComponent<Name>(entity);

                if (name == nullptr || name->Full.empty())
                {
                    return;
                }

                auto* actor = const_cast<RE::Actor*>(a_actor);

                if (actor->extraList == nullptr)
                {
                    return;
                }

                const auto tag = m_Registry.GetComponent<SpeciesTag>(entity);
                const auto species = tag ? tag->Value : Species::Human;

                // The base form is the eternal truth: if the game gave
                // this NPC a real name, the mind must carry it — and an
                // earlier build's stale generated stamp ("Milo Grey" on
                // Mama Murphy) is dropped so the real name shows again.
                // The co-save holds the corrected name from the next
                // save onward.
                const auto baseName = actor->GetObjectReference()
                    ? RE::TESFullName::GetFullName(
                        *actor->GetObjectReference())
                    : std::string_view{};

                // A role label is a title, not a name (0.7.3 Stone 1):
                // the mind wears "Provisioner Cole", never the bare
                // "Provisioner" — memory must tell two provisioners
                // apart. The role may live on the reference rather than
                // the base (the game names supply-line settlers
                // "Provisioner" itself), so the display name is read
                // first. The base never overrides the role name (the
                // converge below would stamp the bare role word back
                // on), and a mind still wearing a pre-0.7.3 name (the
                // bare role word, or a restore-time full name) converges
                // to its role name here.
                if (species == Species::Human)
                {
                    const auto* shown = actor->GetDisplayFullName();
                    const auto displayName = shown
                        ? std::string_view(shown)
                        : std::string_view{};

                    auto role = TLC::Names::IsRoleName(displayName);

                    if (role.empty())
                    {
                        role = TLC::Names::IsRoleName(baseName);
                    }

                    if (!role.empty())
                    {
                        auto& mind = name->Full;

                        if (mind.empty()
                            || TLC::Names::IsGenericName(
                                mind, Species::Human)
                            || !TLC::Names::HasRolePrefix(mind, role))
                        {
                            auto gender = TLC::Names::GenderOf(entity);
                            const auto sex = actor->GetSex();

                            if (sex == RE::SEX::kMale)
                            {
                                gender = TLC::Names::Gender::Male;
                            }
                            else if (sex == RE::SEX::kFemale)
                            {
                                gender = TLC::Names::Gender::Female;
                            }

                            mind = TLC::Names::GenerateUniqueRole(
                                m_UsedNames, role, entity,
                                m_Names, gender);
                        }

                        // The game names supply-line settlers itself:
                        // the actor can already wear the bare role word
                        // as a text-display override, which would
                        // swallow the sim's name. Write through when
                        // the actor shows nothing or the bare role — a
                        // different deliberate name (a player rename)
                        // is respected. Verified in-game: the write is
                        // a no-op against the game's own mask while the
                        // actor is assigned to a supply line (the label
                        // is re-derived from the assignment, ahead of
                        // any extra-data override — a universal FO4
                        // limitation). It still lands the moment the
                        // provisioner is reassigned to another role, so
                        // the write stays.
                        if (shown == nullptr
                            || TLC::Names::EqualsFold(
                                displayName, role))
                        {
                            actor->extraList->SetOverrideName(
                                mind.c_str());
                        }

                        return;
                    }
                }

                if (!TLC::Names::IsGenericName(baseName, species))
                {
                    if (name->Full != baseName)
                    {
                        m_Registry.GetComponent<Name>(entity)->Full =
                            std::string(baseName);

                        if (actor->extraList->HasType(
                                RE::EXTRA_DATA_TYPE::kTextDisplayData))
                        {
                            actor->extraList->RemoveExtra(
                                RE::EXTRA_DATA_TYPE::kTextDisplayData);
                        }
                    }

                    return;
                }

                // A generic base ("Settler", "Worker", "Dog"): write
                // the sim's name once — an actor already showing it is
                // left alone. An animal whose mind still holds the raw
                // species word (a kept game name from before the naming
                // rule) or a stale generated name converges to the rule:
                // owned → a real name from the pool; a stray → nameless,
                // its stamp dropped.
                if (species == Species::Animal
                    && TLC::Names::IsGenericName(
                        name->Full, Species::Animal))
                {
                    if (actor->GetOwner() != nullptr)
                    {
                        m_Registry.GetComponent<Name>(entity)->Full =
                            TLC::Names::GenerateUniqueAnimal(
                                m_UsedNames, entity, m_Names);
                    }
                    else
                    {
                        m_Registry.RemoveComponent<Name>(entity);

                        if (actor->extraList->HasType(
                                RE::EXTRA_DATA_TYPE::kTextDisplayData))
                        {
                            actor->extraList->RemoveExtra(
                                RE::EXTRA_DATA_TYPE::kTextDisplayData);
                        }

                        return;
                    }
                }

                // The Human half of the same rule (0.7.5 field find): a
                // mind whose STORED name is itself a placeholder — a
                // "Worker" or "Settler" persisted before the generic
                // list grew — would be written back to the game as-is
                // and never change. Re-derive a real name in place,
                // then the write below stamps it.
                if (species == Species::Human
                    && TLC::Names::IsGenericName(
                        name->Full, Species::Human))
                {
                    auto gender = TLC::Names::GenderOf(entity);
                    const auto sex = actor->GetSex();

                    if (sex == RE::SEX::kMale)
                    {
                        gender = TLC::Names::Gender::Male;
                    }
                    else if (sex == RE::SEX::kFemale)
                    {
                        gender = TLC::Names::Gender::Female;
                    }

                    m_Registry.GetComponent<Name>(entity)->Full =
                        TLC::Names::GenerateUnique(
                            m_UsedNames, entity, m_Names, gender);
                }

                if (!actor->extraList->HasType(
                        RE::EXTRA_DATA_TYPE::kTextDisplayData))
                {
                    actor->extraList->SetOverrideName(name->Full.c_str());
                }
            });
    }

     void Adapter::RebuildKin()
     {
         // The family gate (0.7.5 field find): the vanilla families
         // never romance. Every loaded actor's base form is indexed
         // once, and each curated kin pair (Kin.h — Blake with Lucy,
         // Abraham with Daniel, ...) folds its members' entities into
         // m_Kin. Derived, never persisted: a kin pair only matters
         // when both actors are loaded (they cannot interact across
         // cells), and the bond gates read this set each second, so a
         // pre-fix save's mistake heals the moment both actors are in.
         m_BaseToMinds.clear();
         m_Kin.clear();

         ForEachLoadedActor(
             [this](const RE::Actor* a_actor)
             {
                 const auto entity =
                     m_Translator.EntityFor(a_actor->GetFormID());

                 if (!entity.IsValid())
                 {
                     return;
                 }

                 const auto* base = a_actor->GetObjectReference();

                 if (base == nullptr)
                 {
                     return;
                 }

                 m_BaseToMinds[base->GetFormID() & 0x00FFFFFF]
                     .push_back(entity);
             });

         for (const auto& pair : Kin::kKinPairs)
         {
             const auto itA = m_BaseToMinds.find(
                 pair.BaseA & 0x00FFFFFF);
             const auto itB = m_BaseToMinds.find(
                 pair.BaseB & 0x00FFFFFF);

             if (itA == m_BaseToMinds.end()
                 || itB == m_BaseToMinds.end())
             {
                 continue;
             }

             for (const auto a : itA->second)
             {
                 for (const auto b : itB->second)
                 {
                     m_Kin.insert(Bonds::PairKey(a, b));
                 }
             }
         }

         if (!m_Kin.empty() && m_Kin.size() != m_LastKinLogged)
         {
             m_LastKinLogged = m_Kin.size();
             REX::INFO(
                 "kin: {} family {} gated from romance — the world's families stay family.",
                 m_Kin.size(), m_Kin.size() == 1 ? "pair" : "pairs");
         }
     }

     void Adapter::ReclassifyLoadedMinds()
     {
         // The species is game truth (ADR-0024): a mind whose stored
         // tag disagrees with its actor's race — a behemoth saved as
         // Human before the 0.7.5 classification fix — is corrected the
         // moment the actor loads. The fix's point: a restored behemoth
         // is fed, not feuding. An animal never carries a pouch (it
         // never bartered); the restore gates that read the corrected
         // tag prune its stall row and its bonds, and the 1-second
         // reconcile never re-forms them.
         ForEachLoadedActor(
             [this](const RE::Actor* a_actor)
             {
                 const auto formId = a_actor->GetFormID();
                 const auto entity = m_Translator.EntityFor(formId);

                 if (!entity.IsValid())
                 {
                     return;
                 }

                 // The companion truth (0.7.5 field find): a mind that
                 // has ever been a companion — HasBeenCompanionFaction,
                 // set permanently at recruitment — stays a full mind
                 // (fed, trading, befriending, feuding) but is never in
                 // the dating pool. Re-derived every pass like the
                 // species, so a pre-fix save heals and a dismissed
                 // companion who joins a settlement is caught the
                 // moment the actor loads.
                 const auto companionFaction = HasBeenCompanionFaction();
                 const bool isCompanion = companionFaction != nullptr
                     && a_actor->IsInFaction(companionFaction);
                 const bool tagged =
                     m_Registry.GetComponent<CompanionTag>(entity) != nullptr;

                 if (isCompanion && !tagged)
                 {
                     m_Registry.AddComponent<CompanionTag>(
                         entity, CompanionTag{});
                     REX::INFO(
                         "companion: {} is a companion — friends and feuds, never romance.",
                         MindLabel(entity));
                 }
                 else if (!isCompanion && tagged)
                 {
                     m_Registry.RemoveComponent<CompanionTag>(entity);
                 }

                 const auto tag = m_Registry.GetComponent<SpeciesTag>(entity);

                 if (!tag)
                 {
                     return;
                 }

                 const auto fresh = ClassifySpecies(a_actor->race);

                 if (fresh == tag->Value)
                 {
                     return;
                 }

                 const auto oldLabel =
                     tag->Value == Species::Animal ? "Animal"
                     : tag->Value == Species::Child ? "Child"
                     : tag->Value == Species::Robot ? "Robot"
                                                    : "Human";
                 const auto newLabel =
                     fresh == Species::Animal ? "Animal"
                     : fresh == Species::Child ? "Child"
                     : fresh == Species::Robot ? "Robot"
                                               : "Human";

                 REX::INFO(
                     "species: {} re-classified {} -> {} (the race table grew).",
                     MindLabel(entity), oldLabel, newLabel);

                 tag->Value = fresh;

                 // An animal never carries a pouch — drop it so the
                 // world's money stays in people's hands.
                 if (fresh != Species::Human)
                 {
                     m_Registry.RemoveComponent<CapPouch>(entity);
                 }

                 // A robot has no biology to sicken (0.8.6b): a
                 // pre-fix save's robot was tagged Human and may carry
                 // an illness — drop the Health component so the
                 // re-classified machine is never ill again (the
                 // illness passes only touch minds with one).
                 if (fresh == Species::Robot)
                 {
                     m_Registry.RemoveComponent<Health>(entity);
                 }
             });
     }

    void Adapter::AssignSettlementGroups()
    {
        using namespace LCE::Simulation;

        // No census, no settlements — the group is the world, and there
        // is nothing to derive membership from (the fallback market is
        // a workshop entity, so it appears in m_Workshops once the
        // census finds nothing and EnsureWorkshop pins it — this guard
        // only skips a truly empty world).
        if (m_Workshops.empty())
        {
            return;
        }

        m_Registry.ForEachWithComponent<Memory>(
            [this](EntityId a_entity, Memory& a_memory)
            {
                if (m_Registry.GetComponent<Groups>(a_entity) != nullptr)
                {
                    return;   // already has a home
                }

                // The settlement is the market the mind remembers — a
                // Trade-kind event whose Other is a workshop form (not a
                // person; the remembered merchant is an actor, and a
                // mind's market is the bench). The group id is the
                // market's form id: deterministic, session-stable, and
                // the same for every mind of the settlement.
                for (const auto& event : a_memory.Events)
                {
                    if (event.Kind != InteractionKind::Trade
                        || !event.Other.IsValid())
                    {
                        continue;
                    }

                    const auto formId = m_Translator.FormFor(event.Other);

                    if (IsWorkshopForm(formId))
                    {
                        m_Registry.AddComponent<Groups>(
                            a_entity,
                            Groups{ { GroupId{ formId } } });
                        return;
                    }
                }
            });
    }

    void Adapter::PushNews(
        const std::string& a_line,
        Tuning::AdapterSettings::NewsCategory a_category)
    {
        // The category gate (0.8.7 presentation rethink): each
        // announcement category is its own toggle — a disabled
        // category never even enters the feed, so it cannot reach the
        // settlement radio either. The log keeps the full record
        // regardless (the verify channel).
        const auto& cfg = m_Settings.News[
            static_cast<std::size_t>(a_category)];

        if (!cfg.Enabled)
        {
            return;
        }

        m_News.Add(a_line);

        // The on-screen window (0.7.0 Stone 3), throttled: a world of
        // news is still a flood if every line pops at once, so events
        // queue into the feed and the screen shows at most one per
        // sim.news.cooldown seconds. A category whose subs are off
        // feeds the radio without popping; the master sim.news.enabled
        // kills the whole on-screen window.
        if (!m_Settings.NewsEnabled || !cfg.Subs)
        {
            return;
        }

        const auto now = std::chrono::steady_clock::now();

        if (now - m_LastNews >= std::chrono::duration<float>(
                m_Settings.NewsCooldown))
        {
            m_LastNews = now;
            // The HUD diagnostic (0.8.1 verification): the on-screen pop is
            // the one thing the log cannot see — a stall inside
            // ShowHUDMessage would show as a gap after this line. Timestamp
            // each pop so the next session can prove (or clear) the
            // message-path hang.
            REX::DEBUG("hud: pop '{}'", a_line);
            RE::SendHUDMessage::ShowHUDMessage(
                a_line.c_str(), cfg.Audio ? "UIMenuOK" : "", false,
                false);
        }
    }

    std::optional<Dialogue::Voice> Adapter::VoiceOfForm(
        std::uint32_t a_formId) const
    {
        // The speaker's voice is game data: the actor's NPC base's
        // voiceType (TESActorBaseData::voiceType). Resolve its editor
        // id to our enum — the vanilla voice-type form ids are stable
        // in Fallout4.esm, so a small name map (verified against the
        // ESM's VTYP table) is all it takes. A null actor, a null NPC,
        // or an unknown voice type is nullopt — the mind stays
        // caption-only.
        auto* actor = RE::TESForm::GetFormByID<RE::Actor>(a_formId);

        if (actor == nullptr)
        {
            return std::nullopt;
        }

        const auto* npc = actor->GetNPC();

        if (npc == nullptr)
        {
            return std::nullopt;
        }

        if (npc->voiceType == nullptr)
        {
            REX::DEBUG(
                "vfx: {:#x} has no voice type on its NPC base.", a_formId);
            return std::nullopt;
        }

        const auto* editorId = npc->voiceType->GetFormEditorID();

        if (editorId == nullptr)
        {
            REX::DEBUG(
                "vfx: {:#x} voice {:#x} has no editor id.", a_formId,
                npc->voiceType->GetFormID());
            return std::nullopt;
        }

        static const struct VoiceName
        {
            const char* Name;
            Dialogue::Voice Voice;
        } kVoices[] = {
            { "MaleOld", Dialogue::Voice::MaleOld },
            { "FemaleOld", Dialogue::Voice::FemaleOld },
            { "MaleEvenToned", Dialogue::Voice::MaleEvenToned },
            { "FemaleEvenToned", Dialogue::Voice::FemaleEvenToned },
            { "MaleRough", Dialogue::Voice::MaleRough },
            { "FemaleRough", Dialogue::Voice::FemaleRough },
            { "MaleBoston", Dialogue::Voice::MaleBoston },
            { "FemaleBoston", Dialogue::Voice::FemaleBoston },
            { "GuardMaleDiamondCity01", Dialogue::Voice::GuardMaleDiamondCity1 },
            { "GuardMaleDiamondCity02", Dialogue::Voice::GuardMaleDiamondCity2 },
            { "GuardMaleVault81", Dialogue::Voice::GuardMaleVault81 },
            { "GuardFemaleVault81", Dialogue::Voice::GuardFemaleVault81 },
            { "MaleChild", Dialogue::Voice::MaleChild },
            { "FemaleChild", Dialogue::Voice::FemaleChild },
            { "MaleGhoul", Dialogue::Voice::GhoulMale },
            { "FemaleGhoul", Dialogue::Voice::GhoulFemale },
        };

        for (const auto& entry : kVoices)
        {
            if (std::strcmp(editorId, entry.Name) == 0)
            {
                return entry.Voice;
            }
        }

        REX::DEBUG(
            "vfx: {:#x} voice editor id '{}' is not a settler/guard/"
            "child bank — caption-only.",
            a_formId, editorId);

        return std::nullopt;
    }

    std::optional<Dialogue::Voice> Adapter::VoiceOf(
        LCE::Simulation::EntityId a_entity) const
    {
        return VoiceOfForm(m_Translator.FormFor(a_entity));
    }

    void Adapter::Say(
        LCE::Simulation::EntityId a_speaker,
        LCE::Simulation::EntityId a_listener,
        Dialogue::Pool a_pool,
        bool a_loud,
        bool a_radio)
    {
        // One line, deterministic per mind and day — the same mind says
        // the same line all day and a different one tomorrow. The
        // voice-aware picker (0.8.7): the speaker's voice decides
        // which lines are even offered — a line only speaks if the
        // speaker's voice bank recorded it, so the audio layer can
        // never play a wrong-voice line. An unresolvable voice (or a
        // voice with nothing in the pool) falls back to the plain
        // picker — captions still carry the story; silence is a safe
        // default.
        std::string line;
        const auto voice = VoiceOf(a_speaker);

        if (voice.has_value())
        {
            line = Dialogue::PickForVoice(
                m_Dialogue, a_pool, a_speaker, CurrentDay(), *voice);
        }

        if (line.empty())
        {
            line = Dialogue::Pick(m_Dialogue, a_pool, a_speaker, CurrentDay());
        }

        if (line.empty())
        {
            return;
        }

        const auto speakerForm = m_Translator.FormFor(a_speaker);
        const auto listenerForm = m_Translator.FormFor(a_listener);

        // The verify channel: the log reads as dialogue — who said what
        // to whom, and in whose voice. The voice tag proves the
        // voice-aware picker ran (0.8.7): a known voice means the line
        // came from PickForVoice (the speaker's bank recorded it);
        // "cap" means the voice was unresolvable or had nothing in the
        // pool and the plain picker ran — captions only, never
        // wrong-voice audio.
        REX::INFO(
            "LCE: {} to {} [{}]: \"{}\"",
            MindLabelForm(speakerForm), MindLabelForm(listenerForm),
            voice.has_value() ? "voice" : "cap", line);

        // The two channels a line can take beyond the log:
        //   - a_radio: the settlement radio's feed, the same queue the
        //     captions read from. Big news — births, deaths, feuds —
        //     broadcasts; ordinary conversation does not (0.8.4 field
        //     truth: the player should hear what is said nearby, not
        //     the whole settlement's small talk).
        //   - the ear: the game's own subtitle display — a
        //     bottom-of-screen line, like dialogue — shown only when
        //     the speaker is within sim.subtitle.radius of the player
        //     (a loud line or a local interaction; a broadcast news
        //     line never pops a subtitle on its own).
        const auto lineLabel =
            MindLabelForm(speakerForm) + ": \"" + line + "\"";

        if (a_radio)
        {
            m_News.Add(lineLabel);
        }

        if (a_loud || !a_radio)
        {
            auto* speaker =
                RE::TESForm::GetFormByID<RE::TESObjectREFR>(speakerForm);
            const auto* player = RE::PlayerCharacter::GetSingleton();

            if (speaker != nullptr && player != nullptr
                && player->GetPosition().GetDistance(speaker->GetPosition())
                    <= m_Settings.SubtitleRadius)
            {
                // The on-screen line is clean — the log's id-bearing
                // label stays in the feed and the receipt. The subtitle
                // reads "Jun Long: \"...\"", never the hex.
                const auto speakerName = MindNameOnly(speakerForm);

                ShowChatter(
                    speakerName.empty()
                        ? line
                        : speakerName + ": \"" + line + "\"");
            }
        }
    }

    void Adapter::InteractPass()
    {
        using namespace LCE::Simulation;

        // The candidates: loaded human minds (children are sim-only —
        // no actor to stand near — and animals don't talk). Their
        // positions are gathered once, so the pass is O(n²) over the
        // loaded few, not the Commonwealth-wide hundreds.
        struct Nearby
        {
            EntityId Id;
            float X, Y, Z;
        };

        std::vector<Nearby> loaded;

        m_Registry.ForEachWithComponent<FormRef>(
            [&](EntityId a_entity, FormRef& a_ref)
            {
                const auto species =
                    m_Registry.GetComponent<SpeciesTag>(a_entity);

                if (!species || species->Value != Species::Human)
                {
                    return;
                }

                auto* actor =
                    RE::TESForm::GetFormByID<RE::Actor>(a_ref.FormId);

                // Genuinely loaded only: GetFormByID resolves actors in
                // streamed-out cells too, and their position reads
                // garbage — without this gate every restored mind looks
                // "within range" of every other and the whole world
                // chats at once (the 329-line wake burst, 0.8.4 field
                // find). A 3D-loaded actor (Get3D non-null) is one the
                // player is actually near.
                if (actor == nullptr || actor->Get3D() == nullptr)
                {
                    return;
                }

                const auto pos = actor->GetPosition();
                loaded.push_back({ a_entity, pos.x, pos.y, pos.z });
            });

        if (loaded.size() < 2)
        {
            return;   // no one to cross paths with
        }

        const auto now = std::chrono::steady_clock::now();
        const auto radiusSq = m_Settings.InteractRadius
            * m_Settings.InteractRadius;

        for (const auto& a : loaded)
        {
            // A walking mind keeps walking — the interaction never
            // interrupts a walk. Resting minds may talk (they are
            // idle, not moving).
            const auto intent = m_Registry.GetComponent<Intent>(a.Id);

            if (intent && intent->Action == ActionType::MoveTo)
            {
                continue;
            }

            // The cadence gate: each mind speaks at most once per
            // jittered cadence.
            const auto cooldownIt = m_InteractCooldown.find(a.Id);

            if (cooldownIt != m_InteractCooldown.end()
                && now < cooldownIt->second)
            {
                continue;
            }

            // The nearest other loaded, non-walking mind within the
            // radius — the mind actually crossing paths.
            EntityId partner;
            float best = radiusSq;

            for (const auto& b : loaded)
            {
                if (b.Id == a.Id)
                {
                    continue;
                }

                const auto bIntent =
                    m_Registry.GetComponent<Intent>(b.Id);

                if (bIntent && bIntent->Action == ActionType::MoveTo)
                {
                    continue;
                }

                const float dx = a.X - b.X;
                const float dy = a.Y - b.Y;
                const float dz = a.Z - b.Z;
                const float d = dx * dx + dy * dy + dz * dz;

                if (d < best)
                {
                    best = d;
                    partner = b.Id;
                }
            }

            if (!partner.IsValid())
            {
                continue;   // alone — nothing to say
            }

            // The per-pair cooldown (0.8.8): the same two minds don't
            // re-exchange within sim.interact.pairCooldown seconds —
            // Gabriel and Lucas shouldn't greet each other twice a
            // minute (0.8.7b field note). Keyed by the unordered pair;
            // no claim, so a cadence-expired mind simply waits out the
            // pair's window and tries again (it always picks the same
            // nearest partner, so the wait is real, not a spin).
            const auto pairA = a.Id.Value();
            const auto pairB = partner.Value();
            const auto pairKey = std::make_pair(
                pairA < pairB ? pairA : pairB,
                pairA < pairB ? pairB : pairA);
            const auto pairIt = m_InteractPairCooldown.find(pairKey);

            if (pairIt != m_InteractPairCooldown.end()
                && now < pairIt->second)
            {
                continue;
            }

            // The daily-cap gate (0.8.8): a mind opens at most
            // sim.interact.dailyCap interactions per sim day (0 = off,
            // today's behaviour). Rolled to the current day on read; a
            // capped mind claims its cadence and waits it out, so the
            // check doesn't spin every frame.
            if (m_Settings.InteractDailyCap > 0)
            {
                auto& opened = m_InteractOpenedToday[a.Id];
                const auto day = CurrentDay();

                if (opened.first != day)
                {
                    opened = { day, 0 };
                }

                if (opened.second >= m_Settings.InteractDailyCap)
                {
                    m_InteractCooldown[a.Id] =
                        now
                        + std::chrono::duration_cast<
                            std::chrono::steady_clock::duration>(
                            std::chrono::duration<float>(
                                m_Settings.InteractCadence
                                * (0.5f + m_Rng.NextFloat())));
                    continue;
                }
            }

            // Claim the slot before the roll: a cooldown-expired mind
            // in company waits a full jittered cadence whether or not
            // it speaks — otherwise a failed chance roll retries next
            // frame and the gate means nothing (0.8.4 field find).
            m_InteractCooldown[a.Id] =
                now
                + std::chrono::duration_cast<
                    std::chrono::steady_clock::duration>(
                    std::chrono::duration<float>(
                        m_Settings.InteractCadence
                        * (0.5f + m_Rng.NextFloat())));

            // The chance gate: not every cooldown-expired mind in
            // company speaks — a crowd stays sparse.
            if (m_Rng.NextFloat() >= m_Settings.InteractChance)
            {
                continue;
            }

            // The pool follows the bond, weighted by sim.interact.weight.*
            // (0.8.8): a bonded pair's family weight is boosted hard (a
            // spouse household talks family most, and the weights shape
            // the rest); a feud pair is a hard row — their story is the
            // row and the physical escalation, and a warm hello between
            // feuders would read wrong (0.8.7b). Everyone else rolls
            // the four pools by weight: greet and gossip dominate the
            // crowd, family and row stay rare between strangers.
            Dialogue::Pool pool = Dialogue::Pool::Greet;
            const auto kind = Bonds::CurrentKind(m_Bonds, a.Id, partner);
            float wGreet = m_Settings.InteractWeightGreet;
            float wGossip = m_Settings.InteractWeightGossip;
            float wFamily = m_Settings.InteractWeightFamily;
            float wRow = m_Settings.InteractWeightRow;

            switch (kind)
            {
            case Bonds::BondKind::Spouse:
            case Bonds::BondKind::Sweetheart:
            case Bonds::BondKind::Friend:
                wFamily *= 10.0f;
                break;
            case Bonds::BondKind::Enemy:
            case Bonds::BondKind::Rival:
                pool = Dialogue::Pool::Row;
                break;
            case Bonds::BondKind::None:
                break;
            }

            if (kind != Bonds::BondKind::Enemy
                && kind != Bonds::BondKind::Rival)
            {
                const float total = wGreet + wGossip + wFamily + wRow;

                if (total > 0.0f)
                {
                    const float roll = m_Rng.NextFloat() * total;

                    if (roll < wGreet)
                    {
                        pool = Dialogue::Pool::Greet;
                    }
                    else if (roll < wGreet + wGossip)
                    {
                        pool = Dialogue::Pool::Gossip;
                    }
                    else if (roll < wGreet + wGossip + wFamily)
                    {
                        pool = Dialogue::Pool::Family;
                    }
                    else
                    {
                        pool = Dialogue::Pool::Row;
                    }
                }
            }

            // The in-world stone (0.8.7b): a non-row crossing becomes
            // a voiced exchange — A greets B, B answers a beat later,
            // the game's own words and subtitles (the log keeps the
            // record; nothing pops on-screen). A rolled row (even
            // between friends) stays a quiet caption line — rows are
            // words, not voices. Both paths stamp the pair cooldown
            // and the day ledger here, once, so an interaction is an
            // interaction whichever way it lands.
            m_InteractPairCooldown[pairKey] =
                now
                + std::chrono::duration_cast<
                    std::chrono::steady_clock::duration>(
                    std::chrono::duration<float>(
                        m_Settings.InteractPairCooldown
                        * (0.75f + 0.5f * m_Rng.NextFloat())));

            if (m_Settings.InteractDailyCap > 0)
            {
                ++m_InteractOpenedToday[a.Id].second;
            }

            if (m_Settings.InteractVoice && pool != Dialogue::Pool::Row)
            {
                auto* aActor = RE::TESForm::GetFormByID<RE::Actor>(
                    m_Translator.FormFor(a.Id));
                auto* bActor = RE::TESForm::GetFormByID<RE::Actor>(
                    m_Translator.FormFor(partner));

                if (aActor != nullptr && bActor != nullptr
                    && !aActor->IsInCombat() && !bActor->IsInCombat())
                {
                    // The register stone (0.8.9): the bond names the
                    // exchange's register before the game picks its
                    // words — family for a bonded household, flirt
                    // for a compatible unpaired pair, greet for the
                    // crowd.
                    OpenExchange(
                        aActor, bActor,
                        RegisterFor(a.Id, partner, kind));
                    continue;
                }
            }

            // Local, not broadcast: the line logs and subtitles when
            // the player is close enough to hear it — the settlement
            // radio never carries small talk (0.8.4 field truth).
            Say(a.Id, partner, pool, /*a_loud=*/false, /*a_radio=*/false);
        }
    }

    void Adapter::ShowChatter(const std::string& a_line)
    {
        // The world's chatter — the game's native HUD notification
        // (top-left, the same queue quest updates and the radio
        // captions use). The first presentation pushed the game's
        // dialogue-subtitle queue (SubtitleManager): it rendered as a
        // bottom-of-screen dialogue box with no audio attached, which
        // read as fake in-conversation dialogue — out of place (0.8.7
        // field report). The proximity lines moved to the notification
        // feed: one line at most every sim.chatter.cooldown seconds,
        // so a settlement's small talk reads as a slow native ticker
        // instead of a subtitle flood. The log keeps the full record.
        const auto now = std::chrono::steady_clock::now();

        if (m_LastChatter.time_since_epoch().count() != 0
            && now - m_LastChatter
                < std::chrono::duration<float>(m_Settings.ChatterCooldown))
        {
            return;
        }

        m_LastChatter = now;

        // The HUD diagnostic (0.8.1 verification): the on-screen pop is
        // the one thing the log cannot see — a stall inside
        // ShowHUDMessage would show as a gap after this line.
        REX::DEBUG("hud: chatter '{}'", a_line);
        RE::SendHUDMessage::ShowHUDMessage(
            a_line.c_str(), "", false, false);
    }

    void Adapter::EscalateToFight(
        LCE::Simulation::EntityId a_aggressor,
        LCE::Simulation::EntityId a_victim,
        std::uint64_t a_day,
        bool a_force)
    {
        // Three gates, all must pass (Fights::RollFight): the pair is
        // an enemy feud (rivals stay verbal — the verbal-first rule),
        // the aggressor's temper is at or above sim.fight.temper (the
        // churlish throw the punch), and the world's coin lands under
        // sim.fight.chance (1.0 forces every eligible escalation — the
        // test knob). The once-per-day gate lives in BookFight (a
        // Combat memory stamped today, co-saved). a_force is the test
        // hook's loop (sim.test.forceFight): the coin is skipped and
        // the once-per-day gate is bypassed — a pinned pair brawls on
        // its own timer so the fight machinery can be watched on
        // demand. The species belt still holds: a forced brawl needs
        // two adults, like any fight.
        //
        // The species belt (0.7.5 field find): blows are people's
        // business — a feud needs two adults. A child can argue (the
        // row already landed), but never throws or takes a punch; an
        // animal never even rows (the cross gate). The belt guards
        // BOTH fight entry points (the bench crossing and the
        // shut-stall slight), whatever the bond book says.
        const auto fightTagA =
            m_Registry.GetComponent<SpeciesTag>(a_aggressor);
        const auto fightTagB =
            m_Registry.GetComponent<SpeciesTag>(a_victim);

        if (!fightTagA || !fightTagB
            || fightTagA->Value != Species::Human
            || fightTagB->Value != Species::Human)
        {
            return;
        }

        const auto kind = Bonds::CurrentKind(m_Bonds, a_aggressor, a_victim);
        const float roll = m_Rng.NextFloat(0.0f, 1.0f);

        if (!a_force
            && !Fights::RollFight(
                kind, TemperOf(a_aggressor),
                m_Settings.FightTemper, m_Settings.FightChance, roll))
        {
            return;
        }

        if (!Fights::BookFight(
                m_Registry, m_Bonds, m_ConflictGates,
                a_aggressor, a_victim,
                a_day, m_CoreTuning, &m_Bus, a_force))
        {
            return;
        }

        // The illness stone (0.8.0): a fight can infect — the victim
        // rolls the wound chance. A wound is nastier than a chill: the
        // severity is seeded higher, so an untreated wound can reach
        // the death line while a mild radstorm cold recovers.
        if (m_Settings.IllnessEnabled
            && m_Settings.Illness.WoundChance > 0.0f)
        {
            const auto woundRoll = m_Rng.NextFloat(0.0f, 1.0f);
            const auto victimHealth =
                m_Registry.GetComponent<Health>(a_victim);

            if (victimHealth
                && TLC::Contract(
                    *victimHealth, SicknessKind::Wound, 0.5f, a_day,
                    m_Settings.Illness,
                    m_Settings.Illness.WoundChance, woundRoll))
            {
                REX::INFO(
                    "illness: {} took a wound and fell ill.",
                    MindLabel(a_victim));
            }
        }

        // The punch lands (0.7.5 polish): the victim is shoved back
        // from the aggressor — the game's own knockback (AIProcess::
        // KnockExplosion, REL::ID-resolved against the installed
        // Address Library) plays the stagger, the same physical push
        // the "Get Out Of My Face" crowd mod uses. Best-effort: a
        // missing actor or process skips the shove; the fight is
        // booked either way, and sim.fight.push = 0 turns it off.
        //
        // The force is the base sim.fight.push with a deterministic
        // per-victim jitter (0.7.5 polish): ±25% off the victim's
        // entity id, so every shove lands a little differently — the
        // same pair's brawl reads the same, a new victim reads new.
        // The retaliation: a victim hot-headed enough to have thrown
        // the first punch (their temper at or above the same
        // sim.fight.temper line) shoves back, its own jitter off the
        // aggressor's id. One exchange, never an endless loop: the
        // aggressor took the first swing, the victim answers once, and
        // the day-gate holds the pair to a single scene today.
        if (m_Settings.FightPush > 0.0f)
        {
            auto* victimActor = RE::TESForm::GetFormByID<RE::Actor>(
                m_Translator.FormFor(a_victim));
            auto* aggressorActor = RE::TESForm::GetFormByID<RE::Actor>(
                m_Translator.FormFor(a_aggressor));

            if (victimActor != nullptr
                && victimActor->currentProcess != nullptr
                && aggressorActor != nullptr
                && aggressorActor->currentProcess != nullptr)
            {
                // The shove is a bench scene — it only reads when the
                // pair is actually near each other. A fight can book
                // between minds that are far apart (the force-test
                // loop fires on a timer wherever the pair is; a
                // restored keeper may not be standing at her stall
                // yet), and KnockExplosion uses the aggressor's
                // position as the knockback ORIGIN — a distant origin
                // throws the victim by a ghost, a fall with no one
                // near. Gate the shove on distance: adjacent or skip
                // (the fight still books — the feud is real, only the
                // animation is deferred to when they meet).
                const auto scene =
                    (aggressorActor->GetPosition()
                        - victimActor->GetPosition())
                        .Length();

                // The bench is a table-scene: a shove needs the pair
                // near enough to touch (about a body apart — the same
                // spacing the paired-push animation syncs). Past that
                // the shove is deferred and the thrower walks over to
                // settle it (the walk-out at the end of this block).
                if (scene > 150.0f)
                {
                    REX::INFO(
                        "LCE: {} and {} brawl at range ({:.0f} u) — {} walks over to settle it.",
                        MindLabelForm(m_Translator.FormFor(a_aggressor)),
                        MindLabelForm(m_Translator.FormFor(a_victim)),
                        scene,
                        MindLabelForm(m_Translator.FormFor(a_aggressor)));

                    // The approach: the thrower walks to the victim's
                    // spot so the next beat lands face to face.
                    Movement::WalkTo(aggressorActor, victimActor);
                }
                else
                {
                    // The jitter is capped at 1.15× so a strong draw
                    // never crosses into the crowd mod's "insane" 10+
                    // ragdoll zone — the scuffle shoves hard, never
                    // launches.
                    const auto pushJitter = std::min(
                        1.15f,
                        0.75f + 0.5f * (0.5f + IdJitter(a_victim, 0.5f)));
                    const auto force = m_Settings.FightPush * pushJitter;

                    // The standing shove (ADR-0042/0043/0045): play the
                    // game's real push — the vanilla paired-push
                    // animation (attacker's kick + victim's half, synced
                    // by MeleeBehavior) — and SCHEDULE the knock-down a
                    // beat later. The flinch is the fallback if the
                    // paired push cannot play (missing form, failed
                    // condition); a same-frame knock overrides the
                    // stagger before it is visible (that is the "falls
                    // with no push" the force tests kept showing); the
                    // push must play, then the fall lands as its
                    // consequence.
                    const auto pushed = FirePairedPush(
                        aggressorActor, victimActor);
                    if (!pushed)
                    {
                        FireHitReaction(
                            victimActor, aggressorActor,
                            m_Settings.FightStagger,
                            m_Settings.FightPushBack);
                    }

                    // The shove's receipt (0.7.5): every punch logs who
                    // and how close, which animation played, and the
                    // fall logs its force when it lands — so a fall can
                    // always be matched to its punch, or proven not to
                    // be one. The paired push is the real shove
                    // (ADR-0045); "flinch" in the receipt means the
                    // fallback played instead.
                    REX::INFO(
                        "LCE: shove: {} pushed by {} — {} at {:.0f} u (fall in {:.1f}s).",
                        MindLabelForm(m_Translator.FormFor(a_victim)),
                        MindLabelForm(m_Translator.FormFor(a_aggressor)),
                        pushed ? "paired push" : "flinch fallback",
                        scene,
                        m_Settings.FightFallDelay);

                    const auto now = std::chrono::steady_clock::now();

                    m_PendingShoves.push_back(
                        PendingShove{
                            ShoveBeat::kFall, a_victim, a_aggressor,
                            now + std::chrono::milliseconds(
                                static_cast<int>(
                                    m_Settings.FightFallDelay * 1000.0f)) });

                    // The scuffle's second beat (0.7.5): a hot-headed
                    // victim answers the punch — but after a beat, not
                    // in the same instant. The forced loop's test pair
                    // answers always, so the full chain — flinch, fall,
                    // get up, answer, counter-fall, slink off — is
                    // watchable on demand.
                    const bool answers =
                        a_force
                        || TemperOf(a_victim) >= m_Settings.FightTemper;

                    if (answers)
                    {
                        m_PendingShoves.push_back(
                            PendingShove{
                                ShoveBeat::kRetaliation,
                                a_victim, a_aggressor,
                                now + std::chrono::seconds(
                                    static_cast<int>(
                                        m_Settings.RetaliationDelay)) });
                    }
                }
            }
        }

        // The words before the blows (0.7.1 Talk's fight pool — "Come
        // on then!", "Put 'em up"): speech rides the news feed, so the
        // radio reads the fight as a caption too — and loud (0.7.5
        // field), so the threat pops on screen before the shove lands,
        // not just in the log.
        Say(a_aggressor, a_victim, Dialogue::Pool::Fight, true);

        const auto a = MindLabelForm(m_Translator.FormFor(a_aggressor));
        const auto b = MindLabelForm(m_Translator.FormFor(a_victim));

        // The forced loop's fights carry their marker so the log and
        // the radio read a test brawl distinctly from the sim's own.
        const auto suffix = a_force ? " (test brawl)" : "";

        PushNews(
            a + " and " + b
                + " come to blows — the feud turns physical." + suffix,
            Tuning::AdapterSettings::NewsCategory::Fight);

        REX::INFO(
            "LCE: {} and {} come to blows — the feud turns physical.{}.",
            a, b, suffix);
    }

    void Adapter::ForceFightLoop()
    {
        // The test hook (0.7.5): sim.test.forceFight pins a pair that
        // brawls on a loop — the full fight machinery (shove,
        // retaliation, news, gossip) on demand, for spectating and
        // verifying without waiting on the sim's coin or a day roll.
        // Off when either form id is 0; the species belt still holds.
        if (m_Settings.ForceFightA == 0 || m_Settings.ForceFightB == 0
            || m_Settings.ForceFightInterval <= 0.0f)
        {
            m_LastForceFight = {};
            return;
        }

        const auto now = std::chrono::steady_clock::now();

        if (m_LastForceFight.time_since_epoch().count() != 0
            && now - m_LastForceFight
                < std::chrono::seconds(
                    static_cast<int>(m_Settings.ForceFightInterval)))
        {
            return;
        }

        const auto entityA =
            m_Translator.EntityFor(m_Settings.ForceFightA);
        const auto entityB =
            m_Translator.EntityFor(m_Settings.ForceFightB);

        if (!entityA.IsValid() || !entityB.IsValid()
            || entityA == entityB)
        {
            return;
        }

        const auto tagA = m_Registry.GetComponent<SpeciesTag>(entityA);
        const auto tagB = m_Registry.GetComponent<SpeciesTag>(entityB);

        if (!tagA || !tagB
            || tagA->Value != Species::Human
            || tagB->Value != Species::Human)
        {
            return;
        }

        // The feud is forced, not earned: pin the pair as enemies so
        // the fight machinery — which requires an enemy feud — always
        // has its fuel, whatever the sim's dispositions say.
        const auto day = CurrentDay();
        m_Bonds[Bonds::PairKey(entityA, entityB)] =
            { Bonds::BondKind::Enemy, day };

        // Alternate who throws the punch so the shove lands on both
        // sides of the loop.
        const auto aggressor =
            m_ForceFightCount++ % 2 == 0 ? entityA : entityB;
        const auto victim = aggressor == entityA ? entityB : entityA;

        EscalateToFight(aggressor, victim, day, true);

        m_LastForceFight = now;
    }

    void Adapter::AudioProbeLoop()
    {
        // The in-world exchange probe (0.8.7a, sim.diag.audioProbe):
        // the experiment that asks whether the DLL can make two loaded
        // settlers ACTUALLY meet — A voices a greeting at B, B answers
        // a short beat later — the game's own words, voices, and
        // subtitles, so the sim's social life reads as the world
        // itself instead of text on screen. The route is
        // AIProcess::ProcessGreet with the OTHER NPC as the target —
        // the same proven-stable native seam the ambient probe proved
        // at the player. Both dead ends are gone: the papyrus-VM Say
        // route (the only one that voices a SPECIFIC INFO) is the
        // PROVEN CRASHER (2026-08-15, PackVariables -> Variable::
        // reset -> null deref), and the nextGreeting seed is proven
        // ignored (the read-back showed the game re-picks its own line
        // every time), so the game picks the words — we pick the
        // moment, the pair, and the direction. One pair exchanges
        // every AudioProbeEvery seconds; the log tags who opened, who
        // answered, and the game's chosen INFOs. Off by default.
        if (!m_Settings.AudioProbe || m_Settings.AudioProbeEvery <= 0.0f)
        {
            // Own state only — the shared exchange slot belongs to the
            // production crossings (ProcessExchange). The probe-off
            // gate once reset it here, which wiped every scheduled
            // answer and broke the one-at-a-time lock (0.8.7b field
            // find: 194 opens, 0 answers).
            m_LastAudioProbe = {};
            return;
        }

        const auto now = std::chrono::steady_clock::now();

        if (m_LastAudioProbe.time_since_epoch().count() != 0
            && now - m_LastAudioProbe
                < std::chrono::seconds(
                    static_cast<int>(m_Settings.AudioProbeEvery)))
        {
            return;
        }

        const auto* player = RE::PlayerCharacter::GetSingleton();

        if (player == nullptr)
        {
            return;
        }

        // The loaded humanoid minds, gathered once — the same 3D gate
        // the interaction pass uses (a streamed-out actor reads a
        // garbage position). A is the one nearest the player within
        // earshot; B is the nearest other to A within a market square's
        // worth — the pair genuinely crossing paths where the player
        // can watch them meet.
        struct Loaded
        {
            LCE::Simulation::EntityId Id;
            RE::Actor* Actor;
            RE::NiPoint3 Pos;
        };

        std::vector<Loaded> loaded;
        const auto playerPos = player->GetPosition();

        m_Registry.ForEachWithComponent<FormRef>(
            [&](LCE::Simulation::EntityId a_entity, FormRef& a_ref)
            {
                const auto species =
                    m_Registry.GetComponent<SpeciesTag>(a_entity);

                if (!species || species->Value != Species::Human)
                {
                    return;
                }

                auto* actor =
                    RE::TESForm::GetFormByID<RE::Actor>(a_ref.FormId);

                if (actor == nullptr || actor->Get3D() == nullptr)
                {
                    return;
                }

                loaded.push_back(
                    { a_entity, actor, actor->GetPosition() });
            });

        static constexpr float kProbePairRadius = 500.0f;

        // A — the nearest to the player within earshot.
        std::optional<Loaded> a;
        float aDist = m_Settings.SubtitleRadius;

        for (const auto& c : loaded)
        {
            const auto d = playerPos.GetDistance(c.Pos);

            if (d <= aDist)
            {
                aDist = d;
                a = c;
            }
        }

        if (!a.has_value())
        {
            return;   // no one in earshot — nothing to probe
        }

        // B — the nearest other loaded humanoid to A, close enough to
        // be genuinely crossing paths.
        std::optional<Loaded> b;
        float bDist = kProbePairRadius;

        for (const auto& c : loaded)
        {
            if (c.Id == a->Id)
            {
                continue;
            }

            const auto d = a->Pos.GetDistance(c.Pos);

            if (d <= bDist)
            {
                bDist = d;
                b = c;
            }
        }

        if (!b.has_value())
        {
            return;   // alone — nothing to exchange
        }

        // A opens — the same exchange the sim's own crossings use, so
        // the probe verifies the production path exactly. The probe
        // always greets (the register stone is production's question).
        OpenExchange(a->Actor, b->Actor, ExchangeRegister::Greet);

        m_LastAudioProbe = now;
    }

    Adapter::GreetResult Adapter::VoiceAt(
        RE::Actor* a_speaker, RE::Actor* a_target,
        RE::DIALOGUE_SUBTYPE a_subtype)
    {
        // The one game call the whole exchange rides: the AI process's
        // greeting entry with the OTHER NPC as the target — the same
        // proven-stable native seam the ambient probe proved at the
        // player. The game picks the words; we pick the moment, the
        // pair, the direction, and the register (the subtype the game
        // is asked to voice). Accepted tells whether the game took the
        // greeting at all; Played is the INFO built into the spoken
        // dialogue item at read-back time — empty when the greeting is
        // QUEUED (the actor was busy; the line may still voice a beat
        // later), which the log tags honestly.
        if (a_speaker == nullptr || a_target == nullptr
            || a_speaker->currentProcess == nullptr)
        {
            return {};
        }

        const bool accepted = a_speaker->currentProcess->ProcessGreet(
            a_speaker, RE::DIALOGUE_TYPE::kMiscellaneous,
            a_subtype, a_target, nullptr,
            true, false, true, true);

        std::uint32_t played = 0;

        if (a_speaker->currentProcess->high != nullptr
            && a_speaker->currentProcess->high->greetTopic != nullptr
            && a_speaker->currentProcess->high->greetTopic->info
                != nullptr)
        {
            played = a_speaker->currentProcess->high->greetTopic
                ->info->GetFormID();
        }

        return { accepted, played };
    }

    Adapter::ExchangeRegister Adapter::RegisterFor(
        LCE::Simulation::EntityId a_a,
        LCE::Simulation::EntityId a_b,
        Bonds::BondKind a_kind)
    {
        // The bond names the register (0.8.9): a bonded household
        // talks family; a compatible unpaired pair flirts (the moment —
        // the game voices its own words); everyone else greets.
        if (a_kind == Bonds::BondKind::Spouse
            || a_kind == Bonds::BondKind::Sweetheart
            || a_kind == Bonds::BondKind::Friend)
        {
            return ExchangeRegister::Family;
        }

        if (a_kind == Bonds::BondKind::None && CompatiblePair(a_a, a_b))
        {
            return ExchangeRegister::Flirt;
        }

        return ExchangeRegister::Greet;
    }

    bool Adapter::CompatiblePair(
        LCE::Simulation::EntityId a_a,
        LCE::Simulation::EntityId a_b)
    {
        // The never-romance gate, reused (0.7.5 / 0.8.6b field finds):
        // children, robots, curated kin, and companions never enter the
        // dating pool — friends and feuds are fine, romance is closed.
        // Both must be single adults (no spouse bond either way) with
        // real warmth between them — at or above the friend line, the
        // same threshold the sweetheart formation reads.
        const auto tagA = m_Registry.GetComponent<SpeciesTag>(a_a);
        const auto tagB = m_Registry.GetComponent<SpeciesTag>(a_b);

        if (!tagA || !tagB
            || tagA->Value != Species::Human
            || tagB->Value != Species::Human)
        {
            return false;
        }

        if (Households::SpouseOf(m_Bonds, a_a).IsValid()
            || Households::SpouseOf(m_Bonds, a_b).IsValid())
        {
            return false;
        }

        if (m_Kin.contains(
                Bonds::PairKey(a_a, a_b))
            || m_Registry.GetComponent<CompanionTag>(a_a)
            || m_Registry.GetComponent<CompanionTag>(a_b))
        {
            return false;
        }

        const float shared = std::min(
            DispositionOf(a_a, a_b), DispositionOf(a_b, a_a));

        return shared >= m_BondThresholds.Friend;
    }

    Adapter::VoiceOutcome Adapter::VoiceRegister(
        RE::Actor* a_speaker, RE::Actor* a_target,
        ExchangeRegister a_register)
    {
        // The register -> subtype map (0.8.9): the family register
        // asks the game for its general greeting first — a warmer or
        // different line where the voice has one — and falls back to
        // the proven hello if the game refuses outright (an accepted
        // but queued greeting is never retried: the line may still
        // voice late, and a second call would double-book). Greet and
        // flirt voice the proven hello; the flirt's difference is the
        // moment — the pair, the compatibility — not a distinct line
        // the game has no INFO bank for.
        VoiceOutcome outcome;
        const char* name = "greet";
        auto subtype = RE::DIALOGUE_SUBTYPE::kMisc_Hello;

        if (a_register == ExchangeRegister::Family
            && m_Settings.InteractRegisterFamily)
        {
            name = "family";
            subtype = RE::DIALOGUE_SUBTYPE::kMisc_Greeting;
        }
        else if (a_register == ExchangeRegister::Flirt)
        {
            name = "flirt";
        }

        outcome.Result = VoiceAt(a_speaker, a_target, subtype);

        // The hello fallback: the family register's greeting was
        // refused outright — nothing voiced — so retry once with the
        // proven hello. A family exchange never goes silent.
        if (!outcome.Result.Accepted
            && subtype != RE::DIALOGUE_SUBTYPE::kMisc_Hello)
        {
            name = "family(hello)";
            outcome.Result = VoiceAt(
                a_speaker, a_target,
                RE::DIALOGUE_SUBTYPE::kMisc_Hello);
        }

        outcome.RegisterName = name;
        return outcome;
    }

    bool Adapter::IsRoadPerson(
        LCE::Simulation::EntityId a_entity) const
    {
        // The road roles (0.8.9 road-feed stone): a non-settler mind
        // whose base form is a road role — Provisioner, Caravan Guard,
        // Caravan Worker — travels with the caravans and the supply
        // lines. A settler-faction actor is home at its settlement,
        // whatever its label; a road person's body follows the game's
        // caravan AI.
        const auto formId = m_Translator.FormFor(a_entity);

        if (formId == 0)
        {
            return false;
        }

        auto* actor = RE::TESForm::GetFormByID<RE::Actor>(formId);

        if (actor == nullptr)
        {
            return false;
        }

        const auto* faction = SettlerFaction();

        if (faction != nullptr && actor->IsInFaction(faction))
        {
            return false;   // a settler — home is the settlement
        }

        const auto* base = actor->GetObjectReference();

        return base != nullptr
            && TLC::Names::IsRoadRole(
                RE::TESFullName::GetFullName(*base));
    }

    void Adapter::FeedRoadPeople()
    {
        // The road feed (0.8.9 road-feed stone): a road person eats
        // from the caravan's own supplies on the road — the market
        // seed excludes them, so the core never walks them to a
        // settlement bench, and this pass restores their hunger when
        // it falls to the threshold, the same meal the market arrival
        // gives a settler. The log names the meal so the travel reads
        // real. 0 disables the feed (a road person then never eats).
        if (m_Settings.RoadFeedThreshold <= 0.0f)
        {
            return;
        }

        m_Registry.ForEachWithComponent<LCE::Simulation::Needs>(
            [&](LCE::Simulation::EntityId a_entity,
                LCE::Simulation::Needs& a_needs)
            {
                if (!IsRoadPerson(a_entity))
                {
                    return;
                }

                for (auto& need : a_needs.List)
                {
                    if (need.Type == LCE::Simulation::NeedType::Hunger
                        && need.Value <= m_Settings.RoadFeedThreshold)
                    {
                        static_cast<void>(RestoreHunger(a_needs));

                        REX::INFO(
                            "road: {} ate on the road.",
                            MindLabel(a_entity));
                        return;
                    }
                }
            });
    }

    void Adapter::OpenExchange(
        RE::Actor* a_speaker, RE::Actor* a_target,
        ExchangeRegister a_register)
    {
        // The lock: one exchange in the world at a time — a crowd
        // never all talks at once. The caller has already gated the
        // pair (loaded, resting, in earshot); this books the opener
        // and schedules B's answer.
        if (m_Exchange.has_value() || a_speaker == nullptr
            || a_target == nullptr
            || a_speaker->currentProcess == nullptr
            || a_target->currentProcess == nullptr)
        {
            return;
        }

        const auto outcome = VoiceRegister(a_speaker, a_target, a_register);

        REX::INFO(
            "exchange: {} greeted {} — {} register, {}, played {:#08x}.",
            MindLabelForm(a_speaker->GetFormID()),
            MindLabelForm(a_target->GetFormID()),
            outcome.RegisterName,
            outcome.Result.Played != 0
                ? "fired"
                : (outcome.Result.Accepted ? "queued" : "refused"),
            outcome.Result.Played);

        // B's answer, a short beat later — the opener's line gets time
        // to finish, and the answer re-checks the pair is still
        // together before it speaks. The register rides along so the
        // answer asks the game for the same register the opener did.
        static constexpr float kExchangeAnswerDelay = 2.5f;

        const auto now = std::chrono::steady_clock::now();

        m_Exchange = Exchange{
            a_target->GetFormID(),
            a_speaker->GetFormID(),
            a_register,
            now
                + std::chrono::duration_cast<
                    std::chrono::steady_clock::duration>(
                    std::chrono::duration<float>(kExchangeAnswerDelay)),
        };
    }

    void Adapter::ProcessExchange()
    {
        // The answer beat (0.8.7b): B answers A a short beat after the
        // opener. The refs are re-resolved by form id, never held
        // across ticks; a partner who parted drops the reply (no ghost
        // exchanges). Runs every tick, independent of the probe flag —
        // the production crossings share the same slot.
        if (!m_Exchange.has_value())
        {
            return;
        }

        const auto now = std::chrono::steady_clock::now();

        if (now < m_Exchange->Due)
        {
            return;
        }

        const auto answererForm = m_Exchange->AnswererForm;
        const auto addresseeForm = m_Exchange->AddresseeForm;
        const auto reg = m_Exchange->Register;

        m_Exchange.reset();

        auto* b = RE::TESForm::GetFormByID<RE::Actor>(answererForm);
        auto* a = RE::TESForm::GetFormByID<RE::Actor>(addresseeForm);

        if (b == nullptr || a == nullptr || b->Get3D() == nullptr
            || a->Get3D() == nullptr || b->currentProcess == nullptr)
        {
            REX::INFO(
                "exchange: {}'s answer dropped — the pair parted.",
                MindLabelForm(answererForm));
            return;
        }

        const auto outcome = VoiceRegister(b, a, reg);

        REX::INFO(
            "exchange: {} answered {} — {} register, {}, played {:#08x}.",
            MindLabelForm(answererForm), MindLabelForm(addresseeForm),
            outcome.RegisterName,
            outcome.Result.Played != 0
                ? "fired"
                : (outcome.Result.Accepted ? "queued" : "refused"),
            outcome.Result.Played);
    }

    void Adapter::ProcessPendingShoves()
    {
        // The scuffle's queued beats (0.7.5): every physical beat of a
        // fight fires on its own schedule — the flinch plays first
        // (fired at the punch), the fall lands a beat later so the
        // stagger is visible (ADR-0043), the hot-headed victim (or
        // forced test pair) answers after the get-up window with its
        // own flinch, its fall follows, and the loser slinks off last.
        // Every beat re-checks the scene: if the pair parted, the beat
        // dies — no ghost punches, no phantom falls.
        const auto now = std::chrono::steady_clock::now();

        // New beats (the counter-fall, the walk-off) are collected here
        // and appended AFTER the loop: push_back while iterating can
        // reallocate the vector and invalidate the loop iterator — the
        // debug STL's "vector iterators incompatible" assertion the
        // first sequenced build hit.
        std::vector<PendingShove> additions;

        for (auto it = m_PendingShoves.begin();
             it != m_PendingShoves.end();)
        {
            if (now < it->Due)
            {
                ++it;
                continue;
            }

            const auto kind = it->Kind;
            const auto victim = it->Victim;    // takes the hit
            const auto thrower = it->Thrower;  // threw it
            it = m_PendingShoves.erase(it);

            if (m_Settings.FightPush <= 0.0f)
            {
                continue;
            }

            auto* victimActor = RE::TESForm::GetFormByID<RE::Actor>(
                m_Translator.FormFor(victim));
            auto* throwerActor = RE::TESForm::GetFormByID<RE::Actor>(
                m_Translator.FormFor(thrower));

            if (victimActor == nullptr
                || victimActor->currentProcess == nullptr
                || throwerActor == nullptr
                || throwerActor->currentProcess == nullptr)
            {
                continue;   // died or unloaded — the beat dies with them
            }

            const auto scene =
                (throwerActor->GetPosition()
                    - victimActor->GetPosition())
                    .Length();

            // Same table-scene as the shove itself: the beats need the
            // pair within reach, or they die — no phantom falls.
            if (scene > 150.0f)
            {
                continue;   // they parted — no beat at range
            }

            switch (kind)
            {
            case ShoveBeat::kFall:
            case ShoveBeat::kCounterFall:
            {
                // The standing guard (0.7.6, ADR-0050): if the victim
                // is still mid-knock — down, getting up, a knock
                // queued — the fall would land on top of it, the
                // ground-slide and the double-down looks. Wait a beat
                // and re-check instead of firing into the down state.
                if (IsDown(victimActor))
                {
                    additions.push_back(PendingShove{
                        kind, victim, thrower,
                        std::chrono::steady_clock::now()
                            + std::chrono::milliseconds(500) });
                    REX::INFO(
                        "LCE: fall waits — {} still on the ground.",
                        MindLabelForm(m_Translator.FormFor(victim)));
                    break;
                }

                // The fall (ADR-0043): the knock-down lands a beat
                // after the flinch so the stagger actually plays.
                // Force jitters off the victim, capped at 1.15× so a
                // strong draw never launches (0.7.6: the base is the
                // tip-over zone now — the push carries the travel, the
                // fall only has to put them down).
                const auto fallJitter = std::min(
                    1.15f,
                    0.75f + 0.5f * (0.5f + IdJitter(victim, 0.5f)));
                const auto force = m_Settings.FightPush * fallJitter;

                victimActor->currentProcess->KnockExplosion(
                    victimActor, throwerActor->GetPosition(), force);

                REX::INFO(
                    "LCE: fall: {} knocked down by {}'s shove — {:.1f} force at {:.0f} u.",
                    MindLabelForm(m_Translator.FormFor(victim)),
                    MindLabelForm(m_Translator.FormFor(thrower)),
                    force, scene);
                break;
            }

            case ShoveBeat::kRetaliation:
            {
                // The standing guard (0.7.6, ADR-0050): the answer
                // must land on its feet — if either is still down or
                // getting up, the counter plays into the get-up and
                // both end up on the floor. Wait a beat and re-check.
                if (IsDown(victimActor) || IsDown(throwerActor))
                {
                    additions.push_back(PendingShove{
                        kind, victim, thrower,
                        std::chrono::steady_clock::now()
                            + std::chrono::milliseconds(500) });
                    REX::INFO(
                        "LCE: answer waits — {} not on their feet.",
                        MindLabelForm(m_Translator.FormFor(victim)));
                    break;
                }

                // The answer (0.7.5): the victim answers the punch
                // after the get-up window — its own paired push now,
                // its fall a beat later, and the loser walks off after
                // that. (victim = the one who answers, thrower = the
                // one who threw first and now takes the counter.)
                const auto answered = FirePairedPush(
                    victimActor, throwerActor);
                if (!answered)
                {
                    FireHitReaction(
                        throwerActor, victimActor,
                        m_Settings.FightStagger,
                        m_Settings.FightPushBack);
                }

                REX::INFO(
                    "LCE: {} shoves {} back — flinch {}, hot heads, both.",
                    MindLabelForm(m_Translator.FormFor(victim)),
                    MindLabelForm(m_Translator.FormFor(thrower)),
                    m_Settings.FightStagger);

                const auto beat = std::chrono::steady_clock::now()
                    + std::chrono::milliseconds(
                        static_cast<int>(
                            m_Settings.FightFallDelay * 1000.0f));

                additions.push_back(
                    PendingShove{
                        ShoveBeat::kCounterFall, thrower, victim, beat });

                // The parting beat (0.7.5 field): the loser must GET UP
                // before fleeing — the walk-off lands after the get-up
                // window, not in the same instant as the counter-fall.
                additions.push_back(
                    PendingShove{
                        ShoveBeat::kWalkOff, thrower, victim,
                        beat + std::chrono::milliseconds(
                            static_cast<int>(
                                m_Settings.FightPartDelay * 1000.0f)) });
                break;
            }

            case ShoveBeat::kWalkOff:
            {
                // The loser slinks off: the one who threw first walks
                // to the far side of the scene from the one who
                // answered — the flee's first visible beat while the
                // engine's Flee action is still a table stub.
                Movement::WalkAwayFrom(
                    victimActor, throwerActor->GetPosition());
                break;
            }
            }
        }

        for (auto& addition : additions)
        {
            m_PendingShoves.push_back(std::move(addition));
        }
    }

    void Adapter::RadioCaptions()
    {
        if (!m_Settings.NewsEnabled)
        {
            return;
        }

        // A settlement radio the player built: while one is near, the
        // settlement tells its story — the news feed as on-screen
        // captions, one per sim.radio.caption.every seconds. The base
        // form is radio.base.formid (default the workshop "Radio" — a
        // hardcoded FormID, flagged for xEdit verification; the key
        // exists so a wrong pin is a config line, never a rebuild).
        auto* player = RE::PlayerCharacter::GetSingleton();

        if (player == nullptr)
        {
            return;
        }

        auto* cell = player->GetParentCell();

        if (cell == nullptr)
        {
            return;
        }

        bool radioNearby = false;

        cell->ForEachReferenceInRange(
            player->GetPosition(), m_Settings.RadioRadius,
            [&](RE::TESObjectREFR* a_ref) -> RE::BSContainer::ForEachResult
            {
                if (a_ref == nullptr)
                {
                    return RE::BSContainer::ForEachResult::kContinue;
                }

                const auto* base = a_ref->GetObjectReference();

                if (base != nullptr
                    && base->GetFormID() == m_Settings.RadioBaseFormId)
                {
                    radioNearby = true;
                    return RE::BSContainer::ForEachResult::kStop;
                }

                return RE::BSContainer::ForEachResult::kContinue;
            });

        if (!radioNearby)
        {
            return;
        }

        const auto now = std::chrono::steady_clock::now();

        if (now - m_LastRadioCaption < std::chrono::duration<float>(
                m_Settings.RadioCaptionEvery))
        {
            return;
        }

        m_LastRadioCaption = now;

        const auto line = m_News.NextLine();

        if (!line.empty())
        {
            // Same HUD diagnostic as PushNews: prove or clear the
            // message-path stall (0.8.1 verification).
            REX::DEBUG("hud: radio '{}'", line);
            RE::SendHUDMessage::ShowHUDMessage(
                line.c_str(), "", false, false);
        }
    }

    void Adapter::StartWorld()
    {
        if (m_Started)
        {
            return;
        }

        const auto count = SeedLoadedActors();

        // The market: if the workshop form is loaded it becomes an entity,
        // and every mind remembers where to trade (ADR-0024 — the adapter
        // reports events; the simulation gives them meaning). A mind that
        // knows the market can decide MoveTo; one that doesn't explores.
        SeedMarket(true);

        // The sellers (0.7.4 Trade with anyone): who sells in the loaded
        // world, remembered by the minds near them — a person who sells
        // out-scores the bench while both are fresh (the seed weight is
        // a hair above the market's), so a hungry mind trades with the
        // trader on the road or at the market stall, not only the bench.
        SeedVendors(true);

        // The conflict source's settlement (0.7.0 Stone 2): every mind
        // with a market memory belongs to its settlement's group — the
        // engine's echo then spreads a slight (or a warmth) through the
        // group, and InheritGroupAttitudes gives a newcomer the
        // settlement's inherited feelings. Derived from the seeded
        // memories, never persisted.
        AssignSettlementGroups();

        REX::INFO("The Commonwealth wakes up: {} settlers became minds.", count);
        LCE::Logging::Info(
            "The Commonwealth wakes up: " + std::to_string(count) + " settlers became minds.");

        const auto children = CountSimOnlyChildren();

        if (children > 0)
        {
            REX::INFO(
                "The Commonwealth wakes up: {} sim-only {} born to their households.",
                children, children == 1 ? "child" : "children");
            LCE::Logging::Info(
                "The Commonwealth wakes up: " + std::to_string(children)
                + (children == 1 ? " sim-only child born to its household."
                                 : " sim-only children born to their households."));
        }

        LCE::Logging::Flush();

        m_Started = true;
    }

    void Adapter::EndWorld()
    {
        if (!m_Started)
        {
            return;
        }

        // Clear keeps the serializers (registered once at init) so the
        // next StartWorld can translate fresh. The co-save stone will
        // replace this with Capture/Restore.
        m_Registry.Clear();
        m_Translator.Clear();
        m_LastLogged.clear();
        m_FeederLogged.clear();
        m_StallKeepers.clear();
        m_Bonds.clear();
        m_ConflictGates.clear();
        m_Burials.clear();
        m_MedicineStock.clear();
        m_Walks.clear();
        // NOTE: m_WorkshopNames deliberately survives EndWorld — it has
        // the same lifetime as m_Workshops (static per load order; a
        // workshop's location name never changes). Clearing it here
        // broke the post-restore pay line: the restore's EndWorld wiped
        // the cache, the re-seed skipped the census (m_WorkshopsReady
        // still true), and every settlement paid under a bare hex
        // (2026-08-14 field find).
        m_ArrivedAt.clear();
        m_WalkRefusedUntil.clear();
        m_InteractCooldown.clear();
        m_InteractPairCooldown.clear();
        m_InteractOpenedToday.clear();
        m_MarketAttendance.clear();
        m_LastWander.clear();
        m_LastCough.clear();
        m_LastCoughGlobal = std::chrono::steady_clock::time_point{};
        m_RecentDeaths.clear();
        m_GriefAnnounced.clear();
        m_IllnessAnnounced.clear();
        m_LastIllnessNews = std::chrono::steady_clock::time_point{};
        m_IllnessNewsCount = 0;
        m_LastRadstormExposureDay =
            std::numeric_limits<std::uint64_t>::max();
        m_PendingDeaths.clear();
        m_SeenAlive.clear();
        m_UsedNames.clear();
        m_News.Clear();

        // The ownership read re-arms per world (0.8.6c field find
        // 2026-08-14): the query once ran at the menu-world wake —
        // before the save's quest state applied (Initialized = false,
        // PlayerOwnsAWorkshop = false in the dump) — and "succeeded"
        // on fresh defaults, so the real load's restored ownership
        // never got read. Every world re-queries from the quest's
        // restored Workshops array, and the retry window re-opens.
        m_OwnershipQueried = false;
        m_OwnershipAttempts = 0;
        m_LastOwnershipAttempt = std::chrono::steady_clock::time_point{};
        m_WorkshopOwned.clear();
        m_OwnershipCount = 0;
        m_OwnershipOwned = 0;

        m_TickCalled = false;
        m_FirstPassLogged = false;
        m_Started = false;
    }

    std::size_t Adapter::CountSimOnlyChildren()
    {
        std::size_t count = 0;

        m_Registry.ForEachWithComponent<SpeciesTag>(
            [&](LCE::Simulation::EntityId a_entity,
                const SpeciesTag& a_tag)
            {
                if (a_tag.Value == Species::Child
                    && m_Registry.GetComponent<FormRef>(a_entity) == nullptr)
                {
                    ++count;
                }
            });

        return count;
    }

    void Adapter::EnsureWorkshop(std::uint32_t a_formId)
    {
        // Already known — the workshop entity survives within one world.
        if (m_Translator.EntityFor(a_formId).IsValid())
        {
            return;
        }

        // The workshop must be a loaded reference to be walked to.
        if (RE::TESForm::GetFormByID<RE::TESObjectREFR>(a_formId) == nullptr)
        {
            return;
        }

        const auto id = m_Registry.CreateEntity();

        m_Registry.AddComponent<FormRef>(id, FormRef{ a_formId });
        m_Translator.Add(a_formId, id);
    }

    // The ownership read (0.8.6c): the game's own WorkshopPlayerOwnership
    // actor value (form 0x33C, Fallout4.esm) read off each workshop
    // ref — the exact storage SetOwnedByPlayer writes and the game's
    // own checks read. Every earlier attempt read the wrong thing
    // (see the body's history); the AV on the persistent ref survives
    // save/load and is readable regardless of cell streaming. True
    // when at least one read landed; the caller SEH-guards the pass
    // and retries from SeedMarket until the AVIF resolves or the
    // window closes. Re-armed per world (EndWorld) so a load that
    // follows the menu-world wake re-reads the restored quest.
    bool Adapter::QueryPlayerOwnedWorkshops()
    {
        auto* game = RE::GameVM::GetSingleton();
        const auto vm = game != nullptr ? game->GetVM() : nullptr;

        if (!vm)
        {
            return false;
        }

        m_WorkshopOwned.clear();
        m_OwnershipCount = 0;
        m_OwnershipOwned = 0;

        // The ownership AVIF (0.8.6c definitive): the game's own
        // WorkshopPlayerOwnership actor value — the exact AV the
        // vanilla WorkshopScript.SetOwnedByPlayer writes on the
        // workshop ref ("SetValue(WorkshopParent.WorkshopPlayerOwnership,
        // (bIsOwned as float))"), and the exact read the game's own
        // checks use ("Workshops[index].GetValue(WorkshopPlayerOwnership)
        // > 0"). WSFW pins its form to 0x33C in Fallout4.esm (a base
        // game form, always loaded). Every earlier attempt read the
        // wrong thing: GetOwner() returns null for every workshop; the
        // WorkshopPlayerOwned AVIF (the singleton member at 0x3A0) is
        // a different form the game never sets for this; the console
        // rejects both getav names ("not found for perimeter actor
        // value"); the per-ref OwnedByPlayer script property reads
        // fresh defaults until the cell streams; and the quest's
        // Workshops script array is built at runtime from the
        // WorkshopsCollection alias — empty until the quest's init
        // completes. The AV on the persistent ref survives save/load
        // and is readable regardless of cell streaming. Per-ref reads
        // land in the form-id-keyed map the requireOwned gate
        // consults; the caller SEH-guards the pass and retries from
        // SeedMarket until the AVIF resolves or the window closes.
        // Re-armed per world (EndWorld) so a load that follows the
        // menu-world wake re-reads the restored quest.
        const auto* avif = RE::TESForm::GetFormByID<
            RE::ActorValueInfo>(0x0000033C);

        if (avif == nullptr)
        {
            REX::WARN(
                "economy: WorkshopPlayerOwnership AVIF (0x33C) not found — ownership reads empty (retry next census; the requireOwned gate would gate everyone meanwhile).");
            return false;
        }

        if (m_OwnershipAttempts == 1 && avif->formEditorID.length() > 0)
        {
            REX::INFO(
                "economy: WorkshopPlayerOwnership AVIF resolved (0x33C, editor id '{}') — reading it off each workshop ref.",
                avif->formEditorID.c_str());
        }

        std::size_t read = 0;
        std::size_t sampled = 0;

        for (const auto& ws : m_Workshops)
        {
            auto* refr =
                RE::TESForm::GetFormByID<RE::TESObjectREFR>(ws.FormId);

            if (refr == nullptr)
            {
                continue;
            }

            const float value = refr->GetActorValue(*avif);
            const bool owned = value >= 0.5f;

            m_WorkshopOwned[ws.FormId] = owned;
            ++read;
            m_OwnershipOwned += owned ? 1 : 0;

            // One-time diagnostics: the first few workshops with
            // their form id, name, and raw flag, so the gate can be
            // verified against the player's own Pip-Boy map.
            if (sampled < 5)
            {
                ++sampled;

                const auto nameIt = m_WorkshopNames.find(ws.FormId);

                REX::INFO(
                    "economy: workshop {:#x} '{}' WorkshopPlayerOwnership = {}.",
                    ws.FormId,
                    nameIt != m_WorkshopNames.end()
                        ? nameIt->second
                        : "?",
                    value);
            }
        }

        m_OwnershipCount = read;

        REX::INFO(
            "economy: WorkshopPlayerOwnership read for {} of {} workshops — {} owned by the player.",
            read, m_Workshops.size(), m_OwnershipOwned);

        return read > 0;
    }

    void Adapter::RefreshWorkshops()
    {
        // Throttled retry: the census can run before the game's REFR
        // data is fully available, so an empty result must never be
        // pinned — a false 0 would lock the whole session into the
        // single-bench fallback. Once a non-empty list is found it is
        // final (static per load order); the seed cycle re-scans an
        // empty one at most every few seconds.
        const auto now = std::chrono::steady_clock::now();

        if (m_LastCensus.time_since_epoch().count() != 0
            && now - m_LastCensus < std::chrono::seconds(5))
        {
            return;
        }

        m_LastCensus = now;
        m_Workshops.clear();

        auto* dataHandler = RE::TESDataHandler::GetSingleton();

        if (dataHandler == nullptr)
        {
            return;
        }

        // FO4 does not store REFRs in the data handler's flat form
        // array — GetFormArray<TESObjectREFR> is a Skyrim-ism that is
        // always empty here (verified in-game: 0 REFRs across retries).
        // Settlement workbenches are all *persistent* refs, and every
        // persistent ref lives in its worldspace's persistent cell,
        // which is loaded at world start — so one pass over the
        // worldspaces' persistent cells finds every settlement market
        // with valid positions, without loading a single cell.
        const auto& worldspaces =
            dataHandler->GetFormArray<RE::TESWorldSpace>();

        std::size_t probed = 0;  // diagnostic refs logged (once)

        for (const auto* world : worldspaces)
        {
            if (world == nullptr)
            {
                continue;
            }

            auto* cell = world->persistentCell;

            if (cell == nullptr)
            {
                continue;
            }

            cell->ForEachReference(
                [&](RE::TESObjectREFR* a_ref)
                {
                    const auto* base = a_ref->GetObjectReference();

                    if (base == nullptr)
                    {
                        return RE::BSContainer::ForEachResult::kContinue;
                    }

                    // Diagnostic (once per session): the first bases in
                    // the persistent cells, so a 0-result census says
                    // what the filter saw, not just that it saw nothing.
                    if (!m_CensusDiagnosed && probed < 3)
                    {
                        REX::INFO(
                            "census probe: world {:#x} persistent REFR {:#x} base {:#x}.",
                            world->GetFormID(), a_ref->GetFormID(),
                            base->GetFormID());
                        ++probed;
                    }

                    if (base->GetFormID() != kWorkshopBaseFormId)
                    {
                        return RE::BSContainer::ForEachResult::kContinue;
                    }

                    const auto pos = a_ref->GetPosition();
                    const auto workshopFormId = a_ref->GetFormID();

                    m_Workshops.push_back(WorkshopPosition{
                        workshopFormId, pos.x, pos.y });

                    // The workshop's display name, captured while the
                    // ref is in hand (the census reads the persistent
                    // cells at world start, before the workshops stream
                    // in — MarketLabel falls back to this cache when the
                    // form is not loaded yet, so the stipend's
                    // per-settlement line names the settlement instead
                    // of a bare hex). The settlement's name lives on
                    // the workshop's LOCATION (Sanctuary is the
                    // location's name); the workbench ref itself reads
                    // the generic "Workshop" — 2026-08-14 field find,
                    // every pay line read "Workshop [hex]". Prefer the
                    // location, fall back to the ref.
                    const auto* workshopLocation =
                        a_ref->GetCurrentLocation();

                    const auto locationName = workshopLocation != nullptr
                        ? RE::TESFullName::GetFullName(*workshopLocation)
                        : std::string_view{};

                    const auto refName = a_ref->GetDisplayFullName();

                    if (!locationName.empty())
                    {
                        m_WorkshopNames[workshopFormId] =
                            std::string(locationName);
                    }
                    else if (refName != nullptr)
                    {
                        m_WorkshopNames[workshopFormId] = refName;
                    }

                    // The ownership read (0.8.6c): does the player own
                    // this workshop? QueryPlayerOwnedWorkshops fills
                    // the map (each ref's OwnedByPlayer flag — every
                    // other read died in the field; see that function
                    // for the whole story). A workshop missing from
                    // the map (ref not found yet, script not bound) is
                    // unowned: the gate's safe default.
                    return RE::BSContainer::ForEachResult::kContinue;
                });
        }

        if (probed > 0)
        {
            m_CensusDiagnosed = true;
        }

        // A found list is final; an empty one is not (see the throttle
        // above) — the legacy single-bench fallback covers the world
        // while the census keeps looking.
        if (!m_Workshops.empty())
        {
            m_WorkshopsReady = true;
        }

        REX::INFO(
            "settlement census: {} worldspaces, {} workshops known (base {:#x}){}.",
            worldspaces.size(), m_Workshops.size(), kWorkshopBaseFormId,
            m_Workshops.empty() ? " — will retry" : " — markets are per settlement");

        // The name-cache diagnostic (0.8.6b field): how many workshop
        // names the census captured, and the first few — so the pay
        // line's settlement naming is verifiable in-game (an empty
        // cache with a full census says the capture read is wrong; a
        // populated cache with hex pay lines says the pay's form ids
        // don't match the census's). One-time.
        if (m_CensusDiagnosed)
        {
            auto it = m_WorkshopNames.begin();
            const auto end =
                it != m_WorkshopNames.end()
                ? std::next(it, std::min<std::size_t>(3, m_WorkshopNames.size()))
                : it;

            std::string sample;

            for (; it != end; ++it)
            {
                if (!sample.empty())
                {
                    sample += ", ";
                }

                sample += FormatHex8(it->first) + "='" + it->second + "'";
            }

            REX::INFO(
                "census: {} workshop names cached{}",
                m_WorkshopNames.size(),
                sample.empty() ? " (empty)."
                               : " — " + sample + ".");
        }
    }

    LCE::Simulation::EntityId Adapter::OwnerEntityFor(
        LCE::Simulation::EntityId a_entity)
    {
        const auto formId = m_Translator.FormFor(a_entity);
        auto* actor = RE::TESForm::GetFormByID<RE::Actor>(formId);

        if (actor == nullptr)
        {
            return {};
        }

        const auto* owner = actor->GetOwner();
        LCE::Simulation::EntityId ownerEntity;

        if (owner != nullptr)
        {
            ownerEntity = m_Translator.EntityFor(owner->GetFormID());
        }

        // One line per animal per world — the readout that proves the
        // ownership resolution in-game (the junkyard dog's owner is
        // likely the player, which is no entity — the dog comes home to
        // be fed).
        if (m_FeederLogged.insert(a_entity).second)
        {
            if (owner == nullptr)
            {
                REX::INFO(
                    "LCE: animal {:#x} has no owner — fed by the settlement.",
                    formId);
            }
            else if (ownerEntity.IsValid())
            {
                REX::INFO(
                    "LCE: animal {:#x} is fed by its owner {:#x} (a settler).",
                    formId, owner->GetFormID());
            }
            else
            {
                REX::INFO(
                    "LCE: animal {:#x} has no sim owner ({:#x}) — fed by the settlement.",
                    formId, owner->GetFormID());
            }
        }

        return ownerEntity;
    }

    void Adapter::ReportArrival(
        LCE::Simulation::EntityId a_entity, std::uint32_t a_targetFormId)
    {
        using namespace LCE::Simulation;

        const auto tag = m_Registry.GetComponent<SpeciesTag>(a_entity);
        const auto species = tag != nullptr ? tag->Value : Species::Human;

        const auto target = m_Translator.EntityFor(a_targetFormId);

        if (!target.IsValid())
        {
            return;   // defensive — the walk target is a translated form
        }

        const auto formId = m_Translator.FormFor(a_entity);
        const auto* label = species == Species::Human
            ? "settler"
            : (species == Species::Child ? "child" : "animal");

        // The trade stone: who did this mind meet? The walk target is
        // either the market (a workshop entity — FormRef only, no
        // SpeciesTag) or a person (a translated mind, remembered as a
        // merchant from a previous trade). A human trades with a person;
        // the first human at a market sets up its stall; a child or an
        // animal is fed by whoever resolved as its feeder (the owner, or
        // the settlement).
        LCE::Simulation::EntityId counterparty = target;
        bool traded = false;
        bool keeperHome = false;   // the stall-keeper at their own bench
        bool familyHome = false;   // the spouse of the keeper — the family bench
        LCE::Simulation::EntityId familySpouse{};   // who the family meal warms
        std::uint32_t marketFormId = a_targetFormId;

        // The walk target is a bench when it carries no species tag —
        // the market itself. (A person target resolves to a merchant or
        // a remembered trader — a mind, not a place.) The Rows crossing
        // scan runs only at benches: the feud's geography (Identity.md)
        // is the stall.
        const auto targetTag =
            m_Registry.GetComponent<SpeciesTag>(target);
        const bool atBench = targetTag == nullptr;

        if (species == Species::Human)
        {
            if (targetTag == nullptr)
            {
                // The bench: resolve the stall-keeper for this market.
                marketFormId = a_targetFormId;

                const auto iterator = m_StallKeepers.find(target);
                const auto stall =
                    iterator != m_StallKeepers.end() ? iterator->second
                                                     : EntityId{};

                const auto spouse = Households::SpouseOf(m_Bonds, a_entity);

                if (stall.IsValid() && spouse.IsValid() && stall == spouse)
                {
                    // The family bench (0.6.0 Stone 3): my spouse keeps
                    // this stall — a meal at home. No exchange: the
                    // household pouch does not pay itself.
                    counterparty = target;
                    traded = false;
                    familyHome = true;
                    familySpouse = spouse;
                }
                else if (stall.IsValid() && stall != a_entity)
                {
                    counterparty = stall;
                    traded = true;
                }
                else if (stall.IsValid())
                {
                    // The keeper themselves at their own bench (this world
                    // or restored from the co-save) — no customers yet.
                    // Not a re-claim: the stall stands, nothing changes
                    // hands, and the map keeps the same keeper.
                    counterparty = target;
                    traded = false;
                    keeperHome = true;
                }
                else
                {
                    // No stall yet — this mind sets it up. Honest Partial:
                    // arrived, no customers, nothing changed hands.
                    m_StallKeepers[target] = a_entity;
                    counterparty = target;
                    traded = false;
                }
            }
            else if (targetTag->Value == Species::Human && target != a_entity)
            {
                // The walk resolved to a person — the remembered merchant
                // (the core's ChooseTarget prefers the trader over the
                // bench once a trade exists). Trade with them directly.
                counterparty = target;
                traded = true;
            }
            else
            {
                // Defensive: a non-human mind as a trade target cannot
                // happen (children and animals never enter Trade memory).
                // No trade — the honest Partial.
                counterparty = target;
                traded = false;
            }
        }

        // The row (0.7.2 Rows): rivals and enemies who cross paths at
        // the same bench have words — the feud's audible half. The
        // attendance book is who walked here today (ephemeral, pruned
        // to the day); the scan finds a feud partner already here, and
        // Rows::Exchange books the wrong on both sides (engine Wronged,
        // −0.25 — an unprompted wrong, not the −0.1 executed let-down
        // of a shut stall), gossips the shouting to the settlement, and
        // publishes on the bus — so a crossing that pushes the pair
        // over the enemy line fires OnBondChange the instant it
        // happens. Words once a day per pair: the memory gate is
        // co-saved, so save/load never double-rows.
        if (atBench)
        {
            const auto day = CurrentDay();
            auto& attendees = m_MarketAttendance[marketFormId];

            attendees.erase(
                std::remove_if(
                    attendees.begin(), attendees.end(),
                    [day](const auto& entry)
                    {
                        return entry.second != day;
                    }),
                attendees.end());

            // The keeper stands at the bench — the feud's geography is
            // the stall. The attendance book only sees walkers ("who
            // walked here today"), so a keeper planted or restored at
            // her own bench never enters it: the very mind every
            // shut-stall slight is aimed at could never row back. Scan
            // her directly alongside today's walkers; the once-per-day
            // Wronged gate makes a keeper who did arrive today a
            // harmless double-scan.
            const auto keeperIt = m_StallKeepers.find(target);
            const auto keeper = keeperIt != m_StallKeepers.end()
                ? keeperIt->second
                : LCE::Simulation::EntityId{};

            const auto cross = [&](LCE::Simulation::EntityId a_other)
            {
                if (!a_other.IsValid() || a_other == a_entity)
                {
                    return;
                }

                // The species gate (0.7.5 field find): a feud needs two
                // people. An animal at the bench is here to be fed, not
                // to row — whatever the bond book says (the book's own
                // gate keeps animals out, this is the belt).
                if (species == Species::Animal)
                {
                    return;
                }

                const auto crossTag =
                    m_Registry.GetComponent<SpeciesTag>(a_other);

                if (crossTag
                    && crossTag->Value == Species::Animal)
                {
                    return;
                }

                if (Rows::Exchange(
                        m_Registry, m_Bonds, m_ConflictGates,
                        a_entity, a_other, day, m_CoreTuning, &m_Bus))
                {
                    // The exchange itself: each says a line to the
                    // other — two voices, the shouting the settlement
                    // hears. Speech rides the news feed, so the radio
                    // reads the row as a caption.
                    Say(a_entity, a_other, Dialogue::Pool::Row);
                    Say(a_other, a_entity, Dialogue::Pool::Row);

                    REX::INFO(
                        "LCE: {} and {} row at the market — words first.",
                        MindLabelForm(formId),
                        MindLabelForm(m_Translator.FormFor(a_other)));

                    // The physical escalation (0.7.5 Fights): a row
                    // between enemies can turn to blows — the temper
                    // and chance rolls decide; the punch lands (Combat
                    // both ways), the feud deepens, and the victim
                    // carries a threat. Rivals stay verbal.
                    EscalateToFight(a_entity, a_other, day);
                }
            };

            for (const auto& [other, otherDay] : attendees)
            {
                if (otherDay == day)
                {
                    cross(other);
                }
            }

            cross(keeper);

            attendees.emplace_back(a_entity, day);
        }

        // The conflict source (0.7.0 Stone 2): a hungry human arrived
        // and the stall is shut. The world-facts gate stops new walks
        // after hours, but a walk already in flight when the hour turns
        // still arrives. No trade, no food — and the mind remembers the
        // let-down. The engine's decided channel for an executed
        // interaction that went badly: ReportOutcome({ keeper, Social,
        // Failure }) cools the disposition 0.1 toward the stall-keeper,
        // and the settlement echo spreads the chill through the group
        // (Simulation.cpp — the sign lives in the kind and the result,
        // never on the weight). A forgiving mind (temper below
        // sim.slight.temper) blames no one — a world outcome, memory
        // only: the stall was just shut. The feud's fuel: two or three
        // shut-stall let-downs cross the rival line, and the 0.6.0 feud
        // arc — gossip, mediation, the settlement's inherited cold
        // shoulder — takes it from there.
        //
        // Bench only (0.7.4 fix): the stall is the bench's geography.
        // A human who walked to a PERSON — the remembered seller — is
        // at the seller's market, not the settlement's; the seller is
        // there, so the trade lands whatever the hour. (The world-facts
        // gate still stops new walks after hours; only walks already in
        // flight arrive, so night trades are the desperate ones.)
        const bool closed = WorldFacts::IsMarketClosed(
            CurrentGameHour(),
            m_Settings.MarketOpenHour, m_Settings.MarketCloseHour);

        if (closed && species == Species::Human
            && atBench && !keeperHome && !familyHome)
        {
            const auto keeperIterator = m_StallKeepers.find(target);
            const auto keeper = keeperIterator != m_StallKeepers.end()
                ? keeperIterator->second
                : EntityId{};

            const bool slighted =
                TemperOf(a_entity) >= m_Settings.SlightTemper;

            ReportOutcome(
                m_Registry, a_entity,
                Outcome{
                    (slighted && keeper.IsValid()) ? keeper : EntityId{},
                    InteractionKind::Social, OutcomeResult::Failure,
                    1.0f },
                m_CoreTuning, &m_Bus, WorldTime{ CurrentDay() });

            REX::INFO(
                "LCE: the stall at {} is shut — {} went hungry{}.",
                MarketLabel(marketFormId), MindLabelForm(formId),
                slighted ? " and blames the keeper" : "");

            // The first words of a feud (0.7.1 Talk, feeding 0.7.2
            // Rows): a slighted mind does not just blame silently — it
            // says something to the keeper. The row pool's early lines
            // ("You ripped me off") are exactly this moment. When the
            // keeper is a feud partner who already rowed here this
            // arrival (the crossing above), the words are spoken — the
            // slight's own Say would repeat the same line, so it stays
            // silent.
            if (slighted && keeper.IsValid()
                && !Rows::AlreadyRowedToday(
                    m_ConflictGates, a_entity, keeper, CurrentDay()))
            {
                Say(a_entity, keeper, Dialogue::Pool::Row);
            }

            // The feud turns physical (0.7.5 Fights): a slighted mind
            // facing an enemy keeper throws the punch instead of just
            // the words — the reliable test path (force the market
            // shut, the slight fires, the enemy keeper catches a fist).
            if (slighted && keeper.IsValid())
            {
                EscalateToFight(a_entity, keeper, CurrentDay());
            }

            return;
        }

        const auto outcome = ArrivalOutcome(species, counterparty, traded);

        // The outcome lands on the bus (0.6.0 stone 08): a sale that
        // warms the buyer past the friend line publishes
        // RelationshipChangedEvent — the instant bond channel. The
        // world day rides along so the event (and the memory it stamps)
        // is anchored to the calendar.
        ReportOutcome(
            m_Registry, a_entity, outcome, m_CoreTuning,
            &m_Bus, LCE::Simulation::WorldTime{ CurrentDay() });

        if (species == Species::Human)
        {
            if (traded)
            {
                // The illness stone (0.8.0): a sick mind at the market
                // buys medicine instead of a meal — the trade stone's
                // second good. Caps leave the pouch (or the household's
                // shared wallet), the hold ends, recovery starts early.
                // A broke sick mind rests instead — honest: sickness
                // without caps means time, not treatment.
                //
                // 0.8.3 — the sick household: medicine is a stocked
                // good (sim.illness.stock per market day — an outbreak
                // can outrun the shelf), and a family cares for its
                // own. A well buyer's trip doses the sick at home —
                // the spouse first, then a child, who has no walk of
                // its own to the bench.
                auto buyerHealth =
                    m_Registry.GetComponent<Health>(a_entity);

                // One dose: the shelf, the household wallet, the hold.
                // Returns true when a dose was actually bought. The
                // wallet is the buyer's household's (PouchOf resolves
                // the shared pouch either way), so a family's medicine
                // comes from the family's caps.
                const auto buyDose =
                    [&](LCE::Simulation::EntityId a_patient,
                        const std::string& a_patientLabel,
                        bool a_forSelf) -> bool
                    {
                        if (m_Settings.Illness.MedicinePrice <= 0.0f)
                        {
                            return false;   // no medicine in this world
                        }

                        const auto price = static_cast<std::uint32_t>(
                            m_Settings.Illness.MedicinePrice);

                        auto buyerPouch = Households::PouchOf(
                            m_Registry, m_Bonds, a_entity);
                        auto sellerPouch = Households::PouchOf(
                            m_Registry, m_Bonds, counterparty);

                        if (buyerPouch == nullptr || sellerPouch == nullptr
                            || buyerPouch->Caps < price)
                        {
                            REX::INFO(
                                "illness: {} is sick but broke — they rest instead of buying medicine.",
                                a_patientLabel);
                            return false;
                        }

                        if (MedicineStockOf(marketFormId) == 0)
                        {
                            REX::INFO(
                                "illness: {} finds the stall out of medicine — they rest instead.",
                                a_patientLabel);
                            return false;
                        }

                        buyerPouch->Caps -= price;
                        sellerPouch->Caps += price;
                        ConsumeMedicine(marketFormId);

                        if (auto patientHealth =
                                m_Registry.GetComponent<Health>(a_patient))
                        {
                            TakeMedicine(
                                *patientHealth, m_Settings.Illness);
                        }

                        if (a_forSelf)
                        {
                            REX::INFO(
                                "illness: {} buys medicine from {} for {} caps — the hold ends, recovery begins.",
                                MindLabelForm(formId),
                                MindLabelForm(
                                    m_Translator.FormFor(counterparty)),
                                price);

                            PushNews(
                                MindLabelForm(formId) + " bought medicine.",
                                Tuning::AdapterSettings::NewsCategory::Illness);
                        }
                        else
                        {
                            REX::INFO(
                                "illness: {} buys medicine for {} — a family cares.",
                                MindLabelForm(formId), a_patientLabel);

                            PushNews(
                                MindLabelForm(formId) + " bought medicine for "
                                + a_patientLabel + ".",
                                Tuning::AdapterSettings::NewsCategory::Illness);
                        }

                        return true;
                    };

                // Self first: the sick at the bench dose themselves.
                if (buyerHealth
                    && buyerHealth->Illness.Kind != SicknessKind::None)
                {
                    buyDose(a_entity, MindLabelForm(formId), true);
                }
                else
                {
                    // The family check: who is sick at home? The spouse
                    // first (the design's headline), then a child — the
                    // ChildMult's path: a child can't reach the bench's
                    // medicine, so a parent's trip is the dose.
                    auto sickAtHome = LCE::Simulation::EntityId{};

                    const auto spouse =
                        Households::SpouseOf(m_Bonds, a_entity);

                    if (spouse.IsValid())
                    {
                        const auto spouseHealth =
                            m_Registry.GetComponent<Health>(spouse);

                        if (spouseHealth
                            && spouseHealth->Illness.Kind
                                != SicknessKind::None)
                        {
                            sickAtHome = spouse;
                        }
                    }

                    if (!sickAtHome.IsValid())
                    {
                        if (const auto rels =
                                m_Registry.GetComponent<Relationships>(
                                    a_entity))
                        {
                            for (const auto& [other, rel] : rels->ByEntity)
                            {
                                const auto tag =
                                    m_Registry.GetComponent<SpeciesTag>(
                                        other);

                                if (tag && tag->Value == Species::Child)
                                {
                                    const auto childHealth =
                                        m_Registry.GetComponent<Health>(other);

                                    if (childHealth
                                        && childHealth->Illness.Kind
                                            != SicknessKind::None)
                                    {
                                        sickAtHome = other;
                                        break;
                                    }
                                }
                            }
                        }
                    }

                    if (sickAtHome.IsValid())
                    {
                        const auto patientLabel = [&]() -> std::string
                        {
                            if (const auto name = m_Registry
                                    .GetComponent<Name>(sickAtHome))
                            {
                                return name->Full;
                            }

                            return MindLabel(sickAtHome);
                        }();

                        buyDose(sickAtHome, patientLabel, false);
                    }
                }

                // The buyer's half of the exchange: a meal at the bench
                // is company. ReportOutcome above built trust (Trade is
                // the core's reliability channel); Remember(Social)
                // warms the disposition — the same +0.1 the keeper's
                // RecordSale warms them — and publishes the crossing on
                // the bus, so a warm enough buyer crosses the friend
                // line the instant the meal lands. This is the
                // courtship's raw material (Life.md): repeated trading
                // at the same bench is how two settlers become friends.
                Remember(
                    m_Registry, a_entity,
                    MemoryEvent{
                        counterparty, InteractionKind::Social,
                        WorldFacts::kFactWeight },
                    m_CoreTuning,
                    WorldTime{ CurrentDay() },
                    &m_Bus);

                // The trader's half of the exchange: they remember the
                // sale and warm toward the customer. The buyer's side —
                // trust earned, the goal served — is the core's work via
                // ReportOutcome above.
                auto traderMemory = m_Registry.GetComponent<Memory>(counterparty);
                auto traderRelationships =
                    m_Registry.GetComponent<Relationships>(counterparty);

                if (traderMemory && traderRelationships)
                {
                    RecordSale(
                        *traderMemory, *traderRelationships,
                        a_entity, m_Settings.SaleWarmth);
                }

                // The physical exchange (the economy stone): the buyer
                // pays what they can afford up to the meal's price and
                // the seller's pouch grows. A broke buyer pays nothing
                // and is still fed — the settlement covers the meal.
                // Price is whole caps, minimum 1 (a free meal is not a
                // market).
                std::uint32_t paid = 0;
                std::uint32_t buyerCaps = 0;
                std::uint32_t sellerCaps = 0;

                // The shared wallet (0.6.0 Stone 3): a married member
                // trades with the household's pouch — their own, or the
                // spouse's — on both sides of the bench. PouchOf resolves
                // it either way, so one wallet round-trips.
                auto buyerPouch =
                    Households::PouchOf(m_Registry, m_Bonds, a_entity);
                auto sellerPouch =
                    Households::PouchOf(m_Registry, m_Bonds, counterparty);
                const auto paidFromHousehold =
                    buyerPouch != nullptr
                    && m_Registry.GetComponent<CapPouch>(a_entity) == nullptr;

                if (buyerPouch && sellerPouch)
                {
                    const auto price = static_cast<std::uint32_t>(
                        m_Settings.MealPrice > 1.0f
                            ? m_Settings.MealPrice
                            : 1.0f);

                    paid = PayForMeal(*buyerPouch, *sellerPouch, price);
                    buyerCaps = buyerPouch->Caps;
                    sellerCaps = sellerPouch->Caps;
                }

                const auto traderFormId = m_Translator.FormFor(counterparty);

                if (paid > 0)
                {
                    REX::INFO(
                        "LCE: {} trades with {} at market {} — fed, {} caps change hands ({}{} left, {} now).",
                        MindLabelForm(formId), MindLabelForm(traderFormId),
                        MarketLabel(marketFormId),
                        paid,
                        paidFromHousehold ? "household; " : "",
                        buyerCaps, sellerCaps);

                    // The market's words (0.7.1 Talk): a paid meal is a
                    // conversation — the buyer says something to the
                    // keeper while the caps change hands. Speech rides
                    // the news feed, so the settlement radio reads it.
                    Say(a_entity, counterparty, Dialogue::Pool::Trade);
                }
                else
                {
                    REX::INFO(
                        "LCE: {} trades with {} at market {} — fed on the settlement's credit (no caps).",
                        MindLabelForm(formId), MindLabelForm(traderFormId),
                        MarketLabel(marketFormId));
                }
            }
            else if (familyHome)
            {
                // The family bench is the marriage's heartbeat: a shared
                // meal at home warms both ways — the same Social warmth
                // a bench-sale carries. Without this the couple's
                // dispositions erode by drift (they stopped trading the
                // moment they married — free meals), and a marriage
                // quietly dies in about an hour; grief for a spouse then
                // finds the love gone (the 2026-08-11 grief test).
                if (familySpouse.IsValid() && familySpouse != a_entity)
                {
                    Remember(
                        m_Registry, a_entity,
                        MemoryEvent{
                            familySpouse, InteractionKind::Social,
                            WorldFacts::kFactWeight },
                        m_CoreTuning,
                        WorldTime{ CurrentDay() },
                        &m_Bus);

                    auto spouseMemory =
                        m_Registry.GetComponent<Memory>(familySpouse);
                    auto spouseRelationships =
                        m_Registry.GetComponent<Relationships>(familySpouse);

                    if (spouseMemory && spouseRelationships)
                    {
                        RecordSale(
                            *spouseMemory, *spouseRelationships,
                            a_entity, m_Settings.SaleWarmth);
                    }
                }

                // The family's words (0.7.1 Talk): a meal at home is the
                // warmest conversation there is.
                if (familySpouse.IsValid() && familySpouse != a_entity)
                {
                    Say(a_entity, familySpouse, Dialogue::Pool::Family);
                }

                REX::INFO(
                    "LCE: {} is at the family stall at market {} — fed from the household's meal.",
                    MindLabelForm(formId), MarketLabel(marketFormId));
            }
            else if (keeperHome)
            {
                REX::INFO(
                    "LCE: {} is at their own stall at market {} — no customers yet.",
                    MindLabelForm(formId), MarketLabel(marketFormId));
            }
            else
            {
                REX::INFO(
                    "LCE: {} sets up the stall at market {} — trade begins when customers come.",
                    MindLabelForm(formId), MarketLabel(marketFormId));
            }
        }
        else
        {
            REX::INFO(
                "LCE: {} arrived — fed, gives nothing in return (Aid, Success).",
                MindLabelForm(formId));
        }

        // The trip pays off (the real test): arriving at the market means
        // food — the settlement's stores feed arrivals, human and animal
        // alike. The need is restored, the loop closes, and the log shows
        // the payoff the sim previously hid.
        auto needs = m_Registry.GetComponent<Needs>(a_entity);

        if (needs)
        {
            const auto previous = RestoreHunger(*needs);

            if (previous >= 0.0f)
            {
                REX::INFO(
                    "LCE: {} fed: Hunger {:.2f} -> 1.00",
                    MindLabelForm(formId), previous);
            }
        }

        // The illness stone's food vector (0.8.0): the settlement's
        // stores are not always clean — the meal itself rolls the food
        // chance. A mild vector (seed 0.2): over the tuned hold it stays
        // under the death line, so a bad meal is a nasty cold, not a
        // death sentence.
        if (m_Settings.IllnessEnabled
            && m_Settings.Illness.FoodChance > 0.0f)
        {
            const auto foodRoll = m_Rng.NextFloat(0.0f, 1.0f);
            auto eaterHealth = m_Registry.GetComponent<Health>(a_entity);

            if (eaterHealth
                && TLC::Contract(
                    *eaterHealth, SicknessKind::Food, 0.2f, CurrentDay(),
                    m_Settings.Illness,
                    m_Settings.Illness.FoodChance, foodRoll))
            {
                REX::INFO(
                    "illness: {} took ill from the settlement's stores.",
                    MindLabelForm(formId));
            }
        }
    }

    void Adapter::ScrubRoadMarketMemories()
    {
        // The 0.8.9 field find: provisioners still clustered at the
        // workbench even with the road-feed stone in place. The cause is
        // the co-save's memory — the seed is idempotent (it skips minds
        // that already remember), so a Trade memory seeded by an older
        // build survives every later seed, and the engine's hunger
        // branch follows Trade memories wherever they point. Road people
        // were excluded from *new* seeds but never scrubbed of *old*
        // ones. Each bench arrival then re-warmed the stale fact,
        // closing a self-sustaining loop.
        m_Registry.ForEachWithComponent<LCE::Simulation::Memory>(
            [this](LCE::Simulation::EntityId a_entity,
                   LCE::Simulation::Memory& a_memory)
            {
                if (!IsRoadPerson(a_entity))
                {
                    return;
                }

                const auto before = a_memory.Events.size();

                a_memory.Events.erase(
                    std::remove_if(
                        a_memory.Events.begin(), a_memory.Events.end(),
                        [this](const LCE::Simulation::MemoryEvent& a_event)
                        {
                            // A market entity is a workshop — FormRef
                            // only, no SpeciesTag (the same test the
                            // arrival handler uses for "at the bench").
                            // A Trade memory to a person is real (the
                            // road people trade with each other); only
                            // the bench pull is scrubbed.
                            return a_event.Kind
                                    == LCE::Simulation::InteractionKind::Trade
                                && m_Registry
                                       .GetComponent<SpeciesTag>(
                                           a_event.Other)
                                       == nullptr;
                        }),
                    a_memory.Events.end());

                if (a_memory.Events.size() != before)
                {
                    REX::DEBUG(
                        "road: scrubbed {} stale market memory{} from {}.",
                        before - a_memory.Events.size(),
                        before - a_memory.Events.size() == 1 ? "" : "ies",
                        MindLabel(a_entity));
                }
            });
    }

    void Adapter::SeedMarket(bool a_announce)
    {
        // The census: one scan over the REFR form array. Static per load
        // order once it finds workshops; an empty result is retried (the
        // array may not be populated yet) while the fallback covers the
        // world — see RefreshWorkshops.
        if (!m_WorkshopsReady)
        {
            RefreshWorkshops();
        }

        // The ownership read retries from here (after the first census
        // fills the workshop list): the WorkshopPlayerOwnership AVIF
        // (form 0x33C) is a base-game form, always loaded, so the
        // resolution itself rarely fails — but the menu-world wake
        // reads fresh defaults before the save's state applies, and
        // the retry window (every few seconds, ~60s total) lets the
        // restored world's reads land; after the window the map stays
        // empty and the requireOwned gate gates everyone (the safe
        // default — the gate defaults OFF anyway). SEH-guarded: the
        // per-ref AV reads touch game memory, and an earlier quest-array
        // iteration crashed on stale objects — a fault must log, not
        // vanish silently.
        if (!m_OwnershipQueried && !m_Workshops.empty())
        {
            const auto now = std::chrono::steady_clock::now();

            const bool due =
                m_LastOwnershipAttempt.time_since_epoch().count() == 0
                || now - m_LastOwnershipAttempt
                       > std::chrono::seconds(5);

            if (due && m_OwnershipAttempts < 12)
            {
                m_LastOwnershipAttempt = now;
                ++m_OwnershipAttempts;

                m_OwnershipQueried = [this]()
                {
                    __try
                    {
                        return QueryPlayerOwnedWorkshops();
                    }
                    __except (EXCEPTION_EXECUTE_HANDLER)
                    {
                        REX::WARN(
                            "economy: ownership query raised {:#x} — reads empty (the requireOwned gate would gate everyone meanwhile).",
                            static_cast<std::uint32_t>(GetExceptionCode()));
                        return false;
                    }
                }();
            }
        }

        if (m_Workshops.empty())
        {
            // Fallback — no census (no REFRs: an interior, a bare world).
            // A lone known market beats no market: the sim degrades to
            // the walking stone's single-bench behavior rather than
            // forgetting to eat.
            EnsureWorkshop(kMarketFormId);

            const auto market = m_Translator.EntityFor(kMarketFormId);

            if (market.IsValid())
            {
                const auto* marketRef =
                    RE::TESForm::GetFormByID<RE::TESObjectREFR>(kMarketFormId);

                // Only minds whose settler is within walking distance of
                // the market remember it (the probe proved why: the
                // process lists carry settler-faction actors from
                // settlements kilometers away, and every one of them was
                // issued a walk to the Sanctuary bench).
                if (marketRef != nullptr)
                {
                    const auto marketPos = marketRef->GetPosition();

                    SeedMarketMemory(
                        m_Registry, market,
                        [this, market](LCE::Simulation::EntityId a_entity) {
                            // The road people (0.8.9): a provisioner or
                            // caravan hand feeds on the road — never
                            // seeded with a settlement market, so the
                            // core never walks it to a bench.
                            if (IsRoadPerson(a_entity))
                            {
                                return LCE::Simulation::EntityId{};
                            }

                            // Settlers trade at the market. A child or an
                            // animal is fed — by its owner when the game
                            // assigns one and the owner is a sim entity,
                            // else by the settlement.
                            const auto tag =
                                m_Registry.GetComponent<SpeciesTag>(a_entity);

                            if (tag == nullptr
                                || tag->Value == Species::Human)
                            {
                                return market;
                            }

                            const auto owner = OwnerEntityFor(a_entity);

                            return owner.IsValid() ? owner : market;
                        },
                        [this, marketPos](LCE::Simulation::EntityId a_entity) {
                            const auto formId = m_Translator.FormFor(a_entity);
                            const auto* actor =
                                RE::TESForm::GetFormByID<RE::Actor>(formId);

                            if (actor == nullptr)
                            {
                                return false;
                            }

                            const auto pos = actor->GetPosition();
                            const auto dx = pos.x - marketPos.x;
                            const auto dy = pos.y - marketPos.y;

                            return std::sqrt(dx * dx + dy * dy) < kMarketRadius;
                        });

                    if (a_announce)
                    {
                        const auto hour = CurrentGameHour();

                        if (WorldFacts::IsMarketClosed(
                                hour, m_Settings.MarketOpenHour,
                                m_Settings.MarketCloseHour))
                        {
                            REX::INFO(
                                "The market is remembered but closed ({}): trade resumes at {:02.0f}:00.",
                                FormatGameHour(hour), m_Settings.MarketOpenHour);
                        }
                        else
                        {
                            REX::INFO("The market is open: every mind remembers where to trade (the Sanctuary workshop — 000250FE).");
                        }
                    }
                }
            }
            else if (a_announce)
            {
                REX::INFO("The market is not loaded — settlers explore until it is.");
            }

            return;
        }

        // Per-settlement markets: every workshop is a market entity, and
        // every mind remembers its own — the nearest workshop within
        // range of where it stands. A settler in Sanctuary knows the
        // Sanctuary bench; a settler at Warwick knows Warwick's; a mind
        // in the wastes knows none and explores until it finds one.
        for (const auto& workshop : m_Workshops)
        {
            EnsureWorkshop(workshop.FormId);
        }

        SeedMarketMemory(
            m_Registry, {},
            [this](LCE::Simulation::EntityId a_entity) {
                // Where is this mind? Its actor must be loaded to know —
                // restored minds load gradually, and the periodic seed
                // catches them a second later.
                const auto formId = m_Translator.FormFor(a_entity);
                const auto* actor =
                    RE::TESForm::GetFormByID<RE::Actor>(formId);

                if (actor == nullptr)
                {
                    return LCE::Simulation::EntityId{};
                }

                const auto pos = actor->GetPosition();
                const auto nearest = NearestWorkshop(
                    pos.x, pos.y, m_Workshops, kMarketRadius);

                if (nearest == 0)
                {
                    return LCE::Simulation::EntityId{};   // in the wastes
                }

                const auto market = m_Translator.EntityFor(nearest);

                if (!market.IsValid())
                {
                    return LCE::Simulation::EntityId{};
                }

                // The road people (0.8.9 road-feed stone): a provisioner
                // or caravan hand feeds on the road — no market memory,
                // no walk to a settlement bench (the 0.8.9 field find:
                // every road person's hunger pulled it to the nearest
                // market, clustering caravans in settlements).
                if (IsRoadPerson(a_entity))
                {
                    return LCE::Simulation::EntityId{};
                }

                // Settlers trade at their settlement's market. A child or
                // an animal is fed — by its owner when the game assigns
                // one and the owner is a sim entity, else by the
                // settlement (the player is no entity — a player-owned
                // dog comes home to be fed).
                const auto tag =
                    m_Registry.GetComponent<SpeciesTag>(a_entity);

                if (tag == nullptr || tag->Value == Species::Human)
                {
                    return market;
                }

                const auto owner = OwnerEntityFor(a_entity);

                return owner.IsValid() ? owner : market;
            });

        // The 0.8.9 field fix: road people carry no bench memories. The
        // seed above only adds — a stale Trade memory restored from an
        // older co-save survives it, and the engine's hunger branch
        // follows it to the workbench. Scrub after every seed so the
        // stale pull is removed and re-warmed arrivals stay scrubbed.
        ScrubRoadMarketMemories();

        if (a_announce)
        {
            // Hour-aware since the world-facts stone: the classic line
            // only when the market is actually open. At night the seed
            // still plants *where* the market is (the location memory is
            // independent of the hours gate) — the announce just tells
            // the truth about what the world is doing now.
            const auto hour = CurrentGameHour();

            if (WorldFacts::IsMarketClosed(
                    hour, m_Settings.MarketOpenHour,
                    m_Settings.MarketCloseHour))
            {
                REX::INFO(
                    "The market is remembered but closed ({}): trade resumes at {:02.0f}:00.",
                    FormatGameHour(hour), m_Settings.MarketOpenHour);
            }
            else
            {
                REX::INFO(
                    "The market is open: every mind remembers where to trade ({} workshops).",
                    m_Workshops.size());
            }
        }
    }

    void Adapter::SeedVendors(bool a_announce)
    {
        using namespace LCE::Simulation;

        // The vendor census: who sells in the loaded world right now
        // (0.7.4 Trade with anyone). A seller is an actor with a
        // merchant container — SimRelevant::IsVendor, the same gate
        // that admits them as minds. Spatial on purpose: a mind only
        // remembers a seller within walking distance, so the census is
        // the loaded lists, not the whole Commonwealth.
        std::vector<TLC::VendorPosition> vendors;

        ForEachLoadedActor(
            [this, &vendors](const RE::Actor* a_actor)
            {
                if (!IsVendor(a_actor))
                {
                    return;
                }

                const auto pos = a_actor->GetPosition();
                vendors.push_back(TLC::VendorPosition{
                    a_actor->GetFormID(), pos.x, pos.y });
            });

        if (a_announce)
        {
            REX::INFO(
                "sim: {} sellers loaded — trade resolves to a person when one is remembered.",
                vendors.size());
        }

        if (vendors.empty())
        {
            return;
        }

        // The loaded sellers by form, for the idempotency check: a mind
        // that already remembers a seller it can still see keeps its
        // memory — the seed only ever adds the fact back to minds that
        // lost it, exactly like the market seed.
        std::unordered_set<std::uint32_t> vendorForms;

        for (const auto& vendor : vendors)
        {
            vendorForms.insert(vendor.FormId);
        }

        m_Registry.ForEachWithComponent<Memory>(
            [this, &vendors, &vendorForms](
                EntityId a_entity, Memory& a_memory)
            {
                // Only people trade. A child or an animal is fed — its
                // food source stays the owner or the settlement, never
                // a seller.
                const auto tag =
                    m_Registry.GetComponent<SpeciesTag>(a_entity);

                if (tag == nullptr || tag->Value != Species::Human)
                {
                    return;
                }

                const auto formId = m_Translator.FormFor(a_entity);
                const auto* actor =
                    RE::TESForm::GetFormByID<RE::Actor>(formId);

                if (actor == nullptr)
                {
                    return;   // not loaded — the refresh catches it
                }

                const auto pos = actor->GetPosition();
                auto nearest =
                    TLC::NearestVendor(pos.x, pos.y, vendors, kMarketRadius);

                // A seller's own stall is not a customer: a vendor mind
                // is always the nearest vendor to itself (distance 0), so
                // when the nearest is the mind's own form, find the next
                // nearest instead.
                if (nearest == formId)
                {
                    nearest = 0;
                    float bestSq = kMarketRadius * kMarketRadius;

                    for (const auto& vendor : vendors)
                    {
                        if (vendor.FormId == formId)
                        {
                            continue;
                        }

                        const auto dx = vendor.X - pos.x;
                        const auto dy = vendor.Y - pos.y;
                        const auto distSq = dx * dx + dy * dy;

                        if (distSq < bestSq)
                        {
                            bestSq = distSq;
                            nearest = vendor.FormId;
                        }
                    }
                }

                if (nearest == 0)
                {
                    return;   // no seller in walking distance
                }

                const auto seller = m_Translator.EntityFor(nearest);

                if (!seller.IsValid() || seller == a_entity)
                {
                    return;   // no entity yet, or the seller is me
                }

                for (const auto& event : a_memory.Events)
                {
                    if (event.Kind != InteractionKind::Trade
                        || !event.Other.IsValid())
                    {
                        continue;
                    }

                    const auto known = m_Translator.FormFor(event.Other);

                    if (known != 0 && vendorForms.contains(known))
                    {
                        return;   // already remembers a seller here
                    }
                }

                a_memory.Events.push_back(MemoryEvent{
                    seller, InteractionKind::Trade, kVendorSeedWeight });
            });
    }

    bool Adapter::IsRadstorm(const RE::TESWeather* a_weather) const
    {
        // Radstorms shut the gatherings: a { invalid, Social } world fact
        // — nobody meets in the green air. Verified against the full xEdit
        // weather list 2026-08-10: only CommonwealthGSRadstorm (001C3D5E)
        // is the live radstorm. The other GS radstorm forms were reviewed
        // and deliberately excluded — Old/Backup (00222394/002392A3) are
        // editor records the game never sets, and NoHazard (0024A3C0)
        // removes the hazard by design: the gate is the green air, not
        // the colour of the sky.
        if (a_weather != nullptr)
        {
            switch (a_weather->GetFormID())
            {
            case 0x001C3D5E:   // CommonwealthGSRadstorm
                return true;
            default:
                break;
            }
        }

        return false;
    }

    void Adapter::PushWorldFacts()
    {
        using namespace LCE::Simulation;

        const auto hour = CurrentGameHour();
        const bool closed = WorldFacts::IsMarketClosed(
            hour, m_Settings.MarketOpenHour, m_Settings.MarketCloseHour);

        const auto* sky = RE::Sky::GetSingleton();
        const bool radstorm =
            IsRadstorm(sky != nullptr ? sky->currentWeather : nullptr);

        // Transitions only — these are the lines the player reads. The
        // push below is silent; announcing every second would drown the
        // log in facts. The player window (0.7.0 Stone 3) turns each
        // transition into a headline.
        if (closed != m_MarketClosed)
        {
            m_MarketClosed = closed;

            if (closed)
            {
                REX::INFO(
                    "world fact: the market is closed ({}) — trade unavailable until {:02.0f}:00.",
                    FormatGameHour(hour), m_Settings.MarketOpenHour);

                PushNews(
                    "the market closed for the night.",
                    Tuning::AdapterSettings::NewsCategory::Market);
            }
            else
            {
                REX::INFO(
                    "world fact: the market is open ({}) — trade available.",
                    FormatGameHour(hour));

                PushNews(
                    "the market opened — the Commonwealth trades.",
                    Tuning::AdapterSettings::NewsCategory::Market);

                // The market day turns (0.8.3): every shelf refills at
                // open, so a stall that sold out yesterday is fresh
                // today — and a stall mid-outbreak can sell out again.
                ReplenishMedicineStock();
            }
        }

        if (radstorm != m_Radstorm)
        {
            m_Radstorm = radstorm;

            if (radstorm)
            {
                REX::INFO("world fact: a radstorm rolls in — no one gathers while it lasts.");
            }
            else
            {
                REX::INFO("world fact: the radstorm passes — gatherings resume.");
            }
        }

        // --- The day's weather: a memory, not a door. --------------------
        // The sky is classified into a category and today's categories are
        // pushed as day-stamped world facts ({ invalid, WeatherRain, 1.0,
        // day } — "day 12 was rainy"). These kinds are never gated, so
        // rain never closes the market; they are pure labels the sim
        // remembers. Re-derived at the edge: after a load the re-push
        // re-seeds today's sky within a second, so weather never needs
        // the co-save. Weather is global knowledge — every mind shares
        // the same sky — so, like the gates, it reaches all minds.
        const auto* calendar = RE::Calendar::GetSingleton();
        const auto day = std::uint64_t(
            calendar != nullptr && calendar->gameDaysPassed != nullptr
                ? calendar->gameDaysPassed->value
                : 0.0f);

        const auto weather = WorldFacts::ClassifyWeather(
            sky != nullptr && sky->currentWeather != nullptr
                ? sky->currentWeather->GetFormID()
                : 0);

        if (day != m_WeatherDay)
        {
            // The world turned: yesterday's categories are left to fade
            // and today's start empty. The turn gets a line — it proves
            // the day tracking in the log.
            m_WeatherDay = day;
            m_WeatherSeen = 0;

            REX::INFO(
                "world fact: the world turns — day {} begins, the sky {}.",
                day, WorldFacts::WeatherLabel(weather));
        }
        else if (weather != m_Weather)
        {
            // A sky change within the day — transitions only.
            m_Weather = weather;

            if (weather != WorldFacts::WeatherKind::Unknown)
            {
                REX::INFO(
                    "world fact: the sky turns {} (day {}) — the day's weather is remembered.",
                    WorldFacts::WeatherLabel(weather), day);
            }
            else
            {
                REX::INFO("world fact: the sky is unclassified — no weather memory.");
            }
        }

        if (weather != WorldFacts::WeatherKind::Unknown)
        {
            // The seen-set is a bitmask over WeatherKind (Clear=1..Radstorm=6).
            m_WeatherSeen |= 1u << (static_cast<unsigned>(weather) - 1u);
        }

        // Refresh every category seen today, stamped with today — "it
        // rained this morning" stays remembered until the world turns.
        // Yesterday's categories are not refreshed, and the tick fades
        // them out: the designed forget.
        if (m_WeatherSeen != 0)
        {
            m_Registry.ForEachWithComponent<Memory>(
                [this, day](EntityId, Memory& a_memory)
                {
                    for (unsigned i = 0; i < 6; ++i)
                    {
                        if ((m_WeatherSeen & (1u << i)) == 0)
                        {
                            continue;
                        }

                        const auto factKind = WorldFacts::WeatherFactKind(
                            static_cast<WorldFacts::WeatherKind>(i + 1));

                        if (factKind)
                        {
                            WorldFacts::ApplyFact(
                                a_memory, *factKind, true, day);
                        }
                    }
                });
        }

        if (!closed && !radstorm)
        {
            return;   // no doors shut — nothing to push
        }

        // The refresh pattern, one door at a time (WorldFacts::ApplyFact):
        // while a door is shut, top the fact back to full weight every
        // second so the tick's fade never erases it — no flicker, no
        // memory growth. When the condition flips, refreshing stops and
        // the tick fades the fact out in ~4.5 s: the close-down the
        // transition line promised. World facts are global knowledge —
        // every mind hears the market shut, loaded or not, near or far
        // (unlike the market-location seed, which is radius-filtered).
        m_Registry.ForEachWithComponent<Memory>(
            [closed, radstorm](EntityId, Memory& a_memory)
            {
                WorldFacts::ApplyFact(
                    a_memory, InteractionKind::Trade, closed);
                WorldFacts::ApplyFact(
                    a_memory, InteractionKind::Social, radstorm);
            });
    }

    void Adapter::ApplyIllness(float a_deltaSeconds)
    {
        using namespace LCE::Simulation;

        if (!m_Settings.IllnessEnabled)
        {
            return;
        }

        // The per-tick curve: every ill mind holds or recovers, and the
        // Fatigue multiplier takes its toll while ill (the visible cost
        // is rest — the mind tires faster and Decide produces Rest more
        // often). Deaths at the bottom are collected and removed after
        // the loop — RemoveMind during iteration is never safe.
        const auto now = std::chrono::steady_clock::now();
        std::vector<std::uint32_t> dead;

        m_Registry.ForEachWithComponent<Health>(
            [&](EntityId a_entity, Health& a_health)
            {
                // The species-aware toll: a child's severity grows
                // faster (sim.illness.childMult).
                const auto tag =
                    m_Registry.GetComponent<SpeciesTag>(a_entity);

                const auto result = TickIllness(
                    a_health, a_deltaSeconds, m_Settings.Illness,
                    tag != nullptr && tag->Value == Species::Child);

                if (result == 2)
                {
                    if (const auto formId = m_Translator.FormFor(a_entity); formId != 0)
                    {
                        dead.push_back(formId);
                    }

                    return;
                }

                // The visible cost while ill: the Fatigue need decays
                // at the multiplied rate — a sick mind tires faster and
                // rests more (the sleep cycle's recovery answers it).
                // The multiplier eases off as health recovers.
                const auto mult =
                    FatigueMultiplier(a_health, m_Settings.Illness);

                if (mult > 1.0f)
                {
                    if (auto needs = m_Registry.GetComponent<Needs>(a_entity))
                    {
                        for (auto& need : needs->List)
                        {
                            if (need.Type == NeedType::Fatigue)
                            {
                                need.Value -= need.DecayRate
                                    * (mult - 1.0f) * a_deltaSeconds;
                            }
                        }
                    }
                }

                // The counter to the fatigue coma (0.8.0 fix): the sick
                // empty their bellies faster too — Hunger decays at the
                // multiplied rate. Within the hold they get hungry enough
                // to decide MoveTo, so the walk to the market (and the
                // medicine) actually happens. Without it, the 2× fatigue
                // toll parked the sick at Rest/Explore and the medicine
                // branch never fired (the 2026-08-13 finding: 108 died,
                // zero medicine buys).
                const auto hungerMult =
                    HungerMultiplier(a_health, m_Settings.Illness);

                if (hungerMult > 1.0f)
                {
                    if (auto needs = m_Registry.GetComponent<Needs>(a_entity))
                    {
                        for (auto& need : needs->List)
                        {
                            if (need.Type == NeedType::Hunger)
                            {
                                need.Value -= need.DecayRate
                                    * (hungerMult - 1.0f) * a_deltaSeconds;
                            }
                        }
                    }
                }

                // The cough (0.8.0 polish): the visible tell. An ill
                // mind plays the game's MTCoughing idle (0xEA85C — the
                // clean, condition-free vanilla record, the cough Mass
                // Fusion uses) at the tuned interval. The tell is a
                // sound the player can hear, not a stat to read — and
                // it stops the moment the mind is well (the result
                // gate: only the still-ill path gets here). Rate-limited
                // per mind and globally (0.8.1 field pass: a
                // settlement-wide outbreak would otherwise be a wall of
                // sound — 50 sick minds × one idle each is 4 coughs a
                // second). Only for loaded actors — the sim child has
                // no game body to cough.
                if (m_Settings.Illness.CoughInterval > 0.0f
                    && a_health.Illness.Kind != SicknessKind::None)
                {
                    const auto globallyOpen =
                        m_Settings.Illness.CoughGlobal <= 0.0f
                        || now - m_LastCoughGlobal
                            >= std::chrono::duration<float>(
                                m_Settings.Illness.CoughGlobal);

                    const auto coughIt = m_LastCough.find(a_entity);

                    if (globallyOpen
                        && (coughIt == m_LastCough.end()
                            || now - coughIt->second
                                >= std::chrono::duration<float>(
                                    m_Settings.Illness.CoughInterval)))
                    {
                        m_LastCough[a_entity] = now;
                        m_LastCoughGlobal = now;

                        const auto formId = m_Translator.FormFor(a_entity);

                        if (formId != 0)
                        {
                            if (auto* actor =
                                    RE::TESForm::GetFormByID<RE::Actor>(
                                        formId))
                            {
                                if (auto* cough =
                                        RE::TESForm::GetFormByID<RE::TESIdleForm>(
                                            0x000EA85C))
                                {
                                    if (actor->currentProcess != nullptr)
                                    {
                                        actor->currentProcess->PlayIdle(
                                            *actor, cough, nullptr);
                                    }
                                }
                            }
                        }
                    }
                }
            });

        for (const auto formId : dead)
        {
            // The death path (0.8.0): the same bookkeeping a kill books —
            // the world keeps its books, the dead never restore. The
            // cause is named so the settlement knows the price of the
            // wastes.
            REX::INFO(
                "illness: {} died of sickness — the wastes claim another.",
                MindLabelForm(formId));

            PushNews(
                MindLabelForm(formId) + " died of sickness.",
                Tuning::AdapterSettings::NewsCategory::Death);

            // The body (0.8.1 field pass): sickness must actually take
            // the body — the game actor dies (the corpse appears; the
            // 0.8.2 burial stone disposes of it later). This is the one
            // death the adapter causes itself — a player kill or a
            // raider's already put the actor down, but illness never
            // touched the actor, so the mind died and the body walked
            // on, re-entered the sim, and "died of sickness" again
            // minutes later (the 2026-08-13 outbreak: four children
            // booked dead, then each caught it again within seconds of
            // their own death). Self as attacker, lethal damage — no
            // one to blame, the settlement stays calm.
            if (auto* actor =
                    RE::TESForm::GetFormByID<RE::Actor>(formId))
            {
                actor->KillImpl(actor, 9999.0f, true, false);
            }

            RemoveMind(formId, true);
        }

        // The once-per-sickness announce (the radio's story): a mind
        // that just fell ill, or just recovered, says so once — the
        // announce set is per (mind, kind) so a restored illness
        // announces once, and the recovered line rides the recovery.
        IllnessNews();
    }

    void Adapter::ApplyIllnessVectors()
    {
        using namespace LCE::Simulation;

        if (!m_Settings.IllnessEnabled)
        {
            return;
        }

        const auto day = CurrentDay();

        // The radstorm vector: a radstorm day exposes every mind once
        // that day (the day gate — exposure rolls per mind per radstorm
        // day, not every second of it). The weather fact is global, so
        // the whole settlement rolls; the chance (sim.illness.radstorm-
        // Chance) decides how many fall ill.
        if (m_Radstorm && day != m_LastRadstormExposureDay)
        {
            m_LastRadstormExposureDay = day;

            m_Registry.ForEachWithComponent<Health>(
                [&](EntityId a_entity, Health& a_health)
                {
                    const auto roll = m_Rng.NextFloat(0.0f, 1.0f);

                    if (Contract(
                            a_health, SicknessKind::Radstorm, 0.3f, day,
                            m_Settings.Illness,
                            m_Settings.Illness.RadstormChance, roll))
                    {
                        REX::INFO(
                            "illness: {} is ill — the radstorm's bite (day {}).",
                            MindLabel(a_entity), day);
                    }
                });
        }

        // The contagion vector: the sick echo outward — each ill mind's
        // settlement peers (the Groups membership, the same settlement
        // the conflict source's echo rides) roll the contagion chance
        // each second. One roll per healthy peer per second: a
        // settlement with a sick member slowly passes it around.
        //
        // Collect the ill first (the sick's settlement set), then roll
        // for every peer in those settlements — the scan is bounded,
        // and the sick are few.
        std::unordered_set<GroupId> sickSettlements;
        std::vector<EntityId> sick;

        m_Registry.ForEachWithComponent<Health>(
            [&](EntityId a_entity, const Health& a_health)
            {
                if (a_health.Illness.Kind == SicknessKind::None)
                {
                    return;
                }

                sick.push_back(a_entity);

                if (const auto groups =
                        m_Registry.GetComponent<Groups>(a_entity))
                {
                    for (const auto& group : groups->Memberships)
                    {
                        sickSettlements.insert(group);
                    }
                }
            });

        if (sick.empty() || sickSettlements.empty())
        {
            return;
        }

        m_Registry.ForEachWithComponent<Health>(
            [&](EntityId a_entity, Health& a_health)
            {
                if (a_health.Illness.Kind != SicknessKind::None)
                {
                    return;   // already ill
                }

                // Only peers of the sick settlements roll.
                const auto groups =
                    m_Registry.GetComponent<Groups>(a_entity);

                if (!groups)
                {
                    return;
                }

                bool sharesSettlement = false;

                for (const auto& group : groups->Memberships)
                {
                    if (sickSettlements.contains(group))
                    {
                        sharesSettlement = true;
                        break;
                    }
                }

                if (!sharesSettlement)
                {
                    return;
                }

                const auto roll = m_Rng.NextFloat(0.0f, 1.0f);

                if (Contract(
                        a_health, SicknessKind::Contagion, 0.25f, day,
                        m_Settings.Illness,
                        m_Settings.Illness.ContagionChance, roll))
                {
                    REX::INFO(
                        "illness: {} caught it from the settlement (day {}).",
                        MindLabel(a_entity), day);
                }
            });
    }

    void Adapter::IllnessNews()
    {
        // The radio's story (0.8.0): once per sickness, the settlement
        // hears who is ill and who recovered. The announce set is keyed
        // per (mind, kind) — a restored illness announces once, and the
        // set clears on EndWorld (a fresh world announces fresh). The
        // burst pacing (0.8.1 field pass): an outbreak is one story,
        // not fifty lines — at most sim.illness.newsMax new names enter
        // the feed per sim.illness.newsInterval window, so a 50-sick
        // radstorm day reads as a radio story that unfolds, not a wall
        // of names crowding out every other line. The rest wait for the
        // next window; each mind still announces exactly once.
        const auto now = std::chrono::steady_clock::now();

        if (now - m_LastIllnessNews
            >= std::chrono::duration<float>(
                m_Settings.Illness.NewsInterval))
        {
            m_LastIllnessNews = now;
            m_IllnessNewsCount = 0;
        }

        if (m_IllnessNewsCount
            >= static_cast<std::size_t>(m_Settings.Illness.NewsMax))
        {
            return;
        }

        m_Registry.ForEachWithComponent<Health>(
            [this](LCE::Simulation::EntityId a_entity, const Health& a_health)
            {
                if (a_health.Illness.Kind == SicknessKind::None)
                {
                    return;
                }

                const auto key =
                    (a_entity.Value() << 4)
                    ^ static_cast<std::uint64_t>(a_health.Illness.Kind);

                if (m_IllnessAnnounced.insert(key).second
                    && m_IllnessNewsCount
                        < static_cast<std::size_t>(
                            m_Settings.Illness.NewsMax))
                {
                    ++m_IllnessNewsCount;
                    PushNews(
                        MindLabel(a_entity) + " is ill.",
                        Tuning::AdapterSettings::NewsCategory::Illness);
                }
            });
    }

    void Adapter::Tick(double a_deltaSeconds)
    {
        if (!m_Started)
        {
            // Abort recovery: a PreLoadGame armed a pending load. Two
            // cases, and the window separates them:
            //   - a REAL load is slow: a 600-actor co-save world can take
            //     far longer than 12s to finish. The old 12s window fired
            //     mid-load, revived a world while the game was still
            //     loading, and that revival killed the load — every
            //     in-game load "aborted" at exactly 12s, every session.
            //   - a true phantom load (the pre-DisableExitSave exit-save
            //     reload) never completes at all.
            // The window is the patience: 60s lets real loads finish; a
            // phantom past that is dead, and the sim revives to survive.
            if (m_AwaitingLoad
                && std::chrono::steady_clock::now() - m_WorldEndedAt
                    > std::chrono::seconds(60))
            {
                m_AwaitingLoad = false;

                // The aborted load's co-save snapshot (if one was read
                // before the abort) belongs to a world that never
                // existed — discard it, or a later, unrelated GameLoaded
                // would restore a stale world over a fresh one.
                if (m_PendingRestore)
                {
                    REX::INFO(
                        "lifecycle: the aborted load's co-save is discarded — "
                        "the world revives fresh.");
                    m_PendingRestore.reset();
                }

                REX::INFO("lifecycle: the pending load aborted — reviving the world.");
                StartWorld();
                return;
            }

            // One-time proof the tick hook fires even before a world
            // exists (the hooks install at Load; worlds start on
            // GameLoaded). If neither this nor the first-pass line ever
            // appears, the game is not simulating — a paused, unfocused,
            // or occluded window throttles the per-frame VM ticks.
            if (!m_TickCalled)
            {
                m_TickCalled = true;
                REX::INFO("Tick: called before the world started (once).");
            }
            return;
        }

        using namespace LCE::Simulation;

        // The market stays open: re-push the fact every second so minds
        // whose actors load after the world started (a restore brings 637
        // entities back, but their actors load gradually) learn where to
        // trade. Idempotent — SeedMarketMemory skips minds that already
        // remember — so this only ever adds to the truly forgotten.
        // Silent: the world-start call announced it.
        if (std::chrono::steady_clock::now() - m_LastMarketSeed
            > std::chrono::seconds(1))
        {
            m_LastMarketSeed = std::chrono::steady_clock::now();

            // The MCM override check (0.8.5): a stat of the MCM
            // settings file, once a second — when the player changed a
            // slider, the settings land in the sim within a second,
            // with no rebuild. Runs before the seed so this tick's
            // Update sees the new tuning.
            CheckMcmOverride();

            // 0.6.0 Stone 1 — the world keeps its books: new settlers
            // become minds, deaths and departures leave the book. Runs
            // before the seed so this tick's Update sees consistent state.
            KeepBooks();

            // 0.7.0 Stone 1's visible tail: a restored world's actors
            // stream in gradually — this names a mind's actor the first
            // time it appears (idempotent; fresh arrivals were already
            // named at seed).
            ApplyLoadedActorNames();

            // The species sweep (0.7.5 field fix): a mind whose actor
            // just loaded and whose race disagrees with the stored tag
            // is corrected — the restore pass caught the loaded ones;
            // this catches the rest as they stream in.
            ReclassifyLoadedMinds();

            // The family gate (0.7.5 field find): rebuild the kin set
            // BEFORE the bond pass so a freshly-loaded family pair is
            // already gated when their dispositions reconcile.
            RebuildKin();

            // The burial sweep (0.8.2): lay the due dead to rest. Runs
            // here — the same per-second cadence as the kin gate — so a
            // corpse is buried within a second of its mourning window
            // expiring, including restored ones whose window passed
            // while the game was away.
            BurialSweep();

            // The test hook's brawl loop (0.7.5): a pinned pair fights
            // on its own timer — spectating and verifying the fight
            // machinery on demand, the once-per-day gate bypassed.
            ForceFightLoop();

            // The audio trigger probe (0.8.7): when sim.diag.audioProbe
            // is on, a nearby pair voices an exchange on its own timer —
            // the in-game ear's verdict on the seam. The production
            // crossings (InteractPass) share the same machinery.
            AudioProbeLoop();

            // The in-world exchange's answer beat (0.8.7b): a due
            // second half fires every tick, probe or not — the
            // production crossings book through the same slot.
            ProcessExchange();

            // The scuffle's second beat (0.7.5): due counter-shoves
            // fire, then the one who threw first walks off — the
            // exchange reads as a sequence, not a double-fall.
            ProcessPendingShoves();

            // 0.6.0 Stone 2 — bonds: the 1-second dissolve net. The
            // event channel is instant; this pass is complete — quiet
            // drift (the core never publishes a dissolve), restores,
            // and anything the bus missed all surface here.
            ReconcileBonds();

            SeedMarket(false);

            // 0.7.4 Trade with anyone: the sellers refresh on the same
            // cadence — vendors stream in and out of the loaded area as
            // they travel, so the who-sells memory re-points at whoever
            // is nearest now (idempotent; silent).
            SeedVendors(false);

            // The conflict source's settlement (0.7.0 Stone 2): late-
            // loading minds join their settlement's group once their
            // market memory lands. Idempotent — a mind with a group
            // keeps it.
            AssignSettlementGroups();

            // The player window (0.7.0 Stone 3): the settlement radio
            // speaks the news while one is near the player.
            RadioCaptions();

            // The world's doors: the market's trading hours and the
            // weather. Pushed on the same cadence as the seed — silent
            // unless a door changes.
            PushWorldFacts();

            // The illness vectors (0.8.0): a radstorm day exposes every
            // mind (once that day), and the sick echo outward to their
            // settlement peers. Runs after the weather push so the
            // radstorm flag is current.
            ApplyIllnessVectors();

            // The earn-caps economy (0.8.6b): the settlement stipend,
            // once per world-day per mind (the StipendMark gates it —
            // the second call of the same day pays nothing). A mind
            // whose actor just streamed in gets paid under its home
            // settlement on the next tick.
            PayStipends();

            // 0.6.0 Stone 5 — the arcs, on a day cadence: mediation for
            // every feud the settlement has heard of, once per day; and
            // birth, at most once per day, when enabled (Stone 6).
            const auto day = CurrentDay();

            if (m_Settings.MediationEnabled
                && day != m_LastMediationDay)
            {
                m_LastMediationDay = day;
                RunMediation();
            }

            if (m_Settings.BirthEnabled && day != m_LastBirthDay)
            {
                m_LastBirthDay = day;
                RunBirth();
            }
        }

        // The grief arc (0.6.0 Stone 5) runs every tick: a grieving mind
        // — a recent death of someone it loved — drains Social faster
        // and seeks company. Derived from persisted components, so no
        // record of its own; the line announces each fresh bereavement
        // once (the memory fades and the line stops on its own).
        {
            const auto grieving = Arcs::ApplyGrief(
                m_Registry, CurrentDay(),
                m_Settings.GriefDecay,
                static_cast<float>(a_deltaSeconds));

            for (const auto& [mind, dead] : grieving)
            {
                // The dead's form is gone from the translator (RemoveMind
                // destroyed it) — the announce reads the recent-deaths
                // map recorded at booking time. Without it the line was
                // dead code (FormFor(dead) was always 0).
                const auto formId = m_Translator.FormFor(mind);
                const auto deadIt = m_RecentDeaths.find(dead.Value());

                if (formId == 0 || deadIt == m_RecentDeaths.end())
                {
                    continue;
                }

                // Once per bereavement, not every frame: the fresh
                // window (weight ≥ 0.9) is ~0.5 s of frames, and the
                // first version announced in all of them (34 lines in
                // half a second, 2026-08-11).
                const auto key = std::make_pair(mind.Value(), dead.Value());

                if (!m_GriefAnnounced.insert(key).second)
                {
                    continue;
                }

                REX::INFO(
                    "arcs: settler {:#x} grieves for {:#x} — they seek company.",
                    formId, deadIt->second);
            }
        }

        // The child's life (0.6.0 Stone 6): sim-only children are fed by
        // their household — no walk, no market, just a full bowl.
        Birth::FeedChildren(
            m_Registry, static_cast<float>(a_deltaSeconds));

        // The child's growth (0.7.7): children age into adults after
        // sim.birth.childhood sim-days. The growth check runs daily.
        {
            static std::uint64_t lastGrowthDay = 0;
            const auto today = CurrentDay();

            if (today != lastGrowthDay)
            {
                lastGrowthDay = today;

                // The birth journey (0.8.9): a newborn hold past
                // sim.baby.holdDays sheds its bundle and becomes a
                // child from the game's own pool — once per day, on
                // the day boundary, so a mid-carry survives the save
                // and the child arrives at the right age.
                AdvanceBabyHolds();

                const auto grew = Birth::GrowChildren(
                    m_Registry, m_Settings.BirthChildhood, today);

                if (grew > 0)
                {
                    REX::INFO(
                        "birth: {} children grew into adults.", grew);

                    PushNews(
                        std::to_string(grew)
                            + " children grew up in the Commonwealth.",
                        Tuning::AdapterSettings::NewsCategory::Birth);

                    // The birth journey (0.8.9) already paired each
                    // child with its body at the two-day mark; a
                    // grown child keeps that actor and walks the
                    // world like any mind. Nothing to pair here.
                }
            }
        }

        // The sleep cycle (0.6.0): a mind rests while its last decision
        // was Rest — or while the engine silenced it because the need it
        // most urgently feels is one rest fixes. The recovery restores
        // Fatigue, Safety, and Comfort at sim.rest.recovery per second
        // (default 0.2/s, a full nap in ~5 s); the engine's need loop
        // only decays. Keyed on the needs, not just the intent, because
        // a Safety-drained mind with no remembered threat makes the
        // engine's Decide return nullopt (nothing to flee) — the intent
        // is removed and no intent-keyed pass could ever see the mind
        // again (the parked-world discovery, 2026-08-11). Restored
        // before Update so this tick's decisions see the rested mind.
        m_Registry.ForEachWithComponent<Needs>(
            [&](EntityId a_entity, Needs& a_needs)
            {
                const auto intent =
                    m_Registry.GetComponent<LCE::Simulation::Intent>(a_entity);

                bool resting = intent != nullptr
                    && intent->Action == LCE::Simulation::ActionType::Rest;

                if (!resting && intent == nullptr)
                {
                    // No intent: the engine parked the mind (nullopt).
                    // The only need that silences Decide is Safety with
                    // no threat; Fatigue and Safety are the rest-fixable
                    // urgencies — either way, it is time to rest.
                    const auto urgent = MostUrgentNeed(a_needs);

                    resting = urgent.has_value()
                        && (*urgent == LCE::Simulation::NeedType::Fatigue
                            || *urgent == LCE::Simulation::NeedType::Safety);
                }

                if (!resting)
                {
                    return;
                }

                // The recovery value is a defensive marker (-1 when a
                // mind somehow lacks Fatigue); a resting mind with one
                // is recovered, nothing to branch.
                (void)RestRecovery(
                    a_needs,
                    m_Settings.RestRecovery,
                    static_cast<float>(a_deltaSeconds));
            });

        // The illness curve (0.8.0): hold-then-recover, the Fatigue
        // toll while ill (a sick mind tires faster and Decide produces
        // Rest more often), and death at the bottom. Runs before Update
        // so this tick's decisions see the sick mind's drained Fatigue.
        ApplyIllness(static_cast<float>(a_deltaSeconds));

        // The road feed (0.8.9 road-feed stone): a provisioner or
        // caravan hand eats from the caravan's supplies when its
        // hunger falls to the threshold — the market seed excludes
        // road people, so this is their only meal. Runs before Update
        // so this tick's decisions see the restored belly.
        FeedRoadPeople();

        // The visible child (0.8.9 deferred-spawn find): each pending
        // child is dressed the moment the game's load routine completes
        // it — throttled inside (a couple of checks a second). Runs
        // after the daily shed so a child that just spawned is dressed
        // the same tick once it is real.
        AdvanceVisualChildren();

        // The core's stateless tick: needs decay, memory fade, goal
        // urgency, then one Intent per mind. All of it on the game thread,
        // with the modder's tuning (the config file) when present. The
        // observation bus rides along so the sim's changes flow out —
        // bond crossings surface as RelationshipChangedEvent (0.6.0
        // stone 08). The TickReport measures this one call (0.8.0 stone
        // 13); the 0.8.6c scale gate logs the worst frame once a minute.
        Update(m_Registry, a_deltaSeconds, m_CoreTuning, &m_Bus, &m_Rng,
               &m_TickReport);

        // The 0.8.6c scale-in-the-field gate: once a minute, log the
        // sim's per-tick cost — how many minds were swept, and the
        // worst single Update's wall time this window. A restored
        // 600+ mind save must tick inside a frame budget; if the worst
        // frame's TotalMs is a big slice of the 16.6 ms budget, the
        // show chugs and the gate fails.
        m_TickMaxMs = std::max(m_TickMaxMs, m_TickReport.TotalMs);

        const auto reportNow = std::chrono::steady_clock::now();

        if (reportNow - m_LastTickReportLog
            >= std::chrono::seconds(60))
        {
            m_LastTickReportLog = reportNow;

            REX::INFO(
                "LCE: tick report — {} entities, {} memory events, "
                "{} relationships, worst frame {:.2f} ms total "
                "(needs {:.2f}, memory {:.2f}, relationships {:.2f}, "
                "goals {:.2f}, decide {:.2f}).",
                m_TickReport.Entities, m_TickReport.MemoryEvents,
                m_TickReport.Relationships, m_TickMaxMs,
                m_TickReport.NeedsMs, m_TickReport.MemoryMs,
                m_TickReport.RelationshipsMs, m_TickReport.GoalsMs,
                m_TickReport.DecideMs);

            m_TickMaxMs = 0.0;
        }

        // The read + the table. "Already acting" is a future refinement —
        // every loaded settler is available for now.
        const auto plan = BuildPlan(
            m_Registry,
            [this](EntityId a_entity) { return IsActorLoaded(m_Translator, a_entity); },
            [this](EntityId a_entity) { return IsTargetLoaded(m_Translator, a_entity); },
            [](EntityId) { return true; });

        ExecutePlan(plan);
        ProbeWalks();

        // The random-interaction trial (0.8.4): after the plan runs,
        // so walking minds are known — a mind mid-walk never speaks.
        // Bisect gate #2 (0.8.7): sim.diag.noInteract skips the pass
        // entirely — the 17:14 crash (walks gated, probe silent, no
        // subtitles) landed 1s after the [voice]: lines this pass
        // produced, so the interact path is the next suspect after the
        // walk was cleared.
        if (!m_Settings.NoInteract)
        {
            InteractPass();
        }

        // One-time proof the whole first pass completed. If the intent
        // lines printed but this is missing, the pass is stuck in
        // ProbeWalks; if neither printed, the hooks didn't fire.
        if (!m_FirstPassLogged)
        {
            m_FirstPassLogged = true;
            REX::INFO(
                "Tick: first pass complete — {} intents, {} active walks.",
                plan.size(), m_Walks.size());
        }
    }

    void Adapter::ExecutePlan(const std::vector<PlanEntry>& a_plan)
    {
        using namespace LCE::Simulation;

        // How many MoveTo intents the walk cap refused this pass. Logged
        // once per pass (rate-limited below) so a starved world is
        // visible without the per-frame flood the first attempt caused
        // (5.1M lines in five minutes — every deferred mind logged every
        // frame of the 600-mind revival flood).
        std::size_t deferred = 0;

        // The hold clock (the meal-cadence stone): the same `now` the
        // Rest/Explore branch rate-limits its commanded holds against.
        const auto now = std::chrono::steady_clock::now();

        for (const auto& entry : a_plan)
        {
            const auto actorFormId = m_Translator.FormFor(entry.Entity);
            const auto targetFormId = m_Translator.FormFor(entry.Intent.Target);

            // The identity stone's voice (0.7.0 Stone 1): decisions speak
            // in names — "Vera Hart [00048B77] decides MoveTo -> Sanctuary
            // workshop [000250FE]" — with the console hex beside each.
            const auto actorLabel = MindLabelForm(actorFormId);

            // Refusal is the contract (the intent is a hint, not a command):
            // an unloaded actor, an unloaded target, or a busy actor. The
            // dropped intent is simply re-decided next tick — nothing queued.
            //
            // Refusal keys are action-stable on purpose (the third field
            // carries the reason): a streamed-out mind flip-flops between
            // near-tied intents every second (Rest/Explore), and if the key
            // carried the action each flip would be a "new" key — one
            // refusal line per LogDecisionEvery seconds for as long as the
            // actor is away (the 2026-08-13 finding: Ellie Taylor's actor
            // streamed out after a fast travel and her refusals wrote 64
            // lines). A constant key means one refusal per reason per mind
            // until its state actually changes — the mind loads, or the
            // intent moves on — and that change is a new key, so it logs.
            if (!entry.ActorLoaded)
            {
                m_Walks.erase(entry.Entity);

                LogPlanEntry(
                    entry.Entity,
                    actorLabel + " decides " + ActionName(entry.Intent.Action)
                        + " -> " + MindLabelForm(targetFormId) + " — refused: actor not loaded",
                    { 0, 0, 1 });
                continue;
            }

            if (!entry.TargetLoaded)
            {
                m_Walks.erase(entry.Entity);

                LogPlanEntry(
                    entry.Entity,
                    actorLabel + " decides " + ActionName(entry.Intent.Action)
                        + " -> " + MindLabelForm(targetFormId) + " — refused: target not loaded",
                    { 0, 0, 2 });
                continue;
            }

            if (!entry.Available)
            {
                m_Walks.erase(entry.Entity);

                LogPlanEntry(
                    entry.Entity,
                    actorLabel + " decides " + ActionName(entry.Intent.Action)
                        + " -> " + MindLabelForm(targetFormId) + " — refused: actor busy",
                    { 0, 0, 3 });
                continue;
            }

            auto* actor = RE::TESForm::GetFormByID<RE::Actor>(actorFormId);

            if (actor == nullptr)
            {
                continue;   // defensive — ActorLoaded already checked
            }

            switch (entry.Intent.Action)
            {
            case ActionType::MoveTo:
            {
                auto* target = RE::TESForm::GetFormByID<RE::TESObjectREFR>(targetFormId);

                if (target == nullptr)
                {
                    continue;   // defensive — TargetLoaded already checked
                }

                // The adapter walks the settler; the core never does
                // (ADR-0024). Refusal leaves the sim to re-decide.
                //
                // The walk session: while the Trade memory lasts the intent
                // stays MoveTo and would re-issue the planner every frame.
                // Issue each walk once per session; the game's command
                // package keeps walking the settler to the destination on
                // its own. The session outlives the intent (ProbeWalks
                // measures it) because the memory fades long before the
                // walk completes.
                auto& session = m_Walks[entry.Entity];
                const auto now = std::chrono::steady_clock::now();

                bool walked = false;

                // The arrival-cooldown guard: a mind that arrived at
                // this target within the cooldown is already where it
                // wanted to be — its MoveTo is satisfied without a new
                // walk. Without it, a fed mind standing at its market is
                // always most-urgent-hungry (fast decay rates), so it
                // re-decides MoveTo every frame, re-arrives instantly,
                // and the arrival → feed loop floods the log (the 0.3/s
                // test: 18k animal-fed lines in under a minute). The
                // session was erased on arrival (the slot frees at
                // once), so the walk table alone cannot answer "was this
                // mind just here?" — this map can.
                const auto arrivedIt = m_ArrivedAt.find(entry.Entity);

                const bool justArrived = arrivedIt != m_ArrivedAt.end()
                    && arrivedIt->second.first == targetFormId
                    && now - arrivedIt->second.second
                        < std::chrono::seconds(10);

                // The refusal cooldown (0.8.x field fix): a mind whose
                // walk was refused — no actor or no AI process — stays
                // parked until the cooldown passes. The refusal is a
                // persistent state (a streamed-out actor), not a
                // one-frame glitch; without the cooldown the mind
                // re-decides MoveTo next frame, is refused again, and
                // the refusal DEBUG line floods the log (~420/s with a
                // handful of such minds). Treat the MoveTo as satisfied
                // while parked — the mind re-attempts after the
                // cooldown, and the moment its actor is ready the walk
                // goes through.
                const auto refusedIt = m_WalkRefusedUntil.find(entry.Entity);

                const bool parked = refusedIt != m_WalkRefusedUntil.end()
                    && now < refusedIt->second;

                if (justArrived
                    || parked
                    || (!session.Reached
                        && session.Target == targetFormId
                        && now - session.Issued < std::chrono::seconds(120)))
                {
                    walked = true;   // already walking / just arrived
                }
                else
                {
                    // Per-market budget: each market's walkers share its
                    // own slice of the cap, so one settlement's hunger
                    // cannot be starved by the Commonwealth-wide flood
                    // (the revival census — 600+ minds all deciding
                    // MoveTo at once — saturates a single global cap and
                    // no market ever trades; the 2026-08-11 test).
                    const auto atThisMarket = std::count_if(
                        m_Walks.begin(), m_Walks.end(),
                        [targetFormId](const auto& a_walk)
                        {
                            return a_walk.second.Target == targetFormId;
                        });

                    if (atThisMarket >= m_Settings.WalkCap)
                    {
                        // Walk cap: erase the session so a refused walk
                        // never lingers (a zombie session — Issued at
                        // the epoch — made ProbeWalks log an instant
                        // "ended" line every frame for every refused
                        // walk; that flood preceded the crash). The mind
                        // re-decides next tick.
                        ++deferred;
                        m_Walks.erase(entry.Entity);
                    }
                    else
                    {
                        walked = Movement::WalkTo(actor, target);

                        if (walked)
                        {
                            session.Target = targetFormId;
                            session.Issued = now;
                        }
                        else
                        {
                            // A refused walk ends the session — erase,
                            // don't reset (see the cap branch: a reset
                            // leaves a zombie that ProbeWalks logs as
                            // instantly ended). Park the mind for the
                            // refusal cooldown so it does not retry
                            // next frame and flood the log.
                            m_Walks.erase(entry.Entity);
                            m_WalkRefusedUntil[entry.Entity] =
                                now
                                + std::chrono::duration_cast<
                                    std::chrono::steady_clock::duration>(
                                    std::chrono::duration<float>(
                                        m_Settings.WalkRefusalCooldown));
                        }
                    }
                }

                char confidence[16];
                std::snprintf(confidence, sizeof(confidence), "%.2f", entry.Intent.Confidence);

                LogPlanEntry(
                    entry.Entity,
                    actorLabel + " decides MoveTo -> " + MindLabelForm(targetFormId)
                        + " (" + confidence + ")",
                    { static_cast<std::uint32_t>(entry.Intent.Action), targetFormId,
                        walked ? 0u : 4u });
            }
            break;

            case ActionType::Rest:
            case ActionType::Explore:
            {
                // The meal-cadence stone: Rest and Explore execute as a
                // bounded wander — a real nearby reference in the
                // actor's own cell (Movement::WanderNear), furniture
                // preferred. The first hold implementation parked the
                // actor in place and worked (meals collapsed from
                // minutes to ~10 s), but it froze the settlement:
                // everyone stood still at the bench. The wander keeps
                // the cadence — the sandbox cannot drift the actor away
                // before the next command — while the world looks
                // alive, and the game plays its own idles between
                // commands (it may even sit a settler at the bench it
                // walked to). Rate-limited to one wander per mind per
                // sim.wander.cooldown (default 30 s — re-issuing
                // mid-walk would yank the actor to a new target),
                // ranging sim.wander.radius (default 4000 units).
                char confidence[16];
                std::snprintf(confidence, sizeof(confidence), "%.2f", entry.Intent.Confidence);

                LogPlanEntry(
                    entry.Entity,
                    actorLabel + " decides " + ActionName(entry.Intent.Action)
                        + " (" + confidence + ")",
                    { static_cast<std::uint32_t>(entry.Intent.Action), 0, 0 });

                const auto wanderIt = m_LastWander.find(entry.Entity);

                if (wanderIt == m_LastWander.end()
                    || now - wanderIt->second
                        >= std::chrono::duration<float>(m_Settings.WanderCooldown))
                {
                    m_LastWander[entry.Entity] = now;
                    Movement::WanderNear(actor, m_Settings.WanderRadius);
                }
            }
            break;

            case ActionType::Socialize:
            case ActionType::Work:
            case ActionType::Flee:
            {
                // Table slots still: socializing is the future stone,
                // work and fleeing are unbuilt. The loop is proven;
                // these execute nothing in-game yet.
                char confidence[16];
                std::snprintf(confidence, sizeof(confidence), "%.2f", entry.Intent.Confidence);

                LogPlanEntry(
                    entry.Entity,
                    actorLabel + " decides " + ActionName(entry.Intent.Action)
                        + " (" + confidence + ")",
                    { static_cast<std::uint32_t>(entry.Intent.Action), 0, 0 });
            }
            break;
            }
        }

        // One line per pass, rate-limited to every few seconds: a starved
        // world stays visible without the per-frame flood (the first
        // attempt — a per-entity DEBUG line — wrote 5.1M lines in five
        // minutes of the 600-mind revival flood).
        if (deferred > 0
            && (m_LastCapLog.time_since_epoch().count() == 0
                || now - m_LastCapLog >= std::chrono::seconds(5)))
        {
            m_LastCapLog = now;
            REX::DEBUG(
                "LCE: walk cap — {} MoveTo(s) deferred this pass ({} active walks, per-market cap {}).",
                deferred, m_Walks.size(), m_Settings.WalkCap);
        }
    }

    void Adapter::LogPlanEntry(
        LCE::Simulation::EntityId a_entity,
        std::string a_message,
        const LogKey& a_key)
    {
        // One line per mind per LogDecisionEvery seconds, at most — and a
        // mind whose intent is unchanged stays quiet after its first line
        // (the pre-0.7.4 key dedupe). Both rules exist because of the
        // 600-mind restored world, not the 11-mind test:
        //   1. A mind flip-flopping between near-tied intents (Rest/Explore
        //      at the same confidence: two needs both ~full, 'most urgent'
        //      swaps as they decay) would write one line per frame per
        //      mind — 22k lines in under three minutes of synchronous
        //      file I/O on the game thread, the drag behind the growing
        //      frame hang. The change-cap holds it to one line per second.
        //   2. A stable mind printing once per second is 600 lines per
        //      second in a restored world — the 0.8.1 field finding that
        //      turned 'little hangs' back on. Same key = not news: print
        //      once, then silence until the intent actually changes.
        const auto now = std::chrono::steady_clock::now();
        const auto it = m_LastLogged.find(a_entity);

        if (it != m_LastLogged.end()
            && (it->second.first == a_key
                || now - it->second.second
                    < std::chrono::duration<float>(m_Settings.LogDecisionEvery)))
        {
            return;   // same intent (quiet) or a recent line (capped)
        }

        m_LastLogged[a_entity] = { a_key, now };
        REX::INFO("{}", a_message);
    }

    void Adapter::ProbeWalks()
    {
        using namespace LCE::Simulation;

        const auto now = std::chrono::steady_clock::now();

        for (auto it = m_Walks.begin(); it != m_Walks.end();)
        {
            auto& session = it->second;

            // A never-issued session (default Issued) is a zombie — drop it
            // silently rather than log it as an instant "ended" (the flood
            // that preceded the crash). ExecutePlan now erases on refusal,
            // so this is defense in depth.
            if (session.Issued == std::chrono::steady_clock::time_point{})
            {
                it = m_Walks.erase(it);
                continue;
            }

            // A session ends 120s after issue — the walk is issued once
            // and the game's planner carries it from there. 120s covers
            // the market radius (≈10,000 units ≈ 140 m) even at a slow
            // walk; the old 60s killed slow walkers mid-path (the radius
            // needs 60-100s at walk speed, and pathing detours stretch
            // it further). The sim re-decides on its own, so a live
            // session outliving its intent is harmless.
            if (now - session.Issued >= std::chrono::seconds(120))
            {
                if (!session.Reached)
                {
                    REX::DEBUG(
                        "LCE: walk session {} ended (closest approach {:.1f} u).",
                        MindLabelForm(m_Translator.FormFor(it->first)),
                        session.MinDistance);
                }

                it = m_Walks.erase(it);
                continue;
            }

            if (session.Reached)
            {
                ++it;
                continue;
            }

            // Resolve the pair: the walker and the destination it was told
            // to walk to.
            const auto actorFormId = m_Translator.FormFor(it->first);
            const auto* actor = RE::TESForm::GetFormByID<RE::Actor>(actorFormId);
            const auto* target =
                RE::TESForm::GetFormByID<RE::TESObjectREFR>(session.Target);

            if (actor == nullptr || target == nullptr)
            {
                ++it;   // unloaded — nothing to measure this tick
                continue;
            }

            // Distance (units) from the actor's DATA position to the
            // destination. The 3D node's world transform was the old
            // source, and it lied: after a fast travel, streaming actors
            // reported positions 120,000+ units from where they stood
            // (a walker ~700 units from the bench "moved" 1.7 km in a
            // frame), so arrival never registered. The data position
            // tracks the actor while loaded and stays sane (last known)
            // when the cell unloads.
            const auto from = actor->GetPosition();
            const auto to = target->GetPosition();
            const auto dx = from.x - to.x;
            const auto dy = from.y - to.y;
            const auto d = std::sqrt(dx * dx + dy * dy);

            // The first reading is the baseline. A walker cannot plausibly
            // stray beyond ~8× its starting distance plus a margin — a
            // reading that absurd is a stream artifact (cell teardown),
            // not progress: skip it without touching the minimum or the
            // arrival check, so a blip cannot corrupt a healthy walk.
            if (session.StartDistance <= 0.0f)
            {
                session.StartDistance = d;
            }
            else if (d > session.StartDistance * 8.0f + 5000.0f)
            {
                ++it;
                continue;
            }

            if (d < session.MinDistance)
            {
                session.MinDistance = d;
            }

            if (d < kArrivalRadius)
            {
                session.Reached = true;

                // Remember the arrival — the guard the MoveTo branch
                // reads: a mind that just got here has its next MoveTo
                // to the same place treated as satisfied, so a fed mind
                // standing at its market cannot loop MoveTo → instant
                // arrival → feed every frame (the 0.3/s hunger test:
                // at fast decay a full mind is always most-urgent-
                // hungry and the walk layer was re-issuing the trip it
                // just completed).
                m_ArrivedAt[it->first] = { session.Target, now };

                ReportArrival(it->first, session.Target);
                REX::INFO(
                    "LCE: {} arrived (d = {:.1f} u).",
                    MindLabelForm(actorFormId), d);

                // The trip is done — end the session now so the walk
                // slot frees immediately (the sleep-cycle discovery:
                // the 120 s timeout kept a completed trip occupying one
                // of the 16 walk slots, and the "already walking"
                // short-circuit then swallowed the fed mind's next
                // MoveTo for the whole 120 s — no new walk, no second
                // meal, no repeat pair, no bond).
                it = m_Walks.erase(it);
                continue;
            }
            else if (m_Settings.LogWalkProbes
                && now - session.LastProbe >= std::chrono::seconds(5)
                && (session.LastDistance < 0.0f
                    || std::fabs(d - session.LastDistance) >= 5.0f))
            {
                session.LastProbe = now;
                session.LastDistance = d;
                REX::DEBUG(
                    "LCE: walk probe {} -> {} d = {:.1f} u (min {:.1f} u).",
                    MindLabelForm(actorFormId), MindLabelForm(session.Target), d,
                    session.MinDistance);
            }

            ++it;
        }
    }
}
