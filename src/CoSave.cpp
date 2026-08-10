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
#include "Components.h"

#include "LCE/Simulation/Behaviour.h"
#include "LCE/Simulation/EntityId.h"
#include "LCE/Simulation/Goals.h"
#include "LCE/Simulation/Memory.h"
#include "LCE/Simulation/Needs.h"
#include "LCE/Simulation/Relationships.h"

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

        const std::array<TypeName, 8>& TypeNames()
        {
            static const std::array<TypeName, 8> kTable{
                TypeName{ "needs", typeid(Needs) },
                TypeName{ "memory", typeid(Memory) },
                TypeName{ "relationships", typeid(Relationships) },
                TypeName{ "goals", typeid(Goals) },
                TypeName{ "intent", typeid(Intent) },
                TypeName{ "formref", typeid(FormRef) },
                TypeName{ "species", typeid(SpeciesTag) },
                TypeName{ "cappouch", typeid(CapPouch) },
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
        const std::vector<StallKeeperPair>& a_stallKeepers)
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

        return writer.Bytes;
    }

    bool Decode(
        const std::vector<std::byte>& a_record,
        RegistrySnapshot& a_out,
        std::uint64_t& a_rngState,
        std::vector<StallKeeperPair>& a_stallKeepers)
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

        return true;
    }
}
