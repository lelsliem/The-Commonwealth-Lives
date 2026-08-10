//=============================================================================//
// TheLivingCommonwealth adapter tests — the harness, mirroring the core's:
// bool-returning suites, no framework (ADR-0010/0029), runs on every build.
// Links LCE.Core only — no game required.
//=============================================================================//

#include "CoSave.h"
#include "Components.h"
#include "Executor.h"
#include "Market.h"
#include "Serialization.h"
#include "Translator.h"

#include "LCE/Simulation/Behaviour.h"
#include "LCE/Simulation/EntityRegistry.h"
#include "LCE/Simulation/Goals.h"
#include "LCE/Simulation/Memory.h"
#include "LCE/Simulation/Needs.h"
#include "LCE/Simulation/Relationships.h"
#include "LCE/Simulation/Simulation.h"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string_view>
#include <utility>
#include <vector>

namespace TLC::Tests
{
    bool TranslatorTest();
    bool SeedingTest();
    bool SerializationTest();
    bool CoSaveTest();
    bool PlanBuilderTest();
    bool MarketTest();
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
    Run("SerializationTest", TLC::Tests::SerializationTest);
    Run("CoSaveTest", TLC::Tests::CoSaveTest);
    Run("PlanBuilderTest", TLC::Tests::PlanBuilderTest);
    Run("MarketTest", TLC::Tests::MarketTest);

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
        const auto needs = SeededNeeds();

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
        source.AddComponent<Needs>(farmer, SeededNeeds());
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
        source.AddComponent<Needs>(farmer, SeededNeeds());
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

        const auto snapshot = source.Capture();
        const auto record = TLC::CoSave::Encode(snapshot);

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
            || !contains(record, "formref"))
        {
            return false;
        }

        // Decode, then restore into a fresh registry — a fresh game
        // session that never saw the first one.
        RegistrySnapshot decoded;

        if (!TLC::CoSave::Decode(record, decoded))
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

            if (TLC::CoSave::Decode(truncated, bad))
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

            if (TLC::CoSave::Decode(badVersion, bad))
            {
                return false;
            }
        }

        {
            auto unknownName = record;

            // Replace the first "needs" name with an unknown name.
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

            RegistrySnapshot bad;

            if (TLC::CoSave::Decode(unknownName, bad))
            {
                return false;
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

        registry.AddComponent<Needs>(settler, SeededNeeds());
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

        bare.AddComponent<Needs>(wanderer, SeededNeeds());
        bare.AddComponent<Memory>(wanderer, Memory{});

        Update(bare, 1.0);

        const auto other = bare.GetComponent<Intent>(wanderer);

        if (!other || other->Action == ActionType::MoveTo)
        {
            return false;
        }

        return true;
    }
}
