# 0.8.12 — The Living Commonwealth: the beta

**Copy this into the Nexus page and the GitHub release.**

---

## Title

**The Living Commonwealth v0.8.12 (beta) — the Commonwealth lives**

## Tag

`0.8.12`

## Short description (350 characters or less)

Settlers in Fallout 4 get hungry, tired, and sick; they trade with real
caps, argue, fight, fall in love, feud, and die; children are born,
carried, and grow up — no scripts, the game just shows the result. The
whole world rides inside your save file. Needs, names, markets with
hours, illness and medicine, MCM tuning, in-game dialogue with the
game's own voices.

## Full description

> A settler goes to market because they are hungry — no script.

Settlers aren't on quest scripts. They are **hungry**, they **remember**
where to trade, and they walk to **their own settlement's market** when
they need to. The exchange is physical: caps change hands, the
stall-keeper's purse grows, trust is earned. The market has **hours** —
it closes at night and nobody walks to a closed bench — and the day's
**weather** is remembered.

They have **names** on the people themselves. They **talk** to each
other using the game's own voice lines, **argue**, and **fight** — the
feud begins on its own from a slight. They **fall in love**, become
**households** (one pouch, one bench, one bed), and **children are
born**: a mother visibly carries a swaddled bundle, and after a couple
of days a real child of the Commonwealth takes its place — dressed,
named, and growing up. The wastes can make them **sick** — they buy
medicine when they can afford it, rest when they can't, and a severe
untreated illness can end them. The dead are **grieved** and **buried**
by the settlement that mourned them. The player **hears** the world
through radio captions and, close up, the words themselves.

The whole world rides inside your save file — save, quit, reload, and
everyone remembers who fed them, who they traded with, and who they
married.

**This is the first public beta.** The mod is complete through 0.8.11
and verified in-game on the author's own load order; the beta is the
wider net. Save often, and report what you see.

## What's in 0.8.x (0.8.0 → 0.8.12)

- **0.8.0–0.8.3 — Illness & Medicine.** Radstorms, shared meals,
  wounds, and contagion make settlers sick. They cough (audibly), tire
  faster, and rest more. The market cures: medicine is a stocked shelf
  with a price, and a family buys for its own. The dead are buried
  after the mourning window.
- **0.8.4–0.8.5 — Life, unprompted.** Settlers who cross paths speak
  unprompted — family lines for family, a quiet row for rivals. A full
  **MCM page** (5 pages / 53 controls) tunes the whole world in-game;
  changes hot-apply within a second.
- **0.8.6 — Scale + economy.** Robots are robots (no hunger, no
  romance). A daily stipend lets everyone earn caps. Ownership and
  player-pays settled. Field-verified at **~620 live minds with the
  sim taking roughly half a frame**.
- **0.8.7–0.8.8 — The game's own words.** Every dialogue pool was
  re-curated from the game's own recordings (~17,500 files surveyed).
  A voice-aware picker means a line only speaks if the speaker's voice
  bank recorded it. Realistic Conversations compatibility ships as a
  tuning file — no xedit, no ESP.
- **0.8.9 — Babies, implemented.** The birth journey gets a body: a
  mother visibly carries a swaddled bundle (the Baby Sim variant when
  installed, the game's own Shaun bundle when not), then a real child
  of the Commonwealth spawns — dressed, named, and growing up.
  Provisioners and caravan guards eat on the road.
- **0.8.11 — Log hygiene.** The quiet-log pass; the walk probe is off
  by default. Loose ends answered (unowned settlements keep their
  minds).
- **0.8.12 — The beta.** Version real, warnings cleared, docs
  reconciled, and the player-facing README written.

## Requirements

- **Fallout 4 1.11.221 (Next-Gen)**
- **F4SE** (Next-Gen, 0.7.8)
- **Address Library for F4SE Plugins**

## Install

1. Install the three requirements.
2. Extract the archive into `Fallout 4/Data/` — the plugin lands at
   `Data\F4SE\Plugins\TheLivingCommonwealth.dll`. (MO2: install as a
   mod and enable it.)
3. Enable `TheLivingCommonwealthAnims.esp` in your load order — it
   delivers the fight kick animation. Without it fights fall back to a
   stagger (nothing crashes).
4. The INI is created with sane defaults on first run — every number
   is tunable. Old saves migrate forward cleanly.

## Optional

- **MCM** — the in-game settings page. Without it the INI alone rules.
- **Baby Sim - Babies That Grow Up** (Nexus 100934) — the carry shows
  its bundle variants; the mod stays untouched and runs as its author
  designed.
- **Realistic Conversations** — its 33 GMST overrides ship as a tuning
  file next to the DLL; delete the file to disable.

## Known limits (honest)

- No custom audio: speech is captions, subtitles when you're near, and
  the game's own voiced dialogue where its voices support it.
- A newborn child is invisible until the next save/load (the one spawn
  route that doesn't crash or half-birth the child).
- The fight kick needs the Anims ESP; the known presentation bugs
  (both actors collapsing, a push with no visible shove) are deferred
  to the post-beta animation pass.
- By design: animals feed but never trade or talk; robots never eat,
  sleep, or fall in love; companions are friendship-only; only human
  couples conceive; unowned settlements still have living settlers.

## Links

- Repository + changelog: <https://github.com/lelsliem/The-Commonwealth-Lives>
- The engine: <https://github.com/lelsliem/Living-Commonwealth-Engine-LCE->
