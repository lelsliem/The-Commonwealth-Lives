//=============================================================================//
//                                                                             //
//   The Living Commonwealth — Fallout 4 adapter for the Living Commonwealth   //
//   Engine (LCE).                                                             //
//                                                                             //
//   Every launch, a tiny hello: the sim's pure logic proves itself.           //
//                                                                             //
//=============================================================================//

#include "Diag.h"

#include "Behaviour.h"
#include "BlobCodec.h"
#include "Bonds.h"
#include "CoSave.h"
#include "Components.h"
#include "Stipend.h"
#include "Names.h"
#include "Serialization.h"
#include "Translator.h"
#include "Tuning.h"

#include "LCE/Config/Configuration.h"
#include "LCE/Simulation/Entity/EntityRegistry.h"
#include "LCE/Simulation/Mind/Needs.h"
#include "LCE/Simulation/Simulation.h"
#include "LCE/Simulation/Substrate/Rng.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#ifdef ERROR
#undef ERROR
#endif

#include <REX/LOG.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace
{
    // The anchor for the module-path lookup — the address of this
    // function lives inside the DLL, so GetModuleHandleExW resolves the
    // DLL's own handle (the same trick the tuning loader uses).
    void ModuleAnchor() {}

    // The INI next to the DLL — Data\F4SE\Plugins\TheLivingCommonwealth.ini.
    [[nodiscard]] std::string ReadConfigText()
    {
        wchar_t modulePath[MAX_PATH]{};
        HMODULE module = nullptr;

        if (!GetModuleHandleExW(
                GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
                reinterpret_cast<LPCWSTR>(&ModuleAnchor),
                &module)
            || GetModuleFileNameW(module, modulePath, MAX_PATH) == 0)
        {
            return {};
        }

        std::filesystem::path ini{ modulePath };
        ini.replace_extension(".ini");

        std::error_code error;

        if (!std::filesystem::exists(ini, error))
        {
            return {};
        }

        std::ifstream stream(ini);
        std::stringstream buffer;
        buffer << stream.rdbuf();
        return buffer.str();
    }

    // One check: a name, a lambda. The lambda returns true on pass.
    int g_Failures = 0;
    int g_Run = 0;

    void Check(const char* a_name, bool a_passed)
    {
        ++g_Run;

        if (a_passed)
        {
            REX::INFO("diag: [PASS] {}", a_name);
        }
        else
        {
            ++g_Failures;
            REX::ERROR("diag: [FAIL] {}", a_name);
        }
    }

    // The codec round-trip — the bytes the co-save rides on.
    bool CodecCheck()
    {
        using namespace TLC::Codec;

        Writer writer;
        writer.U32(0xCAFEBABEu);
        writer.F(3.14159f);
        writer.U64(0x0123456789ABCDEFull);

        Reader reader{ writer.Bytes };

        return reader.U32() == 0xCAFEBABEu
            && reader.F() == 3.14159f
            && reader.U64() == 0x0123456789ABCDEFull;
    }

    // The seeded minds — every species is born with its own need set,
    // all satisfied.
    bool SeedingCheck()
    {
        using namespace LCE::Simulation;
        using TLC::Species;

        const auto human = TLC::SeededNeeds(Species::Human);
        const auto animal = TLC::SeededNeeds(Species::Animal);

        bool humanFull = human.List.size() == 5;
        bool animalTrim = animal.List.size() == 3;

        for (const auto& need : human.List)
        {
            humanFull = humanFull && need.Value == 1.0f;
        }

        return humanFull && animalTrim;
    }

    // The desync jitter — deterministic per id: the same mind re-seeded
    // twice must be identical (the 0.5.x stone's promise).
    bool JitterCheck()
    {
        using namespace LCE::Simulation;
        using TLC::Species;

        auto a = TLC::SeededNeeds(Species::Human);
        auto b = TLC::SeededNeeds(Species::Human);

        TLC::VaryNeeds(a, EntityId{ 0x1234 });
        TLC::VaryNeeds(b, EntityId{ 0x1234 });

        if (a.List.size() != b.List.size())
        {
            return false;
        }

        for (std::size_t i = 0; i < a.List.size(); ++i)
        {
            if (a.List[i].DecayRate != b.List[i].DecayRate)
            {
                return false;
            }
        }

        return true;
    }

    // The species split — who trades, who talks, who is just fed.
    bool BehaviourCheck()
    {
        using TLC::Species;

        const auto& human = TLC::BehaviourFor(Species::Human);
        const auto& child = TLC::BehaviourFor(Species::Child);
        const auto& animal = TLC::BehaviourFor(Species::Animal);

        return human.CanTrade && human.CanTalk
            && !child.CanTrade && child.CanTalk
            && !animal.CanTrade && !animal.CanTalk;
    }

    // The names — the game's placeholders are generic, real names are
    // not; a role label is a title that keeps its prefix.
    bool NamesCheck()
    {
        using namespace TLC::Names;

        if (!IsGenericName("Settler") || IsGenericName("Sturges"))
        {
            return false;
        }

        if (!IsGenericName("Dog", TLC::Species::Animal)
            || IsGenericName("Dogmeat", TLC::Species::Animal))
        {
            return false;
        }

        if (IsRoleName("Provisioner").empty()
            || IsRoleName("Caravan Worker").empty()
            || !IsRoleName("Sturges").empty())
        {
            return false;
        }

        // A generated name is deterministic — the same id always draws
        // the same name from the same pool.
        auto pool = DefaultPool();
        std::unordered_set<std::string> used;

        const auto first = GenerateUnique(used, EntityId{ 0x99 }, pool, Gender::Male);

        if (used.find(first) == used.end())
        {
            return false;
        }

        std::unordered_set<std::string> usedAgain;
        const auto again = GenerateUnique(usedAgain, EntityId{ 0x99 }, pool, Gender::Male);

        return first == again;
    }

    // The bond thresholds — the friendship/spouse/enemy lines parse.
    bool BondsCheck()
    {
        using namespace TLC::Bonds;

        const auto parsed = ParseBondThresholds(std::vector<BondThreshold>{
            { "friend", 0.3f },
            { "spouse", 0.8f },
            { "enemy", -0.6f },
        });

        return parsed.Friend == 0.3f
            && parsed.Spouse == 0.8f
            && parsed.Enemy == -0.6f;
    }

    // The translator — the form<->entity map both ways.
    bool TranslatorCheck()
    {
        using namespace LCE::Simulation;

        TLC::Translator translator;
        const auto farmer = EntityId{ 0x0000000100000005ull };

        translator.Add(0x00012345u, farmer);

        return translator.EntityFor(0x00012345u) == farmer
            && translator.FormFor(farmer) == 0x00012345u
            && translator.Size() == 1;
    }

    // The co-save round-trip — a living world → bytes → back. This is
    // the record that rides inside the save file, so the round-trip is
    // the single most valuable check in the battery.
    bool CoSaveCheck()
    {
        using namespace LCE::Simulation;
        using TLC::CapPouch;
        using TLC::FormRef;
        using TLC::Species;

        EntityRegistry source;
        TLC::RegisterAllSerializers(source);

        const auto farmer = source.CreateEntity();
        source.AddComponent<FormRef>(farmer, FormRef{ 0x00012345u });
        source.AddComponent<Needs>(farmer, TLC::SeededNeeds(Species::Human));
        source.AddComponent<Memory>(farmer, Memory{
            { MemoryEvent{ farmer, InteractionKind::Trade, 1.0f } }
        });
        source.AddComponent<CapPouch>(farmer, CapPouch{ 33 });

        const auto snapshot = source.Capture();

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

        return rngState == 0x5EEDC0DEull
            && !decoded.Entities.empty()
            && stalls.size() == 1
            && bonds.size() == 1
            && gates.size() == 1
            && burials.size() == 1
            && medicineStock.size() == 1
            && medicineStock[0].MarketFormId == 0x000250FEu
            && medicineStock[0].Stock == 4;
    }

    // The earn-caps economy (0.8.6b): the once-per-day stipend sweep.
    bool StipendCheck()
    {
        using namespace LCE::Simulation;
        using TLC::CapPouch;
        using TLC::StipendMark;
        using TLC::StipendReceipt;

        EntityRegistry registry;
        TLC::RegisterAllSerializers(registry);

        // Two humans with pouches, one already paid today.
        const auto first = registry.CreateEntity();
        registry.AddComponent<CapPouch>(first, CapPouch{ 10 });
        registry.AddComponent<StipendMark>(first, StipendMark{ 100 });

        const auto second = registry.CreateEntity();
        registry.AddComponent<CapPouch>(second, CapPouch{ 0 });

        // A third — an animal, no pouch: never paid.
        const auto dog = registry.CreateEntity();

        std::vector<StipendReceipt> receipts;

        TLC::PayStipends(
            registry, 5, 100,
            [](EntityId) { return 0x000250FEu; },
            receipts);

        // Only the unpaid mind draws; the marked one and the animal
        // stay put. One receipt, one settlement, 5 caps out.
        const bool paidOnce =
            registry.GetComponent<CapPouch>(first)->Caps == 10
            && registry.GetComponent<CapPouch>(second)->Caps == 5
            && registry.GetComponent<StipendMark>(second) != nullptr
            && registry.GetComponent<StipendMark>(second)->Day == 100
            && receipts.size() == 1
            && receipts[0].MarketFormId == 0x000250FEu
            && receipts[0].Paid == 1
            && receipts[0].Caps == 5;

        // The second call of the same day pays nothing — the gate holds.
        receipts.clear();

        TLC::PayStipends(
            registry, 5, 100,
            [](EntityId) { return 0x000250FEu; },
            receipts);

        const bool gateHolds =
            receipts.empty()
            && registry.GetComponent<CapPouch>(second)->Caps == 5;

        // A new day pays everyone due again.
        receipts.clear();

        TLC::PayStipends(
            registry, 5, 101,
            [](EntityId) { return 0x000250FEu; },
            receipts);

        const bool nextDay =
            registry.GetComponent<CapPouch>(first)->Caps == 15
            && registry.GetComponent<CapPouch>(second)->Caps == 10
            && receipts.size() == 1
            && receipts[0].Paid == 2
            && receipts[0].Caps == 10;

        // The off switch: stipend 0 pays nothing, ever.
        receipts.clear();

        TLC::PayStipends(
            registry, 0, 102,
            [](EntityId) { return 0x000250FEu; },
            receipts);

        const bool offSwitch =
            receipts.empty()
            && registry.GetComponent<CapPouch>(second)->Caps == 10;

        return paidOnce && gateHolds && nextDay && offSwitch;
    }
}

