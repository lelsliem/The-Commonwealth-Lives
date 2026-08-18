# The Living Commonwealth — Fallout 4 Adapter Project

**Handoff document.** This file exists so a new agent (a new Freebuff tab
rooted at `C:\Fallout4Adaption`) can start this project with full context —
what exists, how it is wired, and what is next — without having been part
of the conversation that built it.

**Companion documents** (in the LCE core repo, `C:\LivingCommonwealthEngine`):
- `Docs/Architecture/PlatformIntegration.md` — the boundary contract this
  project implements (read this first)
- `Docs/ProjectPhilosophy.md` — the Six Design Laws, especially
  Law 001: *simple things; compose the complex*
- `Docs/LearningPath.md` — how the engine works

**Design docs in this repo** (`Docs/Design/`): `Translation.md` (form ↔
entity), `Executor.md` (the tick + intent executor), `Walking.md` (the
walk), `CoSave.md` (the co-save record — v7; v5's bond map and v3's
stall section still ride), `Behaviour.md` (the
species split), `Market.h`'s sibling `SettlementMarkets.md` (the census
+ per-settlement markets), `Trade.md` (the stall-keeper trade),
`Economy.md` (cap pouches), `Tuning.md` (the INI), `WorldFacts.md` (the
gates: market hours + weather), `WeatherFacts.md` (weather memory
events), `Identity.md` (0.7.0 — names, conflict source, the radio),
`RealEvents.md` (0.7.1-0.7.5 — talk, rows, names, trade, fights),
`FutureStones.md` (0.7.6-0.8.0 — fight-feel, babies, illness),
`ReleasePlan.md` (the staged path to 1.0.0, including the honest cuts),
`Run080.md` (the 0.8.x run to the Nexus beta — the working plan),
`InterSettlementTrade.md` (the roads candidate — designed, not
scheduled; the 0.8.6a audit weighs it),
`Landscape.md` (the Nexus survey — what exists, what we own, what to
borrow, what to skip), `RealTest.md` (the live log evidence). Each is
flipped to **verified in-game** as its stone lands; the ones that aren't
verified yet say so.

---

## The Project

**Name:** The Living Commonwealth (the mod). The Fallout 4 adapter for the
Living Commonwealth Engine (LCE). An F4SE plugin (GPL — it links
CommonLibF4) that is a *client* of LCE.Core (MIT). It does NOT
reimplement simulation; per `PlatformIntegration.md` the boundary is the
core's public API: the adapter calls `CreateEntity` / `DestroyEntity` /
`Remember` / `Update` and reads intents via `GetComponent<Intent>`.

**The contract's guarantee:** *an intent is a hint, not a command* — the
adapter decides how to walk the settler, and may refuse. The core never
names a game action and never appears in the adapter's game-facing code.

**The game setup:** Fallout 4 1.11.221 (Next-Gen), F4SE 0.7.8, run through
MO2 (`B:\Modding\MO2`). The plugin logs to
`C:\Users\mrlma\Documents\My Games\Fallout4\F4SE\TheLivingCommonwealth.log`.

---

## Status

