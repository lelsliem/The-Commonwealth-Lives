//=============================================================================//
//                                                                             //
//   The Living Commonwealth — Fallout 4 adapter for the Living Commonwealth   //
//   Engine (LCE).                                                             //
//                                                                             //
//   A co-save is a promise: the world remembers where it left off.                                      //
//                                                                             //
//=============================================================================//

#pragma once

#include "LCE/Simulation/Entity/RegistrySnapshot.h"

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
    // Additive schema changes — a new component type, like `species` or
    // `cappouch` — never bump: the format is self-describing (each
    // component is stored under its stable name), so an old record simply
    // decodes without the new component and the safe default applies (a
    // missing SpeciesTag reads as Human). v2 (the decay-jitter wiring)
    // added the Rng state to the header; v3 (the stall-keepers stone)
    // added the per-world stall section after the entities; v4 (the
    // world-calendar stone) added the world day to every memory event;
    // v5 (the bonds stone) added the per-world bond section after the
    // stalls; v6 (the identity stone) added the registry-level legacy
    // section after the bonds — the dead's stories, which the names
    // stone keys by name; v7 (the once-per-day conflict gates) added
    // the per-world gate section after the legacy — the last day each
    // pair had words and came to blows, so a feud stays a once-a-day
    // scene across save/load — all real format changes, so all bumped.
    // The `name` component itself never bumps (a new component is
    // additive; an old record simply decodes without it and names are
    // back-filled on restore). Older versions are migrated forward on
    // load, never dropped.
    // v8 (the burial stone) added the per-world burial section after
    // the gates - the dead and the day they died, so a corpse is laid
    // to rest once the mourning window passes, even across save/load.
    // v9 (the sick household stone) added the per-world medicine-stock
    // section after the burials - each market's doses left today, so
    // a stall that sold out stays sold out across save/load until the
    // next market day refills it. v10 (the 0.8.9 birth journey)
    // added the per-world baby-hold section after the medicine stock
    // - each held newborn, so a mother mid-carry keeps her visible
    // bundle across save/load. v11 (the 0.8.9 visible-child stone)
    // added the per-world visible-child section after the baby holds -
    // each (mother, pending child actor) pair, so a child waiting to
    // materialize survives the very load that completes it.
    inline constexpr std::uint32_t kRecordVersion = 11;

    // A market's stall-keeper, in the durable form: the market's
    // workbench FormID and the keeper's actor FormID — form ids, not
    // entity ids, because entity ids are session-local (reassigned per
    // world). The adapter translates at the edges (StallKeepersForSave /
    // RestoreStallKeepers).
    using StallKeeperPair = std::pair<std::uint32_t, std::uint32_t>;

    // A bond, in the durable form (v5): the two form ids (stable across
    // sessions, entity ids are session-local), the kind ordinal (the
    // adapter's Bonds::BondKind — append-only, never reorder), and the
    // world day the bond formed. The adapter translates at the edges
    // (BondsForSave / RestoreBonds).
    struct BondPair
    {
        std::uint32_t FormA = 0;
        std::uint32_t FormB = 0;
        std::uint32_t Kind = 0;
        std::uint64_t SinceDay = 0;
    };

    // A pair's once-per-day conflict gates, in the durable form (v7):
    // the two form ids (stable across sessions, entity ids are
    // session-local) and the last world day they had words (RowDay) and
    // came to blows (FightDay) — 0 when never. The gates ride the
    // co-save so a feud stays a single scene per day across save/load.
    // The adapter translates at the edges (ConflictGatesForSave /
    // RestoreConflictGates).
    struct ConflictGatePair
    {
        std::uint32_t FormA = 0;
        std::uint32_t FormB = 0;
        std::uint64_t RowDay = 0;
        std::uint64_t FightDay = 0;
    };

    // A burial, in the durable form (v8): the dead actor's FormID and
    // the world day they died. Form ids are stable across sessions
    // (entity ids are session-local). The adapter records one at death
    // and the burial sweep disables the corpse ref once the mourning
    // window (`sim.death.burialDays`) passes — even if the window
    // expires while the game is away. The adapter translates at the
    // edges (BurialsForSave / RestoreBurials).
    struct BurialEntry
    {
        std::uint32_t FormId = 0;
        std::uint64_t DiedDay = 0;
    };

    // A market's medicine left today, in the durable form (v9): the
    // market's FormID (a workshop bench, or a person-market's actor)
    // and the doses still on the shelf. Form ids are stable across
    // sessions (entity ids are session-local). The adapter translates
    // at the edges (MedicineStockForSave / RestoreMedicineStock) and
    // refills the book on each market-open transition.
    struct MedicineStockPair
    {
        std::uint32_t MarketFormId = 0;
        std::uint32_t Stock = 0;
    };

    // A held newborn, in the durable form (v10, 0.8.9 birth journey):
    // the mother's FormID, the baby-bundle ARMO she is visibly holding,
    // and the world day the child was born. Form ids are stable across
    // sessions (entity ids are session-local). The adapter translates at
    // the edges (BabyHoldsForSave / RestoreBabyHolds); each day the
    // hold advances — when the day passes sim.baby.holdDays, the
    // bundle comes off and a child from the game's own pool takes its
    // place. A hold left mid-carry survives save/load.
    struct BabyHold
    {
        std::uint32_t MotherFormId = 0;
        std::uint32_t BundleFormId = 0;
        std::uint64_t BornDay = 0;
    };

    // A visible child, in the durable form (v11, 0.8.9 deferred-spawn
    // find): the mother's FormID and the child actor spawned at her
    // feet once the bundle comes off. The child is deliberately left
    // un-initialized — invisible until the game's own load routine
    // completes it (facegen, AI process, animation) and it steps out
    // fully real; its base's default outfit (defOutfit, a ChildOutfit*
    // OTFT bundle) is set at spawn so the game dresses it, and the
    // per-tick pass confirms/re-equips once it reads initialized. Form
    // ids are stable across sessions (entity ids are session-local).
    // The adapter translates at the edges
    // (VisualChildrenForSave / RestoreVisualChildren).
    struct VisualChild
    {
        std::uint32_t MotherFormId = 0;
        std::uint32_t FigureFormId = 0;
        std::uint64_t BornDay = 0;
    };

    // Encodes a registry snapshot as the durable record bytes: stable
    // component names (never std::type_index), the core's snapshot version
    // layered underneath, the record's own version, and the Rng state (v2)
    // so a restored world resumes the exact same randomness. Pure — no
    // game types cross it, so it is testable without the game.
    [[nodiscard]] std::vector<std::byte> Encode(
        const LCE::Simulation::RegistrySnapshot& a_snapshot,
        std::uint64_t a_rngState,
        const std::vector<StallKeeperPair>& a_stallKeepers,
        const std::vector<BondPair>& a_bonds,
        const std::vector<ConflictGatePair>& a_gates,
        const std::vector<BurialEntry>& a_burials,
        const std::vector<MedicineStockPair>& a_medicineStock,
        const std::vector<BabyHold>& a_babyHolds = {},
        const std::vector<VisualChild>& a_visualChildren = {});

    // Decodes record bytes back into a snapshot. Returns false — the load
    // is refused, never half-applied — when the record version is newer
    // than this build (a future format is not ours to guess), or the
    // bytes are truncated. Older versions load forward: a component name
    // this build does not know (a type a later build removed) is skipped
    // and dropped — the entity keeps everything else — which is how a
    // breaking removal migrates. The migration is invisible because the
    // format is self-describing; the version gate is the only seam.
    //
    // a_rngState is an in-out: the caller pre-seeds it with the default
    // (a fresh stream), and Decode overwrites it only when the record
    // carried one (v2 and newer). A v1 record — saved before the
    // decay-jitter wiring — restores with the default stream, which is
    // honest: its world never had a saved stream to resume.
    bool Decode(
        const std::vector<std::byte>& a_record,
        LCE::Simulation::RegistrySnapshot& a_out,
        std::uint64_t& a_rngState,
        std::vector<StallKeeperPair>& a_stallKeepers,
        std::vector<BondPair>& a_bonds,
        std::vector<ConflictGatePair>& a_gates,
        std::vector<BurialEntry>& a_burials,
        std::vector<MedicineStockPair>& a_medicineStock,
        std::vector<BabyHold>* a_babyHolds = nullptr,
        std::vector<VisualChild>* a_visualChildren = nullptr);
}
