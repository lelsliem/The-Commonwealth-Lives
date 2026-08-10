//=============================================================================//
//                                                                             //
//   The Living Commonwealth — Fallout 4 adapter for the Living Commonwealth   //
//   Engine (LCE).                                                             //
//                                                                             //
//   A co-save is a promise: the world remembers where it left off.                                      //
//                                                                             //
//=============================================================================//

#pragma once

#include "LCE/Simulation/RegistrySnapshot.h"

#include <cstdint>
#include <vector>

namespace TLC::CoSave
{
    //-------------------------------------------------------------------------
    // The durable record (0.4.0 — "The World Remembers"). The core's
    // snapshot is a process-local exchange format: its component keys are
    // std::type_index, whose values are addresses — meaningless in the
    // next session. The adapter's record is what actually rides inside the
    // save file: the same data with the adapter's own stable component
    // names and its own versioning. Save-compatibility is the adapter's
    // job (core 0.4.0 contract) — the names below are chosen once and
    // never renamed; a rename is a schema change, migrate instead.
    //-------------------------------------------------------------------------

    // The plugin's identity in the co-save. F4SE assigns unique IDs to
    // plugins; the value below is a placeholder to be replaced with the
    // assigned UID before release (the author-handle TODO in xmake.lua is
    // the same open item). The record type names this plugin's records
    // inside its co-save file; both use the same fourcc for now.
    inline constexpr std::uint32_t kSerializationUid = 0x4C434557;   // 'LCEW'
    inline constexpr std::uint32_t kRecordType      = 0x4C434557;   // 'LCEW'

    // The adapter's record schema version. Bumped only on a breaking
    // change to the record *format* (the header, the per-entity layout).
    // Additive schema changes — a new component type, like `species` —
    // never bump: the format is self-describing (each component is
    // stored under its stable name), so an old record simply decodes
    // without the new component and the safe default applies (a missing
    // SpeciesTag reads as Human). Older versions are migrated forward on
    // load, never dropped.
    inline constexpr std::uint32_t kRecordVersion = 1;

    // Encodes a registry snapshot as the durable record bytes: stable
    // component names (never std::type_index), the core's snapshot version
    // layered underneath, and the record's own version. Pure — no game
    // types cross it, so it is testable without the game.
    [[nodiscard]] std::vector<std::byte> Encode(
        const LCE::Simulation::RegistrySnapshot& a_snapshot);

    // Decodes record bytes back into a snapshot. Returns false — the load
    // is refused, never half-applied — when the record version is newer
    // than this build (a future format is not ours to guess), or the
    // bytes are truncated. Older versions load forward: a component name
    // this build does not know (a type a later build removed) is skipped
    // and dropped — the entity keeps everything else — which is how a
    // breaking removal migrates. The migration is invisible because the
    // format is self-describing; the version gate is the only seam.
    bool Decode(
        const std::vector<std::byte>& a_record,
        LCE::Simulation::RegistrySnapshot& a_out);
}
