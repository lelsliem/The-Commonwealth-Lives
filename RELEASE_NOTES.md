# The Living Commonwealth — Release Notes

**Fallout 4 adapter for the Living Commonwealth Engine (LCE)** — an F4SE
plugin that makes the Commonwealth *live*.

> A settler goes to market because they are hungry — no script.

The settlers aren't on quest scripts. They are **hungry**, they
**remember** where to trade, they walk to **their own settlement's
market**, and the exchange is physical: caps change hands, the
stall-keeper's purse grows, trust is earned. The market has **hours** —
it closes at night and nobody walks to a closed bench — and the day's
**weather** is remembered. The game does nothing but show the result.

In-game verified end to end: hungry settlers decide `MoveTo`, walk to the
bench, arrive, trade, and the whole world survives save/load.

---

## 0.8.11 — Log Hygiene + Loose Ends (2026-08-17)

The quiet-log pass before the beta. The decision log was already
capped (one line per mind, and only when its intent changes); the one
real change is the walk probe — the log's single biggest writer —
now off by default (`sim.log.probes`). The arrival and session-end
lines stay; only the progress heartbeats go, so a session's log is a
story, not a wall. Two parked questions answered for good: unowned
settlements keep their minds (the world is alive everywhere, and
`requireOwned` remains the opt-in), and the who-is-the-world calls
from the audit all stand as designed. 0.8.10 animations + fight-feel
is deferred by decision — the kick chain works and the presentation
bugs are documented; it is revisited after the beta.

## 0.8.9 — Babies, implemented (2026-08-15, verified 2026-08-16)

The birth journey gets a body — a newborn is carried, then becomes a
child of the Commonwealth.

- **The carry.** On birth, the mother visibly holds a swaddled
  bundle — the Baby Sim mod's variant when it is installed, the
  game's own Shaun bundle when not — so everyone gets a carry, mod
  or not. The hold rides the save (co-save v10) so a mid-carry
  survives save/load.
- **The child.** After `sim.baby.holdDays` (default 2) the bundle
  comes off and a child spawns at the mother's feet — gender-matched
  from the game's own farm-children pool. The child is deliberately
  left un-initialized (the one spawn route that doesn't crash or
  half-birth): invisible until the game's own save/load routine
  completes it, then it steps out fully real — head, movement,
  correct name and gender. Forcing the init yields the headless
  T-posing child, and the game's own PlaceAtMe is a documented CTD;
  the deferred route is the only one that works.