| Version | Stone | Status |
|---------|-------|--------|
| 0.1.0 | Scaffold + heartbeat | ✅ verified in-game |
| 0.2.0 | Translation | ✅ verified in-game |
| 0.3.0 | Intent executor | ✅ verified in-game |
| 0.4.0 | Co-save | ✅ verified in-game |
| 0.5.0 | Living world | ✅ complete (tag `0.5.0-beta`) |
| 0.6.0 | Life & Emergent Quests | ✅ complete (tag `0.6.0`) |
| 0.7.0 | Identity & the Player Window | ✅ complete (tag `0.7.0`) |
| 0.7.1 | Talk | ✅ verified in-game |
| 0.7.2 | Rows | ✅ verified in-game |
| 0.7.3 | Names for everyone | ✅ verified in-game |
| 0.7.4 | Trade with anyone | ✅ verified in-game |
| 0.7.5 | Fights | ✅ verified in-game |
| 0.7.6 | Fight-feel bug pass | ✅ verified in-game (2026-08-13) |
| 0.7.7 | Babies | ✅ verified in-game (2026-08-13) |
| 0.7.8 | Visible children pairing | ✅ verified in-game (2026-08-13) — SUPERSEDED 0.8.9 / DROPPED 0.8.12 (DecisionLog 0067): the visible child is all vanilla now (deferred spawn from the game's own pool) |
| 0.7.9 | Bugs & polish | ✅ complete (2026-08-13) |
| 0.8.0 | Illness & Medicine | ✅ verified in-game (2026-08-13) — natural radstorm, 73 medicine buys, 0 deaths |
| 0.8.1 | The Illness Field Pass | ✅ shipped — cough gate, burst-paced news, sickness takes the body, children survive, co-save audit |
| 0.8.2 | The Burial | ✅ verified in-game (2026-08-14) |
| 0.8.3 | The Sick Household | ✅ verified in-game (2026-08-14) — stocked shelves + the family care-buy |
| 0.8.4 | Random Interactions | ✅ verified in-game (2026-08-14) — "added life" |
| 0.8.5 | MCM + Settings Manager | ✅ verified in-game (2026-08-14) — real values, hot-apply, persistence |
| 0.8.6a | The Audit | ✅ done — cut list re-tested; MCM KEPT, earn-caps + log hygiene to 0.8.6b |
| 0.8.6b | Redefine & Loose Ends | ✅ done — robots, the stipend, MCM Economy page; who-pays benched then unbenched |
| 0.8.6c | Scale in the Field | ✅ PASSED (2026-08-14) — TickReport, ~620 live minds, half a frame; ownership read unbenched (28/28 owned) |
| 0.8.7 | Dialog growth | ✅ verified in-game (2026-08-15) — curated catalog + the voice-aware picker; the in-world exchange (A-greets-B, both beats, one-at-a-time lock, per-pair cooldown, and the bond-named registers — family/flirt/greet, 0.8.9-register) replaced the text tail |
| 0.8.8 | Timings & Weights | ✅ built (2026-08-15) — sim.interact.pairCooldown / dailyCap / weight.greet·gossip·family·row in the INI + MCM Interactions page; field tuning pending |
| 0.8.9-road | Road feed | ✅ verified in-game (2026-08-15) — road people keep no settlement-market memory and eat from the caravan's supplies at sim.road.feedThreshold (`road: … ate on the road` ×6 on test); guards/traders keep their markets; road people still exchange with each other in passing (15 exchanges on test) — their bonds and memories travel with them |
| 0.8.9 | Babies, implemented | ✅ verified in-game (2026-08-16) — the birth journey v2: the mother carries a swaddled bundle (sim.baby.holdDays, default 2; the game's own Shaun bundle — the external mod was dropped entirely 0.8.12, DecisionLog 0067), then it comes off and a child spawns at her feet — deferred spawn (invisible, then real after a save/load: the only route that produces a fully-initialized child; forced init = headless/T-pose, PlaceAtMe = CTD), gender-matched from the farm-children bases, dressed via the base's default outfit (defOutfit, a ChildOutfit* OTFT bundle — the game's own child-clothing path; the earlier no-clothes bug was an OTFT-vs-ARMO cast miss, fixed 0.8.9-child), never in the settler faction (no duplicate mind), paired with the sim-only child; co-save v11 (BabyHold + VisualChild sections); the crib walk + market baby-goods shelf were cut; 27/27 green |
| 0.8.10 | Animations + fight-feel | 🔲 DEFERRED (2026-08-17) — left alone for now by decision; stays planned, revisited after the beta (kick works; the both-fall/ghost-push presentation bugs are documented in Run080) |
| 0.8.11 | Log hygiene + loose ends | ✅ done (2026-08-17) — the decision-chatter cap was already in place (key-dedupe + sim.log.decisions.every); the walk probe (sim.log.probes, the biggest writer) now defaults OFF; loose ends decided — unowned settlements keep minds, the who-is-the-world calls stand as designed (DecisionLog 0066) |
| 0.8.12 | Final Touches + beta | 🔄 in progress (2026-08-17) — version 0.8.12, warning-free build, docs reconciled (0.8.7 CHANGELOG restored, RELEASE_NOTES to 0.8.11, audio-probe verdict folded in), player README written, the log-level gate verified in-game (quiet log, intact show), the external Baby Sim dependency dropped entirely (DecisionLog 0067 — the visible journey is all vanilla); the release package + Nexus beta remain |
| 1.0.0 | Freeze and ship | 🔲 planned |

**Build:** `xmake` (one command). Two targets:
`TheLivingCommonwealth` (the DLL) and `TheLivingCommonwealth.Tests`
(the harness — links LCE.Core only, no game; run as
`xmake run TheLivingCommonwealth.Tests`). **27/27 suites green.**

---

## Architecture — how the adapter is wired

```
src/main.cpp        F4SE entry: hooks (Tick), messaging (lifecycle),
                    serialization callbacks (co-save glue)
src/Adapter.h/.cpp  the one world object: registry + translator, the
                    lifecycle, the tick loop (Update → plan → execute → probe),
                    species classification (race → Human/Child/Animal),
                    fight execution (paired-push kick, fall, retaliation),
                    birth lifecycle (pregnancy, growth, visible-children pairing)
src/Translator.h/.cpp  formId ↔ EntityId tables (adapter state, game-free)
src/SimRelevant.h/.cpp the settler predicate (WorkshopNPCFaction 0x000337F3)
src/Behaviour.h/.cpp   Species + BehaviourProfile — who trades, who is fed
                       (Human/Child/Animal); SeededNeeds(Species)
src/Executor.h/.cpp    pure plan builder — refusals are the contract
src/Movement.h/.cpp    WalkTo: the pinned Actor::InitiateCommandModeTravelPackage
src/Market.h           the census (persistent-cell scan) + per-settlement
                       market seeding (nearest workshop within ~140 m)
src/Names.h            name pools (gender-split, animal, family tails),
                       role names, the seeded picker
src/Dialogue.h         dialogue pools (greet/gossip/family/trade/row/
                       grief/fight/feud), INI overrides, seeded picker
src/Rows.h             verbal altercations at the bench, Wronged memories
src/Fights.h           physical escalation: paired-push kick, fall,
                       retaliation, once-per-day gate (ConflictGates)
src/Birth.h            pregnancy window, gestation, birth, growth,
                       visible-children pairing (runtime scan)
src/Illness.h          the sickness curve — hold, recover, death,
                       medicine (four vectors: radstorm, food,
                       wound, contagion)
src/Bonds.h            relationship states (friend/sweetheart/spouse/
                       rival/enemy), mutual + sticky derivation
src/Households.h       shared wallet, family bench
src/Gossip.h           bond/death/feud spread to settlement
src/Arcs.h             mediation, grief, birth arc
src/Kin.h              family gate (curated vanilla families)
src/Subtitles.h        on-screen fight lines (game's SubtitleManager)
src/ConflictGates.h    once-per-day fight/row gate (co-saved)
src/Serialization.h/.cpp  per-type serializers
src/CoSave.h/.cpp      the durable co-save record (v7)
src/BlobCodec.h        little-endian byte codec
src/Components.h       FormRef + SpeciesTag + CapPouch + Name + Pregnancy
                       + BirthDay (adapter components)
src/Tuning.h           the INI — sim.* decay rates, meal price, market
                       hours, fight/birth/dialogue tuning
src/WorldFacts.h       the gates: market hours (closed at night) + weather
src/Tick.h/.cpp        the per-frame VM-tick hooks
data/                  TheLivingCommonwealthAnims.esp (unconditional
                       paired-push IDLE records for real kicks)
config/                TheLivingCommonwealth.ini (tuning defaults)
```

**Lifecycle mapping (current, verified):**

| Game event | The adapter does |
|------------|------------------|
| `kGameLoaded` (startup) | `StartWorld`: translate loaded sim-relevant actors into entities (tagged by species, seeded per species); seed the market memory |
| `kPostLoadGame` (a save finished loading) | Apply the co-save restore (or start fresh) |
| `kPreLoadGame` | `EndWorld` (Clear; serializers survive); arm abort recovery (60s) |
| `kDeleteGame` (a save FILE was deleted) | nothing — a running world survives |
| serialization save callback | `CaptureWorld` → `CoSave::Encode` → `WriteRecord` |
| serialization load callback | read record → `CoSave::Decode` → `QueueRestore` (applied on `kPostLoadGame`) |
| new game (revert callback) | `EndWorld` |

**The sim loop** (per frame, game thread): `Update(registry, delta)` →
`BuildPlan` (pure; loaded/available refusals) → `ExecutePlan` (the walk
issue; sessions capped at 16) → `ProbeWalks` (live distance probes).

**The co-save record:** the core's snapshot is process-local (component
keys are stable strings). The adapter owns the durable F4SE co-save
record (stable type names, versioning): **v11** currently — v4 memory
world-days, v5 bonds, v6 legacy, v7 conflict gates, v8 gates/weather,
v9 stipend marks, v10 BabyHold (the carry), v11 VisualChild (the
visible child's pair), plus the co-save's additive components (Name,
CapPouch, SpeciesTag, Pregnancy, BirthDay, Health). The 0.8.1 co-save
audit fixed the stable-name table itself — Health, Pregnancy/BirthDay,
and CompanionTag were registered serializers but never named, so
mid-hold illness and in-progress pregnancy were lost on save/load; all
four names now ride the record (MidOutbreakSaveTest locks the
round-trip). Migration: old saves load
forward; a future component is skipped, never fatal.
