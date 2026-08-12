//=============================================================================//
//                                                                             //
//   The Living Commonwealth — Fallout 4 adapter for the Living Commonwealth   //
//   Engine (LCE).                                                             //
//                                                                             //
//   Capture now, restore later — the mind keeps receipts.                                      //
//                                                                             //
//=============================================================================//

#include "Serialization.h"

#include "BlobCodec.h"
#include "Components.h"

#include "LCE/Simulation/Decision/Behaviour.h"
#include "LCE/Simulation/Entity/EntityRegistry.h"
#include "LCE/Simulation/Mind/Goals.h"
#include "LCE/Simulation/Decision/Legacy.h"
#include "LCE/Simulation/Mind/Memory.h"
#include "LCE/Simulation/Mind/Needs.h"
#include "LCE/Simulation/Mind/Relationships.h"

#include <string>
#include <unordered_map>

namespace TLC
{
    namespace
    {
        using namespace LCE::Simulation;

        //-------------------------------------------------------------------------
        // One serializer per persisted type. Encodings are the adapter's
        // own (little-endian, via TLC::Codec) — the core never interprets
        // the bytes; save-compatibility is the adapter's job.
        //-------------------------------------------------------------------------
        ComponentSerializer<Needs> MakeNeedsSerializer()
        {
            return {
                [](const Needs& needs)
                {
                    Codec::Writer writer;
                    writer.U32(static_cast<std::uint32_t>(needs.List.size()));

                    for (const auto& need : needs.List)
                    {
                        writer.U32(static_cast<std::uint32_t>(need.Type));
                        writer.F(need.Value);
                        writer.F(need.DecayRate);
                    }

                    return writer.Bytes;
                },
                [](const ComponentBlob& blob)
                {
                    Codec::Reader reader{ blob };
                    Needs needs;

                    const auto count = reader.U32();

                    for (std::uint32_t i = 0; i < count; ++i)
                    {
                        needs.List.push_back(Need{
                            static_cast<NeedType>(reader.U32()),
                            reader.F(),
                            reader.F() });
                    }

                    return needs;
                }
            };
        }

        ComponentSerializer<Memory> MakeMemorySerializer()
        {
            return {
                [](const Memory& memory)
                {
                    Codec::Writer writer;
                    writer.U32(static_cast<std::uint32_t>(memory.Events.size()));

                    for (const auto& event : memory.Events)
                    {
                        writer.U64(event.Other.Value());
                        writer.U32(static_cast<std::uint32_t>(event.Kind));
                        writer.F(event.Weight);

                        // v4 (the world-calendar stone): the world day
                        // this was remembered — a restored memory keeps
                        // its day, so "the age of a fact" survives
                        // save/load. Pre-v4 records migrate in CoSave
                        // (day = 0 — time immemorial).
                        writer.U64(event.Day);
                    }

                    return writer.Bytes;
                },
                [](const ComponentBlob& blob)
                {
                    Codec::Reader reader{ blob };
                    Memory memory;

                    const auto count = reader.U32();

                    for (std::uint32_t i = 0; i < count; ++i)
                    {
                        memory.Events.push_back(MemoryEvent{
                            EntityId{ reader.U64() },
                            static_cast<InteractionKind>(reader.U32()),
                            reader.F(),
                            reader.U64() });
                    }

                    return memory;
                }
            };
        }

        ComponentSerializer<Relationships> MakeRelationshipsSerializer()
        {
            return {
                [](const Relationships& relationships)
                {
                    Codec::Writer writer;
                    writer.U32(static_cast<std::uint32_t>(
                        relationships.ByEntity.size()));

                    for (const auto& [other, relationship] : relationships.ByEntity)
                    {
                        writer.U64(other.Value());
                        writer.F(relationship.Disposition);
                        writer.F(relationship.Trust);
                    }

                    return writer.Bytes;
                },
                [](const ComponentBlob& blob)
                {
                    Codec::Reader reader{ blob };
                    Relationships relationships;

                    const auto count = reader.U32();

                    for (std::uint32_t i = 0; i < count; ++i)
                    {
                        const auto other = EntityId{ reader.U64() };

                        relationships.ByEntity[other] =
                            Relationship{ reader.F(), reader.F() };
                    }

                    return relationships;
                }
            };
        }

        ComponentSerializer<Goals> MakeGoalsSerializer()
        {
            return {
                [](const Goals& goals)
                {
                    Codec::Writer writer;
                    writer.U32(goals.Active ? 1u : 0u);

                    if (goals.Active)
                    {
                        writer.U32(static_cast<std::uint32_t>(goals.Active->Type));
                        writer.F(goals.Active->Urgency);
                    }

                    return writer.Bytes;
                },
                [](const ComponentBlob& blob)
                {
                    Codec::Reader reader{ blob };
                    Goals goals;

                    if (reader.U32() != 0)
                    {
                        goals.Active = Goal{
                            static_cast<GoalType>(reader.U32()),
                            reader.F() };
                    }

                    return goals;
                }
            };
        }

