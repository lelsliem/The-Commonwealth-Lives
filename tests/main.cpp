//=============================================================================//
// TheLivingCommonwealth adapter tests — the harness, mirroring the core's:
// bool-returning suites, no framework (ADR-0010/0029), runs on every build.
// Links LCE.Core only — no game required.
//=============================================================================//

#include "Behaviour.h"
#include "BlobCodec.h"
#include "Bonds.h"
#include "CoSave.h"
#include "Components.h"
#include "Executor.h"
#include "Households.h"
#include "Lifecycle.h"
#include "Market.h"
#include "Serialization.h"
#include "Translator.h"
#include "Tuning.h"
#include "WorldFacts.h"

#include "LCE/Config/Configuration.h"
#include "LCE/Events/EventBus.h"
#include "LCE/Simulation/Behaviour.h"
#include "LCE/Simulation/EntityRegistry.h"
#include "LCE/Simulation/Goals.h"
#include "LCE/Simulation/Memory.h"
#include "LCE/Simulation/Needs.h"
#include "LCE/Simulation/Relationships.h"
#include "LCE/Simulation/Simulation.h"
#include "LCE/Simulation/SimulationEvents.h"
#include "LCE/Simulation/WorldTime.h"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string_view>
#include <typeindex>
#include <typeinfo>
#include <unordered_set>
#include <utility>
#include <vector>

namespace TLC::Tests
{
    bool TranslatorTest();
    bool SeedingTest();
    bool BehaviourTest();
    bool SerializationTest();
    bool CoSaveTest();
    bool PlanBuilderTest();
    bool MarketTest();
    bool WorldFactsTest();
    bool TuningTest();
    bool LifecycleTest();
    bool BondTest();
    bool HouseholdTest();
}

namespace
{
    using namespace LCE::Simulation;

    int g_Failures = 0;
    int g_Run = 0;

    void Run(const char* a_name, bool (*a_suite)())
    {
        ++g_Run;

        std::printf("[ RUN  ] %s\n", a_name);

        if (a_suite())
        {
            std::printf("[  OK  ] %s\n", a_name);
        }
        else
        {
            ++g_Failures;
            std::printf("[ FAIL ] %s\n", a_name);
        }
    }
}

int main()
{
    Run("TranslatorTest", TLC::Tests::TranslatorTest);
    Run("SeedingTest", TLC::Tests::SeedingTest);
    Run("BehaviourTest", TLC::Tests::BehaviourTest);
    Run("SerializationTest", TLC::Tests::SerializationTest);
    Run("CoSaveTest", TLC::Tests::CoSaveTest);
    Run("PlanBuilderTest", TLC::Tests::PlanBuilderTest);
    Run("MarketTest", TLC::Tests::MarketTest);
    Run("WorldFactsTest", TLC::Tests::WorldFactsTest);
    Run("TuningTest", TLC::Tests::TuningTest);
    Run("LifecycleTest", TLC::Tests::LifecycleTest);
    Run("BondTest", TLC::Tests::BondTest);
    Run("HouseholdTest", TLC::Tests::HouseholdTest);

    std::printf("%d/%d suites passed.\n", g_Run - g_Failures, g_Run);

    return g_Failures == 0 ? 0 : 1;
}

namespace TLC::Tests
{
    bool TranslatorTest()
    {
        Translator translator;

        const auto farmer = EntityId{ 0x0000000100000005ull };
        const auto merchant = EntityId{ 0x0000000200000006ull };

        translator.Add(0x00012345u, farmer);
        translator.Add(0x00012346u, merchant);

        // Both directions.
        if (translator.EntityFor(0x00012345u) != farmer)
        {
            return false;
        }

        if (translator.FormFor(merchant) != 0x00012346u)
        {
            return false;
        }

        if (translator.Size() != 2)
        {
            return false;
        }

        // Absent = invalid entity / zero form (the core's convention).
        if (translator.EntityFor(0x00099999u).IsValid())
        {
            return false;
        }

        if (translator.FormFor(EntityId{ 0x0000000F0000000Full }) != 0)
        {
            return false;
        }

        // Remove drops both directions.
        translator.Remove(0x00012345u);

        if (translator.EntityFor(0x00012345u).IsValid())
        {
            return false;
        }

        if (translator.FormFor(farmer) != 0)
        {
            return false;
        }

        if (translator.Size() != 1)
        {
            return false;
        }

        translator.Clear();

        return translator.Size() == 0;
    }

    bool SeedingTest()
    {
        // A fresh human mind — the trading, talking kind (Behaviour.h).
        const auto needs = SeededNeeds(Species::Human);

        // A fresh mind has all five needs, all satisfied.
        if (needs.List.size() != 5)
        {
            return false;
        }

        bool sawHunger = false;
        bool sawFatigue = false;
        bool sawSocial = false;
        bool sawSafety = false;
        bool sawComfort = false;

        for (const auto& need : needs.List)
        {
            if (need.Value != 1.0f)
            {
                return false;
            }

            switch (need.Type)
            {
            case NeedType::Hunger:
                sawHunger = true;
                break;
            case NeedType::Fatigue:
                sawFatigue = true;
                break;
            case NeedType::Social:
                sawSocial = true;
                break;
            case NeedType::Safety:
                sawSafety = true;
                break;
            case NeedType::Comfort:
                sawComfort = true;
                break;
            }
        }

        return sawHunger && sawFatigue && sawSocial && sawSafety && sawComfort;
    }

