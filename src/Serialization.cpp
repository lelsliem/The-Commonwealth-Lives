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

#include "LCE/Simulation/Behaviour.h"
#include "LCE/Simulation/EntityRegistry.h"
#include "LCE/Simulation/Goals.h"
#include "LCE/Simulation/Memory.h"
#include "LCE/Simulation/Needs.h"
#include "LCE/Simulation/Relationships.h"

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
                            reader.F() });
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
    }
}
