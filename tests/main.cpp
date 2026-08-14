//=============================================================================//
// TheLivingCommonwealth adapter tests — the harness, mirroring the core's:
// bool-returning suites, no framework (ADR-0010/0029), runs on every build.
// Links LCE.Core only — no game required.
//=============================================================================//

#include "Arcs.h"
#include "Behaviour.h"
#include "Birth.h"
#include "BlobCodec.h"
#include "Bonds.h"
#include "CoSave.h"
#include "Components.h"
#include "Dialogue.h"
#include "Executor.h"
#include "Gossip.h"
#include "Households.h"
#include "Kin.h"
#include "Lifecycle.h"
#include "Market.h"
#include "Names.h"
#include "News.h"
#include "Fights.h"
#include "Rows.h"
#include "Serialization.h"
#include "Stipend.h"
#include "TradeLedger.h"
#include "Translator.h"
#include "Tuning.h"
#include "WorldFacts.h"

#include "LCE/Simulation/Society/Groups.h"

#include "LCE/Config/Configuration.h"
#include "LCE/Events/EventBus.h"
#include "LCE/Simulation/Decision/Behaviour.h"
#include "LCE/Simulation/Entity/EntityRegistry.h"
#include "LCE/Simulation/Mind/Goals.h"
#include "LCE/Simulation/Mind/Memory.h"
#include "LCE/Simulation/Mind/Needs.h"
#include "LCE/Simulation/Mind/Relationships.h"
#include "LCE/Simulation/Simulation.h"
#include "LCE/Simulation/SimulationEvents.h"
#include "LCE/Simulation/Substrate/WorldTime.h"

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
    bool SleepCycleTest();
    bool GossipTest();
    bool ArcsTest();
    bool BirthTest();
    bool NamesTest();
    bool DialogueTest();
    bool RowsTest();
    bool FightsTest();
    bool SocietyTest();
    bool KinTest();
    bool CoSaveV7Test();
    bool MidOutbreakSaveTest();
    bool IllnessTest();
    bool TradeLedgerTest();
    bool StipendTest();
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
    Run("SleepCycleTest", TLC::Tests::SleepCycleTest);
    Run("GossipTest", TLC::Tests::GossipTest);
    Run("ArcsTest", TLC::Tests::ArcsTest);
    Run("BirthTest", TLC::Tests::BirthTest);
    Run("NamesTest", TLC::Tests::NamesTest);
    Run("DialogueTest", TLC::Tests::DialogueTest);
    Run("RowsTest", TLC::Tests::RowsTest);
    Run("FightsTest", TLC::Tests::FightsTest);
    Run("SocietyTest", TLC::Tests::SocietyTest);
    Run("KinTest", TLC::Tests::KinTest);
    Run("CoSaveV7Test", TLC::Tests::CoSaveV7Test);
    Run("MidOutbreakSaveTest", TLC::Tests::MidOutbreakSaveTest);
    Run("IllnessTest", TLC::Tests::IllnessTest);
    Run("TradeLedgerTest", TLC::Tests::TradeLedgerTest);
    Run("StipendTest", TLC::Tests::StipendTest);

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
        // empty bond section, v7 with a pair of conflict gates — the
        // last days they rowed and fought, so a feud stays a once-a-day
        // scene across save/load.
        const auto record = TLC::CoSave::Encode(
            snapshot, 0x5EEDC0DEull,
            { { 0x000250FEu, 0x0001A4DAu } },
            { { 0x000250FEu, 0x0001A4DBu,
                static_cast<std::uint32_t>(
                    TLC::Bonds::BondKind::Enemy),
                12 } },
            { { 0x00012345u, 0x00012346u, 11, 12 } },
            { { 0x0000000Du, 17 } },
            { { 0x000250FEu, 4 } });

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
        std::vector<TLC::CoSave::ConflictGatePair> gates;
        std::vector<TLC::CoSave::BurialEntry> burials;
        std::vector<TLC::CoSave::MedicineStockPair> medicineStock;

        if (!TLC::CoSave::Decode(
                record, decoded, rngState, stalls, bonds, gates, burials, medicineStock)
            || rngState != 0x5EEDC0DEull
            || bonds.size() != 1
            || bonds[0].Kind != static_cast<std::uint32_t>(
                TLC::Bonds::BondKind::Enemy))
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

        // The v7 gate section round-trips exactly: the same pair, the
        // same last days of words and blows.
        if (gates.size() != 1
            || gates[0].FormA != 0x00012345u
            || gates[0].FormB != 0x00012346u
            || gates[0].RowDay != 11
            || gates[0].FightDay != 12)
        {
            return false;
        }

        // The v8 burial section round-trips exactly: the same dead, the
        // same day they died — the mourning window keeps ticking across
        // save/load.
        if (burials.size() != 1
            || burials[0].FormId != 0x0000000Du
            || burials[0].DiedDay != 17)
        {
            return false;
        }

        // The v9 medicine-stock section round-trips exactly: the shelf
        // at the market has 4 doses left — not the day's full stock — so
        // a stall that sold out stays sold out across save/load until
        // the next market day refills it.
        if (medicineStock.size() != 1
            || medicineStock[0].MarketFormId != 0x000250FEu
            || medicineStock[0].Stock != 4)
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
            std::vector<TLC::CoSave::ConflictGatePair> gates;
        std::vector<TLC::CoSave::BurialEntry> burials;
        std::vector<TLC::CoSave::MedicineStockPair> medicineStock;

            if (TLC::CoSave::Decode(
                    truncated, bad, rngState, stalls, bonds, gates, burials, medicineStock))
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
            std::vector<TLC::CoSave::ConflictGatePair> gates;
        std::vector<TLC::CoSave::BurialEntry> burials;
        std::vector<TLC::CoSave::MedicineStockPair> medicineStock;

            if (TLC::CoSave::Decode(
                    badVersion, bad, rngState, stalls, bonds, gates, burials, medicineStock))
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
            std::vector<TLC::CoSave::ConflictGatePair> gates;
        std::vector<TLC::CoSave::BurialEntry> burials;
        std::vector<TLC::CoSave::MedicineStockPair> medicineStock;

            if (!TLC::CoSave::Decode(
                    unknownName, migrated, rngState, stalls, bonds, gates, burials, medicineStock))
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
            std::vector<TLC::CoSave::ConflictGatePair> gates;
        std::vector<TLC::CoSave::BurialEntry> burials;
        std::vector<TLC::CoSave::MedicineStockPair> medicineStock;

            if (!TLC::CoSave::Decode(
                    writer.Bytes, decoded, rngState, stalls, bonds, gates, burials, medicineStock)
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

            // A version one past this build — refuse, never half-apply.
            // (Self-maintaining: when kRecordVersion bumps, this stays a
            // genuinely future version instead of silently matching the
            // current one, which is what happened when 6 caught up to the
            // original hardcoded 6.)
            writer.U32(TLC::CoSave::kRecordVersion + 1);
            writer.U32(0);
            writer.U32(0);

            RegistrySnapshot bad;
            std::uint64_t rngState = 0;
            std::vector<TLC::CoSave::StallKeeperPair> stalls;
            std::vector<TLC::CoSave::BondPair> bonds;
            std::vector<TLC::CoSave::ConflictGatePair> gates;
        std::vector<TLC::CoSave::BurialEntry> burials;
        std::vector<TLC::CoSave::MedicineStockPair> medicineStock;

            if (TLC::CoSave::Decode(
                    writer.Bytes, bad, rngState, stalls, bonds, gates, burials, medicineStock))
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
            std::vector<TLC::CoSave::ConflictGatePair> gates;
        std::vector<TLC::CoSave::BurialEntry> burials;
        std::vector<TLC::CoSave::MedicineStockPair> medicineStock;

            if (!TLC::CoSave::Decode(
                    writer.Bytes, decoded, rngState, stalls, bonds, gates, burials, medicineStock)
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
            std::vector<TLC::CoSave::ConflictGatePair> gates;
        std::vector<TLC::CoSave::BurialEntry> burials;
        std::vector<TLC::CoSave::MedicineStockPair> medicineStock;

            if (!TLC::CoSave::Decode(
                    writer.Bytes, decoded, rngState, stalls, bonds, gates, burials, medicineStock)
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
            }            // Empty census — no workshops.
            if (NearestWorkshop(0.0f, 0.0f, {}, kMarketRadius) != 0)
            {
                return false;
            }

        }

        //-------------------------------------------------------------------------
        // NearestVendor (0.7.4 Trade with anyone) — the same spatial
        // rule for the sellers: the nearest within walking distance, 0
        // when none is in range. Pure, like NearestWorkshop.
        //-------------------------------------------------------------------------
        {
            // Three sellers: Carla on the road west (200,0), a market
            // stall-keeper east (1000,0), a trader far north (60000,0).
            const std::vector<VendorPosition> vendors{
                { 0x0001F25E, 200.0f, 0.0f },
                { 0x0002C1B0, 1000.0f, 0.0f },
                { 0x0003A4C2, 60000.0f, 0.0f } };

            // A settler near the road remembers the closest seller.
            if (NearestVendor(100.0f, 0.0f, vendors, kMarketRadius)
                != 0x0001F25E)
            {
                return false;
            }

            // Closer to the market stall, the stall-keeper wins.
            if (NearestVendor(900.0f, 0.0f, vendors, kMarketRadius)
                != 0x0002C1B0)
            {
                return false;
            }

            // Near the far trader, the far trader is the nearest seller.
            if (NearestVendor(59000.0f, 0.0f, vendors, kMarketRadius)
                != 0x0003A4C2)
            {
                return false;
            }

            // A mind at the edge of the world knows none of them — the
            // far trader is 60 km out, beyond the walking radius.
            if (NearestVendor(-50000.0f, 0.0f, vendors, kMarketRadius)
                != 0)
            {
                return false;
            }

            // Empty census — no sellers.
            if (NearestVendor(0.0f, 0.0f, {}, kMarketRadius) != 0)
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
        // 5b. The monogamy cap (0.7.5 field find): one spouse each. A
        //     pair that would cross the spouse line while either side
        //     already holds a spouse bond with someone else caps at
        //     sweetheart; an existing marriage is never broken.
        //-------------------------------------------------------------------------
        {
            const auto a2 = EntityId{ 201 };
            const auto b2 = EntityId{ 202 };
            const auto c2 = EntityId{ 203 };
            const auto d2 = EntityId{ 204 };

            BondMap bonds;

            // a and b marry.
            ApplyPair(
                bonds, PairKey(a2, b2), 0.9f, 0.9f, t, 1, nullptr);

            if (bonds[PairKey(a2, b2)].Kind != BondKind::Spouse)
            {
                return false;
            }

            // a's heart warms to c — but a is already married: capped.
            ApplyPair(
                bonds, PairKey(a2, c2), 0.9f, 0.9f, t, 2, nullptr);

            if (bonds[PairKey(a2, c2)].Kind != BondKind::Sweetheart)
            {
                return false;
            }

            // b's heart warms to c too — same cap, b is married.
            ApplyPair(
                bonds, PairKey(b2, c2), 0.9f, 0.9f, t, 3, nullptr);

            if (bonds[PairKey(b2, c2)].Kind != BondKind::Sweetheart)
            {
                return false;
            }

            // The marriage stands.
            if (bonds[PairKey(a2, b2)].Kind != BondKind::Spouse)
            {
                return false;
            }

            // And a fresh heart CAN marry: d has no spouse.
            ApplyPair(
                bonds, PairKey(c2, d2), 0.9f, 0.9f, t, 4, nullptr);

            if (bonds[PairKey(c2, d2)].Kind != BondKind::Spouse)
            {
                return false;
            }
        }

        //-------------------------------------------------------------------------
        // 5c. The animal gate (0.7.5 field find): an animal is fed, not
        //     bonded. The reconcile pass skips a pair where either side
        //     is an animal — whatever the dispositions say, a dog never
        //     rows, feuds, or fights.
        //-------------------------------------------------------------------------
        {
            EntityRegistry registry;

            const auto person = registry.CreateEntity();
            const auto dog = registry.CreateEntity();

            registry.AddComponent<SpeciesTag>(
                person, SpeciesTag{ Species::Human });
            registry.AddComponent<SpeciesTag>(
                dog, SpeciesTag{ Species::Animal });

            registry.AddComponent<Relationships>(person, Relationships{
                { { dog, Relationship{ -0.9f, 0.0f } } } });
            registry.AddComponent<Relationships>(dog, Relationships{
                { { person, Relationship{ -0.9f, 0.0f } } } });

            BondMap bonds;
            std::size_t changes = 0;

            Reconcile(
                registry, t, bonds, 12,
                [&](EntityId, EntityId, BondKind, BondKind,
                    std::uint64_t)
                {
                    ++changes;
                });

            // A deep mutual dislike — and nothing forms: no feud, no
            // row, no fight. The pair is simply not a pair.
            if (!bonds.empty() || changes != 0)
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
                snapshot, 0x5EEDC0DEull, {}, savedBonds, {}, {}, {});

            RegistrySnapshot decoded;
            std::uint64_t rngState = 0;
            std::vector<TLC::CoSave::StallKeeperPair> stalls;
            std::vector<TLC::CoSave::BondPair> bonds;
            std::vector<TLC::CoSave::ConflictGatePair> gates;
        std::vector<TLC::CoSave::BurialEntry> burials;
        std::vector<TLC::CoSave::MedicineStockPair> medicineStock;

            if (!TLC::CoSave::Decode(
                    record, decoded, rngState, stalls, bonds, gates, burials, medicineStock)
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
                    static_cast<std::uint32_t>(BondKind::Friend), 1 } },
                {}, {}, {});

            std::vector<TLC::CoSave::BondPair> badBonds;
            std::vector<TLC::CoSave::ConflictGatePair> badGates;
            std::vector<TLC::CoSave::BurialEntry> badBurials;
        std::vector<TLC::CoSave::MedicineStockPair> badMedicineStock;

            if (!TLC::CoSave::Decode(
                    badRecord, decoded, rngState, stalls, badBonds, badGates, badBurials, badMedicineStock)
                || !badBonds.empty()
                || !badGates.empty()
                || !badBurials.empty())
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
            std::vector<TLC::CoSave::ConflictGatePair> migratedGates;
            std::vector<TLC::CoSave::BurialEntry> migratedBurials;
        std::vector<TLC::CoSave::MedicineStockPair> migratedMedicineStock;

            if (!TLC::CoSave::Decode(
                    legacy.Bytes, decoded, rngState, stalls,
                    migratedBonds, migratedGates, migratedBurials, migratedMedicineStock)
                || !migratedBonds.empty()
                || !migratedGates.empty()
                || !migratedBurials.empty())
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

        //-------------------------------------------------------------------------
        // 6. The one-wallet-per-mind guard (the polygamy edge): the bond
        //    layer can honestly read Spouse to two minds at once, but a
        //    second merge must never fold a third pouch into the shared
        //    wallet — InHousehold answers true for either role, and
        //    Enforce skips the second marriage instead of merging it.
        //-------------------------------------------------------------------------
        {
            EntityRegistry registry;

            const auto keeper = registry.CreateEntity();
            const auto first = registry.CreateEntity();
            const auto second = registry.CreateEntity();

            registry.AddComponent<CapPouch>(keeper, CapPouch{ 40 });
            registry.AddComponent<CapPouch>(first, CapPouch{ 60 });
            registry.AddComponent<CapPouch>(second, CapPouch{ 20 });

            Bonds::BondMap bonds;
            bonds[Bonds::PairKey(keeper, first)] =
                Bonds::PairBond{ Bonds::BondKind::Spouse, 1 };

            // The first marriage forms a household — the holder keeps
            // the merged wallet, the other member has no pouch.
            if (!FormHousehold(registry, keeper, first))
            {
                return false;
            }

            if (!InHousehold(registry, bonds, keeper)
                || !InHousehold(registry, bonds, first))
            {
                return false;   // both roles of a shared wallet
            }

            // The keeper's second marriage (the bond layer honestly says
            // Spouse to two minds). The guard lives at the callers' level
            // — Enforce skips pairs where either side is already
            // householded, so the second pouch must stay personal.
            bonds[Bonds::PairKey(keeper, second)] =
                Bonds::PairBond{ Bonds::BondKind::Spouse, 2 };

            Enforce(registry, bonds);

            const auto keeperPouch =
                registry.GetComponent<CapPouch>(keeper);
            const auto secondPouch =
                registry.GetComponent<CapPouch>(second);

            if (!keeperPouch || keeperPouch->Caps != 100
                || !secondPouch || secondPouch->Caps != 20)
            {
                return false;   // 40+60 merged; 20 stays personal
            }

            // And the roles read right: the keeper and the first spouse
            // are the two halves of the shared wallet (both in the
            // household); the second spouse, still holding a personal
            // pouch, is not.
            if (!InHousehold(registry, bonds, keeper)
                || InHousehold(registry, bonds, second)
                || !InHousehold(registry, bonds, first))
            {
                return false;
            }
        }

        return true;
    }

    bool SleepCycleTest()
    {
        using namespace TLC;

        //-------------------------------------------------------------------------
        // 1. RestRecovery — a nap restores the needs it fixes: Fatigue,
        //    Safety, and Comfort, each at rate × delta, capped at 1.0
        //    (fully rested). The sleep cycle's raw material: the need
        //    loop only decays, Rest is the recovery side. Social is
        //    deliberately untouched — that is the future Socialize
        //    stone's recovery, not a nap's.
        //-------------------------------------------------------------------------
        {
            auto needs = SeededNeeds(Species::Human);

            // Drain every need a nap restores: 0.2/s over 5 s.
            (void)RestRecovery(needs, 0.2f, -5.0f);

            const auto find = [&needs](LCE::Simulation::NeedType a_type)
            {
                return std::find_if(
                    needs.List.begin(), needs.List.end(),
                    [a_type](const LCE::Simulation::Need& a_need)
                    {
                        return a_need.Type == a_type;
                    });
            };

            const auto fatigue = find(LCE::Simulation::NeedType::Fatigue);
            const auto safety = find(LCE::Simulation::NeedType::Safety);
            const auto comfort = find(LCE::Simulation::NeedType::Comfort);

            if (fatigue == needs.List.end()
                || safety == needs.List.end()
                || comfort == needs.List.end())
            {
                return false;   // all seeds have these needs
            }

            if (fatigue->Value > 0.001f
                || safety->Value > 0.001f
                || comfort->Value > 0.001f)
            {
                return false;   // drained
            }

            const auto recovered = RestRecovery(needs, 0.2f, 4.0f);

            if (recovered < 0.79f || recovered > 0.81f)
            {
                return false;   // fatigue: 0.8 after 4 s at 0.2/s
            }

            if (safety->Value < 0.79f || safety->Value > 0.81f
                || comfort->Value < 0.79f || comfort->Value > 0.81f)
            {
                return false;   // safety and comfort recover too
            }

            const auto napped = RestRecovery(needs, 0.2f, 4.0f);

            if (napped != 1.0f)
            {
                return false;   // capped at full
            }

            const auto social = find(LCE::Simulation::NeedType::Social);

            if (social != needs.List.end() && social->Value < 0.99f)
            {
                return false;   // a nap neither restores nor drains social
            }
        }

        //-------------------------------------------------------------------------
        // 2. The loop closes — the discovery made real. A fed mind with
        //    drained Fatigue decides Rest (fatigue is the most urgent
        //    need); while resting it recovers; once rested, hunger is
        //    the most urgent need again and the mind decides MoveTo —
        //    it walks to market once more. Without the recovery the
        //    mind would park in Rest forever (the bug the 24h-market
        //    test exposed: only Hunger is ever restored, and only on
        //    the meal).
        //-------------------------------------------------------------------------
        {
            EntityRegistry registry;

            const auto settler = registry.CreateEntity();
            const auto market = registry.CreateEntity();

            registry.AddComponent<Needs>(settler, SeededNeeds(Species::Human));
            registry.AddComponent<Memory>(settler, Memory{});
            registry.AddComponent<FormRef>(market, FormRef{ kMarketFormId });

            SeedMarketMemory(registry, market);

            // The meal: hunger full, fatigue drained — the post-meal
            // state that parked every fed mind (fatigue 0 from the
            // long session, hunger 1.0 from the bench).
            auto needs = registry.GetComponent<Needs>(settler);

            if (!needs)
            {
                return false;
            }

            for (auto& need : needs->List)
            {
                if (need.Type == LCE::Simulation::NeedType::Hunger)
                {
                    need.Value = 1.0f;
                }

                if (need.Type == LCE::Simulation::NeedType::Fatigue)
                {
                    need.Value = 0.0f;
                }
            }

            Update(registry, 1.0);

            const auto intent = registry.GetComponent<Intent>(settler);

            if (!intent || intent->Action != ActionType::Rest)
            {
                return false;   // drained fatigue -> Rest
            }

            // The sleep cycle: rest recovers fatigue, and the next
            // Update decides from the rested mind.
            (void)RestRecovery(*needs, 0.2f, 5.0f);   // a full nap

            Update(registry, 1.0);

            const auto again = registry.GetComponent<Intent>(settler);

            if (!again || again->Action != ActionType::MoveTo)
            {
                return false;   // rested -> hungry -> walks to market
            }

            if (again->Target != market)
            {
                return false;   // to the remembered market
            }
        }

        //-------------------------------------------------------------------------
        // 3. The parked-mind rescue (the review-pass discovery): a mind
        //    whose Safety is most urgent with no remembered threat gets
        //    nullopt from the engine's Decide — you can't flee from
        //    nothing — so its intent is removed and it parks forever,
        //    invisible to any intent-keyed recovery pass. The recovery
        //    therefore reads the needs: a mind with no intent whose most
        //    urgent need is Safety (or Fatigue) is resting, recovers,
        //    and rejoins the decide loop.
        //-------------------------------------------------------------------------
        {
            EntityRegistry registry;

            const auto settler = registry.CreateEntity();

            registry.AddComponent<Needs>(settler, SeededNeeds(Species::Human));

            auto needs = registry.GetComponent<Needs>(settler);

            if (!needs)
            {
                return false;
            }

            for (auto& need : needs->List)
            {
                if (need.Type == LCE::Simulation::NeedType::Hunger)
                {
                    need.Value = 1.0f;
                }

                if (need.Type == LCE::Simulation::NeedType::Fatigue)
                {
                    need.Value = 0.2f;
                }

                if (need.Type == LCE::Simulation::NeedType::Safety)
                {
                    need.Value = 0.0f;   // most urgent — and no threat
                }
            }

            const auto urgent = MostUrgentNeed(*needs);

            if (!urgent.has_value()
                || *urgent != LCE::Simulation::NeedType::Safety)
            {
                return false;   // the setup: Safety is the urgent need
            }

            // The engine parks the mind: nullopt, no intent.
            Update(registry, 1.0);

            if (registry.GetComponent<Intent>(settler))
            {
                return false;   // parked — no decision, no intent
            }

            // The recovery pass (reads needs, not intents) sees the
            // most-urgent Safety as rest and recovers it — a nap.
            (void)RestRecovery(*needs, 0.2f, 6.0f);

            // The next Update decides again — the mind is un-parked.
            Update(registry, 1.0);

            if (!registry.GetComponent<Intent>(settler))
            {
                return false;   // un-parked — a decision exists again
            }
        }

        return true;
    }

    bool GossipTest()
    {
        using namespace Gossip;

        //-------------------------------------------------------------------------
        // 1. Spread — the settlement hears. Three minds with memory; a
        //    bond between A and B means C hears about both, A hears
        //    about B, B hears about A — and nobody gossips to itself.
        //-------------------------------------------------------------------------
        {
            EntityRegistry registry;

            const auto a = registry.CreateEntity();
            const auto b = registry.CreateEntity();
            const auto c = registry.CreateEntity();

            registry.AddComponent<Memory>(a, Memory{});
            registry.AddComponent<Memory>(b, Memory{});
            registry.AddComponent<Memory>(c, Memory{});

            const auto spread = SpreadBond(
                registry, a, b, InteractionKind::Social, 3);

            if (spread != 4)
            {
                return false;   // A→B, B→A, C→A, C→B
            }

            const auto hearsAbout = [&](EntityId who, EntityId subject)
            {
                const auto memory = registry.GetComponent<Memory>(who);

                for (const auto& event : memory->Events)
                {
                    if (event.Other == subject
                        && event.Kind == InteractionKind::Social)
                    {
                        return true;
                    }
                }

                return false;
            };

            if (!hearsAbout(a, b) || !hearsAbout(b, a)
                || !hearsAbout(c, a) || !hearsAbout(c, b))
            {
                return false;
            }

            // Nobody heard about themselves — the subject does not
            // gossip to itself.
            const auto memory = registry.GetComponent<Memory>(c);

            for (const auto& event : memory->Events)
            {
                if (event.Other == c)
                {
                    return false;
                }
            }
        }

        //-------------------------------------------------------------------------
        // 2. Spread — an invalid subject spreads nothing; the day is
        //    stamped onto the fact.
        //-------------------------------------------------------------------------
        {
            EntityRegistry registry;

            const auto a = registry.CreateEntity();
            registry.AddComponent<Memory>(a, Memory{});

            if (Spread(registry, {}, InteractionKind::Death, 9) != 0)
            {
                return false;
            }

            const auto b = registry.CreateEntity();
            registry.AddComponent<Memory>(b, Memory{});

            if (Spread(registry, b, InteractionKind::Death, 9) != 1)
            {
                return false;
            }

            const auto memory = registry.GetComponent<Memory>(a);
            const auto& event = memory->Events.back();

            if (event.Other != b || event.Kind != InteractionKind::Death
                || event.Day != 9 || event.Weight != WorldFacts::kFactWeight)
            {
                return false;
            }
        }

        return true;
    }

    bool ArcsTest()
    {
        using namespace Arcs;

        //-------------------------------------------------------------------------
        // 1. Grieving — a mind with a fresh death memory of someone it
        //    loved (disposition at/above the friend line) is grieving;
        //    a stranger's death, or a faded one, is not.
        //-------------------------------------------------------------------------
        {
            EntityRegistry registry;

            const auto mourner = registry.CreateEntity();
            const auto loved = registry.CreateEntity();
            const auto stranger = registry.CreateEntity();

            registry.AddComponent<Needs>(mourner, Needs{});

            Memory memory;
            memory.Events.push_back(MemoryEvent{
                loved, InteractionKind::Death, 1.0f, 1 });
            memory.Events.push_back(MemoryEvent{
                stranger, InteractionKind::Death, 1.0f, 1 });
            registry.AddComponent<Memory>(mourner, std::move(memory));

            Relationships relationships;
            relationships.ByEntity[loved] =
                Relationship{ 0.5f, 0.0f };   // loved — above the friend line
            relationships.ByEntity[stranger] =
                Relationship{ 0.1f, 0.0f };   // barely known
            registry.AddComponent<Relationships>(
                mourner, std::move(relationships));

            if (!Grieving(registry, mourner, 1, 0.5f))
            {
                return false;   // loved the dead — grief
            }

            // A faded memory (below the fresh line) is not a fresh grief.
            if (Grieving(registry, mourner, 1, 1.01f))
            {
                return false;
            }
        }

        //-------------------------------------------------------------------------
        // 2. ApplyGrief — a grieving mind's Social need drains at the
        //    grief rate; the fresh pairs are returned exactly once.
        //-------------------------------------------------------------------------
        {
            EntityRegistry registry;

            const auto mourner = registry.CreateEntity();
            const auto loved = registry.CreateEntity();

            registry.AddComponent<Needs>(
                mourner, SeededNeeds(Species::Human));

            Memory memory;
            memory.Events.push_back(MemoryEvent{
                loved, InteractionKind::Death, 1.0f, 1 });
            registry.AddComponent<Memory>(mourner, std::move(memory));

            Relationships relationships;
            relationships.ByEntity[loved] = Relationship{ 0.5f, 0.0f };
            registry.AddComponent<Relationships>(
                mourner, std::move(relationships));

            const auto fresh =
                ApplyGrief(registry, 1, 0.01f, 10.0f, 0.9f);

            if (fresh.size() != 1 || fresh[0].first != mourner
                || fresh[0].second != loved)
            {
                return false;
            }

            // Re-read the component the registry actually holds — the
            // module modified it in place.
            const auto& needs =
                *registry.GetComponent<Needs>(mourner);

            const auto social = std::find_if(
                needs.List.begin(), needs.List.end(),
                [](const Need& a_need)
                {
                    return a_need.Type == NeedType::Social;
                });

            if (social == needs.List.end()
                || social->Value > 1.0f - 0.09f)
            {
                return false;   // drained ~0.1 at 0.01/s × 10 s
            }

            // The next pass: the memory still sits above the fresh line,
            // but the mind is only announced once per tick by the
            // adapter's log — the module simply reports what is fresh.
            if (ApplyGrief(registry, 1, 0.01f, 1.0f, 0.9f).size() != 1)
            {
                return false;
            }
        }

        //-------------------------------------------------------------------------
        // 3. Mediate — a liked mediator cools the feud (the pair warms
        //    toward zero); an unloved meddler is told off (the feud
        //    holds, they cool toward the meddler).
        //-------------------------------------------------------------------------
        {
            EntityRegistry registry;

            const auto enemyA = registry.CreateEntity();
            const auto enemyB = registry.CreateEntity();
            const auto mediator = registry.CreateEntity();

            // The mediator has heard of both sides — the settlement
            // knows its own feuds.
            Memory mediatorMemory;
            mediatorMemory.Events.push_back(MemoryEvent{
                enemyA, InteractionKind::Social, 1.0f, 1 });
            mediatorMemory.Events.push_back(MemoryEvent{
                enemyB, InteractionKind::Social, 1.0f, 1 });
            registry.AddComponent<Memory>(
                mediator, std::move(mediatorMemory));

            // Both sides like the mediator (pull > 0) — the cooling
            // attempt should land.
            Relationships relA;
            relA.ByEntity[enemyB] = Relationship{ -0.7f, 0.0f };
            relA.ByEntity[mediator] = Relationship{ 0.4f, 0.0f };
            registry.AddComponent<Relationships>(enemyA, std::move(relA));

            Relationships relB;
            relB.ByEntity[enemyA] = Relationship{ -0.7f, 0.0f };
            relB.ByEntity[mediator] = Relationship{ 0.4f, 0.0f };
            registry.AddComponent<Relationships>(enemyB, std::move(relB));

            const auto attempts = Mediate(
                registry, { { enemyA, enemyB } });

            if (attempts.size() != 1 || !attempts[0].Cooled
                || attempts[0].Mediator != mediator)
            {
                return false;
            }

            const auto afterA =
                registry.GetComponent<Relationships>(enemyA);
            const auto afterB =
                registry.GetComponent<Relationships>(enemyB);

            if (afterA->ByEntity[enemyB].Disposition
                    != -0.7f + 0.05f
                || afterB->ByEntity[enemyA].Disposition
                    != -0.7f + 0.05f)
            {
                return false;   // the feud cooled a step
            }

            // The unloved meddler: pull <= 0 — the feud holds and the
            // pair cools toward the meddler.
            EntityRegistry registry2;

            const auto cA = registry2.CreateEntity();
            const auto cB = registry2.CreateEntity();
            const auto meddler = registry2.CreateEntity();

            Memory meddlerMemory;
            meddlerMemory.Events.push_back(MemoryEvent{
                cA, InteractionKind::Social, 1.0f, 1 });
            meddlerMemory.Events.push_back(MemoryEvent{
                cB, InteractionKind::Social, 1.0f, 1 });
            registry2.AddComponent<Memory>(
                meddler, std::move(meddlerMemory));

            Relationships relCA;
            relCA.ByEntity[cB] = Relationship{ -0.7f, 0.0f };
            relCA.ByEntity[meddler] = Relationship{ 0.0f, 0.0f };
            registry2.AddComponent<Relationships>(cA, std::move(relCA));

            Relationships relCB;
            relCB.ByEntity[cA] = Relationship{ -0.7f, 0.0f };
            relCB.ByEntity[meddler] = Relationship{ 0.0f, 0.0f };
            registry2.AddComponent<Relationships>(cB, std::move(relCB));

            const auto attempts2 = Mediate(
                registry2, { { cA, cB } });

            if (attempts2.size() != 1 || attempts2[0].Cooled)
            {
                return false;   // nobody liked the meddler
            }

            const auto afterCA =
                registry2.GetComponent<Relationships>(cA);
            const auto afterCB =
                registry2.GetComponent<Relationships>(cB);

            if (afterCA->ByEntity[meddler].Disposition != -0.05f
                || afterCB->ByEntity[meddler].Disposition != -0.05f)
            {
                return false;   // the pair cooled toward the meddler
            }
        }

        //-------------------------------------------------------------------------
        // 4. Gossip makes mediation reachable (the feud-start fix,
        //    2026-08-11): a third mind that only knows the pair through
        //    Gossip::SpreadBond — the feud-start spread — is found as
        //    the mediator. Without the spread, no mediator exists and
        //    Mediate returns nothing (the in-game bug: gossip from the
        //    rival stage had faded by the next day's pass, so the daily
        //    mediation could never find anyone).
        //-------------------------------------------------------------------------
        {
            EntityRegistry registry;

            const auto enemyA = registry.CreateEntity();
            const auto enemyB = registry.CreateEntity();
            const auto neighbour = registry.CreateEntity();

            // The pair exists as relationships; the neighbour is a mind
            // with an empty memory — it knows nothing yet.
            registry.AddComponent<Relationships>(
                enemyA, Relationships{});
            registry.AddComponent<Relationships>(
                enemyB, Relationships{});
            registry.AddComponent<Memory>(neighbour, Memory{});

            // The feud-start spread (what OnBondChange now does on the
            // Enemy crossing): every other mind hears of both sides.
            Gossip::SpreadBond(
                registry, enemyA, enemyB,
                InteractionKind::Social, 1);

            // The neighbour now knows both — Mediate finds it, and with
            // nobody liking anyone (pull 0) it is told off, not cool.
            const auto attempts = Mediate(
                registry, { { enemyA, enemyB } });

            if (attempts.size() != 1
                || attempts[0].Mediator != neighbour
                || attempts[0].Cooled)
            {
                return false;
            }
        }

        return true;
    }

    bool BirthTest()
    {
        using namespace Birth;

        const NeedRates rates;   // the code's own defaults

        //-------------------------------------------------------------------------
        // 1. Create — a child mind: sim-only (no FormRef), fed from
        //    full needs, warm to both parents — and the parents know
        //    their child.
        //-------------------------------------------------------------------------
        {
            EntityRegistry registry;

            const auto parentA = registry.CreateEntity();
            const auto parentB = registry.CreateEntity();

            // The parents are minds — they carry relationships (the
            // component Create warms with the child).
            registry.AddComponent<FormRef>(parentA, FormRef{ 0x1234 });
            registry.AddComponent<FormRef>(parentB, FormRef{ 0x5678 });
            registry.AddComponent<Relationships>(parentA, Relationships{});
            registry.AddComponent<Relationships>(parentB, Relationships{});

            const auto child = Create(registry, parentA, parentB, rates);

            if (!child.IsValid() || child == parentA || child == parentB)
            {
                return false;
            }

            const auto species =
                registry.GetComponent<SpeciesTag>(child);
            const auto needs = registry.GetComponent<Needs>(child);
            const auto memory = registry.GetComponent<Memory>(child);
            const auto goals = registry.GetComponent<Goals>(child);
            const auto form = registry.GetComponent<FormRef>(child);

            if (!species || species->Value != Species::Child
                || !needs || !memory || !goals || form != nullptr)
            {
                return false;   // a sim-only child mind
            }

            for (const auto& need : needs->List)
            {
                if (need.Value < 1.0f - 0.001f)
                {
                    return false;   // born satisfied
                }
            }

            const auto childRels =
                registry.GetComponent<Relationships>(child);

            if (!childRels
                || childRels->ByEntity[parentA].Disposition < 0.4f
                || childRels->ByEntity[parentB].Disposition < 0.4f)
            {
                return false;   // the child loves both parents
            }

            const auto relA =
                registry.GetComponent<Relationships>(parentA);
            const auto relB =
                registry.GetComponent<Relationships>(parentB);

            if (!relA || relA->ByEntity[child].Disposition < 0.5f
                || !relB || relB->ByEntity[child].Disposition < 0.5f)
            {
                return false;   // the parents know their child
            }
        }

        //-------------------------------------------------------------------------
        // 2. Create — invalid parents birth nothing.
        //-------------------------------------------------------------------------
        {
            EntityRegistry registry;

            const auto parentA = registry.CreateEntity();

            if (Create(registry, parentA, {}, rates).IsValid())
            {
                return false;
            }
        }

        //-------------------------------------------------------------------------
        // 3. FeedChildren — a sim-only child is fed by the household;
        //    a child with a game form is a real, walkable mind and is
        //    left to the market.
        //-------------------------------------------------------------------------
        {
            EntityRegistry registry;

            const auto child = registry.CreateEntity();
            auto childNeeds = SeededNeeds(Species::Child);
            childNeeds.List[0].Value = 0.5f;   // hungry
            registry.AddComponent<Needs>(child, childNeeds);
            registry.AddComponent<SpeciesTag>(
                child, SpeciesTag{ Species::Child });

            const auto walker = registry.CreateEntity();
            registry.AddComponent<Needs>(
                walker, SeededNeeds(Species::Child));
            registry.AddComponent<SpeciesTag>(
                walker, SpeciesTag{ Species::Child });
            registry.AddComponent<FormRef>(walker, FormRef{ 0x9999 });

            const auto fed = FeedChildren(registry, 1.0f);

            if (fed != 1)
            {
                return false;   // only the sim-only child ate
            }

            const auto hungerOf = [](const Needs& a_needs)
            {
                for (const auto& need : a_needs.List)
                {
                    if (need.Type == NeedType::Hunger)
                    {
                        return need.Value;
                    }
                }

                return -1.0f;
            };

            // Fed toward full at 0.2/s: 0.5 + 0.2 = 0.7. The walker (a
            // real mind with a form) is left to the market, untouched
            // at 1.0.
            if (hungerOf(*registry.GetComponent<Needs>(child))
                    != 0.5f + 0.2f * 1.0f
                || hungerOf(*registry.GetComponent<Needs>(walker))
                    != 1.0f)
            {
                return false;
            }
        }

        return true;
    }

    bool NamesTest()
    {
        using namespace TLC::Names;

        // IsGenericName: the game's placeholders are generic; a real
        // name is not. Empty counts (some refs read an empty full-name).
        if (!IsGenericName(""))
        {
            return false;
        }

        if (!IsGenericName("Settler") || !IsGenericName("settler")
            || !IsGenericName("Workshop Worker")
            || !IsGenericName("Worker"))   // the Sanctuary work crews
        {
            return false;
        }

        if (IsGenericName("Sturges"))
        {
            return false;
        }

        // Species-aware: a plain species word is generic for an animal
        // (an owned dog gets a real name) but a proper creature name
        // keeps itself; a species word is NOT generic for a human.
        if (!IsGenericName("Dog", Species::Animal)
            || !IsGenericName("Brahmin", Species::Animal)
            || !IsGenericName("Cat", Species::Animal))
        {
            return false;
        }

        if (IsGenericName("Dogmeat", Species::Animal)
            || IsGenericName("Rex", Species::Animal)
            || IsGenericName("Dog", Species::Human))
        {
            return false;
        }

        // The Red Rocket resident is a role label too — owned, it gets
        // a real name.
        if (!IsGenericName("Junkyard Dog", Species::Animal)
            || IsGenericName("Dogmeat", Species::Animal))
        {
            return false;
        }

        // A role label is not a placeholder either (0.7.3 Stone 1):
        // "Provisioner" is a title, not a name — the sim keeps the
        // role and adds the person ("Provisioner Cole") so memory can
        // tell two provisioners apart. So IsGenericName stays false
        // for a role (the role branch owns it), while the workshop
        // placeholders stay generic.
        if (IsGenericName("Provisioner")
            || IsGenericName("Guard")
            || !IsGenericName("Settler") || !IsGenericName("Workshop Worker"))
        {
            return false;
        }

        // IsRoleName: the role list is recognized case-insensitively
        // and returns the canonical word; a real name is not a role;
        // and an animal's "Junkyard Dog" is a species word, not a role.
        if (IsRoleName("Provisioner") != "Provisioner"
            || IsRoleName("guard") != "Guard"
            || IsRoleName("MINUTEMAN") != "Minuteman"
            || IsRoleName("Caravan Guard") != "Caravan Guard"
            || IsRoleName("Caravan Worker") != "Caravan Worker"
            || !IsRoleName("Sturges").empty()
            || !IsRoleName("Junkyard Dog").empty())
        {
            return false;
        }

        const auto pool = DefaultPool();

        // The gendered pools are disjoint — the same id draws different
        // names for a man and a woman.
        const auto id = EntityId{ 42 };
        const auto male = GenerateName(id, pool, Gender::Male);
        const auto female = GenerateName(id, pool, Gender::Female);

        if (male == female)
        {
            return false;
        }

        // Determinism: the same id draws the same name every time.
        if (GenerateName(id, pool, Gender::Male) != male)
        {
            return false;
        }

        // A person's name is first + family.
        const auto space = male.find(' ');

        if (space == std::string::npos || space + 1 >= male.size())
        {
            return false;
        }

        // FamilyOf: "Vera Hart" -> "Hart"; a single word -> "".
        if (FamilyOf("Vera Hart") != "Hart"
            || !FamilyOf("Rex").empty())
        {
            return false;
        }

        // Animals draw from their own pool and carry no family name.
        const auto animal = GenerateAnimalName(EntityId{ 7 }, pool);

        if (animal.find(' ') != std::string::npos)
        {
            return false;
        }

        // Dedup: a pre-claimed name is never handed out again, and the
        // first free draw is deterministic.
        std::unordered_set<std::string> used;
        const auto claimed = GenerateName(EntityId{ 3 }, pool, Gender::Female);
        used.insert(claimed);
        const auto first = GenerateUnique(
            used, EntityId{ 3 }, pool, Gender::Female);

        if (used.find(first) == used.end() || first == claimed)
        {
            return false;
        }

        // ChildName: a first name plus the family name.
        const auto child = ChildName(
            "Vance", EntityId{ 9 }, pool, Gender::Female);

        if (child.find("Vance") == std::string::npos)
        {
            return false;
        }

        // A role name keeps its title and adds a gendered first name —
        // "Provisioner Cole", never a family name — and dedups like
        // any name: a claimed "Provisioner Cole" steps to the next
        // free draw.
        const auto roleName = GenerateRoleName(
            "Provisioner", EntityId{ 21 }, pool, Gender::Male);

        if (roleName.rfind("Provisioner ", 0) != 0
            || roleName.find(' ', 12) != std::string::npos)
        {
            return false;
        }

        std::unordered_set<std::string> usedRoles;
        usedRoles.insert(roleName);
        const auto nextRole = GenerateUniqueRole(
            usedRoles, "Provisioner", EntityId{ 21 }, pool, Gender::Male);

        if (nextRole == roleName
            || nextRole.rfind("Provisioner ", 0) != 0)
        {
            return false;
        }

        // HasRolePrefix: the title must lead the name — a role-named
        // mind is recognised, a bare role word or a plain full name is
        // not.
        if (!HasRolePrefix("Provisioner Cole", "Provisioner")
            || HasRolePrefix("Cole Hart", "Provisioner")
            || HasRolePrefix("Provisioner", "Provisioner"))
        {
            return false;
        }

        // The INI pools: a provided list replaces its default, a missing
        // list keeps its default, and a broken (empty) list keeps the
        // default too.
        LCE::Config::Configuration config;
        config.Set("names.first.male", "Zeke, Milo");
        config.Set("names.first.female", "Ada");
        config.Set("names.first.animal", "");

        const auto fromIni = PoolFrom(config);

        if (fromIni.MaleFirsts
                != std::vector<std::string>{ "Zeke", "Milo" }
            || fromIni.FemaleFirsts
                != std::vector<std::string>{ "Ada" }
            || fromIni.AnimalFirsts != pool.AnimalFirsts
            || fromIni.Lasts != pool.Lasts)
        {
            return false;
        }

        // A name drawn from the INI pool uses the INI's lists.
        const auto zeke = GenerateName(
            EntityId{ 5 }, fromIni, Gender::Male);

        if (zeke.rfind("Zeke ", 0) != 0 && zeke.rfind("Milo ", 0) != 0)
        {
            return false;
        }

        // The news feed: capped, ordered, and rotatable — the radio's
        // story.
        TLC::News::NewsFeed feed;
        feed.Cap = 3;
        feed.Add("first");
        feed.Add("second");
        feed.Add("third");
        feed.Add("fourth");   // pushes out "first"

        if (feed.Lines.size() != 3 || feed.NextLine() != "second")
        {
            return false;
        }

        return true;
    }

    bool DialogueTest()
    {
        using namespace TLC::Dialogue;

        const auto pool = DefaultPool();

        // Every pool is populated — the author's starter sets are all
        // non-empty, so a mind always has something to say.
        if (pool.Greet.empty() || pool.Gossip.empty() || pool.Row.empty()
            || pool.Trade.empty() || pool.Family.empty()
            || pool.Grief.empty() || pool.Fight.empty()
            || pool.Feud.empty())
        {
            return false;
        }

        // The author's five-line row ramp is the pool's core: the
        // complaint, the dare, the taunt, the break point, and the
        // escalation, in order.
        const auto& row = pool.Row;

        if (row.size() < 5 || row[0] != "You ripped me off"
            || row[1] != "You do that again, I dare you"
            || row[2] != "Go on, one more time"
            || row[3] != "I've had it with you"
            || row[4] != "Want some? Let's go")
        {
            return false;
        }

        // Determinism: the same mind, the same day, the same pool — the
        // same line, every time.
        const auto id = EntityId{ 42 };
        const auto line = Pick(pool, Pool::Trade, id, 10);

        if (line.empty() || Pick(pool, Pool::Trade, id, 10) != line)
        {
            return false;
        }

        // A new day changes the line — the world moves on, so a mind
        // does not say the same thing forever. The day is an input to
        // the seed (determinism proves it is used); across a week of
        // days a real pool of many lines cannot hold one line forever
        // (a 10-line pool across 7 days: a fixed line every day would
        // need 7 collisions in a row — astronomically unlikely).
        bool allSame = true;

        for (std::uint64_t day = 11; day < 18; ++day)
        {
            if (Pick(pool, Pool::Trade, id, day) != line)
            {
                allSame = false;
                break;
            }
        }

        if (allSame)
        {
            return false;
        }

        // Two pools do not pick in lockstep: the same mind, same day,
        // different categories are salted apart. (With large pools this
        // is near-certain; the salt guarantees it structurally.)
        const auto greet = Pick(pool, Pool::Greet, id, 10);
        const auto gossip = Pick(pool, Pool::Gossip, id, 10);
        const auto trade = Pick(pool, Pool::Trade, id, 10);

        if (greet == gossip && gossip == trade)
        {
            return false;   // all three identical — the salt failed
        }

        // The INI pools: a provided list replaces its default, a missing
        // list keeps its default, and a broken (empty) list keeps the
        // default too — same contract as the names.
        LCE::Config::Configuration config;
        config.Set("dialogue.greet", "Mornin', Peaceful day");
        config.Set("dialogue.row", "");

        const auto fromIni = PoolFrom(config);

        if (fromIni.Greet
                != std::vector<std::string>{ "Mornin'", "Peaceful day" }
            || fromIni.Row != pool.Row)
        {
            return false;
        }

        // An empty pool says nothing — silence is a safe default.
        DialoguePool silent{ pool };
        silent.Feud.clear();

        if (!Pick(silent, Pool::Feud, id, 10).empty())
        {
            return false;
        }

        return true;
    }

    bool RowsTest()
    {
        using namespace LCE::Simulation;
        using namespace TLC::Bonds;

        // 0.7.2 Rows — the verbal altercation: rivals and enemies who
        // cross paths have words. Each remembers the other wronged them
        // (the engine's Wronged channel, −0.25 — an unprompted wrong,
        // not the −0.1 executed let-down of a shut stall), the
        // settlement hears the shouting (gossip), and the wrong can
        // push the pair over the feud line.

        EntityRegistry registry;

        const auto a = registry.CreateEntity();
        const auto b = registry.CreateEntity();
        const auto c = registry.CreateEntity();   // the settlement

        registry.AddComponent<Memory>(a, Memory{});
        registry.AddComponent<Memory>(b, Memory{});
        registry.AddComponent<Memory>(c, Memory{});

        // A rival pair: dispositions −0.5 each (below the −0.6 enemy
        // line, deep in rival country), booked as rivals.
        registry.AddComponent<Relationships>(a, Relationships{});
        registry.AddComponent<Relationships>(b, Relationships{});

        auto& ra = *registry.GetComponent<Relationships>(a);
        auto& rb = *registry.GetComponent<Relationships>(b);
        ra.ByEntity[b] = Relationship{};
        rb.ByEntity[a] = Relationship{};
        ra.ByEntity[b].Disposition = -0.5f;
        rb.ByEntity[a].Disposition = -0.5f;

        BondMap bonds;
        bonds[PairKey(a, b)] = PairBond{ BondKind::Rival, 10 };

        TLC::ConflictGates::Map gates;

        // The row lands.
        if (!TLC::Rows::Exchange(
                registry, bonds, gates, a, b, 42, {}, nullptr))
        {
            return false;
        }

        // Both remember the other wronged them, today.
        for (const auto& who : { a, b })
        {
            const auto other = who == a ? b : a;
            const auto memory = registry.GetComponent<Memory>(who);
            bool saw = false;

            for (const auto& event : memory->Events)
            {
                if (event.Kind == InteractionKind::Wronged
                    && event.Other == other && event.Day == 42)
                {
                    saw = true;
                }
            }

            if (!saw)
            {
                return false;
            }
        }

        // A wrong is a wrong — full loss, −0.25 each way.
        if (std::fabs(ra.ByEntity[b].Disposition - (-0.75f)) > 0.0001f
            || std::fabs(rb.ByEntity[a].Disposition - (-0.75f)) > 0.0001f)
        {
            return false;
        }

        // And the row pushed the pair over the feud line: the 1-second
        // re-derive (the production path) now reads Enemy.
        const auto thresholds =
            ParseBondThresholds(DefaultBondThresholds());
        ApplyPair(
            bonds, PairKey(a, b), -0.75f, -0.75f, thresholds, 42, nullptr);

        if (bonds[PairKey(a, b)].Kind != BondKind::Enemy)
        {
            return false;
        }

        // The settlement heard the shouting: the third mind knows both
        // faces through the gossip channel.
        const auto memoryC = registry.GetComponent<Memory>(c);
        bool knowsA = false;
        bool knowsB = false;

        for (const auto& event : memoryC->Events)
        {
            knowsA = knowsA || event.Other == a;
            knowsB = knowsB || event.Other == b;
        }

        if (!knowsA || !knowsB)
        {
            return false;
        }

        // Words once a day — and the gate is the durable day-scoped
        // map, not the fading memory (a Wronged memory erases itself in
        // seconds under the default sim.memory.fade — the 0.7.5 fix):
        // the same pair rows again today — no.
        if (TLC::Rows::Exchange(
                registry, bonds, gates, a, b, 42, {}, nullptr))
        {
            return false;
        }

        // The regression the fix exists for: even after the Wronged
        // memories fade away mid-day (erase them, exactly what
        // sim.memory.fade does in seconds), the gate still holds — the
        // day-scoped map, not the memory, is the truth. The old build
        // failed here: the pair re-rowed every ~10s forever.
        for (const auto& who : { a, b })
        {
            registry.GetComponent<Memory>(who)->Events.clear();
        }

        if (TLC::Rows::Exchange(
                registry, bonds, gates, a, b, 42, {}, nullptr))
        {
            return false;
        }

        if (!TLC::ConflictGates::RowedToday(gates, a, b, 42)
            || TLC::ConflictGates::RowedToday(gates, a, b, 43))
        {
            return false;   // today yes, tomorrow not yet
        }

        // Tomorrow the words return.
        if (!TLC::Rows::Exchange(
                registry, bonds, gates, a, b, 43, {}, nullptr))
        {
            return false;
        }

        if (!TLC::ConflictGates::RowedToday(gates, a, b, 43))
        {
            return false;
        }

        // A row needs a feud: friends and strangers stay quiet.
        EntityRegistry quiet;

        const auto f1 = quiet.CreateEntity();
        const auto f2 = quiet.CreateEntity();
        quiet.AddComponent<Memory>(f1, Memory{});
        quiet.AddComponent<Memory>(f2, Memory{});
        quiet.AddComponent<Relationships>(f1, Relationships{});
        quiet.AddComponent<Relationships>(f2, Relationships{});

        auto& r1 = *quiet.GetComponent<Relationships>(f1);
        auto& r2 = *quiet.GetComponent<Relationships>(f2);
        r1.ByEntity[f2] = Relationship{};
        r2.ByEntity[f1] = Relationship{};
        r1.ByEntity[f2].Disposition = 0.4f;
        r2.ByEntity[f1].Disposition = 0.4f;

        BondMap friendBonds;
        friendBonds[PairKey(f1, f2)] = PairBond{ BondKind::Friend, 5 };

        TLC::ConflictGates::Map quietGates;

        if (TLC::Rows::Exchange(
                quiet, friendBonds, quietGates, f1, f2, 1, {}, nullptr))
        {
            return false;
        }

        BondMap noneBonds;

        if (TLC::Rows::Exchange(
                quiet, noneBonds, quietGates, f1, f2, 1, {}, nullptr))
        {
            return false;
        }

        // A quiet pair never touched the gate.
        if (!quietGates.empty())
        {
            return false;
        }

        return true;
    }

    bool FightsTest()
    {
        using namespace LCE::Simulation;
        using namespace TLC::Bonds;

        // 0.7.5 Fights — the physical escalation: an enemy pair's row
        // can turn to blows. RollFight is three gates — the feud line
        // (enemies only; rivals stay verbal — the verbal-first rule),
        // the aggressor's temper line (the churlish throw the punch),
        // and the chance coin (1.0 forces). BookFight lands the Combat
        // on both sides (the engine's unprompted-wrong channel, −0.25),
        // the settlement hears the blows (gossip), and blows happen
        // once a day per pair.

        // RollFight: a rival stays verbal — even hot-headed, even
        // forced.
        if (TLC::Fights::RollFight(BondKind::Rival, 1.5f, 1.0f, 1.0f, 0.0f))
        {
            return false;
        }

        // An enemy with a calm temper below the line swallows it.
        if (TLC::Fights::RollFight(BondKind::Enemy, 0.9f, 1.0f, 1.0f, 0.0f))
        {
            return false;
        }

        // Enemy, hot temper, the coin lands under the chance — blows.
        if (!TLC::Fights::RollFight(BondKind::Enemy, 1.0f, 1.0f, 0.1f, 0.05f))
        {
            return false;
        }

        // The coin respects the chance strictly: at or above it, no
        // fight.
        if (TLC::Fights::RollFight(BondKind::Enemy, 1.2f, 1.0f, 0.1f, 0.1f))
        {
            return false;
        }

        // 1.0 forces even a near-miss roll — the test knob.
        if (!TLC::Fights::RollFight(BondKind::Enemy, 1.0f, 1.0f, 1.0f, 0.999f))
        {
            return false;
        }

        // BookFight: an enemy pair — both remember the Combat, today,
        // and the disposition cools further (the feud deepens).
        EntityRegistry registry;

        const auto a = registry.CreateEntity();
        const auto b = registry.CreateEntity();
        const auto c = registry.CreateEntity();   // the settlement

        registry.AddComponent<Memory>(a, Memory{});
        registry.AddComponent<Memory>(b, Memory{});
        registry.AddComponent<Memory>(c, Memory{});

        registry.AddComponent<Relationships>(a, Relationships{});
        registry.AddComponent<Relationships>(b, Relationships{});

        auto& ra = *registry.GetComponent<Relationships>(a);
        auto& rb = *registry.GetComponent<Relationships>(b);
        ra.ByEntity[b] = Relationship{};
        rb.ByEntity[a] = Relationship{};
        ra.ByEntity[b].Disposition = -0.8f;
        rb.ByEntity[a].Disposition = -0.8f;

        BondMap bonds;
        bonds[PairKey(a, b)] = PairBond{ BondKind::Enemy, 10 };

        TLC::ConflictGates::Map gates;

        if (!TLC::Fights::BookFight(
                registry, bonds, gates, a, b, 42, {}, nullptr))
        {
            return false;
        }

        // Both remember the other's blows, stamped today.
        for (const auto& who : { a, b })
        {
            const auto other = who == a ? b : a;
            const auto memory = registry.GetComponent<Memory>(who);
            bool saw = false;

            for (const auto& event : memory->Events)
            {
                if (event.Kind == InteractionKind::Combat
                    && event.Other == other && event.Day == 42)
                {
                    saw = true;
                }
            }

            if (!saw)
            {
                return false;
            }
        }

        // The feud deepens: the Combat wrong costs −0.25 each way.
        if (std::fabs(ra.ByEntity[b].Disposition - (-1.05f)) > 0.0001f
            || std::fabs(rb.ByEntity[a].Disposition - (-1.05f)) > 0.0001f)
        {
            return false;
        }

        // Blows once a day — and the gate is the durable day-scoped
        // map, not the fading Combat memory (which erases itself in
        // seconds under the default sim.memory.fade — the 0.7.5 fix
        // that ended the fight waves): the same pair again today — no.
        if (TLC::Fights::BookFight(
                registry, bonds, gates, a, b, 42, {}, nullptr))
        {
            return false;
        }

        // The regression the fix exists for: even after the Combat
        // memories fade away mid-day (erase them, exactly what
        // sim.memory.fade does in seconds), the gate still holds — the
        // day-scoped map, not the memory, is the truth. The old build
        // failed here: the pair re-fought every ~10s, shoving both
        // sides repeatedly — the fight waves you saw.
        for (const auto& who : { a, b })
        {
            registry.GetComponent<Memory>(who)->Events.clear();
        }

        if (TLC::Fights::BookFight(
                registry, bonds, gates, a, b, 42, {}, nullptr))
        {
            return false;
        }

        if (!TLC::ConflictGates::FoughtToday(gates, a, b, 42)
            || TLC::ConflictGates::FoughtToday(gates, a, b, 43))
        {
            return false;   // today yes, tomorrow not yet
        }

        // The gate is per-day: tomorrow they can fight again.
        if (!TLC::Fights::BookFight(
                registry, bonds, gates, a, b, 43, {}, nullptr))
        {
            return false;
        }

        // The test hook's loop (0.7.5): a FORCED fight bypasses the
        // once-per-day gate — the same pair, the same day, forced again:
        // yes. That is the point of sim.test.forceFight — a pinned pair
        // brawls on its own timer, shove and all, without waiting for a
        // day roll.
        if (!TLC::Fights::BookFight(
                registry, bonds, gates, a, b, 43, {}, nullptr, true))
        {
            return false;
        }

        // And the forced fight still lands the same bookkeeping.
        if (!TLC::ConflictGates::FoughtToday(gates, a, b, 43))
        {
            return false;
        }

        // A rival pair never books — words stay words.
        EntityRegistry quiet;

        const auto f1 = quiet.CreateEntity();
        const auto f2 = quiet.CreateEntity();
        quiet.AddComponent<Memory>(f1, Memory{});
        quiet.AddComponent<Memory>(f2, Memory{});

        BondMap rivalBonds;
        rivalBonds[PairKey(f1, f2)] = PairBond{ BondKind::Rival, 3 };

        TLC::ConflictGates::Map quietGates;

        if (TLC::Fights::BookFight(
                quiet, rivalBonds, quietGates, f1, f2, 1, {}, nullptr))
        {
            return false;
        }

        // A pair that never fought never touched the gate.
        if (!quietGates.empty())
        {
            return false;
        }

        const auto m1 = quiet.GetComponent<Memory>(f1);

        for (const auto& event : m1->Events)
        {
            if (event.Kind == InteractionKind::Combat)
            {
                return false;
            }
        }

        // The settlement hears the blows: the third mind knows both
        // faces through the gossip channel.
        const auto memoryC = registry.GetComponent<Memory>(c);
        bool knowsA = false;
        bool knowsB = false;

        for (const auto& event : memoryC->Events)
        {
            knowsA = knowsA || event.Other == a;
            knowsB = knowsB || event.Other == b;
        }

        return knowsA && knowsB;
    }

    bool SocietyTest()
    {
        // The conflict source's substrate (0.7.0 Stone 2): the
        // temperament is deterministic per mind and bounded around 1.0,
        // and the engine's group echo spreads a feeling — warm or cold —
        // through the settlement at sim.group.inheritance strength.

        // Temper: deterministic, spread ±20% (roughly 0.8–1.2), and not
        // one value for everyone.
        const auto id = EntityId{ 123 };

        if (TemperOf(id) != TemperOf(id))
        {
            return false;
        }

        const auto t = TemperOf(EntityId{ 999 });

        if (t < 0.8f || t > 1.2f)
        {
            return false;
        }

        float distinct = 0.0f;

        for (std::uint64_t i = 1; i <= 8; ++i)
        {
            distinct += TemperOf(EntityId{ i });
        }

        if (std::fabs(distinct - 8.0f * TemperOf(EntityId{ 1 })) < 0.001f)
        {
            return false;   // not one temper for everyone
        }

        // The echo: two minds share a settlement; a third is their
        // target. A warmth echoes +0.1 * 0.5 = +0.05 to the mate; a
        // wrong echoes −0.25 * 0.5 = −0.125.
        EntityRegistry registry;

        const auto subject = registry.CreateEntity();
        const auto mate = registry.CreateEntity();
        const auto target = registry.CreateEntity();

        registry.AddComponent<Groups>(
            subject, Groups{ { GroupId{ 7 } } });
        registry.AddComponent<Groups>(
            mate, Groups{ { GroupId{ 7 } } });
        registry.AddComponent<Memory>(subject, Memory{});
        registry.AddComponent<Memory>(mate, Memory{});
        registry.AddComponent<Memory>(target, Memory{});

        Remember(
            registry, subject,
            MemoryEvent{ target, InteractionKind::Social, 1.0f });

        const auto mateRelationships =
            registry.GetComponent<Relationships>(mate);

        if (!mateRelationships)
        {
            return false;
        }

        const auto iterator = mateRelationships->ByEntity.find(target);

        if (iterator == mateRelationships->ByEntity.end()
            || std::fabs(iterator->second.Disposition - 0.05f) > 0.0001f)
        {
            return false;
        }

        Remember(
            registry, subject,
            MemoryEvent{ target, InteractionKind::Wronged, 1.0f });

        if (std::fabs(iterator->second.Disposition - (-0.075f)) > 0.0001f)
        {
            return false;   // 0.05 − 0.125 — the settlement cools
        }

        // No group, no echo: a stranger's feelings stay private.
        EntityRegistry isolated;

        const auto a = isolated.CreateEntity();
        const auto b = isolated.CreateEntity();
        const auto c = isolated.CreateEntity();

        isolated.AddComponent<Memory>(a, Memory{});
        isolated.AddComponent<Memory>(b, Memory{});
        isolated.AddComponent<Memory>(c, Memory{});

        Remember(isolated, a,
            MemoryEvent{ c, InteractionKind::Social, 1.0f });

        if (isolated.GetComponent<Relationships>(b) != nullptr)
        {
            return false;
        }

        return true;
    }

    bool KinTest()
    {
        // The family gate (0.7.5 field find): the vanilla families
        // never romance. The curated pairs match regardless of order
        // and on their low 24 bits (stable whatever the load order),
        // and the bond book refuses a romantic kind for a kin pair —
        // family can be friends, never lovers.
        using namespace TLC::Bonds;

        // IsKinBasePair: order-insensitive, low-24-bit matching.
        if (!Kin::IsKinBasePair(0x0006B4D3, 0x0006B4D2)
            || !Kin::IsKinBasePair(0x0006B4D2, 0x0006B4D3))
        {
            return false;   // Blake x Lucy, both orders
        }

        if (!Kin::IsKinBasePair(0x0003F22C, 0x0003F22B))
        {
            return false;   // Abigail x Daniel
        }

        if (Kin::IsKinBasePair(0x0006B4D3, 0x0006B4D1))
        {
            return false;   // Blake x Connie are MARRIED, not kin
        }

        if (Kin::IsKinBasePair(0x0001A4D7, 0x000250FE))
        {
            return false;   // unrelated pair is not kin
        }

        // ApplyPair with the kin flag: a sweetheart-grade warmth forms
        // only friendship for kin; a non-kin pair forms normally.
        BondMap bonds;
        BondThresholds thresholds;
        bool changed = false;
        const auto onChanged = [&changed](
            EntityId, EntityId, BondKind, BondKind, std::uint64_t)
        {
            changed = true;
        };

        const auto a = EntityId{ 1001 };
        const auto b = EntityId{ 1002 };

        // Kin pair, sweetheart-grade shared disposition: caps at Friend.
        Bonds::ApplyPair(
            bonds, Bonds::PairKey(a, b),
            0.9f, 0.9f, thresholds, 42, onChanged, true);

        if (!changed
            || bonds.at(Bonds::PairKey(a, b)).Kind != BondKind::Friend)
        {
            return false;   // family never crosses the sweetheart line
        }

        // The same warmth on a non-kin pair: spouses.
        const auto c = EntityId{ 1003 };
        const auto d = EntityId{ 1004 };

        Bonds::ApplyPair(
            bonds, Bonds::PairKey(c, d),
            0.9f, 0.9f, thresholds, 42, onChanged, false);

        if (bonds.at(Bonds::PairKey(c, d)).Kind != BondKind::Spouse)
        {
            return false;
        }

        // A pre-fix save's mistake heals: a kin pair already on the
        // books as spouses is downgraded to friends on the next pass.
        BondMap stale;
        stale[Bonds::PairKey(a, b)] = { BondKind::Spouse, 10 };

        Bonds::ApplyPair(
            stale, Bonds::PairKey(a, b),
            0.9f, 0.9f, thresholds, 42, onChanged, true);

        if (stale.at(Bonds::PairKey(a, b)).Kind != BondKind::Friend)
        {
            return false;   // the stale romance dissolves to family
        }

        // The companion gate (0.7.5 field find), through the full
        // reconcile pass: a CompanionTag mind warms to a settler at
        // spouse-grade — the pair must cap at Friend. Enemies stay
        // enemies (the user's rule: friends and feuds are fine, the
        // dating pool is closed).
        EntityRegistry companionRegistry;

        const auto companion = companionRegistry.CreateEntity();
        const auto settler = companionRegistry.CreateEntity();

        companionRegistry.AddComponent<CompanionTag>(
            companion, CompanionTag{});
        companionRegistry.AddComponent<SpeciesTag>(
            companion, SpeciesTag{ Species::Human });
        companionRegistry.AddComponent<SpeciesTag>(
            settler, SpeciesTag{ Species::Human });

        auto companionR = Relationships{};
        auto settlerR = Relationships{};
        companionR.ByEntity[settler] = { 0.9f, 0.9f };
        settlerR.ByEntity[companion] = { 0.9f, 0.9f };
        companionRegistry.AddComponent<Relationships>(
            companion, std::move(companionR));
        companionRegistry.AddComponent<Relationships>(
            settler, std::move(settlerR));

        BondMap companionBonds;

        Bonds::Reconcile(
            companionRegistry, thresholds, companionBonds, 42, nullptr);

        const auto companionKind =
            Bonds::CurrentKind(companionBonds, companion, settler);

        if (companionKind != BondKind::Friend)
        {
            return false;   // the companion's dating pool is closed
        }

        // And the negative side stays open: the same pair at enemy
        // grade forms an enemy feud — the user's "friends with enemy
        // is fine".
        auto enemyR = Relationships{};
        auto enemyROther = Relationships{};
        enemyR.ByEntity[settler] = { -0.9f, -0.9f };
        enemyROther.ByEntity[companion] = { -0.9f, -0.9f };
        companionRegistry.GetComponent<Relationships>(companion)->ByEntity =
            enemyR.ByEntity;
        companionRegistry.GetComponent<Relationships>(settler)->ByEntity =
            enemyROther.ByEntity;

        BondMap enemyBonds;

        Bonds::Reconcile(
            companionRegistry, thresholds, enemyBonds, 43, nullptr);

        if (Bonds::CurrentKind(enemyBonds, companion, settler)
            != BondKind::Enemy)
        {
            return false;   // feuds are allowed for companions
        }

        return true;
    }

    bool CoSaveV7Test()
    {
        // The v6 record (0.7.0): a Name component rides an entity, and
        // the registry-level legacy store rides the new section — the
        // dead's stories survive save/load with the world that remembers
        // them.
        EntityRegistry source;
        RegisterAllSerializers(source);

        const auto farmer = source.CreateEntity();

        source.AddComponent<FormRef>(farmer, FormRef{ 0x00012345u });
        source.AddComponent<Name>(farmer, Name{ "Vera Hart" });
        source.AddComponent<Needs>(farmer, SeededNeeds(Species::Human));

        source.LeaveLegacy(LegacyFact{
            farmer, 42, "the miller's pledge", 1.0f });

        const auto snapshot = source.Capture();
        const auto record = TLC::CoSave::Encode(
            snapshot, 0x5EEDull, {}, {}, {}, {}, {});

        // The name rides under its stable key; the legacy's name rides
        // in the record bytes.
        const auto contains =
            [](const std::vector<std::byte>& bytes, std::string_view needle)
            {
                for (std::size_t i = 0;
                     i + needle.size() <= bytes.size(); ++i)
                {
                    bool match = true;

                    for (std::size_t j = 0; j < needle.size(); ++j)
                    {
                        if (std::to_integer<char>(bytes[i + j])
                            != needle[j])
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

        if (!contains(record, "name")
            || !contains(record, "the miller's pledge"))
        {
            return false;
        }

        // Decode: the v6 section hands the legacy blob back.
        RegistrySnapshot decoded;
        std::uint64_t rngState = 0;
        std::vector<TLC::CoSave::StallKeeperPair> stalls;
        std::vector<TLC::CoSave::BondPair> bonds;
        std::vector<TLC::CoSave::ConflictGatePair> gates;
        std::vector<TLC::CoSave::BurialEntry> burials;
        std::vector<TLC::CoSave::MedicineStockPair> medicineStock;

        if (!TLC::CoSave::Decode(
                record, decoded, rngState, stalls, bonds, gates, burials, medicineStock))
        {
            return false;
        }

        if (!decoded.Legacy.has_value())
        {
            return false;
        }

        // Restore into a fresh registry: the name and the legacy come
        // back with the world.
        EntityRegistry restored;
        RegisterAllSerializers(restored);
        restored.Restore(decoded);

        EntityId restoredFarmer;
        std::size_t farmers = 0;

        restored.ForEachWithComponent<FormRef>(
            [&](EntityId a_entity, const FormRef& a_form)
            {
                if (a_form.FormId == 0x00012345u)
                {
                    restoredFarmer = a_entity;
                    ++farmers;
                }
            });

        if (farmers != 1)
        {
            return false;
        }

        const auto name = restored.GetComponent<Name>(restoredFarmer);

        if (!name || name->Full != "Vera Hart")
        {
            return false;
        }

        const auto legacy =
            restored.ReadLegacy("the miller's pledge");

        if (!legacy || legacy->Day != 42
            || legacy->Name != "the miller's pledge")
        {
            return false;
        }

        // Migration: a v5 record (pre-0.7 — no legacy section) decodes
        // with no legacy — the safe default, exactly like a missing
        // component.
        TLC::Codec::Writer writer;
        writer.U32(5);                    // record version v5
        writer.U32(snapshot.Version);     // the core's snapshot version
        writer.U32(0);                    // no entities
        writer.U64(0x1234ull);            // the v2 rng header
        writer.U32(0);                    // no stalls (v3)
        writer.U32(0);                    // no bonds (v5)

        RegistrySnapshot v5Decoded;
        std::uint64_t v5Rng = 0;
        std::vector<TLC::CoSave::StallKeeperPair> v5Stalls;
        std::vector<TLC::CoSave::BondPair> v5Bonds;
        std::vector<TLC::CoSave::ConflictGatePair> v5Gates;
        std::vector<TLC::CoSave::BurialEntry> v5Burials;
        std::vector<TLC::CoSave::MedicineStockPair> v5MedicineStock;

        if (!TLC::CoSave::Decode(
                writer.Bytes, v5Decoded, v5Rng, v5Stalls, v5Bonds, v5Gates, v5Burials, v5MedicineStock))
        {
            return false;
        }

        if (v5Decoded.Legacy.has_value() || !v5Gates.empty())
        {
            return false;   // v5 predates both the legacy and the gates
        }

        return true;
    }

    bool MidOutbreakSaveTest()
    {
        // The mid-outbreak round-trip (0.8.1 hardening): a sickness
        // caught mid-hold — kind, severity grown partway, the hold
        // partially spent, health at the reduced amount — must survive
        // a co-save capture/restore byte-exactly. The live save test
        // (2026-08-13) showed the world restores; this locks the
        // illness state itself.
        EntityRegistry source;
        RegisterAllSerializers(source);

        const auto sick = source.CreateEntity();

        source.AddComponent<FormRef>(sick, FormRef{ 0x0009B1DBu });
        source.AddComponent<Name>(sick, Name{ "Bill Sutton" });
        source.AddComponent<Health>(
            sick, Health{
                0.4f,   // holding
                Sickness{
                    SicknessKind::Radstorm,
                    0.58f,      // severity grown partway (seed 0.3 + 0.28)
                    42u,        // contracted day
                    137.5f } }  // 162.5 s of the hold spent
        );

        // The 0.7.7 gap (the audit's second find): a pregnancy in
        // progress must ride too — it was registered but never named, so
        // an expecting couple lost the conception on save/load. Same
        // round-trip, same exactness.
        const auto expecting = source.CreateEntity();

        source.AddComponent<FormRef>(expecting, FormRef{ 0x00048BA8u });
        source.AddComponent<BirthDay>(expecting, BirthDay{ 91u });
        source.AddComponent<Pregnancy>(
            expecting, Pregnancy{
                88u,                    // conception day
                91u,                    // due day
                0x00048B77u,            // parent A
                0x00048BA9u } );        // parent B

        const auto snapshot = source.Capture();
        const auto record = TLC::CoSave::Encode(
            snapshot, 0x5EEDull, {}, {}, {}, {}, {});

        RegistrySnapshot decoded;
        std::uint64_t rngState = 0;
        std::vector<TLC::CoSave::StallKeeperPair> stalls;
        std::vector<TLC::CoSave::BondPair> bonds;
        std::vector<TLC::CoSave::ConflictGatePair> gates;
        std::vector<TLC::CoSave::BurialEntry> burials;
        std::vector<TLC::CoSave::MedicineStockPair> medicineStock;

        if (!TLC::CoSave::Decode(
                record, decoded, rngState, stalls, bonds, gates, burials, medicineStock))
        {
            return false;
        }

        EntityRegistry restored;
        RegisterAllSerializers(restored);
        restored.Restore(decoded);

        bool found = false;
        bool exact = false;

        restored.ForEachWithComponent<Health>(
            [&](EntityId a_entity, const Health& a_health)
            {
                const auto form = restored.GetComponent<FormRef>(a_entity);

                if (form == nullptr || form->FormId != 0x0009B1DBu)
                {
                    return;
                }

                found = true;

                exact = a_health.Value == 0.4f
                    && a_health.Illness.Kind == SicknessKind::Radstorm
                    && a_health.Illness.Severity == 0.58f
                    && a_health.Illness.ContractedDay == 42u
                    && a_health.Illness.Remaining == 137.5f;
            });

        bool pregFound = false;
        bool pregExact = false;

        restored.ForEachWithComponent<Pregnancy>(
            [&](EntityId a_entity, const Pregnancy& a_preg)
            {
                const auto form = restored.GetComponent<FormRef>(a_entity);

                if (form == nullptr || form->FormId != 0x00048BA8u)
                {
                    return;
                }

                pregFound = true;

                const auto birthday = restored.GetComponent<BirthDay>(a_entity);

                pregExact = a_preg.ConceptionDay == 88u
                    && a_preg.DueDay == 91u
                    && a_preg.ParentA == 0x00048B77u
                    && a_preg.ParentB == 0x00048BA9u
                    && birthday != nullptr
                    && birthday->Day == 91u;
            });

        return found && exact && pregFound && pregExact;
    }

    bool IllnessTest()
    {
        const IllnessSettings settings;   // the code's own defaults

        //-------------------------------------------------------------------------
        // 1. Contract — a roll under the chance falls ill (health drops
        //    to the hold, the sickness is set); a miss stays well; an
        //    already-ill mind never stacks.
        //-------------------------------------------------------------------------
        {
            Health health;

            if (!Contract(
                    health, SicknessKind::Food, 0.2f, 100,
                    settings, 0.1f, 0.05f))
            {
                return false;   // roll under the chance — ill
            }

            if (health.Value != settings.Hold
                || health.Illness.Kind != SicknessKind::Food
                || health.Illness.ContractedDay != 100
                || health.Illness.Remaining != settings.Duration)
            {
                return false;
            }

            // A miss stays well.
            Health healthy;
            if (Contract(
                    healthy, SicknessKind::Radstorm, 0.1f, 101,
                    settings, 0.3f, 0.9f))
            {
                return false;
            }

            if (healthy.Value != 1.0f
                || healthy.Illness.Kind != SicknessKind::None)
            {
                return false;
            }

            // Already ill — no stacking: the second contract refuses
            // and leaves the first illness untouched.
            if (Contract(
                    health, SicknessKind::Wound, 0.9f, 102,
                    settings, 1.0f, 0.0f))
            {
                return false;
            }

            if (health.Illness.Kind != SicknessKind::Food)
            {
                return false;
            }
        }

        //-------------------------------------------------------------------------
        // 2. TickIllness — hold then recover. During the hold window the
        //    health holds at the reduced amount and severity climbs;
        //    after the window the health climbs back to whole.
        //-------------------------------------------------------------------------
        {
            Health health;
            Contract(
                health, SicknessKind::Food, 0.0f, 100,
                settings, 1.0f, 0.0f);

            // Mid-hold: still ill, severity grew, health still held.
            const auto mid = TickIllness(
                health, settings.Duration * 0.5f, settings, false);

            if (mid != 1 || health.Value != settings.Hold
                || health.Illness.Severity <= 0.0f)
            {
                return false;
            }

            // Walk the hold out in small steps: severity crosses the
            // death cap before the window ends (0.01/s × 120 s = 1.2),
            // so the tail of the hold drains health — the rescue
            // window. The window ends before the drain can kill, and
            // recovery climbs back to whole. Tick in 1 s steps so the
            // rescue actually resolves; one giant step would apply the
            // whole drain in a single tick (correct, but then the test
            // would be testing a death, not a recovery).
            int state = 1;
            int ticks = 0;
            for (; ticks < 10000 && state == 1; ++ticks)
            {
                state = TickIllness(health, 1.0f, settings, false);
            }

            if (state == 2)
            {
                return false;   // mild case must survive its own hold
            }

            if (state != 0 || health.Value != 1.0f
                || health.Illness.Kind != SicknessKind::None)
            {
                return false;   // healed — well again
            }
        }

        //-------------------------------------------------------------------------
        // 3. The fatal path — untreated severity at the cap drains health
        //    toward zero: the rescue window. TickIllness reports 2 (died)
        //    and the sickness clears (dead, not sick).
        //-------------------------------------------------------------------------
        {
            Health health;
            Contract(
                health, SicknessKind::Wound, 1.0f, 100,
                settings, 1.0f, 0.0f);

            int state = 1;
            int steps = 0;
            for (; steps < 100000 && state == 1; ++steps)
            {
                state = TickIllness(health, 1.0f, settings, false);
            }

            if (state != 2 || health.Value != 0.0f
                || health.Illness.Kind != SicknessKind::None)
            {
                return false;   // the fatal path resolved
            }
        }

        //-------------------------------------------------------------------------
        // 4. Medicine — a dose ends the hold: recovery starts now and the
        //    severity-cap drain stops (the rescue path). A well mind
        //    can't waste a dose.
        //-------------------------------------------------------------------------
        {
            Health health;
            Contract(
                health, SicknessKind::Wound, 1.0f, 100,
                settings, 1.0f, 0.0f);

            if (!TakeMedicine(health, settings))
            {
                return false;
            }

            // After the dose the severity stays capped but Remaining is
            // gone — the next tick recovers instead of draining.
            const auto after = TickIllness(health, 1.0f, settings, false);

            if (after != 1 || health.Value <= settings.Hold)
            {
                return false;
            }

            Health well;
            if (TakeMedicine(well, settings))
            {
                return false;   // nothing to treat
            }
        }

        //-------------------------------------------------------------------------
        // 5. The fatigue toll — 1.0 when well, FatigueMult while holding,
        //    easing back toward 1.0 as health recovers.
        //-------------------------------------------------------------------------
        {
            if (FatigueMultiplier(Health{}, settings) != 1.0f)
            {
                return false;   // well — no toll
            }

            Health health;
            Contract(
                health, SicknessKind::Contagion, 0.0f, 100,
                settings, 1.0f, 0.0f);

            // Holding at the floor — the full toll.
            if (FatigueMultiplier(health, settings)
                != settings.FatigueMult)
            {
                return false;
            }

            // Part-way back — eased off but not gone.
            health.Value = (settings.Hold + 1.0f) * 0.5f;
            const auto eased = FatigueMultiplier(health, settings);

            if (eased <= 1.0f || eased >= settings.FatigueMult)
            {
                return false;
            }
        }

        //-------------------------------------------------------------------------
        // 6. Children are more fragile — their severity climbs at the
        //    child multiplier.
        //-------------------------------------------------------------------------
        {
            Health child;
            Contract(
                child, SicknessKind::Contagion, 0.0f, 100,
                settings, 1.0f, 0.0f);

            Health adult;
            Contract(
                adult, SicknessKind::Contagion, 0.0f, 100,
                settings, 1.0f, 0.0f);

            TickIllness(child, 10.0f, settings, true);
            TickIllness(adult, 10.0f, settings, false);

            if (child.Illness.Severity
                != adult.Illness.Severity * settings.ChildMult)
            {
                return false;
            }
        }

        //-------------------------------------------------------------------------
        // 7. The 0.8.1 field finding — fragility must not be a death
        //    sentence. A child's common vectors (radstorm, contagion,
        //    food at the shipped seeds) run the full hold and recover
        //    by rest alone; only a wound crosses the death line. At
        //    ChildMult 2.0 every untreated childhood illness was fatal
        //    (any seed ≥ 0.2 reached the cap inside the hold) while
        //    children can't reach the market's medicine — four children
        //    "died" in one outbreak, then re-entered and caught it
        //    again. The tuned default must make the common cold a
        //    child's recovery, and the wound its earned death.
        {
            // A contagion-child (seed 0.25): the whole hold, no
            // medicine — recover, never die.
            Health child;
            Contract(
                child, SicknessKind::Contagion, 0.25f, 100,
                settings, 1.0f, 0.0f);

            auto state = 0;

            // Run the full hold plus the recovery climb (health needs
            // (1 - Hold) / Recovery ≈ 12 s to return once the hold
            // ends) — the child must come out whole.
            const auto maxT = settings.Duration + 60.0f;

            for (auto t = 0.0f; t < maxT; t += 10.0f)
            {
                state = TickIllness(child, 10.0f, settings, true);

                if (state == 2)
                {
                    return false;   // a common cold must not kill a child
                }

                if (state == 0)
                {
                    break;          // recovered
                }
            }

            if (state != 0 || child.Illness.Kind != SicknessKind::None)
            {
                return false;
            }

            // A wound-child (seed 0.5): the earned death — it crosses
            // the line late and drains.
            Health wounded;
            Contract(
                wounded, SicknessKind::Wound, 0.5f, 100,
                settings, 1.0f, 0.0f);

            auto died = false;

            for (auto t = 0.0f; t < settings.Duration * 1.5f; t += 10.0f)
            {
                if (TickIllness(wounded, 10.0f, settings, true) == 2)
                {
                    died = true;
                    break;
                }
            }

            if (!died)
            {
                return false;   // a wound is the child's earned death
            }
        }

        return true;
    }

    bool TradeLedgerTest()
    {
        using namespace TradeLedger;

        //-------------------------------------------------------------------------
        // 1. NextBudget — the stall serves what it fed yesterday, never
        //    below the floor (a settlement that fed nobody still has a
        //    little to offer).
        //-------------------------------------------------------------------------
        if (NextBudget(30, 1) != 30
            || NextBudget(0, 1) != 1
            || NextBudget(2, 5) != 5)
        {
            return false;
        }

        //-------------------------------------------------------------------------
        // 2. AfterMeal / IsDry — meals draw the budget down to zero, and
        //    a dry stall stays dry (never negative, never into debt).
        //-------------------------------------------------------------------------
        {
            auto budget = NextBudget(30, 1);
            budget = AfterMeal(budget);

            if (budget != 29 || IsDry(budget))
            {
                return false;
            }

            auto dry = NextBudget(1, 1);   // one meal left...
            dry = AfterMeal(dry);          // ...and it is served

            if (!IsDry(dry) || AfterMeal(dry) != 0)
            {
                return false;
            }
        }

        //-------------------------------------------------------------------------
        // 3. CaravanTopUp — the courier's return refills tomorrow's
        //    budget, empty or not.
        //-------------------------------------------------------------------------
        if (CaravanTopUp(0, 25) != 25
            || CaravanTopUp(10, 25) != 35)
        {
            return false;
        }

        //-------------------------------------------------------------------------
        // 4. CanSendCaravan — one courier at a time: only a settlement
        //    that ran dry yesterday with no courier already on the road
        //    (a famine can't spawn a caravan army).
        //-------------------------------------------------------------------------
        if (!CanSendCaravan(true, false)
            || CanSendCaravan(false, false)
            || CanSendCaravan(true, true))
        {
            return false;
        }

        //-------------------------------------------------------------------------
        // 5. CaravanCost — the keeper spends what the hoard covers,
        //    never into debt.
        //-------------------------------------------------------------------------
        if (CaravanCost(40, 40) != 40
            || CaravanCost(10, 40) != 10
            || CaravanCost(0, 40) != 0)
        {
            return false;
        }

        return true;
    }

    //-------------------------------------------------------------------------
    // The earn-caps economy (0.8.6b): the once-per-day settlement
    // stipend. Pins the pure sweep — the paid-once gate, the off switch,
    // the per-settlement tally, and the round-trip of the StipendMark.
    //-------------------------------------------------------------------------
    bool StipendTest()
    {
        using TLC::CapPouch;
        using TLC::StipendMark;
        using TLC::StipendReceipt;

        EntityRegistry registry;
        TLC::RegisterAllSerializers(registry);

        // Two humans with pouches — one already paid today, one due. An
        // animal with no pouch is never paid.
        const auto paid = registry.CreateEntity();
        registry.AddComponent<CapPouch>(paid, CapPouch{ 10 });
        registry.AddComponent<StipendMark>(paid, StipendMark{ 100 });

        const auto due = registry.CreateEntity();
        registry.AddComponent<CapPouch>(due, CapPouch{ 0 });

        const auto dog = registry.CreateEntity();

        std::vector<StipendReceipt> receipts;

        TLC::PayStipends(
            registry, 5, 100,
            [](EntityId) { return 0x000250FEu; },
            receipts);

        // Only the due mind draws; the paid one and the animal stay.
        if (registry.GetComponent<CapPouch>(paid)->Caps != 10
            || registry.GetComponent<CapPouch>(due)->Caps != 5
            || registry.GetComponent<StipendMark>(due) == nullptr
            || registry.GetComponent<StipendMark>(due)->Day != 100
            || receipts.size() != 1
            || receipts[0].MarketFormId != 0x000250FEu
            || receipts[0].Paid != 1
            || receipts[0].Caps != 5)
        {
            return false;
        }

        // The second call of the same day pays nothing — the gate holds.
        receipts.clear();

        TLC::PayStipends(
            registry, 5, 100,
            [](EntityId) { return 0x000250FEu; },
            receipts);

        if (!receipts.empty()
            || registry.GetComponent<CapPouch>(due)->Caps != 5)
        {
            return false;
        }

        // A new day pays everyone due again — and the tally groups both
        // under one settlement.
        receipts.clear();

        TLC::PayStipends(
            registry, 5, 101,
            [](EntityId) { return 0x000250FEu; },
            receipts);

        if (registry.GetComponent<CapPouch>(paid)->Caps != 15
            || registry.GetComponent<CapPouch>(due)->Caps != 10
            || receipts.size() != 1
            || receipts[0].Paid != 2
            || receipts[0].Caps != 10)
        {
            return false;
        }

        // Two settlements tally separately.
        receipts.clear();

        TLC::PayStipends(
            registry, 5, 102,
            [&paid](EntityId a_entity)
            {
                return a_entity == paid ? 0x000250FEu : 0x00046B0Bu;
            },
            receipts);

        if (receipts.size() != 2)
        {
            return false;
        }

        // The off switch: stipend 0 pays nothing, ever.
        receipts.clear();

        TLC::PayStipends(
            registry, 0, 103,
            [](EntityId) { return 0x000250FEu; },
            receipts);

        if (!receipts.empty()
            || registry.GetComponent<CapPouch>(due)->Caps != 15)
        {
            return false;
        }

        // The mark survives the co-save round-trip.
        const auto snapshot = registry.Capture();
        EntityRegistry restored;
        TLC::RegisterAllSerializers(restored);
        restored.Restore(snapshot);

        // The marks hold their last-paid day (102 — the day-103 call
        // was the off-switch, which pays nothing), and the pouches
        // round-trip with them.
        return restored.GetComponent<StipendMark>(due) != nullptr
            && restored.GetComponent<StipendMark>(due)->Day == 102
            && restored.GetComponent<CapPouch>(due)->Caps == 15;
    }
}