- **The clothes (the dress find).** Children kept spawning in their
  pants because the game's child-outfit records are OTFT bundles, not
  ARMOs — the equip silently no-op'd. The child's base now gets its
  default outfit at spawn (the game's own child-clothing path), so
  children appear dressed and stay dressed across reloads.
- **The road feed (0.8.9-road).** Provisioners, caravan guards, and
  caravan workers eat from the caravan's supplies on the road at
  `sim.road.feedThreshold` — no market clustering — and their bonds
  and memories travel with them, so a provisioner greets a settler
  they befriended when passing through.
- The crib walk and market baby-goods shelf were cut: the Baby Sim
  mod runs as its author designed; the sim only adds the moment of
  birth and the moment of the child. The baby-mod integration stays a
  soft dependency (author engaged).

## 0.8.8 — Timings & Weights (2026-08-15)

The exchange show's pacing is tunable from the INI and the MCM
Interactions page: `sim.interact.pairCooldown` (a specific pair
never re-exchanges within the window), `sim.interact.dailyCap` (how
many interactions a mind opens per day), and the weighted pools
(`weight.greet / .gossip / .family / .row`) — greet and gossip
dominate the crowd, family is boosted for bonded pairs, and a feud
pair is a hard row whatever the weights say. The bond names the
register the exchange asks the game to voice (family for a bonded
household, flirt for a compatible unpaired pair, greet for the
crowd); the field verdict: the game refuses `kMisc_Greeting` for
settler voices, so that register defaults off.

## 0.8.7 — Dialog Growth: the game's own lines (2026-08-15)

The drafted dialogue was replaced by the game's own recordings — a
content swap, zero new machinery. The voice-bank survey extracted
and classified the game's settler bank (~17,500 files), guards,
Gen3 synths, children, and ghouls; every pool now carries the game's
real lines (Greet 30, Gossip 23, Row 32, Trade 30, Family 16,
Grief 15, Fight 2, Feud 2), shipped in code and the INI
(`dialogue.*`).

**The voice-aware picker.** A line only speaks if the speaker's voice
bank recorded it — ground truth from the game's own Voices BA2 name
table. The game resolves audio by voice type, so a voice can never
say a line its bank lacks; when no line exists for that voice, the
mind stays mute (captions only). Named voices (Sturges, Marcy,
companions) have no recording of the generic lines — the mute rule
stands for them.

**Realistic Conversations compatibility.** The 2018 ESP's 33 GMST
overrides are re-delivered as a tuning file next to the DLL — no
xedit patch, no ESP, no load-order slot; a missing file is the off
switch.

**The audio trigger probe verdict.** The experiment asking whether
the DLL can make the game actually play a curated line's .fuz came
back no — no audible playback on either route (VM Say / ProcessGreet)
without crashing. Speech shows in-world instead: the exchange
registers drive the game's own voiced dialogue where its voices
support it, subtitles appear when the player is close enough to
hear, and the settlement radio carries news as captions.

## 0.8.6 — Scale in the Field (2026-08-14)

The hard gate, measured: the adapter feeds the engine's TickReport and
field-verified on a **646-mind save** — four consecutive reports at
618–619 live minds, worst frame 4.13–8.17 ms against the 16.6 ms frame
budget, game smooth throughout. The sim's whole share is roughly half a
frame. Decide is the heaviest pass (1.5–2.5 ms — a later optimization
target, not a gate failure).

**The ownership read is unbenched for real** — the last 0.8.6b
failure. `requireOwned` now reads the game's own
`WorkshopPlayerOwnership` actor value (form 0x33C, the exact AV
`SetOwnedByPlayer` writes), re-armed per world so the loaded save's
restored state is read instead of the menu-world's fresh defaults.
Field-verified: **28 of 28 workshops owned**, matching the player's
map. The bug-hunt cleanup rode along: a cough idle null-guard (a crash
waiting to happen), the quest-prop debug dump dropped from the hot
path, and the 27/27 harness.

## 0.8.6b — Redefine & Loose Ends (2026-08-14)

The audit's two real gaps closed.

- **Robots are robots.** A field find (Buddy the Mr. Handy seeded as
  Human and caught the flu) became the Robot species: they talk but
  have no biological needs — no hunger, no fatigue, no Health (never
  ill), no pouch, no stipend, never walk to the market, never romance.
  Befriends and feuds like anyone. Handy, Protectron, SentryBot,
  Assaultron, EyeBot; pre-fix saves re-classify and heal on load.
- **Everyone can earn caps.** The daily settlement stipend
  (`sim.economy.stipend`, default-off) pays every mind in a settlement
  a small daily wage — the non-keeper broke-gap that made sickness a
  death sentence for the poor is closed. The household shares the
  wage. The player opts in via the MCM's "Daily stipend" slider.
- **Who pays the wage** — `sim.economy.stipend.source` (settlement
  minted | player) and `sim.economy.stipend.requireOwned` (only
  player-owned settlements pay). Both knobs field-failed, were
  reverted to the working minted stipend, then each was unbenched:
  player-pays fixed (the FO4 RemoveItem count's sign was the bug) and
  ownership fixed in 0.8.6c.
- **MCM restructured** — a dedicated Economy page (Market, Medicine,
  Stipend together); Illness is sickness mechanics only.

## 0.8.6a — The Audit (2026-08-14)

The honest pass: every cut candidate from the Release Plan re-tested
against what actually shipped. MCM was rescinded to KEEP (the tuning
page proved its worth); the audit added the two real gaps to 0.8.6b —
the earn-caps economy and log hygiene.

## 0.8.5 — MCM + Settings Manager (2026-08-14)

The player tunes the world in-game. A full MCM page — 5 pages / 53
controls: Life, Interactions, Relationships, Illness, Birth & Fights —
binds every player-facing knob to MCM's ModSettings. A slider change
lands in the sim within a second: hot-applied, no rebuild, and it
survives a restart. No native Papyrus bridge by design (the VM header
has no registration surface; the override-file route achieves the same
with zero crash risk). MCM stays a soft dependency — no MCM, the INI
alone rules.

## 0.8.4 — Random Interactions (2026-08-14)

Settlers who cross paths sometimes speak unprompted — no hunger drive,
no market. The interaction pass runs each second: a cooldown-expired,
non-walking mind finds its nearest non-walking neighbour inside
`sim.interact.radius` (400 u), rolls `sim.interact.chance` (0.4), and
speaks. The pool follows the bond — family for family, a quiet row for
rivals, greet/gossip for strangers. **Speech is local, not broadcast**:
it logs, and subtitles only when the player is within
`sim.subtitle.radius` of the speaker — the settlement radio never
carries small talk. The trial passed in-game (186 genuinely-near
speakers; the author judged it "added life").

## 0.8.3 — The Sick Household (2026-08-14)

Medicine is not an endless shelf, and a family cares for its own.

- **Stocked medicine** — each market's doses per day ride the co-save;
  a sold-out stall stays sold out across save/load until the next
  market day, and a sick mind at an empty shelf rests instead.
- **The family care-buy** — one shared dose path used twice: the sick
  dose themselves, then a well buyer buys for the household — the
  spouse first, then a sick child (who has no walk of its own to the
  bench). The shared wallet pays; `X bought medicine for Y — a family
  cares.`

## 0.8.2 — The Burial (2026-08-13, verified 2026-08-14)

The settlement lays its own to rest. A corpse no longer stays in the
cell forever: after the mourning window (`sim.death.burialDays`,
default 3), the sweep disables the body, logs `the settlement laid X
to rest`, and the settlement hears it. The burial book rides the
co-save, so a death whose window expires while the game is away is
still buried on the next load.

## 0.8.1 — The Illness Field Pass (2026-08-13)

Two scale fixes the day-12 outbreak exposed — an outbreak is one
story, not a wall of sound and a wall of names. The global cough gate
(one cough anywhere per 4 s, on top of each mind's own interval) keeps
the tell audible and kills the cacophony. Illness news is burst-paced
(at most 4 new names per 10 s window) so the radio stays readable.
Plus: **sickness takes the body** — the illness death now kills the
game actor (the corpse appears; 0.8.2 buries it later) — and
**children survive the cold** (childhood illness retuned; death stays
rare and earned). The co-save audit made illness and pregnancy truly
persist: a mid-hold sickness and an in-progress pregnancy restore
exactly across save/load.

## 0.8.0 — Illness & Medicine (2026-08-13)

The Commonwealth gets sick — and the market becomes the cure.

Every settler now carries **health**. A radstorm day, a bad meal, a
wound from a fight, or the sick passing it to the healthy — four ways
to fall ill. A sick mind holds at a reduced health while the sickness
runs, tires faster and rests more, and **coughs** (the game's own
coughing idle) so you can hear the outbreak. Untreated, a severe
illness can end a mind — but death is rare and earned, not a meat
grinder.

**The market cures.** A sick settler with 25 caps buys medicine at the
stall — the hold ends, recovery begins, and the seller's pouch grows.
Broke sick settlers rest instead and ride out the illness honestly.
The whole loop is verified in-game on a natural radstorm day: 73
medicine buys, 0 deaths.

Everything is tunable from the INI (`sim.illness.*`): contraction
chances per vector, the hold window, recovery speed, severity growth,
the medicine price, the cough cadence.

## 0.7.9 — Bugs & Polish (2026-08-13)

The clean run into Illness. The full codebase audit — every INI default
vs the code, every comment, every doc — found **no bugs**. Everything
consistent, 23/23 tests green. Version 0.7.9.

The 0.7.x run is complete: talk, rows, names for everyone, trade with
anyone, fights, the real kick, babies, visible children, and polish.
Next: 0.8.0 Illness & Medicine.

## 0.7.8 — Visible Children (2026-08-13)

Born children can now be **seen**. When a sim-only child grows up
(`sim.birth.childhood` days), the adapter scans the game for child
actors — from the Baby Sim mod or any source — and pairs the child to
a real actor: it walks, trades, and bonds like any mind. The pairing is
entirely in code — no patch ESP, no load-order fragility. Without the
mod nothing changes (`sim.birth.visible`, default off): children stay
sim-only, as before. The Baby Sim mod itself is *usable now*; shipping
it as a requirement waits on the author's permission.

## 0.7.7 — Babies: The Birth Lifecycle (2026-08-13)

A child is now *born*, not just created. Bonded human couples conceive
(`sim.birth.chance`), carry a pregnancy through `sim.birth.gestation`
sim-days, and the birth fires on the due day — the settlement hears it,
the parents bond, the child is named and fed. After `sim.birth.childhood`
sim-days the child grows up: its needs normalize and it walks to market
like any mind. Only humans conceive (no robot babies), the day-261
crash is fixed, and the whole journey survives save/load.

## 0.7.6 — The Fight-Feel Pass: The Kick Is Real (2026-08-13)

Two presentation bugs closed. **The kick finally plays for real** — the
game's own push-kick idle refuses to play outside combat, so the mod
ships its own 380-byte ESP (unconditional clone of the proven recipe)
and the adapter plays it on the attacker: a real kick on the first
shove AND the retaliation. **The fall tips instead of sliding** — the
knock force drops to the tip-over zone so the victim falls in place;
the paired push is the shove. And both actors must be on their feet
before the next beat — no more double-collapses. The experiment adding
body-slam and push animations was reverted; the kick alone reads right.

---

## 0.7.0 — Identity & the Player Window (2026-08-11)

The world is alive even when the player isn't watching — settlers have
**names**, relationships can go **bad**, and the player **hears** what's
happening.

- **Names on the people themselves.** The workshop view, pip-boy and
  hover read *Mara Price*, not "Settler" — the name is written onto the
  actor (the same mechanism as the console's `SetDisplayName`) and
  persists in the save. The game's own names always win (Sturges stays
  Sturges). The author curates four name lists in the INI
  (`names.first.male/.female/.animal`, `names.last`); owned animals are
  named (the junkyard dog is *Bandit*), strays stay nameless, and pet
  names are unique per world. Every log line, bond, death, birth, and
  news item speaks in names.
- **The feud is real.** The world has a conflict source: a hungry
  arrival at a **closed market** is a slight — `the stall is shut — X
  went hungry and blames the keeper` — the settlement's echo agrees,
  rival and enemy bonds form, and the 0.6.0 feud arc finally begins on
  its own: `X is feuding with Y`, gossip through the settlement, and
  mediation (`Titus Pratt cooled the feud between …`). The engine's
  `sim.hunger.desperate` gate (INI-tunable) means a critically hungry
  mind will walk to a shut market anyway — slights, feuds, and famine
  stories are all possible.
- **The player hears the world.** World events — bonds, feuds, births,
  deaths, market openings — become one-line news: throttled **HUD
  notifications** ("X and Y became friends") and a **settlement radio**
  (build it in the workshop) that speaks the news as on-screen captions
  while you're near. Real audio is planned after 0.9.0.
- **The whole world round-trips the co-save** (record **v6**): names,
  bond maps, stall-keepers, cap pouches, the Rng stream, memory world-
  days, and the legacy section (a death bequeaths its memories and
  leaves its name behind; a child inherits the parents' memories of the
  people).
- **Tuning without recompiling** — every rhythm is in the INI: market
  hours, hunger/fatigue/safety/social/comfort decay, bond thresholds,
  the slight's temper line, news cooldown, radio radius and caption
  cadence, name lists.

---

## 0.6.0 — Life & Emergent Quests (2026-08-11)

Settlers are born, live, and die; they make friends and enemies; and
quests happen because life happens — no scripts.

- **The world keeps its books.** Arrivals wake mid-session, deaths and
  departures leave it, and every survivor remembers who is gone
  (`gossip: 643 minds remember settler X is gone`). A real kill books
  confirming; the dead never restore and never ghost-walk.
- **Bonds from how people treat each other.** Friends, sweethearts, and
  spouses emerge from shared meals and survive save/load — the buyer's
  half of a trade warms the keeper, and feelings cool slowly (the
  living drift clock), so bonds accumulate.
- **Households.** A married couple shares one pouch, one stall, one
  bench, one bed — the family bench feeds the spouse for free.
- **The sleep cycle.** A fed mind rests, recovers its Fatigue
  (`sim.rest.recovery`), and walks again — so the same settlers keep
  meeting and their bonds deepen. Settlers **mill around** between
  meals (Rest/Explore command a bounded wander to a real nearby
  reference, furniture preferred).
- **Grief is real.** A widowed settler drains its Social seeking
  company — `arcs: settler X grieves for Y`.
- **Children.** A spouse household's sim-only child — fed, bonded, and
  living in the co-save like any mind (`sim.birth.enabled`).

---

## 0.5.0-beta — The Living World (2026-08-10)

### What's in this release

- **0.1 — The heartbeat.** The plugin loads and breathes.
- **0.2 — The translation.** Settler-faction actors become minds.
- **0.3 — The intent executor.** The sim ticks every frame and settlers walk.
- **0.4 — The co-save.** The world rides inside the save file (record v4:
  entities, the seeded RNG stream, who runs each market's stall, and each
  memory's world day — the timestamp survives save/load).
- **0.5 — The living world.**
  - Species split: children and animals don't barter — a dog gets fed and
    gives nothing in return.
  - World facts: the market's hours push to memory — closed at night,
    nobody walks to a closed bench.
  - Weather memory events: settlers remember the day's sky.
  - Per-settlement markets: a persistent-cell census (FO4 never fills the
    REFR form array) gives every settlement its own bench and its own
    stall-keeper.
  - The trade stone: a stall-keeper per market; the buyer remembers the
    merchant.
  - The economy stone: cap pouches that round-trip through the co-save.
  - Per-mind decay desync (`VaryNeeds`) plus the engine's per-tick decay
    jitter — a seeded RNG, persisted in the co-save.
  - Hardening: `DeleteGame` no longer kills a running world, and the walk
    probe reads the actor's data position instead of a lying 3D transform.

### Install

1. Install **F4SE** (Next-Gen, 0.7.8) and **Address Library**.
2. Copy `F4SE/` from this archive into your `Fallout 4/Data/` folder —
   the plugin lands at `Data\F4SE\Plugins\TheLivingCommonwealth.dll`.
3. The INI (`Data\F4SE\Plugins\TheLivingCommonwealth.ini`) is created
   with sane defaults on first run — every number in it is tunable.

### Notes for a beta

- Requires **Fallout 4 1.11.221 (Next-Gen)**.
- The name lists are the author's curated pools — swap them freely in the
  INI.
- The F4SE serialization UID is a placeholder; it only matters if the
  mod later reaches the F4SE plugin registry.

### Links

- Repo: <https://github.com/lelsliem/The-Commonwealth-Lives>
- The engine: <https://github.com/lelsliem/Living-Commonwealth-Engine-LCE->
