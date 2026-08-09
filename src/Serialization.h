//=============================================================================//
//                                                                             //
//   The Living Commonwealth — Fallout 4 adapter for the Living Commonwealth   //
//   Engine (LCE).                                                             //
//                                                                             //
//   QUOTE: <the author's line goes here>                                      //
//                                                                             //
//=============================================================================//

#pragma once

namespace LCE::Simulation
{
    class EntityRegistry;
}

namespace TLC
{
    //-------------------------------------------------------------------------
    // Registers the adapter's serializers for every component type it
    // persists (Needs, Memory, Relationships, Goals, Intent, FormRef).
    // Call once at plugin init, before any world exists — the registrations
    // survive Clear() and are required by Restore (core 0.4.0 contract).
    //-------------------------------------------------------------------------
    void RegisterAllSerializers(LCE::Simulation::EntityRegistry& registry);
}
