# Identity & the Player Window — 0.7.0 "The Player Listens"

**Stone:** adapter 0.7.0
**Status:** ✅ COMPLETE (2026-08-11) — all three stones implemented,
**19/19 harness suites green** (NamesTest, SocietyTest, CoSaveV6Test
added) and **verified in-game**: names on the actors, the feud arc
proven end-to-end in the wild, HUD news + radio captions on screen.
The engine's 0.7.0 side (Legacy — stones 10–12, 28/28 suites) is
wired into the adapter's death and birth paths, so the two halves are
proven together.
**Related:** the feud and grief arcs (`Life.md` — 0.7.0 supplies the
feud's *organic* fuel), the species split (animals are named only when
owned; children carry no barter), the co-save (`CoSave.md` — record
**v6**: the registry-level legacy section; the `name` component itself
is additive and never bumps), the engine's decided negative-social
channels (2026-08-11 — kind and result, never a sign), the engine's
stone 09 (Society — Groups & Traits: the echo and traits substrate),
the 0.6.0 close-out (ADR-0018), **0.7.2 Rows** (`RealEvents.md` — the
feud's verbal scene: rivals and enemies crossing at the same bench
row, the engine's Wronged channel, gossip spread; the conflict source
stays the shut stall, ADR-0024).

---

## The problem

0.6.0 proved the world can *feel*: settlers befriend, marry, grieve, and
bear children — all from shared meals, all without a script. But the
sim still talked about people as hex refids, nothing in the world made
a relationship go **bad**, and the player could only watch the log.
0.7.0 closes all three: settlers get **names**, the world gets a
**conflict source** (so feuds can begin on their own), and the player
gets **ears** (the radio) — with MCM honestly deferred (below).

## Stone 1 — Names: the sim speaks in people, not hex — ✅ built

**The design, as built.**

- **Born with a name.** A `Name` component (adapter-side — names are
  game knowledge, the ADR-0024 boundary), persisted in the co-save
  under the stable name `name`, exactly like `CapPouch`. Every person
  carries one; a sim-only child is named by its household's family
  name ("the Vances" stay the Vances).
- **The game's name first.** An actor with a real full name (Sturges,
  Marcy — the named NPCs FO4 keeps) uses it, read at seed time from
  the base form (`TESFullName::GetFullName`). A generic "Settler" gets
  a **procedural Commonwealth name** — and the author curates the
  lists in the INI:
  - `names.first.male` / `names.first.female` — gender-split pools;
    the actor's sex picks the list (an unset sex draws from the id),
  - `names.last` — the shared family names,
  - `names.first.animal` — a **separate animal pool**, and the naming
    rule is the ownership rule: **an owned animal gets a name (a dog
    is "Rex", not "Rex Hart"); a stray stays nameless** — its label
    is "animal [FF0197BF]" until someone claims it.
  Each list is comma-separated; a missing or broken list keeps the
  built-in default (a bad line never breaks the world). Drawn
  deterministically per entity id (the Mix pattern) and **deduped**
  against the world's live names — no two "Vance" in the same room.
- **Back-filled on restore.** A restored mind without a `name` predates
  the stone — seed one on `ApplyRestore` (gender from the actor's sex
  when it resolves, the id's draw otherwise; animals by the ownership
  rule), the same pattern the economy used for pouches. A pre-0.7 save
  wakes into a named world.
- **Co-save.** The `name` component is **additive** — the record's
  format is self-describing (each component under its stable name), so
  an old record simply decodes without it and names are back-filled.
  The record did bump to **v6**, but for the *legacy section* (Stone 2
  below), a real format change. The draft's "v6 for names" proved
  unnecessary; the code's documented convention (additive components
  never bump) won.
- **The voice.** Decisions, bonds, gossip, arcs, births, deaths,
  mediation, trades — every channel speaks names, with the console hex
  beside each: `Vera Hart [00048B77] decides MoveTo -> Sanctuary
  workshop [000250FE]`. An unnamed mind keeps the species label:
  `animal [FF0197BF]`.
