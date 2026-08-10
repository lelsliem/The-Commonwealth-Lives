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

        const std::array<TypeName, 7>& TypeNames()
        {
            static const std::array<TypeName, 7> kTable{
                TypeName{ "needs", typeid(Needs) },
                TypeName{ "memory", typeid(Memory) },
                TypeName{ "relationships", typeid(Relationships) },
                TypeName{ "goals", typeid(Goals) },
                TypeName{ "intent", typeid(Intent) },
                TypeName{ "formref", typeid(FormRef) },
                TypeName{ "species", typeid(SpeciesTag) },
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

    std::vector<std::byte> Encode(const RegistrySnapshot& a_snapshot)
    {
        Codec::Writer writer;

        writer.U32(kRecordVersion);
        writer.U32(a_snapshot.Version);
        writer.U32(static_cast<std::uint32_t>(a_snapshot.Entities.size()));

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

        return writer.Bytes;
    }

    bool Decode(const std::vector<std::byte>& a_record, RegistrySnapshot& a_out)
    {
        Codec::Reader reader{ a_record };

        // Header: record version, core snapshot version, entity count.
        if (reader.Remaining() < 12)
        {
            return false;
        }

        if (reader.U32() > kRecordVersion)
        {
            return false;   // a newer format is not ours to guess — refuse
        }

        a_out.Version = reader.U32();
        const auto entityCount = reader.U32();

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

        return true;
    }
}