    bool BehaviourTest()
    {
        //-------------------------------------------------------------------------
        // The behaviour split (0.5.0): the core is species-agnostic — the
        // adapter decides which minds trade and which are fed. The profile
        // table is the single source of that truth.
        //-------------------------------------------------------------------------

        // A human trades and talks, and the market means Trade for them.
        const auto& human = BehaviourFor(Species::Human);

        if (!human.CanTrade || !human.CanTalk ||
            human.MarketKind != InteractionKind::Trade ||
            !human.NeedsSocial || !human.NeedsComfort)
        {
            return false;
        }

        // A child talks but does not trade — fed by the settlement like
        // an animal, social like a person.
        const auto& child = BehaviourFor(Species::Child);

        if (child.CanTrade || !child.CanTalk ||
            child.MarketKind != InteractionKind::Aid ||
            !child.NeedsSocial || !child.NeedsComfort)
        {
            return false;
        }

        // ...and a child is seeded with the full need set, same as an
        // adult — it plays, it gets tired, it wants comfort.
        if (SeededNeeds(Species::Child).List.size() != 5)
        {
            return false;
        }

        // An animal cannot trade, buy, or talk — the market means being
        // fed (Aid), and it has no social or comfort drives to act on.
        const auto& animal = BehaviourFor(Species::Animal);

        if (animal.CanTrade || animal.CanTalk ||
            animal.MarketKind != InteractionKind::Aid ||
            animal.NeedsSocial || animal.NeedsComfort)
        {
            return false;
        }

        // An animal is seeded with the universal needs only — Hunger,
        // Fatigue, Safety — so it never produces a Socialize or Work
        // intent (no social drive, no comfort drive to work off).
        const auto animalNeeds = SeededNeeds(Species::Animal);

        if (animalNeeds.List.size() != 3)
        {
            return false;
        }

        bool sawSocial = false;
        bool sawComfort = false;

        for (const auto& need : animalNeeds.List)
        {
            sawSocial = sawSocial || need.Type == NeedType::Social;
            sawComfort = sawComfort || need.Type == NeedType::Comfort;
        }

        if (sawSocial || sawComfort)
        {
            return false;
        }

        // The SpeciesTag rides the co-save: a restored dog stays a dog.
        EntityRegistry source;
        RegisterAllSerializers(source);

        const auto dog = source.CreateEntity();
        source.AddComponent<SpeciesTag>(dog, SpeciesTag{ Species::Animal });

        EntityRegistry restored;
        RegisterAllSerializers(restored);
        restored.Restore(source.Capture());

        const auto tag = restored.GetComponent<SpeciesTag>(dog);

        if (tag == nullptr || tag->Value != Species::Animal)
        {
            return false;
        }

        // The arrival outcome is per-species — and since the trade stone,
        // per outcome: a human who traded gets a real Trade, Success; one
        // who found no one (the stall-keeper setting up) gets Trade,
        // Partial; a child or animal is fed — Aid, Success, nothing given
        // in return, a_traded ignored (they never barter).
        const auto feeder = source.CreateEntity();
        const auto trader = source.CreateEntity();

        const auto humanTrade = ArrivalOutcome(
            Species::Human, trader, true);

        if (humanTrade.Other != trader
            || humanTrade.Kind != InteractionKind::Trade
            || humanTrade.Result != OutcomeResult::Success)
        {
            return false;
        }

        const auto humanArrival = ArrivalOutcome(
            Species::Human, feeder, false);

        if (humanArrival.Other != feeder
            || humanArrival.Kind != InteractionKind::Trade
            || humanArrival.Result != OutcomeResult::Partial)
        {
            return false;
        }

        const auto dogArrival = ArrivalOutcome(
            Species::Animal, feeder, true);

        if (dogArrival.Other != feeder
            || dogArrival.Kind != InteractionKind::Aid
            || dogArrival.Result != OutcomeResult::Success)
        {
            return false;
        }

        const auto childArrival = ArrivalOutcome(
            Species::Child, feeder, false);

        if (childArrival.Other != feeder
            || childArrival.Kind != InteractionKind::Aid
            || childArrival.Result != OutcomeResult::Success)
        {
            return false;
        }

        // The trader's half of the exchange (the trade stone): a sale is
        // remembered and warms the stall-keeper toward the customer.
        Memory traderMemory;
        Relationships traderRelationships;

        RecordSale(traderMemory, traderRelationships, feeder, 0.1f);
        RecordSale(traderMemory, traderRelationships, feeder, 0.1f);
        RecordSale(traderMemory, traderRelationships, trader, 0.1f);

        if (traderMemory.Events.size() != 3
            || traderMemory.Events[0].Kind != InteractionKind::Trade
            || traderMemory.Events[0].Other != feeder
            || traderRelationships.ByEntity[feeder].Disposition != 0.2f
            || traderRelationships.ByEntity[trader].Disposition != 0.1f)
        {
            return false;
        }

        // Zero warmth is memory-only — a sale the seller never warmed to.
        Memory coldMemory;
        Relationships coldRelationships;
        RecordSale(coldMemory, coldRelationships, feeder, 0.0f);

        if (coldMemory.Events.size() != 1
            || coldRelationships.ByEntity.size() != 0)
        {
            return false;
        }

        //-------------------------------------------------------------------------
        // The economy stone — SeedPouch and PayForMeal.
        //-------------------------------------------------------------------------
        {
            // A born pouch is deterministic (same id, same purse) and
            // modest: a few meals, not a fortune.
            const auto idA = EntityId{ 0x1010101010101010ull };
            const auto idB = EntityId{ 0x2020202020202020ull };

            const auto pouchA = SeedPouch(idA);
            const auto pouchB = SeedPouch(idB);

            if (pouchA != SeedPouch(idA)
                || pouchA < 20 || pouchA > 60
                || pouchB < 20 || pouchB > 60)
            {
                return false;
            }

            // The exchange: the buyer pays the full price when they can
            // afford it, and the seller's pouch grows.
            CapPouch rich{ 50 };
            CapPouch seller{ 10 };

            const auto full = PayForMeal(rich, seller, 5);

            if (full != 5 || rich.Caps != 45 || seller.Caps != 15)
            {
                return false;
            }

            // A broke buyer pays what they have — never into debt — and
            // the meal is still covered (paid < price means the
            // settlement's credit).
            CapPouch broke{ 3 };
            CapPouch stall{ 0 };

            const auto partial = PayForMeal(broke, stall, 5);

            if (partial != 3 || broke.Caps != 0 || stall.Caps != 3)
            {
                return false;
            }

            // A penniless buyer pays nothing; the seller gains nothing;
            // nobody goes negative.
            CapPouch empty{ 0 };
            CapPouch quiet{ 7 };

            const auto none = PayForMeal(empty, quiet, 5);

            if (none != 0 || empty.Caps != 0 || quiet.Caps != 7)
            {
                return false;
            }
        }

        // The hunger loop's payoff: arriving at the market restores the
        // Hunger need and reports what it was.
        auto needs = SeededNeeds(Species::Animal);
        needs.List[0].Value = 0.12f;

        const auto previous = RestoreHunger(needs);

        if (previous != 0.12f || needs.List[0].Value != 1.0f)
        {
            return false;
        }

        // A mind without a Hunger need is left alone (defensive).
        Needs noHunger;
        noHunger.List.push_back(Need{ NeedType::Fatigue, 0.4f, 0.1f });

        if (RestoreHunger(noHunger) != -1.0f || noHunger.List[0].Value != 0.4f)
        {
            return false;
        }

        // The born ambition: a human carries AcquireFood; a child or an
        // animal carries none (their loop closes on the feed alone).
        const auto humanGoals = SeededGoals(Species::Human);

        if (!humanGoals.Active
            || humanGoals.Active->Type != GoalType::AcquireFood)
        {
            return false;
        }

        if (SeededGoals(Species::Child).Active
            || SeededGoals(Species::Animal).Active)
        {
            return false;
        }

        //-------------------------------------------------------------------------
        // VaryNeeds — the desync stone. Deterministic per entity id
        // (same id, same jitter, forever — a saved mind's rhythm survives
        // restore), bounded (values stay in [0, 1], rates stay positive),
        // and different ids get different temperaments. The decay-rate
        // jitter is the metabolism: the stagger persists after every feed.
        //-------------------------------------------------------------------------
        {
            auto a = SeededNeeds(Species::Human);
            auto b = SeededNeeds(Species::Human);

            const auto idA = EntityId{ 0x1010101010101010ull };
            const auto idB = EntityId{ 0x2020202020202020ull };

            VaryNeeds(a, idA);
            VaryNeeds(b, idB);

            // Bounded and sane: values within [0, 1], rates positive.
            for (const auto& need : a.List)
            {
                if (need.Value < 0.0f || need.Value > 1.0f
                    || need.DecayRate <= 0.0f)
                {
                    return false;
                }
            }

            // Deterministic: re-vary a fresh seed with the same id and
            // get the identical temperament.
            auto aAgain = SeededNeeds(Species::Human);

            VaryNeeds(aAgain, idA);

            for (std::size_t i = 0; i < a.List.size(); ++i)
            {
                if (a.List[i].Value != aAgain.List[i].Value
                    || a.List[i].DecayRate != aAgain.List[i].DecayRate)
                {
                    return false;
                }
            }

            // Distinct ids desync: the two temperaments differ somewhere.
            bool different = false;

            for (std::size_t i = 0; i < a.List.size(); ++i)
            {
                if (a.List[i].Value != b.List[i].Value
                    || a.List[i].DecayRate != b.List[i].DecayRate)
                {
                    different = true;
                    break;
                }
            }

            if (!different)
            {
                return false;
            }

            // The jitter is a temperament, not a coin flip: every need in
            // one mind moves the same way, so urgency ordering stays
            // sensible (the same id, the same span, the same sign).
            auto value = SeededNeeds(Species::Human);
            VaryNeeds(value, idA);

            const auto sign = a.List[0].Value - 1.0f;

            for (std::size_t i = 1; i < value.List.size(); ++i)
            {
                const auto thisSign = value.List[i].Value - 1.0f;

                if ((thisSign < 0.0f) != (sign < 0.0f))
                {
                    return false;
                }
            }
        }

        return true;
    }

    bool SerializationTest()
    {
        //---------------------------------------------------------------------
        // A living registry — the farmer, the merchant, and their minds —
        // captured and restored through the adapter's own serializers.
        //---------------------------------------------------------------------
        EntityRegistry source;
        RegisterAllSerializers(source);

        const auto farmer = source.CreateEntity();
        const auto merchant = source.CreateEntity();

        source.AddComponent<FormRef>(farmer, FormRef{ 0x00012345u });
        source.AddComponent<FormRef>(merchant, FormRef{ 0x00012346u });
        source.AddComponent<Needs>(farmer, SeededNeeds(Species::Human));
        source.AddComponent<Memory>(farmer, Memory{
            { MemoryEvent{ merchant, InteractionKind::Trade, 1.0f, 42 } }
        });
        source.AddComponent<Relationships>(farmer, Relationships{
            { { merchant, Relationship{ 0.4f, 0.6f } } }
        });
        source.AddComponent<Goals>(farmer, Goals{ Goal{
            GoalType::AcquireFood, 0.5f } });
        source.AddComponent<Intent>(farmer, Intent{
            ActionType::MoveTo, merchant, 0.82f });
        source.AddComponent<CapPouch>(farmer, CapPouch{ 33 });

        const auto snapshot = source.Capture();

        if (snapshot.Version != kSnapshotVersion)
        {
            return false;
        }

        if (snapshot.Entities.size() != 2)
        {
            return false;
        }

        //---------------------------------------------------------------------
        // A fresh registry — a fresh game session — restores the world.
        //---------------------------------------------------------------------
        EntityRegistry restored;
        RegisterAllSerializers(restored);
        restored.Restore(snapshot);

        if (!restored.IsAlive(farmer) || !restored.IsAlive(merchant))
        {
            return false;
        }

        const auto form = restored.GetComponent<FormRef>(farmer);
        const auto needs = restored.GetComponent<Needs>(farmer);
        const auto memory = restored.GetComponent<Memory>(farmer);
        const auto relationships = restored.GetComponent<Relationships>(farmer);
        const auto goals = restored.GetComponent<Goals>(farmer);
        const auto intent = restored.GetComponent<Intent>(farmer);

        if (!form || form->FormId != 0x00012345u)
        {
            return false;
        }

        if (!needs || needs->List.size() != 5)
        {
            return false;
        }

        if (!memory || memory->Events.size() != 1
            || memory->Events[0].Other != merchant
            || memory->Events[0].Kind != InteractionKind::Trade
            || memory->Events[0].Weight != 1.0f
            || memory->Events[0].Day != 42)
        {
            return false;
        }

        if (!relationships
            || relationships->ByEntity.size() != 1
            || relationships->ByEntity.at(merchant).Trust != 0.6f)
        {
            return false;
        }

        if (!goals || !goals->Active
            || goals->Active->Type != GoalType::AcquireFood
            || goals->Active->Urgency != 0.5f)
        {
            return false;
        }

        if (!intent || intent->Action != ActionType::MoveTo
            || intent->Target != merchant
            || intent->Confidence != 0.82f)
        {
            return false;
        }

        const auto pouch = restored.GetComponent<CapPouch>(farmer);

        if (!pouch || pouch->Caps != 33)
        {
            return false;
        }

        // The merchant owns only its FormRef — nothing was invented.
        if (restored.GetComponent<Needs>(merchant))
        {
            return false;
        }

        return true;
    }

