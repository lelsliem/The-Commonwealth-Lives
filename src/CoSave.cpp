//=============================================================================//
//                                                                             //
//   The Living Commonwealth — Fallout 4 adapter for the Living Commonwealth   //
//   Engine (LCE).                                                             //
//                                                                             //
//   Save, quit, load — the Commonwealth picks up where it dropped.                                      //
//                                                                             //
//=============================================================================//

#include "CoSave.h"

#include "BlobCodec.h"
#include "Bonds.h"
#include "Components.h"

#include "LCE/Simulation/Decision/Behaviour.h"
#include "LCE/Simulation/Entity/EntityId.h"
#include "LCE/Simulation/Mind/Goals.h"
#include "LCE/Simulation/Mind/Memory.h"
#include "LCE/Simulation/Mind/Needs.h"
#include "LCE/Simulation/Mind/Relationships.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <typeindex>

namespace TLC::CoSave
{
    namespace
    {
        using namespace LCE::Simulation;

        //-------------------------------------------------------------------------
        // The stable names — the durable record's keys. One entry per
        // registered serializer (Serialization.cpp's RegisterAllSerializers);
        // a component type with no name is not persisted. Chosen once,
        // never renamed.
        //-------------------------------------------------------------------------
        struct TypeName
        {
            std::string_view Name;
            std::type_index Type;
        };

        // The table decides what rides the record — every component a
        // mind carries must be named here or it silently never persists.
        // The 2026-08-13 audit found three gaps: Pregnancy/BirthDay
        // (0.7.7) and Health (0.8.0) were registered as serializers but
        // never named, so an in-progress pregnancy and a mid-hold
        // illness were both lost on save/load (CompanionTag was also
        // missing, but it re-derives every second, so the co-save was
        // never load-bearing for it). Additive: old records never
        // contained these types, and an unknown name decodes as a
        // graceful drop.
        const std::array<TypeName, 13>& TypeNames()
        {
            static const std::array<TypeName, 13> kTable{
                TypeName{ "needs", typeid(Needs) },
                TypeName{ "memory", typeid(Memory) },
                TypeName{ "relationships", typeid(Relationships) },
                TypeName{ "goals", typeid(Goals) },
                TypeName{ "intent", typeid(Intent) },
                TypeName{ "formref", typeid(FormRef) },
                TypeName{ "species", typeid(SpeciesTag) },
                TypeName{ "cappouch", typeid(CapPouch) },
                TypeName{ "name", typeid(Name) },
                TypeName{ "companion", typeid(CompanionTag) },
                TypeName{ "birthday", typeid(BirthDay) },
                TypeName{ "pregnancy", typeid(Pregnancy) },
                TypeName{ "health", typeid(Health) },
            };

            return kTable;
        }

        std::string_view NameForType(std::type_index a_type)
        {
            for (const auto& entry : TypeNames())
            {
                if (entry.Type == a_type)
                {
                    return entry.Name;
                }
            }

            return {};
        }

        // typeid(void) is the sentinel for "unknown" — it can never be a
        // component type.
        std::type_index TypeForName(std::string_view a_name)
        {
            for (const auto& entry : TypeNames())
            {
                if (entry.Name == a_name)
                {
                    return entry.Type;
                }
            }

            return typeid(void);
        }
    }