- **Verify (verified in-game 2026-08-11):** the log greets the world
  by name, and the names now live **on the actors themselves** —
  `SetOverrideName`, the same mechanism as the console's
  `SetDisplayName` — so the workshop view, pip-boy and hover read
  "Mara Price", not "Settler". Three fixes the verification hunt
  surfaced: (1) the game name is read from the **base form**, never
  the reference — the reference's full-name lookup is empty for most
  actors, so *everyone* looked generic and Mama Murphy, Marcy and Jun
  were being renamed; (2) a **per-second sweep** names a restored
  mind's actor the first time it streams in (fast-travel to a
  settlement names its settlers on arrival) and self-heals stale
  stamps against the base form; (3) the shipped INI's pools were
  **synced to the curated lists** — the INI overrode `Names.h`
  silently, so the curated pools (family tails included) never ran.
  Pets dedupe per world (a restore-time pass — the five Bandits
  became five unique names), owned dogs draw from the animal pool
  again ("Junkyard Dog" is a species label). Names survive save/load
  and fast travel; the news feed (Stone 3) names the people it talks
  about.

  **0.7.3 — the roles gain people (built 2026-08-12, verification
  pending).** A role label ("Provisioner", "Guard", "Minuteman",
  "Caravan Guard", "Trader", "Merchant") is a title, not a name:
  every provisioner in the Commonwealth reads identical in memory, so
  a Trade memory could never tell its seller apart. The sim now keeps
  the role as a prefix and adds the person — "Provisioner Cole",
  "Guard Mara" — deterministic per id, deduped against the world like
  any name, and applied on all three naming paths (seed, the per-
  second sweep, restore self-heal). Real names still win: Sturges
  stays Sturges, and the game-name-first rule is untouched for
  everyone with a proper name. Raiders never reach the sim (no
  workshop faction), so enemies keep the game's own labels.

## Stone 2 — The conflict source: relationships good *and bad* — ✅ verified in-game

**The ask.** Nothing in 0.6.0 made dispositions go negative, so rival
and enemy bonds — and therefore the feud arc — could never begin on
their own. 0.7.0 gives the world a reason to dislike.

**The engine answered the open question (2026-08-11):** no sign
parameter — *kind and result, never a sign*. `Remember`'s direction
lives in the kind (Aid/Social warm, Wronged/Combat sour, Trade builds
trust); `event.Weight` is salience only. The two designed channels for
negative social are `ReportOutcome(Other, Social, Failure)` (−0.1, an
executed interaction that went badly) and `Remember(Other, Wronged)`
(−0.25, an unprompted wrong). The draft's `Remember({keeper, −Social})`
is superseded — the slight now rides the decided channels.

**The design, as built** — one mechanism, engine-native:

1. **A shut stall is a slight.** A hungry human whose walk lands at a
   **closed market** finds no trade and no food (the world-facts gate
   stops new walks, but a walk already in flight when the hour turns
   still arrives). The arrival reports
   `ReportOutcome({keeper, Social, Failure})` — −0.1 disposition toward
   the stall-keeper, the settlement echo spreading the chill. **A
   forgiving mind blames no one:** the temper line
   (`Behaviour.h TemperOf`, the engine's `JitteredTraits` substrate,
   ~0.8–1.2 around 1.0) decides — at or above `sim.slight.temper`
   (default 1.0) the slight lands on the keeper; below, the mind
   shrugs it off (a world outcome — memory only: the stall was just
   shut). Same channel, opposite sign from trust — no new machinery.
2. **The settlement agrees (the echo).** Every mind with a market
   memory belongs to its settlement's group (`Groups`, keyed by the
   market's form id, derived at seed/restore — never persisted). The
   engine's `SpreadToGroupMates` carries each slight fainter
   (`sim.group.inheritance`, default 0.5) to the whole settlement —
   the community grows cold toward a keeper who keeps failing them.
   A newcomer inherits the cold shoulder via `InheritGroupAttitudes`
   before they ever meet the feud's villain.
3. **The feud becomes emergent.** Two or three shut-stall let-downs
   cross the rival line (−0.3); the existing bond derivation names the
   pair, gossip spreads it, and the 0.6.0 feud arc — mediation once a
   day — finally runs in the wild. Nothing is scripted.
4. **Scarcity (deferred).** When the market cannot feed the settlement,
   refusals multiply and slights compound — the famine arc shares this
   machinery when it arrives. Not built in 0.7.0's first pass.

- **Tuning:** `sim.slight.temper` (the blame line),
  `sim.group.inheritance` (engine), the existing
  `sim.bond.threshold.rival/enemy` lines.
- **Verify (verified in-game 2026-08-11).** The engine answered the
  hand-over (`81cfe48` → fixed in core commit `509a54d`):
  `sim.hunger.desperate` (default 0.0 = never desperate, unchanged;
  the adapter ships 0.2) — below the threshold a mind ignores the
  closed sign and walks to the shut market anyway, so the arrival
  lands and the slight fires. With the market forced shut (test
  recipe), the whole chain ran in the wild: `the stall at 00054BAE
  is shut — Paladin Danse went hungry and blames the keeper` →
  rival bonds (the direct slight −0.1 plus the settlement echo) →
  `bonds: X is feuding with Y` → feud gossip → `arcs: Titus Pratt
  cooled the feud between …` (127 mediation attempts in the stressed
  session). Two adapter fixes landed during the hunt:
  - **The feud headline fires on any crossing into Enemy** — the
    old code only said "X is feuding with Y" for a direct jump from
    nothing; the normal rival → enemy path (how shut-stall feuds
    actually arrive) logged only "are now enemies". Now any crossing
    into Enemy is the feud line, and it pushes the news.
  - **The feud is mediated at formation.** `sim.memory.fade` is
    0.2/s — a gossip fact (weight 1.0) dies in ~4.5 s, and gossip is
    written once, never refreshed, so by the next day's mediation
    pass nobody remembered the feud and no mediator could ever be
    found (the arc silently did nothing — day 271's pass ran with a
    feud in the book and produced zero attempts). The fix: the Enemy
    crossing now spreads the feud gossip (the "a feud starts"
    channel Gossip.h always documented but never wired) *and*
    attempts mediation immediately, while the settlement still
    knows. The once-per-day pass stays as a backstop.