    bool CoSaveTest()
    {
        //-------------------------------------------------------------------------
        // The durable round-trip (0.4.0): a living world → core snapshot →
        // adapter record (stable names) → bytes → back → Restore. This is
        // the record that actually rides inside the save file.
        //-------------------------------------------------------------------------
        EntityRegistry source;
        RegisterAllSerializers(source);

        const auto farmer = source.CreateEntity();
        const auto merchant = source.CreateEntity();

        source.AddComponent<FormRef>(farmer, FormRef{ 0x00012345u });
        source.AddComponent<FormRef>(merchant, FormRef{ 0x00012346u });
        source.AddComponent<Needs>(farmer, SeededNeeds(Species::Human));
        source.AddComponent<Memory>(farmer, Memory{
            { MemoryEvent{ merchant, InteractionKind::Trade, 1.0f } }
        });
        source.AddComponent<Relationships>(farmer, Relationships{
            { { merchant, Relationship{ 0.4f, 0.6f } } }
        });
        source.AddComponent<Goals>(farmer, Goals{ Goal{
            GoalType::AcquireFood, 0.5f } });
        source.AddComponent<Intent>(farmer, Intent{
            ActionType::MoveTo, merchant, 0.82f });
        source.AddComponent<CapPouch>(farmer, CapPouch{ 33 });

        const auto snapshot = source.Capture();

        // v3 (the stall-keepers stone): the record carries who runs each
        // market's stall, as (market FormID, keeper FormID) — form ids,
        // never the session-local entity ids. v5 rides along with an
        // empty bond section.
        const auto record = TLC::CoSave::Encode(
            snapshot, 0x5EEDC0DEull,
            { { 0x000250FEu, 0x0001A4DAu } },
            {});

        // The record carries the adapter's stable names — literally in the
        // bytes — never the process-local std::type_index addresses.
        const auto contains = [](const std::vector<std::byte>& bytes, std::string_view needle)
        {
            for (std::size_t i = 0; i + needle.size() <= bytes.size(); ++i)
            {
                bool match = true;

                for (std::size_t j = 0; j < needle.size(); ++j)
                {
                    if (std::to_integer<char>(bytes[i + j]) != needle[j])
                    {
                        match = false;
                        break;
                    }
                }

                if (match)
                {
                    return true;
                }
            }

            return false;
        };

        if (!contains(record, "needs")
            || !contains(record, "intent")
            || !contains(record, "formref")
            || !contains(record, "cappouch"))
        {
            return false;
        }

        // Decode, then restore into a fresh registry — a fresh game
        // session that never saw the first one. The v2 header carries the
        // Rng state: decode must hand it back exactly, overwriting the
        // caller's pre-seeded default.
        RegistrySnapshot decoded;
        std::uint64_t rngState = 0xABCDEF0123456789ull;
        std::vector<TLC::CoSave::StallKeeperPair> stalls;
        std::vector<TLC::CoSave::BondPair> bonds;

        if (!TLC::CoSave::Decode(
                record, decoded, rngState, stalls, bonds)
            || rngState != 0x5EEDC0DEull
            || !bonds.empty())
        {
            return false;
        }

        // The stall section round-trips exactly: the same market under
        // the same keeper.
        if (stalls.size() != 1
            || stalls[0].first != 0x000250FEu
            || stalls[0].second != 0x0001A4DAu)
        {
            return false;
        }

        EntityRegistry restored;
        RegisterAllSerializers(restored);
        restored.Restore(decoded);

        if (!restored.IsAlive(farmer) || !restored.IsAlive(merchant))
        {
            return false;
        }

        const auto form = restored.GetComponent<FormRef>(farmer);
        const auto needs = restored.GetComponent<Needs>(farmer);
        const auto memory = restored.GetComponent<Memory>(farmer);
        const auto relationships = restored.GetComponent<Relationships>(farmer);
        const auto goals = restored.GetComponent<Goals>(farmer);
        const auto intent = restored.GetComponent<Intent>(farmer);

        if (!form || form->FormId != 0x00012345u)
        {
            return false;
        }

        if (!needs || needs->List.size() != 5)
        {
            return false;
        }

        if (!memory || memory->Events.size() != 1
            || memory->Events[0].Other != merchant
            || memory->Events[0].Kind != InteractionKind::Trade
            || memory->Events[0].Weight != 1.0f)
        {
            return false;
        }

        if (!relationships
            || relationships->ByEntity.size() != 1
            || relationships->ByEntity.at(merchant).Trust != 0.6f)
        {
            return false;
        }

        if (!goals || !goals->Active
            || goals->Active->Type != GoalType::AcquireFood
            || goals->Active->Urgency != 0.5f)
        {
            return false;
        }

        if (!intent || intent->Action != ActionType::MoveTo
            || intent->Target != merchant
            || intent->Confidence != 0.82f)
        {
            return false;
        }

        const auto pouch = restored.GetComponent<CapPouch>(farmer);

        if (!pouch || pouch->Caps != 33)
        {
            return false;
        }

        const auto merchantForm = restored.GetComponent<FormRef>(merchant);

        if (!merchantForm || merchantForm->FormId != 0x00012346u)
        {
            return false;
        }

        // The merchant owns only its FormRef — nothing was invented.
        if (restored.GetComponent<Needs>(merchant))
        {
            return false;
        }

        //-------------------------------------------------------------------------
        // Refusal paths: a truncated record, an unsupported version, and
        // an unknown component name all refuse the load — never half-apply.
        //-------------------------------------------------------------------------
        {
            auto truncated = record;
            truncated.resize(truncated.size() - 1);

            RegistrySnapshot bad;
            std::uint64_t rngState = 0;
            std::vector<TLC::CoSave::StallKeeperPair> stalls;
            std::vector<TLC::CoSave::BondPair> bonds;

            if (TLC::CoSave::Decode(
                    truncated, bad, rngState, stalls, bonds))
            {
                return false;
            }
        }

        {
            auto badVersion = record;
            badVersion[0] = std::byte{ 0xEF };   // the record version is
            badVersion[1] = std::byte{ 0xBE };   // the first u32,
            badVersion[2] = std::byte{ 0xAD };   // little-endian
            badVersion[3] = std::byte{ 0xDE };

            RegistrySnapshot bad;
            std::uint64_t rngState = 0;
            std::vector<TLC::CoSave::StallKeeperPair> stalls;
            std::vector<TLC::CoSave::BondPair> bonds;

            if (TLC::CoSave::Decode(
                    badVersion, bad, rngState, stalls, bonds))
            {
                return false;
            }
        }

        {
            // Migration (0.4.0): a component this build no longer knows —
            // a type a later build removed — is skipped and dropped; the
            // entity keeps everything else. The old behavior refused the
            // whole record; the migration promise is that old saves load
            // forward. Replace the first "needs" name with an unknown one.
            auto unknownName = record;

            for (std::size_t i = 0; i + 5 <= unknownName.size(); ++i)
            {
                bool match = true;

                for (std::size_t j = 0; j < 5; ++j)
                {
                    if (std::to_integer<char>(unknownName[i + j]) != "needs"[j])
                    {
                        match = false;
                        break;
                    }
                }

                if (match)
                {
                    for (std::size_t j = 0; j < 5; ++j)
                    {
                        unknownName[i + j] = std::byte{ 'x' };
                    }
                    break;
                }
            }

            RegistrySnapshot migrated;
            std::uint64_t rngState = 0;
            std::vector<TLC::CoSave::StallKeeperPair> stalls;
            std::vector<TLC::CoSave::BondPair> bonds;

            if (!TLC::CoSave::Decode(
                    unknownName, migrated, rngState, stalls, bonds))
            {
                return false;
            }

            // Exactly one component (the patched Needs) was dropped: the
            // round-trip snapshot carries 8 named components (farmer's
            // seven — including the cap pouch — + merchant's FormRef);
            // 7 survive.
            std::size_t total = 0;

            for (const auto& entity : migrated.Entities)
            {
                total += entity.Components.size();
            }

            if (total != 7)
            {
                return false;
            }
        }

        //-------------------------------------------------------------------------
        // Migration, the real seam: an old record (version 0, from before
        // `species` existed) loads forward — the missing component is
        // simply absent and the safe default applies (a mind without a
        // SpeciesTag reads as Human). A removed component type ("legacy")
        // is skipped, not fatal. A newer record (version 4) is refused:
        // a future format is not ours to guess.
        //-------------------------------------------------------------------------
        {
            TLC::Codec::Writer writer;

            writer.U32(0);                     // record version 0 (older)
            writer.U32(0);                     // core snapshot version
            writer.U32(1);                     // one entity

            writer.U64(EntityId{ 7 }.Value());
            writer.U32(2);                     // two components

            constexpr std::string_view formName = "formref";
            writer.U8(static_cast<std::uint8_t>(formName.size()));
            writer.Raw(formName.data(), formName.size());
            writer.U32(4);
            writer.U32(0x00012345u);

            constexpr std::string_view legacyName = "legacy";   // removed type
            writer.U8(static_cast<std::uint8_t>(legacyName.size()));
            writer.Raw(legacyName.data(), legacyName.size());
            writer.U32(4);
            writer.U32(0xDEADBEEFu);

            RegistrySnapshot decoded;
            std::uint64_t rngState = 0x1122334455667788ull;
            std::vector<TLC::CoSave::StallKeeperPair> stalls;
            std::vector<TLC::CoSave::BondPair> bonds;

            if (!TLC::CoSave::Decode(
                    writer.Bytes, decoded, rngState, stalls, bonds)
                || decoded.Entities.size() != 1
                || decoded.Entities[0].Components.size() != 1)
            {
                return false;   // legacy dropped, formref kept
            }

            // A v0 record predates the Rng header — the caller's default
            // stream is untouched (that world never had a saved stream).
            if (rngState != 0x1122334455667788ull)
            {
                return false;
            }

            // A v0 record predates the stall section — no keeper is
            // restored; the market's stall re-derives on first arrival.
            if (!stalls.empty())
            {
                return false;
            }

            EntityRegistry restored;
            RegisterAllSerializers(restored);
            restored.Restore(decoded);

            const auto form = restored.GetComponent<FormRef>(EntityId{ 7 });

            if (!form || form->FormId != 0x00012345u)
            {
                return false;
            }

            if (restored.GetComponent<SpeciesTag>(EntityId{ 7 }))
            {
                return false;   // a pre-species save has no tag — Human
            }
        }

        {
            TLC::Codec::Writer writer;

            writer.U32(6);   // newer than this build — refuse, never half-apply
            writer.U32(0);
            writer.U32(0);

            RegistrySnapshot bad;
            std::uint64_t rngState = 0;
            std::vector<TLC::CoSave::StallKeeperPair> stalls;
            std::vector<TLC::CoSave::BondPair> bonds;

            if (TLC::CoSave::Decode(
                    writer.Bytes, bad, rngState, stalls, bonds))
            {
                return false;
            }
        }

        //-------------------------------------------------------------------------
        // Migration across the Rng header (v1 → v2): a v1 record — saved
        // before the decay-jitter wiring — has no Rng state in its header.
        // It loads forward; the caller's default stream stands (the world
        // is reseeded fresh, which is honest: it never had a saved stream).
        //-------------------------------------------------------------------------
        {
            TLC::Codec::Writer writer;

            writer.U32(1);   // the pre-jitter record version
            writer.U32(0);
            writer.U32(1);   // one entity

            writer.U64(EntityId{ 9 }.Value());
            writer.U32(1);   // one component

            constexpr std::string_view formName = "formref";
            writer.U8(static_cast<std::uint8_t>(formName.size()));
            writer.Raw(formName.data(), formName.size());
            writer.U32(4);
            writer.U32(0x00012345u);

            RegistrySnapshot decoded;
            std::uint64_t rngState = 0xFEEDFACE00000000ull;
            std::vector<TLC::CoSave::StallKeeperPair> stalls;
            std::vector<TLC::CoSave::BondPair> bonds;

            if (!TLC::CoSave::Decode(
                    writer.Bytes, decoded, rngState, stalls, bonds)
                || rngState != 0xFEEDFACE00000000ull   // untouched
                || decoded.Entities.size() != 1
                || !stalls.empty()   // v1 predates the stall section
                || !bonds.empty())   // and the bond section
            {
                return false;
            }
        }

        //-------------------------------------------------------------------------
        // Migration across the memory payload (v3 → v4): a v3 record —
        // saved before the world-calendar stone — has no world day in its
        // memory events. It loads forward; the migrated events read
        // Day = 0 ("time immemorial"), which is honest: those facts
        // predate the calendar. A v3 record also carries the Rng header
        // (v2+) and the stall section (v3+), so the fixture must too.
        //-------------------------------------------------------------------------
        {
            TLC::Codec::Writer writer;

            writer.U32(3);   // the pre-calendar record version
            writer.U32(0);   // core snapshot version
            writer.U32(1);   // one entity

            writer.U64(0x1234ull);   // v2+ header: the Rng state
            writer.U64(EntityId{ 11 }.Value());
            writer.U32(1);   // one component

            constexpr std::string_view memoryName = "memory";
            writer.U8(static_cast<std::uint8_t>(memoryName.size()));
            writer.Raw(memoryName.data(), memoryName.size());

            // The memory blob in the OLD format: count, then
            // (U64 other, U32 kind, F weight) — no day.
            TLC::Codec::Writer memoryBlob;
            memoryBlob.U32(1);
            memoryBlob.U64(EntityId{ 12 }.Value());
            memoryBlob.U32(
                static_cast<std::uint32_t>(InteractionKind::Trade));
            memoryBlob.F(1.0f);

            writer.U32(
                static_cast<std::uint32_t>(memoryBlob.Bytes.size()));
            writer.Raw(memoryBlob.Bytes.data(), memoryBlob.Bytes.size());

            writer.U32(0);   // v3+ stall section — none

            RegistrySnapshot decoded;
            std::uint64_t rngState = 0;
            std::vector<TLC::CoSave::StallKeeperPair> stalls;
            std::vector<TLC::CoSave::BondPair> bonds;

            if (!TLC::CoSave::Decode(
                    writer.Bytes, decoded, rngState, stalls, bonds)
                || decoded.Entities.size() != 1
                || !bonds.empty())   // v3 predates the bond section
            {
                return false;
            }

            EntityRegistry restored;
            RegisterAllSerializers(restored);
            restored.Restore(decoded);

            const auto memory = restored.GetComponent<Memory>(EntityId{ 11 });

            if (!memory || memory->Events.size() != 1
                || memory->Events[0].Other != EntityId{ 12 }
                || memory->Events[0].Kind != InteractionKind::Trade
                || memory->Events[0].Weight != 1.0f
                || memory->Events[0].Day != 0)
            {
                return false;   // migrated, unstamped — time immemorial
            }
        }

        return true;
    }