        ComponentSerializer<Intent> MakeIntentSerializer()
        {
            return {
                [](const Intent& intent)
                {
                    Codec::Writer writer;
                    writer.U32(static_cast<std::uint32_t>(intent.Action));
                    writer.U64(intent.Target.Value());
                    writer.F(intent.Confidence);

                    return writer.Bytes;
                },
                [](const ComponentBlob& blob)
                {
                    Codec::Reader reader{ blob };

                    return Intent{
                        static_cast<ActionType>(reader.U32()),
                        EntityId{ reader.U64() },
                        reader.F() };
                }
            };
        }

        ComponentSerializer<FormRef> MakeFormRefSerializer()
        {
            return {
                [](const FormRef& formRef)
                {
                    Codec::Writer writer;
                    writer.U32(formRef.FormId);

                    return writer.Bytes;
                },
                [](const ComponentBlob& blob)
                {
                    Codec::Reader reader{ blob };

                    return FormRef{ reader.U32() };
                }
            };
        }

        ComponentSerializer<SpeciesTag> MakeSpeciesTagSerializer()
        {
            return {
                [](const SpeciesTag& tag)
                {
                    Codec::Writer writer;
                    writer.U32(static_cast<std::uint32_t>(tag.Value));

                    return writer.Bytes;
                },
                [](const ComponentBlob& blob)
                {
                    Codec::Reader reader{ blob };

                    return SpeciesTag{ static_cast<Species>(reader.U32()) };
                }
            };
        }

        ComponentSerializer<CapPouch> MakeCapPouchSerializer()
        {
            return {
                [](const CapPouch& pouch)
                {
                    Codec::Writer writer;
                    writer.U32(pouch.Caps);

                    return writer.Bytes;
                },
                [](const ComponentBlob& blob)
                {
                    Codec::Reader reader{ blob };

                    return CapPouch{ reader.U32() };
                }
            };
        }

        ComponentSerializer<Name> MakeNameSerializer()
        {
            return {
                [](const Name& name)
                {
                    Codec::Writer writer;
                    writer.U32(static_cast<std::uint32_t>(name.Full.size()));
                    writer.Raw(name.Full.data(), name.Full.size());

                    return writer.Bytes;
                },
                [](const ComponentBlob& blob)
                {
                    Codec::Reader reader{ blob };
                    const auto length = reader.U32();
                    const auto bytes = reader.Raw(length);

                    return Name{ std::string(
                        reinterpret_cast<const char*>(bytes.data()),
                        bytes.size()) };
                }
            };
        }

        // The registry-level legacy store (0.7.0 stone 12, engine side):
        // facts keyed by name — "the miller's pledge". One serializer,
        // registered once at init, rides the co-save's v6 section like the
        // component stores ride their rows.
        ComponentSerializer<std::unordered_map<std::string, LegacyFact>>
        MakeLegacySerializer()
        {
            return {
                [](const std::unordered_map<std::string, LegacyFact>& facts)
                {
                    Codec::Writer writer;
                    writer.U32(static_cast<std::uint32_t>(facts.size()));

                    for (const auto& [key, fact] : facts)
                    {
                        writer.U32(static_cast<std::uint32_t>(key.size()));
                        writer.Raw(key.data(), key.size());

                        writer.U64(fact.Owner.Value());
                        writer.U64(fact.Day);
                        writer.U32(static_cast<std::uint32_t>(fact.Name.size()));
                        writer.Raw(fact.Name.data(), fact.Name.size());
                        writer.F(fact.Weight);
                    }

                    return writer.Bytes;
                },
                [](const ComponentBlob& blob)
                {
                    Codec::Reader reader{ blob };
                    std::unordered_map<std::string, LegacyFact> facts;

                    const auto count = reader.U32();
                    facts.reserve(count);

                    for (std::uint32_t i = 0; i < count; ++i)
                    {
                        const auto keyLength = reader.U32();
                        const auto keyBytes = reader.Raw(keyLength);
                        const auto key = std::string(
                            reinterpret_cast<const char*>(keyBytes.data()),
                            keyBytes.size());

                        LegacyFact fact;
                        fact.Owner = EntityId{ reader.U64() };
                        fact.Day = reader.U64();

                        const auto nameLength = reader.U32();
                        const auto nameBytes = reader.Raw(nameLength);
                        fact.Name = std::string(
                            reinterpret_cast<const char*>(nameBytes.data()),
                            nameBytes.size());
                        fact.Weight = reader.F();

                        facts[std::move(key)] = std::move(fact);
                    }

                    return facts;
                }
            };
        }
    }

    void RegisterAllSerializers(EntityRegistry& registry)
    {
        registry.RegisterSerializer<Needs>(MakeNeedsSerializer());
        registry.RegisterSerializer<Memory>(MakeMemorySerializer());
        registry.RegisterSerializer<Relationships>(MakeRelationshipsSerializer());
        registry.RegisterSerializer<Goals>(MakeGoalsSerializer());
        registry.RegisterSerializer<Intent>(MakeIntentSerializer());
        registry.RegisterSerializer<FormRef>(MakeFormRefSerializer());
        registry.RegisterSerializer<SpeciesTag>(MakeSpeciesTagSerializer());
        registry.RegisterSerializer<CapPouch>(MakeCapPouchSerializer());
        registry.RegisterSerializer<Name>(MakeNameSerializer());

        // The registry-level legacy store — the co-save's v6 section.
        registry.RegisterLegacySerializer(MakeLegacySerializer());
    }
}