## Stone 3 — The player window: the radio — ✅ verified in-game (MCM deferred)

**The news feed (verified in-game 2026-08-11).** World events — bonds,
feuds, births, deaths, market openings — become one-line news with
names: "Marcy and Jun became friends", "a feud has begun between Vera
Hart and Cole Wells", "a child was born to the Vances". Each lands as
an on-screen HUD notification, throttled by `sim.news.cooldown`
(default 5 s — a flood of lines is noise, not news), and appends to
the in-memory news feed (`News.h`) — the settlement's story. The log's
own line stays the verify channel; the feed is the player's window.
The pop-ups verified on-screen: bond formations ("X and Y became
friends") and the feud headlines.

**The settlement radio (verified in-game 2026-08-11).** A radio the
player has built (base form `radio.base.formid`, default 0x1A17D —
the workshop "Radio", **flagged for xEdit verification**; the key
exists so a wrong pin is a config line, never a rebuild) speaks the
news as on-screen captions while within `sim.radio.radius` (3000
units): one caption per `sim.radio.caption.every` (45 s), rotating
through the feed — the settlement tells its own story. Captions
verified rotating on screen.

**MCM + Settings Manager — deferred, honestly.** The behavior MCM
would expose is already real — the INI is the source of truth, loaded
at boot, every key surviving reload (that half of the verify sentence
is proven by the tuning stone). The missing half is the *UI page*,
which needs the MCM mod (a soft dependency) and a Papyrus surface
compiled with the Creation Kit. Both are an author-side asset task,
not a sim task; the INI does the job meanwhile. Real radio *audio* is
scheduled after 0.9.0 — voice needs assets; captions don't, so the
settlement radio speaks in text now.

## What the adapter asked from the engine — answered

Nothing new was needed. Stone 1 and 3 are adapter-side game knowledge;
Stone 2 rides the decided channels and shipped machinery. The engine's
0.7.0 (Legacy — Bequeath, InheritMemory, the legacy store) was wired
in as its proving ground: a death bequeaths the dead's memories to the
household's heir and leaves the dead's name as a registry-level legacy
(v6); a birth inherits the parents' memories of the people (the feud
travels on the memory channel, the inherited cold shoulder on the
group echo).

## The 0.7.0 board (this document)

- [x] Stone 1 — Names: `Name` component (additive; record v6 for the
      legacy section), game names first, gender-split + animal pools in
      the INI, owned animals named / strays nameless, back-filled on
      restore; every channel speaks names — **verified in-game**: the
      names are written onto the actors (SetOverrideName), the base-
      form read fix, the per-second sweep, the INI synced to the
      curated pools, pets unique — and 0.7.3 gives the role placeholders
      people ("Provisioner Cole", "Guard Mara") so memory can tell
      two provisioners apart
- [x] Stone 2 — The conflict source: closed-market arrivals report
      `ReportOutcome({keeper, Social, Failure})`, the temper line
      (`sim.slight.temper`) decides who blames, settlement `Groups`
      spread the echo, rival and enemy bonds form and the feud arc
      begins on its own — **verified in-game**: engine gate
      `sim.hunger.desperate` (`509a54d`), headline on any enemy
      crossing, feud mediated at formation (gossip dies in ~4.5 s)
- [x] Stone 3 — The radio: the news feed (HUD notifications + feed),
      then settlement-radio captions — **verified in-game**; audio
      radio scheduled after 0.9.0
- [ ] MCM + Settings Manager — **deferred**: the INI already delivers
      the tuning behavior; the UI page needs MCM + the CK (author-side
      asset task)
- [x] Tuning: `sim.slight.temper`, `sim.news.enabled/cooldown`,
      `sim.radio.caption.every/radius`, `radio.base.formid`,
      `names.first.male/female/animal`, `names.last`, the engine's
      `sim.group.inheritance` + legacy keys — all in the shipped INI

**Deferred, honestly:** MCM's UI page (above), the radio's real audio
(after 0.9.0 — voice needs assets), the famine arc's scarcity engine
(shares Stone 2's machinery), 0.8.0's agency pillar (hands in the
world).