    bool PlanBuilderTest()
    {
        //---------------------------------------------------------------------
        // A hungry farmer with a decided intent to move to the merchant.
        //---------------------------------------------------------------------
        EntityRegistry registry;

        const auto farmer = registry.CreateEntity();
        const auto merchant = registry.CreateEntity();

        registry.AddComponent<Intent>(farmer, Intent{
            ActionType::MoveTo, merchant, 0.82f });

        const auto all = [](EntityId) { return true; };
        const auto none = [](EntityId) { return false; };

        // Everything loaded and available → one executable plan entry.
        {
            const auto plan = BuildPlan(registry, all, all, all);

            if (plan.size() != 1)
            {
                return false;
            }

            const auto& entry = plan[0];

            if (entry.Entity != farmer
                || entry.Intent.Action != ActionType::MoveTo
                || entry.Intent.Target != merchant
                || !entry.ActorLoaded
                || !entry.TargetLoaded
                || !entry.Available)
            {
                return false;
            }
        }

        // An unloaded actor is refused — the sim re-decides next tick.
        {
            const auto plan = BuildPlan(registry, none, all, all);

            if (plan.size() != 1 || plan[0].ActorLoaded)
            {
                return false;
            }
        }

        // An unloaded target (the merchant is in another cell) is refused.
        {
            const auto plan = BuildPlan(registry, all, none, all);

            if (plan.size() != 1 || plan[0].TargetLoaded)
            {
                return false;
            }
        }

        // A busy actor is refused.
        {
            const auto plan = BuildPlan(registry, all, all, none);

            if (plan.size() != 1 || plan[0].Available)
            {
                return false;
            }
        }

        // A targetless intent (Explore) is never refused for an unloaded
        // target — there is nothing to load.
        {
            EntityRegistry exploreOnly;

            const auto wanderer = exploreOnly.CreateEntity();

            exploreOnly.AddComponent<Intent>(wanderer, Intent{
                ActionType::Explore, EntityId{}, 0.5f });

            const auto plan = BuildPlan(exploreOnly, all, none, all);

            if (plan.size() != 1
                || plan[0].Intent.Action != ActionType::Explore
                || !plan[0].TargetLoaded)
            {
                return false;
            }
        }

        // The target predicate sees the merchant, not the actor.
        {
            EntityId seen{};

            const auto plan = BuildPlan(registry, all, [&](EntityId a_entity) {
                seen = a_entity;
                return true;
            }, all);

            if (plan.size() != 1)
            {
                return false;
            }

            if (seen != merchant)
            {
                return false;
            }
        }

        // A mind with no decision produces no plan.
        {
            EntityRegistry empty;

            const auto plan = BuildPlan(empty, all, all, all);

            if (!plan.empty())
            {
                return false;
            }
        }

        return true;
    }