namespace TLC::Diag
{
    bool SelfTestRequested()
    {
        const auto config = Tuning::ParseConfig(ReadConfigText());
        const auto raw = config.Get("sim.diag.selfTest");

        if (raw.empty())
        {
            return false;
        }

        try
        {
            return std::stof(std::string(raw)) != 0.0f;
        }
        catch (const std::exception&)
        {
            return false;
        }
    }

    bool RunSelfTest()
    {
        g_Run = 0;
        g_Failures = 0;

        Check("codec round-trip", CodecCheck());
        Check("seeded needs", SeedingCheck());
        Check("decay jitter determinism", JitterCheck());
        Check("species behaviour split", BehaviourCheck());
        Check("names (generic/role/deterministic)", NamesCheck());
        Check("bond thresholds", BondsCheck());
        Check("translator", TranslatorCheck());
        Check("co-save round-trip", CoSaveCheck());
        Check("stipend once-per-day", StipendCheck());

        const auto passed = g_Run - g_Failures;

        if (g_Failures == 0)
        {
            REX::INFO(
                "diag: self-test — {}/{} checks passed. The sim's pure logic is healthy.",
                passed, g_Run);
        }
        else
        {
            REX::ERROR(
                "diag: self-test — {}/{} checks FAILED. The sim's pure logic is broken.",
                passed, g_Run);
        }

        return g_Failures == 0;
    }
}