    std::vector<std::byte> Encode(
        const RegistrySnapshot& a_snapshot,
        std::uint64_t a_rngState,
        const std::vector<StallKeeperPair>& a_stallKeepers,
        const std::vector<BondPair>& a_bonds,
        const std::vector<ConflictGatePair>& a_gates,
        const std::vector<BurialEntry>& a_burials,
        const std::vector<MedicineStockPair>& a_medicineStock)
    {
        Codec::Writer writer;

        writer.U32(kRecordVersion);
        writer.U32(a_snapshot.Version);
        writer.U32(static_cast<std::uint32_t>(a_snapshot.Entities.size()));

        // v2 (the decay-jitter wiring): the Rng state rides the header so
        // a restored world resumes the exact same randomness. Read back
        // version-gated in Decode.
        writer.U64(a_rngState);

        for (const auto& entity : a_snapshot.Entities)
        {
            // Only components with a stable name are persisted. Every
            // registered type is named, so the filter never bites today —
            // it keeps the record honest if a serializer is added without
            // a name.
            std::vector<const SnapshotComponent*> named;

            for (const auto& component : entity.Components)
            {
                if (!NameForType(component.Type).empty())
                {
                    named.push_back(&component);
                }
            }

            writer.U64(entity.Id.Value());
            writer.U32(static_cast<std::uint32_t>(named.size()));

            for (const auto* component : named)
            {
                const auto name = NameForType(component->Type);

                writer.U8(static_cast<std::uint8_t>(name.size()));
                writer.Raw(name.data(), name.size());
                writer.U32(static_cast<std::uint32_t>(component->Data.size()));
                writer.Raw(component->Data.data(), component->Data.size());
            }
        }

        // v3 (the stall-keepers stone): the per-world stall section — who
        // runs each market's stall, as (market FormID, keeper FormID)
        // pairs, stable across sessions. Present only in v3+ records;
        // older records end after the entities.
        writer.U32(static_cast<std::uint32_t>(a_stallKeepers.size()));

        for (const auto& stall : a_stallKeepers)
        {
            writer.U32(stall.first);
            writer.U32(stall.second);
        }

        // v5 (the bonds stone): the per-world bond section — who is
        // bonded to whom, as (form A, form B, kind ordinal, since-day)
        // tuples, stable across sessions. Present only in v5+ records;
        // older records end after the stalls, and the bonds re-derive
        // from the restored relationships on the first reconcile pass.
        writer.U32(static_cast<std::uint32_t>(a_bonds.size()));

        for (const auto& bond : a_bonds)
        {
            writer.U32(bond.FormA);
            writer.U32(bond.FormB);
            writer.U32(bond.Kind);
            writer.U64(bond.SinceDay);
        }

        // v6 (the identity stone): the registry-level legacy section —
        // the serialized legacy store (via the registered legacy
        // serializer), present only in v6+ records. Older records end
        // after the bonds; a restored world simply starts with no
        // legacies, which is honest for a save made before the dead
        // could leave one.
        if (a_snapshot.Legacy.has_value())
        {
            writer.U8(1);
            writer.U32(static_cast<std::uint32_t>(
                a_snapshot.Legacy->size()));
            writer.Raw(
                a_snapshot.Legacy->data(), a_snapshot.Legacy->size());
        }
        else
        {
            writer.U8(0);
        }

        // v7 (the once-per-day conflict gates): the per-world gate
        // section — the last day each pair had words and came to blows,
        // as (form A, form B, row day, fight day) tuples, stable across
        // sessions. Present only in v7+ records; older records end after
        // the legacy, and a restored world's pairs are free to row and
        // fight again that day — honest for a save made before the gate
        // could be remembered.
        writer.U32(static_cast<std::uint32_t>(a_gates.size()));

        for (const auto& gate : a_gates)
        {
            writer.U32(gate.FormA);
            writer.U32(gate.FormB);
            writer.U64(gate.RowDay);
            writer.U64(gate.FightDay);
        }

        // v8 (the burial stone): the per-world burial section — the
        // dead's form id and the day they died, as (form id, day)
        // tuples, stable across sessions. Present only in v8+ records;
        // older records end after the gates, and a corpse whose death
        // predates the burial book is simply left to the game (it may
        // already be cleaned up) — honest for a save made before the
        // stone existed.
        writer.U32(static_cast<std::uint32_t>(a_burials.size()));

        for (const auto& burial : a_burials)
        {
            writer.U32(burial.FormId);
            writer.U64(burial.DiedDay);
        }

        // v9 (the sick household stone): the per-world medicine-stock
        // section — each market's doses left today, as (market form id,
        // stock) pairs, stable across sessions. Present only in v9+
        // records; older records end after the burials, and every stall
        // simply starts the day at full stock — honest for a save made
        // before a stall could sell out.
        writer.U32(static_cast<std::uint32_t>(a_medicineStock.size()));

        for (const auto& entry : a_medicineStock)
        {
            writer.U32(entry.MarketFormId);
            writer.U32(entry.Stock);
        }

        return writer.Bytes;
    }