    bool MarketTest()
    {
        //-------------------------------------------------------------------------
        // The walking stone's decision half: a hungry settler who
        // remembers the market decides MoveTo -> market. The market is the
        // Sanctuary workshop (REFR 000250FE); the seed is the adapter's
        // report (ADR-0024: the adapter reports events, the simulation
        // gives them meaning).
        //-------------------------------------------------------------------------
        EntityRegistry registry;

        const auto settler = registry.CreateEntity();
        const auto market = registry.CreateEntity();

        registry.AddComponent<Needs>(settler, SeededNeeds(Species::Human));
        registry.AddComponent<Memory>(settler, Memory{});
        registry.AddComponent<FormRef>(market, FormRef{ kMarketFormId });

        SeedMarketMemory(registry, market);

        Update(registry, 1.0);

        const auto intent = registry.GetComponent<Intent>(settler);

        if (!intent
            || intent->Action != ActionType::MoveTo
            || intent->Target != market)
        {
            return false;
        }

        // The same hungry mind WITHOUT the market memory has nowhere to
        // trade — it explores instead of moving.
        EntityRegistry bare;

        const auto wanderer = bare.CreateEntity();

        bare.AddComponent<Needs>(wanderer, SeededNeeds(Species::Human));
        bare.AddComponent<Memory>(wanderer, Memory{});

        Update(bare, 1.0);

        const auto other = bare.GetComponent<Intent>(wanderer);

        if (!other || other->Action == ActionType::MoveTo)
        {
            return false;
        }

        // The seed is idempotent: a mind that already remembers the
        // market keeps its single event, so the tick's periodic re-seed
        // (the adapter re-pushes the "market is open" fact every second)
        // never grows memory.
        {
            EntityRegistry world;

            const auto resident = world.CreateEntity();
            const auto stall = world.CreateEntity();

            world.AddComponent<Memory>(resident, Memory{});
            world.AddComponent<FormRef>(stall, FormRef{ kMarketFormId });

            SeedMarketMemory(world, stall);
            SeedMarketMemory(world, stall);
            SeedMarketMemory(world, stall);

            const auto memory = world.GetComponent<Memory>(resident);

            if (!memory || memory->Events.size() != 1)
            {
                return false;
            }

            // A mind that truly forgot (the core erases faded events
            // below its threshold) is re-seeded — the fact comes back.
            memory->Events.clear();

            SeedMarketMemory(world, stall);

            if (memory->Events.size() != 1)
            {
                return false;
            }
        }

        // The food-source resolver: a mind remembers whoever resolves as
        // its source, not the fallback market. The idempotent guard keys
        // on the source, so a changed source re-seeds with the new one.
        {
            EntityRegistry world;

            const auto human = world.CreateEntity();
            const auto dog = world.CreateEntity();
            const auto owner = world.CreateEntity();
            const auto market = world.CreateEntity();

            world.AddComponent<Memory>(human, Memory{});
            world.AddComponent<Memory>(dog, Memory{});
            world.AddComponent<FormRef>(market, FormRef{ kMarketFormId });

            const auto resolve = [market, owner, dog](EntityId a_entity) {
                return a_entity == dog ? owner : market;
            };

            SeedMarketMemory(world, market, resolve);

            const auto humanMemory = world.GetComponent<Memory>(human);
            const auto dogMemory = world.GetComponent<Memory>(dog);

            if (!humanMemory || humanMemory->Events.size() != 1
                || humanMemory->Events[0].Other != market)
            {
                return false;
            }

            if (!dogMemory || dogMemory->Events.size() != 1
                || dogMemory->Events[0].Other != owner)
            {
                return false;
            }

            // Re-seed: the dog still targets its owner (idempotent), and
            // a mind with no valid source (a grazer) is not seeded at all.
            SeedMarketMemory(world, market, resolve);

            if (dogMemory->Events.size() != 1)
            {
                return false;
            }

            const auto stray = world.CreateEntity();
            world.AddComponent<Memory>(stray, Memory{});

            const auto graze = [](EntityId) { return EntityId{}; };

            SeedMarketMemory(world, market, graze);

            const auto strayMemory = world.GetComponent<Memory>(stray);

            if (!strayMemory || !strayMemory->Events.empty())
            {
                return false;
            }
        }

        //-------------------------------------------------------------------------
        // NearestWorkshop — the per-settlement rule. Pure: position +
        // squared distance, no game types, testable.
        //-------------------------------------------------------------------------
        {
            // Three workshops in a line: Sanctuary (0,0), Red Rocket
            // (~13000,0), Abernathy (~22000,0).
            const std::vector<WorkshopPosition> workshops{
                { 0x000250FE, 0.0f, 0.0f },
                { 0x000DD0D0, 13000.0f, 0.0f },
                { 0x000DD0D1, 22000.0f, 0.0f } };

            // A settler at Sanctuary's bench finds Sanctuary.
            if (NearestWorkshop(10.0f, 10.0f, workshops, kMarketRadius)
                != 0x000250FE)
            {
                return false;
            }

            // A settler at Red Rocket finds Red Rocket (closer than
            // Sanctuary or Abernathy).
            if (NearestWorkshop(13000.0f, 0.0f, workshops, kMarketRadius)
                != 0x000DD0D0)
            {
                return false;
            }

            // A mind at Abernathy finds Abernathy.
            if (NearestWorkshop(22100.0f, 50.0f, workshops, kMarketRadius)
                != 0x000DD0D1)
            {
                return false;
            }

            // Beyond the radius from every workshop (36 km out) — none.
            if (NearestWorkshop(36000.0f, 0.0f, workshops, kMarketRadius)
                != 0)
            {
                return false;
            }

            // Empty census — no workshops.
            if (NearestWorkshop(0.0f, 0.0f, {}, kMarketRadius) != 0)
            {
                return false;
            }
        }

        return true;
    }

    bool WorldFactsTest()
    {
        using namespace TLC::WorldFacts;

        //-------------------------------------------------------------------------
        // IsMarketClosed — the pure hour gate behind the world-facts
        // stone. Default hours 8–20: open from 08:00 (inclusive) through
        // 19:59, closed from 20:00 (exclusive) through 07:59.
        //-------------------------------------------------------------------------
        if (!IsMarketClosed(6.0f, kMarketOpenHour, kMarketCloseHour)
            || IsMarketClosed(8.0f, kMarketOpenHour, kMarketCloseHour)
            || IsMarketClosed(12.0f, kMarketOpenHour, kMarketCloseHour)
            || IsMarketClosed(19.99f, kMarketOpenHour, kMarketCloseHour)
            || !IsMarketClosed(20.0f, kMarketOpenHour, kMarketCloseHour)
            || !IsMarketClosed(23.0f, kMarketOpenHour, kMarketCloseHour))
        {
            return false;
        }

        // Overnight hours wrap: open 20:00–08:00 trades through midnight.
        if (IsMarketClosed(22.0f, 20.0f, 8.0f)
            || IsMarketClosed(23.99f, 20.0f, 8.0f)
            || IsMarketClosed(0.0f, 20.0f, 8.0f)
            || IsMarketClosed(7.99f, 20.0f, 8.0f)
            || !IsMarketClosed(8.0f, 20.0f, 8.0f)
            || !IsMarketClosed(12.0f, 20.0f, 8.0f))
        {
            return false;
        }

        //-------------------------------------------------------------------------
        // HasFact — the idempotent guard. A fact is a memory event with
        // an invalid Other. The market-location memory (a valid Other)
        // must NOT read as a fact: knowing where the market is is not the
        // same as the door being shut.
        //-------------------------------------------------------------------------
        {
            Memory memory;

            if (HasFact(memory, InteractionKind::Trade))
            {
                return false;
            }

            memory.Events.push_back(MemoryEvent{
                EntityId{ 42 }, InteractionKind::Trade, 1.0f });

            if (HasFact(memory, InteractionKind::Trade))
            {
                return false;   // location memory — not a fact
            }

            ApplyFact(memory, InteractionKind::Trade, true);
            ApplyFact(memory, InteractionKind::Social, true);

            if (!HasFact(memory, InteractionKind::Trade)
                || !HasFact(memory, InteractionKind::Social)
                || HasFact(memory, InteractionKind::Combat))
            {
                return false;
            }
        }

        //-------------------------------------------------------------------------
        // ApplyFact — the refresh pattern. While active, the fact is
        // remembered at full weight: repeated applies never duplicate it
        // (one event per kind — memory does not grow), and an eroded
        // weight is topped back up so the tick's fade cannot erase a shut
        // door. Inactive applies leave the mind alone: the core's fade is
        // the designed reopen.
        //-------------------------------------------------------------------------
        {
            Memory memory;

            ApplyFact(memory, InteractionKind::Trade, true);
            ApplyFact(memory, InteractionKind::Trade, true);
            ApplyFact(memory, InteractionKind::Trade, true);

            if (memory.Events.size() != 1
                || !HasFact(memory, InteractionKind::Trade)
                || memory.Events[0].Weight != kFactWeight)
            {
                return false;
            }

            // The tick erodes salience; the next refresh restores it.
            memory.Events[0].Weight -= 0.2f;

            ApplyFact(memory, InteractionKind::Trade, true);

            if (memory.Events.size() != 1
                || memory.Events[0].Weight != kFactWeight)
            {
                return false;
            }

            // A second active door is a second event, not a collision.
            ApplyFact(memory, InteractionKind::Social, true);

            if (memory.Events.size() != 2)
            {
                return false;
            }

            // Inactive leaves the memory alone — the fact fades on the
            // core's clock, which is what reopens the door.
            const auto before = memory.Events.size();

            ApplyFact(memory, InteractionKind::Combat, false);

            if (memory.Events.size() != before)
            {
                return false;
            }

            // A truly forgotten fact (the core erased it) is re-pushed.
            memory.Events.clear();

            ApplyFact(memory, InteractionKind::Trade, true);

            if (memory.Events.size() != 1
                || !HasFact(memory, InteractionKind::Trade))
            {
                return false;
            }
        }

        //-------------------------------------------------------------------------
        // ClassifyWeather — the verified live-weather forms (xEdit dump
        // 2026-08-10). Only the forms the game actually sets classify;
        // editor backups, interiors, and the unknown leave no fact (we
        // do not remember what we do not know).
        //-------------------------------------------------------------------------
        if (ClassifyWeather(0x0002B52A) != WeatherKind::Clear    // CommonwealthClear
            || ClassifyWeather(0x001D670E) != WeatherKind::Clear // ClearestSkies
            || ClassifyWeather(0x0012A18E) != WeatherKind::Clear // SanctuaryClear
            || ClassifyWeather(0x001C8556) != WeatherKind::Overcast // Overcast
            || ClassifyWeather(0x000F1033) != WeatherKind::Overcast // GSOvercast
            || ClassifyWeather(0x001CA7E4) != WeatherKind::Rain   // CommonwealthRain
            || ClassifyWeather(0x001C3473) != WeatherKind::Fog    // Foggy
            || ClassifyWeather(0x001BD481) != WeatherKind::Fog    // GSFoggy
            || ClassifyWeather(0x001CC186) != WeatherKind::Misty  // Misty
            || ClassifyWeather(0x001CD096) != WeatherKind::Misty  // MistyRainy
            || ClassifyWeather(0x001C3D5E) != WeatherKind::Radstorm) // GSRadstorm
        {
            return false;
        }

        // Editor backups are never set at runtime; interiors, FX, and
        // unknown forms classify as Unknown.
        if (ClassifyWeather(0x0022239A) != WeatherKind::Unknown   // RainBackup
            || ClassifyWeather(0x002392A3) != WeatherKind::Unknown // GSRadstormBackup
            || ClassifyWeather(0x001A65F0) != WeatherKind::Unknown // DefaultInteriorWeather
            || ClassifyWeather(0x001F61FD) != WeatherKind::Unknown // CGPrewarNukeFXWeather
            || ClassifyWeather(0) != WeatherKind::Unknown
            || ClassifyWeather(0xDEADBEEF) != WeatherKind::Unknown)
        {
            return false;
        }

        //-------------------------------------------------------------------------
        // WeatherFactKind — the category as the fact label the sim
        // memory carries. Unknown has no fact.
        //-------------------------------------------------------------------------
        if (WeatherFactKind(WeatherKind::Clear) != InteractionKind::WeatherClear
            || WeatherFactKind(WeatherKind::Overcast) != InteractionKind::WeatherOvercast
            || WeatherFactKind(WeatherKind::Rain) != InteractionKind::WeatherRain
            || WeatherFactKind(WeatherKind::Fog) != InteractionKind::WeatherFog
            || WeatherFactKind(WeatherKind::Misty) != InteractionKind::WeatherMisty
            || WeatherFactKind(WeatherKind::Radstorm) != InteractionKind::WeatherRadstorm
            || WeatherFactKind(WeatherKind::Unknown).has_value())
        {
            return false;
        }

        // WeatherLabel — the player-facing word.
        if (std::string_view(WeatherLabel(WeatherKind::Rain)) != "rainy"
            || std::string_view(WeatherLabel(WeatherKind::Fog)) != "foggy"
            || std::string_view(WeatherLabel(WeatherKind::Radstorm)) != "a radstorm"
            || std::string_view(WeatherLabel(WeatherKind::Unknown)) != "unclassified")
        {
            return false;
        }

        //-------------------------------------------------------------------------
        // ApplyFact with a day — the weather stamp. A pushed weather fact
        // carries the world day it was remembered; a refresh tops the
        // weight AND re-stamps (re-seen today is remembered today). The
        // gates call with no day and stay unstamped. Weather kinds are
        // distinct fact slots — rain and a closed market coexist.
        //-------------------------------------------------------------------------
        {
            Memory memory;

            ApplyFact(memory, InteractionKind::WeatherRain, true, 12);
            ApplyFact(memory, InteractionKind::WeatherRain, true, 12);

            if (memory.Events.size() != 1
                || memory.Events[0].Day != 12
                || memory.Events[0].Weight != kFactWeight)
            {
                return false;
            }

            // A refresh on a later day re-stamps: the rain is today's again.
            memory.Events[0].Weight -= 0.2f;

            ApplyFact(memory, InteractionKind::WeatherRain, true, 13);

            if (memory.Events.size() != 1
                || memory.Events[0].Day != 13
                || memory.Events[0].Weight != kFactWeight)
            {
                return false;
            }

            // Distinct slots: rain + a closed market are two events.
            ApplyFact(memory, InteractionKind::Trade, true);

            if (memory.Events.size() != 2)
            {
                return false;
            }

            // The no-day call stays unstamped (Day 0) — gates are doors,
            // not day memories.
            ApplyFact(memory, InteractionKind::Social, true);

            if (memory.Events.size() != 3 || memory.Events[2].Day != 0)
            {
                return false;
            }
        }

        return true;
    }

