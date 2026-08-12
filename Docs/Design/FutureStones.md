# 0.7.6 → 0.8.0 — the run to Illness · "The Commonwealth Grows"

**Status:** PLAN (2026-08-12) — design written, nothing built.
**Related:** Roadmap.md (the milestone ledger), ReleasePlan.md (the
staged path), RealEvents.md (talk/rows/fights), Illness.md (0.8.0's
design), Birth.h (the current birth stone), Fights.h + the fight INI
keys (0.7.6's raw material), the engine's AdapterProject.md (the
boundary contract this run lives inside).

---

## The run, in build order

Each stone is verified in-game before the next — the standing rule.
Nothing here needs the engine, and nothing here blocks on the engine
(it is already at 0.8.1-alpha, ahead of us); the only external
dependency is the baby mod's permission, which gates *editing* in
0.7.8, never building or using it.

```
0.7.6  Fight-feel bug pass     — the brawl reads as a scuffle   (no deps)
0.7.7  Babies                  — the sim's birth lifecycle      (no deps)
0.7.8  Baby & kid items        — bottles, cribs, visible kids   (external mod: usable now, editable on permission)
0.7.9  Bugs & polish           — every stone's field feedback   (no deps)
0.8.0  Illness & Medicine      — health holds, recovers, ends   (no deps; design already in Illness.md)
```

Why this order: 0.7.6 first because it is small, self-contained, and
closes the one stone the player has called "off" — a clean hand to
everything after. 0.7.7 before 0.7.8 because the sim-side journey
(pregnancy → birth → child mind → growth) is the part we own; the
visible actors and the items bolt onto it once the external mod's
permission lands. 0.7.9 is the sweep that folds every field note back
in before the big stone. 0.8.0 lands last — it is the milestone the
0.8.x release package will be cut from.

---

## Engine needs audit — Request D: none

Every stone below was checked against the engine's public surface
(`Include/LCE/…` — the boundary is `CreateEntity` / `DestroyEntity` /
`Remember` / `Update` / `GetComponent<Intent>` plus `ReportOutcome`,
the EventBus, Groups/Traits, Legacy, Rng, WorldTime, Needs/Goals):

| Stone | Rides existing surface | New engine surface |
|-------|------------------------|--------------------|
| 0.7.6 Fight-feel | KnockExplosion + paired idles are game calls, not engine | **none** |
| 0.7.7 Babies | `CreateEntity` (sim-only children already do this), `Remember`, Needs, the co-save additive record | **none** (the engine has no age/child concept — it does not need one; the journey is a component at the edge) |
| 0.7.8 Items | game objects via the external mod; the sim uses its own mind data | **none** |
| 0.7.9 Polish | — | **none** |
| 0.8.0 Illness | fact-plus-tick `Health` component (Illness.md answered the engine's hold-then-recover question: a component at the edge, not a NeedType) | **none** |

The engine's own endgame verdicted the same: *six of the seven
patterns need zero new engine work and disease needs none either*
(core `Docs/AdapterProject.md`). This run is a pure adapter effort on
the existing contract. No hand-off to the engine tab is required;
this section is the record of that audit.

---

## 0.7.6 — Fight-feel bug pass

**Goal:** the brawl reads as a scuffle, not two collapses.

The field notes from the 0.7.5 tests, and the concrete suspects:

1. **The ghost-push slide.** After the kick and the fall, the victim
   drags along the ground — the fall's `KnockExplosion` impulse
   applying travel on top of the paired-push motion. Suspect: the
   knock fires at full force while the paired idle is still playing
   its exit. Fix options, in order of preference:
   - Zero the knock's directional impulse now that the paired push
     carries the fall (the push already travels the victim); the
     knock keeps only the down state.
   - Or delay the knock until the paired idle's exit frame
     (`sim.fight.fall.delay` already spaces flinch → fall; the slide
     is the knock's own drift after that).
2. **The both-fall look.** When the retaliation lands, both actors
   can be down at once. Suspect: the counter-fall fires before the
   first victim has finished getting up (the get-up animation is
   slower than the 3–7s beats assume). Fix: the retaliation beat
   re-checks the pair is standing (an animation-state or time-since-
   fall guard) before it fires.
3. **Anything the receipts still flag.** The `shove:` / `fall:`
   receipts print the distance and force — if a beat fires at range
   or with phantom force during the test loop, it shows in the log.

**Files:** `src/Adapter.cpp` (the shove block + `ProcessPendingShoves`),
`src/Tuning.h` + `config/TheLivingCommonwealth.ini` (any new knob;
existing keys: `sim.fight.chance/temper/push/stagger/pushback/fall.delay/
retaliation.delay/part.delay`, `sim.test.forceFight.a/b/interval`).

**Verification:** the force-fight loop
(`sim.test.forceFight.a/b/interval`, currently Sturges vs Jun Long at
the bench), player standing at the bench: the kick lands, the victim
falls **without sliding**, gets up, the retaliation lands only when
both are on their feet, the loser walks off. The log's receipts prove
each beat fired at close range; the eyes prove it reads as a scuffle.
Then a normal (non-forced) session to confirm the once-per-day gate
still holds.

**Engine needs:** none.

---

## 0.7.7 — Babies: the sim's birth lifecycle made whole

**Goal:** a child is *born*, not just *created* — conception,
gestation, birth, and growth are a journey, and the world notices.

What exists (Birth.h): a spouse household's sim-only child is created
(gated by `sim.birth.enabled`, one per sim day), the parents warm to
it, the household feeds it, it is counted at wake, named, and co-saved
like any mind. What the journey adds:

1. **A pregnancy window.** A `Pregnancy` component (adapter-owned,
   co-save additive, stable name `"pregnancy"`): conception day,
   due day, the parents' IDs. Spouses conceive on a day-roll
   (`sim.birth.chance`, default low) once the household is stable
   (bonded + fed); the due day is conception + `sim.birth.gestation`
   sim days. During the window the household's news can hint at it
   (a radio line, gossip) — the baby is coming.
2. **The birth event.** On the due day the existing birth fires
   (Birth.h's path), but now it is an *event* with a receipt: the
   settlement hears it (gossip, the birth news already exists), the
   parents bond to the child (already), and the birth day is stamped
   on the child.
3. **Growth.** A `Growth` day on the child (birth day, carried in the
   `Pregnancy`/child record): at `sim.birth.childhood` sim days the
   child's species tag moves Child → Human (adult), its needs profile
   normalizes (it starts walking to market like any mind), and the
   household's bonds stay. This is the sim-side transition — the
   visible actor half waits on 0.7.8.
4. **The co-save.** The `Pregnancy`/birth-day record is additive
   (like `name`/`cappouch`) — no record-version bump unless a format
   change forces one.

**Files:** `src/Birth.h/.cpp` (the journey), `src/Components.h` (the
`Pregnancy` component), `src/Tuning.h` + the INI
(`sim.birth.chance`, `sim.birth.gestation`, `sim.birth.childhood`),
the sweep + the co-save serializer.

**Verification:** `sim.birth.enabled = 1` + a low `sim.birth.gestation`
(so a test can watch a full cycle): a bonded pair conceives, the
gestation window passes with the radio hint, the birth fires with
gossip, the child is fed and counted at wake, and after
`sim.birth.childhood` days it grows into a walking mind. Save/load
mid-journey: the pregnancy, the child, and the growth day restore.

**Engine needs:** none (the sim-only child already rides
`CreateEntity` + `Remember`; growth is a component swap at the edge).

---

## 0.7.8 — Baby & kid items: bottles, cribs, visible children

**Goal:** children are *seen*, not just counted — and the things they
need exist in the world.

The external baby mod (Nexus 100934 et al.) is the requirement. It is
**usable now** (the sim can reference what it provides); *editing* it
or shipping it as a hard requirement waits on the author's permission
(already requested). Scope:

1. **Visible children.** When the sim's child grows (0.7.7), the
   adapter pairs the sim-only child with a real game child actor the
   external mod provides — the child gets a form, a translator entry,
   and walks the world like any mind. Until the mod is in, children
   stay sim-only (the 0.7.7 behavior) — the adapter degrades
   gracefully.
2. **The items.** Bottles, cribs, and the feeding props the mod adds:
   the sim's feeding/walking paths can target them (the walk already
   resolves to real references — `Movement::WanderNear` prefers
   furniture). The market can sell the baby goods (the trade stone's
   second good slot).
3. **Names.** Children use the existing name pools (they already do —
   the sim's names predate 0.7.7); no new naming work.

**Files:** the translator/sim-relevance path (admit the mod's child
actors), `Movement` (target the props), the market/trade lists.

**Verification:** with the mod installed, a born child gains a visible
actor and is seen at the settlement; bottles/cribs appear as walk
targets; without the mod, nothing changes (children stay sim-only).
The permission question is tracked in the release notes, not the code.

**Engine needs:** none.

---

## 0.7.9 — Bugs & polish

**Goal:** the clean run into Illness — every field note from the
0.7.6–0.7.8 sessions folded back in.

A named pass, not a bucket: the fight-feel notes that survived 0.7.6,
the baby-journey notes from 0.7.7, the item integration notes from
0.7.8, plus the standing list (news polish, the feed's flood control,
the docs and INI comments reconciled, perf sanity at scale using the
engine's `TickReport` once a minute). Each fix lands with its ADR and
its log receipt. No engine change; no new stones.

**Engine needs:** none.

---

## 0.8.0 — Illness & Medicine

**Goal:** the wasteland has a price beyond hunger — radstorms, bad
food, and wounds make settlers sick; the sick rest and buy medicine;
untreated sickness can end a mind. The full design is already written
and answered (`Docs/Design/Illness.md`): a `Health` component
(adapter-owned, co-save additive), fact-plus-tick (not a NeedType),
the sick rest more via a Fatigue multiplier, medicine is the trade
stone's second good, and the radio reports the ill. This is the
milestone the 0.8.x release package is cut from.

**Engine needs:** none (Illness.md answered the engine's one open
question: a component at the edge, zero new surface).

---

## The hand-off to the engine tab

One paragraph, for the record: **the adapter's 0.7.6–0.8.0 run needs
no new engine surface.** The engine is already ahead (0.8.1-alpha);
this run is five adapter stones on the existing contract. The only
cross-repo touchpoints are read-only: `TickReport` for the 0.7.9 perf
sanity pass, and the co-save staying additive. No Request D.
