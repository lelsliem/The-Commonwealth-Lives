//=============================================================================//
// TheLivingCommonwealth adapter tests — the harness, mirroring the core's:
// bool-returning suites, no framework (ADR-0010/0029), runs on every build.
// Links LCE.Core only — no game required.
//=============================================================================//

#include "Components.h"
#include "Executor.h"
#include "Serialization.h"
#include "Translator.h"

#include "LCE/Simulation/Behaviour.h"
#include "LCE/Simulation/EntityRegistry.h"
#include "LCE/Simulation/Goals.h"
#include "LCE/Simulation/Memory.h"
#include "LCE/Simulation/Needs.h"
#include "LCE/Simulation/Relationships.h"

#include <cstdint>
#include <cstdio>

namespace TLC::Tests
{
    bool TranslatorTest();
    bool SeedingTest();
    bool SerializationTest();
    bool PlanBuilderTest();
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
    Run("PlanBuilderTest", TLC::Tests::PlanBuilderTest);

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
}