    bool TuningTest()
    {
        //-------------------------------------------------------------------------
        // ParseConfig — the config file's text becomes the core's
        // Configuration service. One `key = value` per line; `;` and `#`
        // comments, blanks, and CRLF all survive; surrounding whitespace
        // is trimmed; malformed lines are skipped, never fatal.
        //-------------------------------------------------------------------------
        const auto config = Tuning::ParseConfig(
            "; The Living Commonwealth tuning\r\n"
            "# another comment\r\n"
            "\r\n"
            "sim.memory.fade = 0.25\r\n"
            "sim.hunger.decay = 0.002\r\n"
            "sim.fatigue.decay = 0.003\r\n"
            "sim.safety.decay = 0.004\r\n"
            "sim.social.decay = 0.005\r\n"
            "sim.comfort.decay = 0.006\r\n"
            "sim.sale.warmth = 0.25\r\n"
            "sim.meal.price = 3\r\n"
            "market.open.hour = 9\r\n"
            "market.close.hour=21\r\n"
            "   padded.key   =   spaced   \r\n"
            "malformed line without equals\r\n"
            "= orphan value\r\n");

        if (!config.Has("sim.memory.fade")
            || config.Get("sim.memory.fade") != "0.25"
            || !config.Has("market.open.hour")
            || config.Get("market.open.hour") != "9"
            || !config.Has("market.close.hour")
            || config.Get("market.close.hour") != "21"
            || !config.Has("padded.key")
            || config.Get("padded.key") != "spaced"
            || config.Has("malformed line without equals")
            || config.Has("= orphan value"))
        {
            return false;
        }

        //-------------------------------------------------------------------------
        // AdapterSettingsFrom — the adapter's own keys (market hours)
        // override the defaults; missing keys and broken values keep them
        // (a broken line must never break the world).
        //-------------------------------------------------------------------------
        const auto settings = Tuning::AdapterSettingsFrom(config);

        if (settings.MarketOpenHour != 9.0f
            || settings.MarketCloseHour != 21.0f
            || settings.Rates.Hunger != 0.002f
            || settings.Rates.Fatigue != 0.003f
            || settings.Rates.Safety != 0.004f
            || settings.Rates.Social != 0.005f
            || settings.Rates.Comfort != 0.006f
            || settings.SaleWarmth != 0.25f
            || settings.MealPrice != 3.0f)
        {
            return false;
        }

        const auto defaults = Tuning::AdapterSettingsFrom(
            LCE::Config::Configuration{});

        if (defaults.MarketOpenHour != WorldFacts::kMarketOpenHour
            || defaults.MarketCloseHour != WorldFacts::kMarketCloseHour
            || defaults.Rates.Hunger != 0.1f
            || defaults.Rates.Fatigue != 0.1f
            || defaults.Rates.Safety != 0.1f
            || defaults.Rates.Social != 0.1f
            || defaults.Rates.Comfort != 0.1f
            || defaults.SaleWarmth != 0.1f
            || defaults.MealPrice != 5.0f)
        {
            return false;
        }

        const auto broken = Tuning::ParseConfig(
            "market.open.hour = not-a-number\n"
            "sim.hunger.decay = slow\n");
        const auto brokenSettings = Tuning::AdapterSettingsFrom(broken);

        if (brokenSettings.MarketOpenHour != WorldFacts::kMarketOpenHour
            || brokenSettings.Rates.Hunger != 0.1f)
        {
            return false;
        }

        // The tuned rates actually reach fresh minds: SeededNeeds applies
        // them, and an animal's profile (no Social/Comfort needs) still
        // seeds only its universal needs — with the tuned rates.
        const auto needs = SeededNeeds(Species::Human, defaults.Rates);
        const auto animal = SeededNeeds(Species::Animal, defaults.Rates);

        if (needs.List.size() != 5
            || animal.List.size() != 3
            || animal.List[0].DecayRate != 0.1f)
        {
            return false;
        }

        return true;
    }

    bool LifecycleTest()
    {
        // Empty world: a relevant, alive settler arrives; a dead or
        // irrelevant actor never becomes a mind.
        {
            const std::unordered_set<std::uint32_t> known;

            const std::vector<Lifecycle::Scan> scans{
                { 0x00000101u, true,  false },   // relevant + alive -> arrival
                { 0x00000102u, false, false },   // not relevant      -> nothing
                { 0x00000103u, true,  true  },   // dead              -> nothing
            };

            const auto events = Lifecycle::Diff(known, scans);

            if (events.size() != 1
                || events[0].Kind != Lifecycle::EventKind::Arrival
                || events[0].FormId != 0x00000101u)
            {
                return false;
            }
        }

        // A known mind that dies is a death — even if it also left the
        // faction (a corpse is a death, not a departure).
        {
            const std::unordered_set<std::uint32_t> known{ 0x00000101u };

            const std::vector<Lifecycle::Scan> scans{
                { 0x00000101u, false, true },   // dead    -> death
                { 0x00000102u, true,  false },  // new     -> arrival
            };

            const auto events = Lifecycle::Diff(known, scans);

            if (events.size() != 2
                || events[0].Kind != Lifecycle::EventKind::Death
                || events[0].FormId != 0x00000101u
                || events[1].Kind != Lifecycle::EventKind::Arrival
                || events[1].FormId != 0x00000102u)
            {
                return false;
            }
        }

        // A known mind alive but out of the faction departs; an alive,
        // relevant known mind is left alone; a dead one is a death.
        {
            const std::unordered_set<std::uint32_t> known{
                0x00000101u, 0x00000102u, 0x00000103u };

            const std::vector<Lifecycle::Scan> scans{
                { 0x00000101u, false, false },  // left     -> departure
                { 0x00000102u, true,  false },  // fine     -> nothing
                { 0x00000103u, false, true  },  // dead     -> death
            };

            const auto events = Lifecycle::Diff(known, scans);

            if (events.size() != 2
                || events[0].Kind != Lifecycle::EventKind::Departure
                || events[0].FormId != 0x00000101u
                || events[1].Kind != Lifecycle::EventKind::Death
                || events[1].FormId != 0x00000103u)
            {
                return false;
            }
        }

        // The process lists can hold an actor twice — one classification
        // per form id per pass, never a double booking.
        {
            const std::unordered_set<std::uint32_t> known;

            const std::vector<Lifecycle::Scan> scans{
                { 0x00000101u, true, false },
                { 0x00000101u, true, false },   // duplicate sighting
                { 0x00000102u, true, false },
            };

            const auto events = Lifecycle::Diff(known, scans);

            if (events.size() != 2)
            {
                return false;
            }
        }

        return true;
    }