    bool Decode(
        const std::vector<std::byte>& a_record,
        RegistrySnapshot& a_out,
        std::uint64_t& a_rngState,
        std::vector<StallKeeperPair>& a_stallKeepers,
        std::vector<BondPair>& a_bonds,
        std::vector<ConflictGatePair>& a_gates,
        std::vector<BurialEntry>& a_burials,
        std::vector<MedicineStockPair>& a_medicineStock)
    {
        Codec::Reader reader{ a_record };

        // Header: record version, core snapshot version, entity count,
        // then (v2) the Rng state.
        if (reader.Remaining() < 12)
        {
            return false;
        }

        const auto recordVersion = reader.U32();

        if (recordVersion > kRecordVersion)
        {
            return false;   // a newer format is not ours to guess — refuse
        }

        a_out.Version = reader.U32();
        const auto entityCount = reader.U32();

        // v2 carries the Rng state in the header; older records leave the
        // caller's default (a fresh stream) untouched.
        if (recordVersion >= 2)
        {
            if (reader.Remaining() < 8)
            {
                return false;
            }

            a_rngState = reader.U64();
        }

        a_out.Entities.clear();
        a_out.Entities.reserve(entityCount);

        for (std::uint32_t i = 0; i < entityCount; ++i)
        {
            // Entity: id (8) + component count (4).
            if (reader.Remaining() < 12)
            {
                return false;
            }

            SnapshotEntity entity;
            entity.Id = EntityId{ reader.U64() };
            const auto componentCount = reader.U32();

            for (std::uint32_t j = 0; j < componentCount; ++j)
            {
                if (reader.Remaining() < 1)
                {
                    return false;
                }

                const auto nameLength = reader.U8();

                if (reader.Remaining() < nameLength)
                {
                    return false;
                }

                const auto nameBytes = reader.Raw(nameLength);
                const std::string_view name{
                    reinterpret_cast<const char*>(nameBytes.data()),
                    nameBytes.size() };

                const auto type = TypeForName(name);

                if (reader.Remaining() < 4)
                {
                    return false;
                }

                const auto dataLength = reader.U32();

                if (reader.Remaining() < dataLength)
                {
                    return false;
                }

                auto data = reader.Raw(dataLength);

                // v4 (the world-calendar stone): MemoryEvent now carries
                // its world day, and the memory component's payload
                // gained a U64 day after the weight. A pre-v4 record's
                // blobs predate the field — migrate them here so the
                // version-blind component deserializer never sees two
                // formats. Day = 0 is "time immemorial": the sim treats
                // an unstamped fact as ancient, which is honest for a
                // memory saved before the calendar existed.
                if (recordVersion < 4 && name == "memory")
                {
                    Codec::Reader legacy{ data };

                    if (legacy.Remaining() < 4)
                    {
                        return false;
                    }

                    const auto eventCount = legacy.U32();
                    Codec::Writer migrated;
                    migrated.U32(eventCount);

                    for (std::uint32_t k = 0; k < eventCount; ++k)
                    {
                        // Old layout: U64 other + U32 kind + F weight
                        // (8 + 4 + 4 = 16 bytes).
                        if (legacy.Remaining() < 16)
                        {
                            return false;
                        }

                        migrated.U64(legacy.U64());
                        migrated.U32(legacy.U32());
                        migrated.F(legacy.F());
                        migrated.U64(0);   // no day in the old format
                    }

                    data = std::move(migrated.Bytes);
                }

                if (type == typeid(void))
                {
                    // A component this build no longer knows — a removed
                    // type from an older record (migration, 0.4.0). Its
                    // bytes are consumed and dropped; the entity keeps
                    // everything else. A newer record never reaches here:
                    // the version gate above refuses it.
                    continue;
                }

                entity.Components.push_back(
                    SnapshotComponent{ type, std::move(data) });
            }

            a_out.Entities.push_back(std::move(entity));
        }

        // v3 (the stall-keepers stone): the per-world stall section
        // follows the entities. Older records have no section — the
        // caller's stall list stands empty, and the market's stall
        // re-derives on the first arrival (a safe default, like a missing
        // component).
        a_stallKeepers.clear();

        if (recordVersion >= 3)
        {
            if (reader.Remaining() < 4)
            {
                return false;
            }

            const auto stallCount = reader.U32();
            a_stallKeepers.reserve(stallCount);

            for (std::uint32_t i = 0; i < stallCount; ++i)
            {
                if (reader.Remaining() < 8)
                {
                    return false;
                }

                const auto marketFormId = reader.U32();
                const auto keeperFormId = reader.U32();

                a_stallKeepers.emplace_back(marketFormId, keeperFormId);
            }
        }

        // v5 (the bonds stone): the per-world bond section follows the
        // stalls. Older records have no section — the caller's bond list
        // stands empty, and the adapter's reconcile pass re-derives bonds
        // from the restored relationships (a safe default, like a missing
        // component). A malformed entry (a self-pair, an unknown kind) is
        // skipped, not fatal — same tolerance as an unknown component
        // name; truncation is still a refusal.
        a_bonds.clear();

        if (recordVersion >= 5)
        {
            if (reader.Remaining() < 4)
            {
                return false;
            }

            const auto bondCount = reader.U32();
            a_bonds.reserve(bondCount);

            for (std::uint32_t i = 0; i < bondCount; ++i)
            {
                if (reader.Remaining() < 20)   // 4 + 4 + 4 + 8
                {
                    return false;
                }

                BondPair bond;
                bond.FormA = reader.U32();
                bond.FormB = reader.U32();
                bond.Kind = reader.U32();
                bond.SinceDay = reader.U64();

                const auto kind = static_cast<Bonds::BondKind>(bond.Kind);

                if (bond.FormA == 0 || bond.FormB == 0
                    || bond.FormA == bond.FormB
                    || kind < Bonds::BondKind::None
                    || kind > Bonds::BondKind::Spouse)
                {
                    continue;   // malformed — skipped, never half-applied
                }

                a_bonds.push_back(bond);
            }
        }

        // v6 (the identity stone): the registry-level legacy section
        // follows the bonds. Older records have no section — the
        // snapshot's legacy stays empty (no facts, a safe default, like
        // a missing component). Truncation is still a refusal.
        if (recordVersion >= 6)
        {
            if (reader.Remaining() < 1)
            {
                return false;
            }

            const auto hasLegacy = reader.U8() != 0;

            if (hasLegacy)
            {
                if (reader.Remaining() < 4)
                {
                    return false;
                }

                const auto legacyLength = reader.U32();

                if (reader.Remaining() < legacyLength)
                {
                    return false;
                }

                a_out.Legacy = reader.Raw(legacyLength);
            }
        }

        // v7 (the once-per-day conflict gates): the per-world gate
        // section follows the legacy. Older records have no section —
        // the caller's gate list stands empty, and a restored world's
        // pairs are free to row and fight again that day (a safe
        // default, like a missing component). A malformed entry (a
        // self-pair) is skipped, not fatal — same tolerance as an
        // unknown component name; truncation is still a refusal.
        a_gates.clear();

        if (recordVersion >= 7)
        {
            if (reader.Remaining() < 4)
            {
                return false;
            }

            const auto gateCount = reader.U32();
            a_gates.reserve(gateCount);

            for (std::uint32_t i = 0; i < gateCount; ++i)
            {
                if (reader.Remaining() < 24)   // 4 + 4 + 8 + 8
                {
                    return false;
                }

                ConflictGatePair gate;
                gate.FormA = reader.U32();
                gate.FormB = reader.U32();
                gate.RowDay = reader.U64();
                gate.FightDay = reader.U64();

                if (gate.FormA == 0 || gate.FormB == 0
                    || gate.FormA == gate.FormB)
                {
                    continue;   // malformed — skipped, never half-applied
                }

                a_gates.push_back(gate);
            }
        }

        // v8 (the burial stone): the per-world burial section follows
        // the gates. Older records have no section — the caller's burial
        // list stands empty, and a corpse whose death predates the book
        // is left to the game (a safe default, like a missing component).
        // A malformed entry (a zero form id) is skipped, not fatal — same
        // tolerance as an unknown component name; truncation is still a
        // refusal.
        a_burials.clear();

        if (recordVersion >= 8)
        {
            if (reader.Remaining() < 4)
            {
                return false;
            }

            const auto burialCount = reader.U32();
            a_burials.reserve(burialCount);

            for (std::uint32_t i = 0; i < burialCount; ++i)
            {
                if (reader.Remaining() < 12)   // 4 + 8
                {
                    return false;
                }

                BurialEntry burial;
                burial.FormId = reader.U32();
                burial.DiedDay = reader.U64();

                if (burial.FormId == 0)
                {
                    continue;   // malformed — skipped, never half-applied
                }

                a_burials.push_back(burial);
            }
        }

        // v9 (the sick household stone): the per-world medicine-stock
        // section follows the burials. Older records have no section —
        // the caller's stock list stands empty, and every stall starts
        // the day at full stock (a safe default, like a missing
        // component). A malformed entry (a zero market form id) is
        // skipped, not fatal — same tolerance as an unknown component
        // name; truncation is still a refusal.
        a_medicineStock.clear();

        if (recordVersion >= 9)
        {
            if (reader.Remaining() < 4)
            {
                return false;
            }

            const auto stockCount = reader.U32();
            a_medicineStock.reserve(stockCount);

            for (std::uint32_t i = 0; i < stockCount; ++i)
            {
                if (reader.Remaining() < 8)   // 4 + 4
                {
                    return false;
                }

                MedicineStockPair entry;
                entry.MarketFormId = reader.U32();
                entry.Stock = reader.U32();

                if (entry.MarketFormId == 0)
                {
                    continue;   // malformed — skipped, never half-applied
                }

                a_medicineStock.push_back(entry);
            }
        }

        return true;
    }
}