    bool BondTest()
    {
        using namespace Bonds;

        const BondThresholds t;   // the defaults

        //-------------------------------------------------------------------------
        // 1. Derive — formation. The pair's shared disposition is the
        //    minimum of the two directions: both must feel it (mutual
        //    warmth); negatives are mutual too (one strong dislike names
        //    the pair).
        //-------------------------------------------------------------------------
        {
            if (Derive(0.35f, 0.40f, t, BondKind::None) != BondKind::Friend
                || Derive(0.65f, 0.70f, t, BondKind::None)
                    != BondKind::Sweetheart
                || Derive(0.85f, 0.90f, t, BondKind::None)
                    != BondKind::Spouse
                || Derive(-0.35f, -0.40f, t, BondKind::None)
                    != BondKind::Rival
                || Derive(-0.65f, -0.70f, t, BondKind::None)
                    != BondKind::Enemy)
            {
                return false;
            }

            // One-sided warmth is not yet a bond; a thin pair is none.
            if (Derive(0.35f, -0.10f, t, BondKind::None)
                    != BondKind::None
                || Derive(0.20f, 0.20f, t, BondKind::None)
                    != BondKind::None)
            {
                return false;
            }
        }

        //-------------------------------------------------------------------------
        // 2. Derive — sticky dissolution. A bond persists until the pair
        //    falls halfway back from its line (drift is quiet); below it,
        //    the bond dissolves.
        //-------------------------------------------------------------------------
        {
            if (Derive(0.16f, 0.16f, t, BondKind::Friend)
                    != BondKind::Friend   // above half (0.15)
                || Derive(0.14f, 0.14f, t, BondKind::Friend)
                    != BondKind::None
                || Derive(0.31f, 0.31f, t, BondKind::Sweetheart)
                    != BondKind::Sweetheart   // above half (0.30)
                || Derive(-0.31f, -0.31f, t, BondKind::Enemy)
                    != BondKind::Enemy   // above half (−0.30)
                || Derive(-0.29f, -0.29f, t, BondKind::Enemy)
                    != BondKind::None)
            {
                return false;
            }
        }

        //-------------------------------------------------------------------------
        // 3. Derive — upgrades are immediate, downgrades wait for the
        //    sticky dissolve, and a family flip is news.
        //-------------------------------------------------------------------------
        {
            if (Derive(0.65f, 0.65f, t, BondKind::Friend)
                    != BondKind::Sweetheart   // upgraded
                || Derive(-0.65f, -0.65f, t, BondKind::Rival)
                    != BondKind::Enemy   // upgraded
                || Derive(0.35f, 0.35f, t, BondKind::Sweetheart)
                    != BondKind::Sweetheart   // no downgrade
                || Derive(-0.35f, -0.35f, t, BondKind::Enemy)
                    != BondKind::Enemy   // no downgrade
                || Derive(-0.40f, -0.40f, t, BondKind::Friend)
                    != BondKind::Rival   // friends turned rivals
                || Derive(0.40f, 0.40f, t, BondKind::Enemy)
                    != BondKind::Friend)   // feud resolved into friendship
            {
                return false;
            }
        }

        //-------------------------------------------------------------------------
        // 4. Thresholds — the typed copy parses the core's watch-list;
        //    a partial config falls back to the defaults for the rest.
        //-------------------------------------------------------------------------
        {
            const std::vector<BondThreshold> lines{
                { "friend", 0.25f }, { "spouse", 0.75f },
            };

            const auto parsed = ParseBondThresholds(lines);

            if (parsed.Friend != 0.25f
                || parsed.Spouse != 0.75f
                || parsed.Sweetheart != 0.6f   // untouched — the default
                || parsed.Rival != -0.3f
                || parsed.Enemy != -0.6f)
            {
                return false;
            }

            const auto defaults = DefaultBondThresholds();

            if (defaults.size() != 5)
            {
                return false;
            }
        }

        //-------------------------------------------------------------------------
        // 5. Reconcile — the 1-second pass walks the registry, derives
        //    each unordered pair once, and reports changes. A workshop
        //    (no SpeciesTag) is never a bond partner.
        //-------------------------------------------------------------------------
        {
            EntityRegistry registry;

            const auto a = registry.CreateEntity();
            const auto b = registry.CreateEntity();
            const auto workshop = registry.CreateEntity();

            registry.AddComponent<SpeciesTag>(a, SpeciesTag{ Species::Human });
            registry.AddComponent<SpeciesTag>(b, SpeciesTag{ Species::Human });
            registry.AddComponent<FormRef>(workshop, FormRef{ 0x000250FEu });

            registry.AddComponent<Relationships>(a, Relationships{
                { { b, Relationship{ 0.4f, 0.0f } },
                  { workshop, Relationship{ 0.5f, 0.0f } } } });
            registry.AddComponent<Relationships>(b, Relationships{
                { { a, Relationship{ 0.4f, 0.0f } } } });

            BondMap bonds;
            std::vector<BondKind> changes;
            std::uint64_t sinceDay = 0;

            Reconcile(
                registry, t, bonds, 12,
                [&](EntityId, EntityId, BondKind old, BondKind fresh,
                    std::uint64_t since)
                {
                    changes.push_back(old);
                    changes.push_back(fresh);
                    sinceDay = since;
                });

            // One pair, one event: the workshop is skipped; the pair
            // formed as friends on day 12.
            if (changes.size() != 2
                || changes[0] != BondKind::None
                || changes[1] != BondKind::Friend
                || sinceDay != 12
                || bonds.size() != 1)
            {
                return false;
            }

            // Drift below the dissolve line — the second pass reports
            // the quiet dissolve the events never announce.
            registry.GetComponent<Relationships>(a)->ByEntity[b]
                = Relationship{ 0.10f, 0.0f };
            registry.GetComponent<Relationships>(b)->ByEntity[a]
                = Relationship{ 0.10f, 0.0f };

            changes.clear();

            Reconcile(
                registry, t, bonds, 12,
                [&](EntityId, EntityId, BondKind old, BondKind fresh,
                    std::uint64_t since)
                {
                    changes.push_back(old);
                    changes.push_back(fresh);
                    sinceDay = since;
                });

            if (changes.size() != 2
                || changes[0] != BondKind::Friend
                || changes[1] != BondKind::None
                || sinceDay != 0   // dissolved — no since-day
                || bonds.size() != 1)   // resting row kept, kind None
            {
                return false;
            }
        }

        //-------------------------------------------------------------------------
        // 6. The event channel — the full wiring: a configured line, an
        //    experience that crosses it, the RelationshipChangedEvent on
        //    the bus, and the adapter's handler folding the pair into the
        //    bond map. One crossing, one change — resting stays silent.
        //-------------------------------------------------------------------------
        {
            LCE::Config::Configuration config;
            config.Set(std::string("sim.bond.threshold.friend"), "0.05");

            const auto tuning =
                SimulationTuning::FromConfiguration(config);
            const auto thresholds =
                ParseBondThresholds(tuning.BondThresholds);

            LCE::Events::EventBus bus;
            EntityRegistry registry;

            const auto a = registry.CreateEntity();
            const auto b = registry.CreateEntity();

            registry.AddComponent<SpeciesTag>(a, SpeciesTag{ Species::Human });
            registry.AddComponent<SpeciesTag>(b, SpeciesTag{ Species::Human });

            // b already feels warmly about a — one aid from a crosses the
            // friend line for the pair.
            registry.AddComponent<Relationships>(a, Relationships{});
            registry.AddComponent<Relationships>(b, Relationships{
                { { a, Relationship{ 0.5f, 0.0f } } } });

            BondMap bonds;
            int changes = 0;
            BondKind lastKind = BondKind::None;
            std::uint64_t lastDay = 0;

            bus.Subscribe(
                std::type_index(typeid(RelationshipChangedEvent)),
                [&](const LCE::Events::Event& event)
                {
                    const auto& crossing =
                        static_cast<const RelationshipChangedEvent&>(event);

                    const auto dToOther =
                        registry.GetComponent<Relationships>(crossing.Subject)
                            ->ByEntity[crossing.Other].Disposition;

                    float dOtherToMe = 0.0f;

                    if (const auto reverse = registry.GetComponent<Relationships>(
                            crossing.Other))
                    {
                        const auto iterator =
                            reverse->ByEntity.find(crossing.Subject);

                        if (iterator != reverse->ByEntity.end())
                        {
                            dOtherToMe = iterator->second.Disposition;
                        }
                    }

                    ApplyPair(
                        bonds, PairKey(crossing.Subject, crossing.Other),
                        dToOther, dOtherToMe, thresholds, crossing.Day,
                        [&](EntityId, EntityId, BondKind, BondKind fresh,
                            std::uint64_t since)
                        {
                            ++changes;
                            lastKind = fresh;
                            lastDay = since;
                        });
                });

            ReportOutcome(
                registry, a,
                { b, InteractionKind::Aid, OutcomeResult::Success },
                tuning, &bus, WorldTime{ 7 });

            // One aid warms a→b by the core's DispositionGain (0.1) past
            // the 0.05 line; the event fires once, the pair forms as
            // friends on day 7.
            if (changes != 1
                || lastKind != BondKind::Friend
                || lastDay != 7
                || bonds.size() != 1)
            {
                return false;
            }
        }

        //-------------------------------------------------------------------------
        // 7. The co-save (v5): bonds round-trip exactly — form pair, kind,
        //    since-day — and a v4 record (pre-bonds) decodes with an
        //    empty bond section (the reconcile pass re-derives instead).
        //-------------------------------------------------------------------------
        {
            EntityRegistry source;
            RegisterAllSerializers(source);

            const auto a = source.CreateEntity();
            const auto b = source.CreateEntity();

            source.AddComponent<FormRef>(a, FormRef{ 0x00011111u });
            source.AddComponent<FormRef>(b, FormRef{ 0x00022222u });
            source.AddComponent<SpeciesTag>(a, SpeciesTag{ Species::Human });
            source.AddComponent<SpeciesTag>(b, SpeciesTag{ Species::Human });

            const auto snapshot = source.Capture();

            const std::vector<TLC::CoSave::BondPair> savedBonds{
                { 0x00011111u, 0x00022222u,
                  static_cast<std::uint32_t>(BondKind::Spouse), 42 },
            };

            const auto record = TLC::CoSave::Encode(
                snapshot, 0x5EEDC0DEull, {}, savedBonds);

            RegistrySnapshot decoded;
            std::uint64_t rngState = 0;
            std::vector<TLC::CoSave::StallKeeperPair> stalls;
            std::vector<TLC::CoSave::BondPair> bonds;

            if (!TLC::CoSave::Decode(
                    record, decoded, rngState, stalls, bonds)
                || rngState != 0x5EEDC0DEull
                || bonds.size() != 1
                || bonds[0].FormA != 0x00011111u
                || bonds[0].FormB != 0x00022222u
                || bonds[0].Kind
                    != static_cast<std::uint32_t>(BondKind::Spouse)
                || bonds[0].SinceDay != 42)
            {
                return false;
            }

            // A malformed bond — a self-pair — is skipped, not fatal.
            const auto badRecord = TLC::CoSave::Encode(
                snapshot, 0, {},
                { { 0x00011111u, 0x00011111u,
                    static_cast<std::uint32_t>(BondKind::Friend), 1 } });

            std::vector<TLC::CoSave::BondPair> badBonds;

            if (!TLC::CoSave::Decode(
                    badRecord, decoded, rngState, stalls, badBonds)
                || !badBonds.empty())
            {
                return false;
            }

            // A v4 record (the world-calendar stone, pre-bonds): header +
            // Rng + entities(0) + stalls(0) — decodes with no bonds.
            TLC::Codec::Writer legacy;
            legacy.U32(4);
            legacy.U32(0);
            legacy.U32(0);
            legacy.U64(0x5EEDC0DEull);
            legacy.U32(0);

            std::vector<TLC::CoSave::BondPair> migratedBonds;

            if (!TLC::CoSave::Decode(
                    legacy.Bytes, decoded, rngState, stalls, migratedBonds)
                || !migratedBonds.empty())
            {
                return false;
            }
        }

        return true;
    }

    bool HouseholdTest()
    {
        using namespace Households;

        //-------------------------------------------------------------------------
        // 1. FormHousehold — two pouches become one shared wallet on the
        //    lower-id member; a second call is a no-op (idempotent, so
        //    the caller may run it freely).
        //-------------------------------------------------------------------------
        {
            EntityRegistry registry;

            const auto a = registry.CreateEntity();   // lower id
            const auto b = registry.CreateEntity();

            registry.AddComponent<CapPouch>(a, CapPouch{ 40 });
            registry.AddComponent<CapPouch>(b, CapPouch{ 60 });

            if (!FormHousehold(registry, a, b))
            {
                return false;
            }

            const auto pouchA = registry.GetComponent<CapPouch>(a);
            const auto pouchB = registry.GetComponent<CapPouch>(b);

            if (!pouchA || pouchA->Caps != 100 || pouchB)
            {
                return false;   // merged onto the holder, other removed
            }

            if (FormHousehold(registry, a, b))
            {
                return false;   // already shared — no second merge
            }
        }

        //-------------------------------------------------------------------------
        // 2. The pouch living on the higher-id member moves to the
        //    deterministic holder (so a restored world always puts the
        //    wallet in the same hands).
        //-------------------------------------------------------------------------
        {
            EntityRegistry registry;

            const auto a = registry.CreateEntity();
            const auto b = registry.CreateEntity();   // higher id

            registry.AddComponent<CapPouch>(b, CapPouch{ 25 });

            if (!FormHousehold(registry, a, b))
            {
                return false;
            }

            const auto pouchA = registry.GetComponent<CapPouch>(a);
            const auto pouchB = registry.GetComponent<CapPouch>(b);

            if (!pouchA || pouchA->Caps != 25 || pouchB)
            {
                return false;
            }
        }

        //-------------------------------------------------------------------------
        // 3. DissolveHousehold — the shared wallet splits: the holder
        //    keeps the remainder, the other member receives half. A
        //    second call is a no-op.
        //-------------------------------------------------------------------------
        {
            EntityRegistry registry;

            const auto a = registry.CreateEntity();
            const auto b = registry.CreateEntity();

            registry.AddComponent<CapPouch>(a, CapPouch{ 101 });

            std::uint32_t holderShare = 0;
            std::uint32_t otherShare = 0;

            if (!DissolveHousehold(registry, a, b, holderShare, otherShare)
                || holderShare != 51 || otherShare != 50)
            {
                return false;
            }

            const auto pouchA = registry.GetComponent<CapPouch>(a);
            const auto pouchB = registry.GetComponent<CapPouch>(b);

            if (!pouchA || pouchA->Caps != 51
                || !pouchB || pouchB->Caps != 50)
            {
                return false;
            }

            if (DissolveHousehold(registry, a, b, holderShare, otherShare))
            {
                return false;   // already split
            }
        }

        //-------------------------------------------------------------------------
        // 4. SpouseOf and PouchOf — the married pair resolves each
        //    other's wallet; an unmarried mind without a pouch has none.
        //-------------------------------------------------------------------------
        {
            EntityRegistry registry;

            const auto a = registry.CreateEntity();
            const auto b = registry.CreateEntity();
            const auto c = registry.CreateEntity();   // unmarried

            registry.AddComponent<CapPouch>(a, CapPouch{ 80 });
            registry.AddComponent<SpeciesTag>(a, SpeciesTag{ Species::Human });
            registry.AddComponent<SpeciesTag>(b, SpeciesTag{ Species::Human });
            registry.AddComponent<SpeciesTag>(c, SpeciesTag{ Species::Human });

            Bonds::BondMap bonds;
            bonds[Bonds::PairKey(a, b)] =
                Bonds::PairBond{ Bonds::BondKind::Spouse, 5 };

            if (SpouseOf(bonds, a) != b || SpouseOf(bonds, c).IsValid())
            {
                return false;
            }

            auto pouchB = PouchOf(registry, bonds, b);
            auto pouchC = PouchOf(registry, bonds, c);

            if (pouchB == nullptr || pouchB->Caps != 80)
            {
                return false;   // b trades with the shared wallet
            }

            if (pouchC != nullptr)
            {
                return false;   // unmarried and pouchless — none
            }
        }

        //-------------------------------------------------------------------------
        // 5. Enforce — the silent invariant: a married pair with two
        //    pouches merges (a restored marriage), and an unmarried
        //    human without a pouch is seeded. Never reports.
        //-------------------------------------------------------------------------
        {
            EntityRegistry registry;

            const auto a = registry.CreateEntity();
            const auto b = registry.CreateEntity();
            const auto c = registry.CreateEntity();

            registry.AddComponent<SpeciesTag>(a, SpeciesTag{ Species::Human });
            registry.AddComponent<SpeciesTag>(b, SpeciesTag{ Species::Human });
            registry.AddComponent<SpeciesTag>(c, SpeciesTag{ Species::Human });
            registry.AddComponent<CapPouch>(a, CapPouch{ 30 });
            registry.AddComponent<CapPouch>(b, CapPouch{ 70 });

            Bonds::BondMap bonds;
            bonds[Bonds::PairKey(a, b)] =
                Bonds::PairBond{ Bonds::BondKind::Spouse, 5 };

            Enforce(registry, bonds);

            const auto pouchA = registry.GetComponent<CapPouch>(a);
            const auto pouchB = registry.GetComponent<CapPouch>(b);
            const auto pouchC = registry.GetComponent<CapPouch>(c);

            if (!pouchA || pouchA->Caps != 100 || pouchB)
            {
                return false;   // married pair merged silently
            }

            if (!pouchC || pouchC->Caps == 0)
            {
                return false;   // unmarried human re-seeded
            }
        }

        return true;
    }
}
