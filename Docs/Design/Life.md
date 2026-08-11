# Life & Emergent Quests — 0.6.0 "The Commonwealth Remembers"

**Status:** plan (2026-08-10) — written before any 0.6.0 code; stones build
in order below. The 0.5.0 boundary contract is the foundation; what the
adapter asks of the core is itemized at the bottom and handed over to the
engine in the engine's `Docs/AdapterProject.md`.

## The vision

A settler isn't a mind with a hunger bar. A settler is **born**, **lives**,
**makes friends and enemies**, and **dies** — and the settlement remembers.
Quests happen because life happens: a feud starts over a failed trade, a
widow grieves and then decides between vengeance and comfort, a couple
falls in love at the same bench, a hungry town turns on itself. No quest
scripts. The quest is the behaviour — visible in the world and in the log.

> The test plan is one sentence: *the Commonwealth remembers.*

Kill a settler, save, reload: the survivor still grieves, the feud still
stands, the gossip is still town news, and the child still knows its
parents.

## What life entails (the pillars)

| Pillar | The question | In the sim |
|--------|--------------|------------|
| **Born** | Where do minds come from? | Arrivals (new workshop settlers become minds), then births (a couple has a child — experimental, INI-gated). |
| **Live** | What is a day? | Already real: wake, hunger, market, trade, rest, social. 0.6.0 adds work rhythms and household life. |
| **Die** | What ends a mind? | Combat death and departure. The mind is destroyed; the memory of them lives on in others. |
| **Relate** | How do minds feel? | Bonds — named states (friend, sweetheart, spouse, rival, enemy) built on the core's Disposition/Trust, persisted in the co-save. |
| **Quest** | Why do things happen? | Arcs — adapter-side state machines that steer intents when state (needs, bonds, memory, weather, market) crosses a line. |

## The adapter's 0.6.0 stones

Each stone is a buildable unit with an in-game verification. They build in
order; each one leaves the world strictly more alive.

### Stone 1 — The world keeps its books (lifecycle) — VERIFIED IN-GAME (2026-08-10)

- **Arrivals:** every loaded, sim-relevant actor that is not yet a mind
  becomes one on the tick's one-second census — new settlers, radio
  beacon recruits, and animals alike. The wake seed and the census share
  one `SeedMind` path.
- **Deaths:** the census reads the game's own markers (a killer handle,
  the corpse-cleanup timer, a deleted ref); a dead mind is removed — the
  walk session, the last-log key, the feeder line, and any stall it
  kept (the market re-derives its keeper) — and a **death fact**
  `{ the dead, Death, weight, day }` is pushed to every surviving mind
  (the core gained `InteractionKind::Death` — append-only ordinal).
- **Departures:** a known mind whose actor is alive but no longer in the
  settler faction is removed with a goodbye line. The bond-driven
  walk-out departure (the mind decides to leave) stays with Stone 2,
  where bonds define it — the bookkeeping is ready.
- **Verify (in-game, 2026-08-10):** a real kill books in two passes
  (`reads dead — first pass, not booked (confirming).` then `died — the
  world keeps its books.` a second later); the dead do not restore and
  never ghost-walk after save/reload; mid-session arrivals wake as new
  minds; and a fresh new game books **zero** false deaths while already-
  dead settlers from old saves are parked and never booked. The road
  there took three hardenings (ADR-0011): the 3D gate (streamed-in
  actors only), two-pass confirmation (a corpse reads dead twice, an
  artifact once), and the alive-first rule (a death is a transition — a
  mind must have read alive before it can be booked dead; the spawn
  burst after a big load reads the same actors dead on first sight for
  ~2 s, and only an alive reading un-parks them).

### Stone 2 — Bonds (relationships, good and bad) — VERIFIED IN-GAME (2026-08-11)

- Named bond states derived from the core's `Relationship`
  (Disposition + Trust), thresholds from the INI:
  friend +0.3, sweetheart +0.6, spouse +0.8, rival −0.3, enemy −0.6 —
  the adapter's own defaults when the config file names no lines.
- **Two channels, one derivation** (`Bonds.h`, pure + tested): the
  `RelationshipChangedEvent` on the adapter's `EventBus` (Request A —
  the core crosses a line mid-mutation; the adapter re-derives that pair
  instantly) and the 1-second `ReconcileBonds` pass (drift is quiet in
  the core — a bond cooling below its line is a dissolve, not an event,
  so only the pass sees it). Both feed `Bonds::ApplyPair`, so they can
  never disagree.
- **Mutual and sticky.** The pair's shared disposition is the *minimum*
  of the two directions — both must feel it (one-sided warmth is not
  yet a bond). Formation is immediate at the line; dissolution waits
  until the pair falls halfway back (friend +0.3 dissolves below +0.15)
  — a fresh bond must not vanish to the next drift tick. Same-family
  downgrades are sticky too; a family flip (friends turned rivals) is
  news.
- Bonds are **persisted** (co-save v5: form-id pair, kind, since-Day) — a
  spouse is still a spouse after reload; a v4 save loads with no bonds
  and the pass re-derives them from the restored relationships.
- Log lines: `settler X and settler Y became friends.` /
  `settler X is feuding with settler Y.` — with species labels
  (`child`, `animal`) so a dog bonding with its feeder speaks plainly.
  Formation, upgrade, family flip, and dissolution each get their line.
- **Verify:** watch two settlers socialize into a bond; reload; the bond
  stands (the `bonds: N bonds restored from the co-save.` line at
  restore, and the bond lines on formation).
- **Two in-game discoveries shaped the wiring (2026-08-11):** (1) the
  core's `Trade` outcome builds *trust*, never disposition — only Aid /
  Social warm feelings — so a buyer's disposition toward the keeper
  never grew from trading at all. The buyer's half is now a
  `Remember({keeper, Social})` alongside the trade: a meal at the bench
  is company, the courtship's raw material, and it publishes the
  crossing on the bus (instant bond log). (2) The core's drift default
  (0.05/s, half-life ~14 s) erased any warmth between meals — the
  adapter's world runs the same slow clock the INI sets
  (`sim.drift.rate = 0.0002`, half-life ~1 h, injected when the config
  names none). Four shared meals cross the friend line; meals every
  ~10 s (the fast demo rhythm) bond in under a minute.

### Stone 3 — Households — BUILT + TESTED (2026-08-11) and VERIFIED in-game

- A household is the deepest bond made concrete (`Households.h`, pure +
  tested): the moment a pair's bond becomes **Spouse** (the +0.8 mutual
  line), their two pouches merge into **one shared wallet** (on the
  deterministic lower-id holder); when the marriage dissolves, the wallet
  splits (holder keeps the remainder, the other takes half). The reaction
  rides the bond-change handler — whichever channel crossed the line
  (event or 1-second pass) forms the household exactly once.
- **One bench:** a spouse arriving at the market where their partner keeps
  the stall is at the *family bench* — fed, no exchange (the household
  pouch does not pay itself). The keeper's spouse is the one customer who
  never pays.
- **The shared wallet round-trips** — `PouchOf` resolves a married
  member's pouch to the spouse's when they don't physically hold it, on
  both sides of the bench, and the invariant *one pouch per married pair,
  one per unmarried human* is enforced silently (`Enforce`) on restore
  and as defensive repair each pass. Households are **derived state** —
  the marriage rides the bond map (co-save v5) and the wallet rides the
  `CapPouch` component — so no record bump was needed (ADR-0013). A dead
  spouse's pouch passes to the widow(er), not the void.
- **Deferred (documented, not built):** walking to market *together* (no
  path coordination) — the shared-bench behavior is the observable half,
  and it waits for the companionship intents of a later stone. The same
  *bed* was the rest intent's work — delivered below as the sleep cycle.
- **Verified in-game 2026-08-11:** the spouse bond emerged organically
  from shared meals — `0x2a8a7` and the Sanctuary keeper `0x50976`
  went friend → sweetheart → spouse across sessions (each meal warms
  both directions +0.1). The marriage rode the co-save's v5 bond map
  (`bonds: 23 bonds restored`), the household reformed silently on
  restore (ADR-0013), and the family bench fed the spouse for free:
  `settler 0x2a8a7 is at the family stall at market 0x1d0e2 — fed from
  the household's meal.` The remaining cosmetic — a `(household; N
  left, M now)` wallet line when the pair buys elsewhere — is a matter
  of the pair meeting more.
- **The meal-cadence gap (observed, deferred):** the sim's Rest and
  Explore intents are table slots that execute nothing in-game, so the
  game's sandbox wanders settlers away from the market between meals —
  a pair shares a meal every 7–10 minutes instead of every ~10 s. A
  future stone (game-side Rest/Explore execution — a commanded
  rest/wander that holds settlers near the bench) would collapse the
  meal cadence and make bonds and marriages form in minutes. This is
  the missing half of the sleep cycle's in-world presence.

### Stone 3.5 — The sleep cycle — BUILT + TESTED (2026-08-11, 16/16 suites)

- The 24-hour market test exposed the gap: **a fed mind parks in Rest
  forever.** The engine's need loop only decays; nothing ever restored
  Fatigue. Only Hunger was restored (the meal), so the moment a mind ate,
  its drained Fatigue became the most urgent need → Rest → and Rest was
  a table slot that did nothing. Seven trades, seven different buyers,
  zero repeats — no pair ever shared a second meal, so no bond could
  form (the household test's dead end).
- The fix (`Behaviour.h` `RestRecovery`, wired in `Adapter::Tick` before
  `Update`): a mind whose last intent was Rest recovers the needs a nap
  fixes — Fatigue, Safety, and Comfort — at `sim.rest.recovery` per
  second (default 0.2/s — a full nap in ~5 s). The next `Update` decides
  from the rested mind: hunger is most urgent again → `MoveTo` → walk to
  market → trade → meal → Rest → recover → repeat. The same pair finally
  meets repeatedly, and bonds can form.
- **The parked-mind rescue (review pass, ADR-0015):** the recovery is
  keyed on the *needs*, not just the Rest intent. A mind whose Safety is
  most urgent with no remembered threat gets `nullopt` from the engine's
  Decide (nothing to flee) — the intent is removed and no intent-keyed
  pass could ever see it again. Reading the needs, a mind with no intent
  whose most urgent need is Fatigue or Safety counts as resting,
  recovers, and rejoins the loop.
- **The loop closes** (SleepCycleTest): a fed mind with drained Fatigue
  decides Rest; after a nap it decides MoveTo to the remembered market;
  and a Safety-parked mind (no intent, no threat) recovers and decides
  again. Without the recovery it parks forever — the tests pin the exact
  bugs the 24h-market run and the review pass exposed.
- **The walk layer, fixed too (ADR-0015):** arrival now ends the walk
  session (the slot frees instantly) and a Reached session no longer
  counts as "already walking" — a fed mind standing at the bench re-walks
  the moment it is hungry again. Before the fix the finished trip held
  one of the 16 walk slots for the full 120 s timeout and swallowed the
  re-walk: 7 trades, 7 buyers, zero repeats.
- **Verify:** relaunch the fast demo (24h market + `sim.hunger.decay =
  0.1`) — customers return to the same bench for second meals; the first
  `bonds: settler X and settler Y became friends.` lands within minutes;
  a marriage (and `households: … now a household`) follows. The verify
  chain ran 2026-08-11 — friendships cascaded from shared meals, the
  first marriage formed, survived save/load, and the family bench fed
  the spouse (Stone 3's verify line).

### Stone 3.75 — The meal-cadence wander (Rest/Explore executes in-game) — VERIFIED IN-GAME (2026-08-11)

- The marriage test's honest gap: a fed mind that decides Rest or Explore
  got *nothing* — both were table slots. The game's sandbox took over and
  wandered the settler away from its bench; its market memory faded; the
  next shared meal was 7–10 minutes away instead of the ~10 s the
  cooldown allows (the observed stall in the first marriage hunt).
- **The first fix (`Movement::HoldPlace`)** parked the actor in place —
  the verified command-mode travel package targeted at itself. It worked
  (meals collapsed to the cooldown cadence, verified in-game), but it
  froze the settlement: everyone stood still at the bench.
- **The fix now (`Movement::WanderNear`):** Rest and Explore execute as
  a bounded wander — a real, nearby reference in the actor's own cell
  (furniture preferred, then any non-actor object), commanded through
  the same travel package. On arrival the sandbox resumes until the
  next command (rate-limited to one wander per mind per 30 s), so the
  game plays its own idles between commands — it may even sit a settler
  at the bench it walked to. The cadence holds (the sandbox cannot
  drift the actor away before the next command), and the settlement
  looks alive: settlers mill around home between meals. Empty cells
  fall back to HoldPlace — parked, never teleported.
- Walk, rest, explore — the sim's three movement intents are all real
  game commands now. Socialize/Work/Flee remain table slots (their
  stones are future work).
- **Verify (2026-08-11):** settlers mill around the settlement between
  meals instead of freezing at the bench (approved in-game), and the
  same pair's meals stay on the cooldown cadence — the first
  friendship and later a marriage landed within minutes at demo pace.

### Stone 4 — Gossip — VERIFIED IN-GAME (2026-08-11)

- Word spreads: a bond crossing (friend, sweetheart, spouse, rival,
  enemy), a death, a feud writes a fact to every mind of the settlement
  (`Gossip.h`). The "gossip radius" is the settlement itself — one book
  per settlement; strangers and fresh arrivals don't know (gossip is
  written once, it is not replayed).
- Wired into both news channels: `OnBondChange` (every formation names
  both participants) and `RemoveMind` (a death names the dead — the fact
  the grief arc reads). Pure, harness-tested without the game.
- The observable half: a death logs `gossip: N minds remember settler
  X is gone.` (one line per death — the count `Gossip::Spread`
  returned), so the stone is verifiable in the log instead of silent.
- **Verify (2026-08-11):** a kill logged `gossip: 643 minds remember
  settler 0x2f2a7 is gone.` (641 on the follow-up kill) — the
  settlement remembers the death; the survivor's grief line followed
  (Stone 5); a newly arrived settler never hears it (written once,
  never replayed).

### Stone 5 — Emergent arcs (the quests) — VERIFIED IN-GAME (2026-08-11)

Arcs read the world's own state — bonds, gossip, deaths — and steer the
sim's numbers, so a story is never a script. Two arcs ship built; the
other three stay sketched below.

1. **The Feud — BUILT** — an enemy pair the settlement has heard of
   draws a mediator once per day (`Arcs::Mediate`, run by
   `Adapter::RunMediation` on the day cadence): a third mind who knows
   both sides steps in. A mediator liked by both cools the feud (each
   side warms a step toward zero and trusts the mediator more); a
   meddler nobody likes is told off and the feud holds. The settlement
   pulls its own apart — no scripts.
2. **The Grief — BUILT** — a loved death (a `Death` memory still heavy,
   of someone at/above the friend line) is grief (`Arcs::Grieving`): the
   survivor's Social drains at `sim.arc.grief.decay` extra per second
   (`Arcs::ApplyGrief`, every tick) — they seek company. Derived from
   persisted components, so grief survives save/load for free. The
   vengeance/comfort fork stays sketched for a later stone.
3. **The Courtship** — repeated socializing at the same place raises
   disposition; +0.6 → sweetheart; mutual +0.8 → spouse (hands off to
   the household stone). Now visible in-game: shared meals *are* the
   courtship (the observed friendships → sweethearts → marriage chain,
   2026-08-11).
4. **The Departure** — all bonds negative and needs unmet → the settler
   leaves; the settlement remembers them (a goodbye fact, then the mind
   despawns). Not built — sketched.
5. **The Famine** — the market can't feed the settlement; hunger spikes,
   dispositions fray, some settlers consider leaving, and the market
   stays open late while the famine lasts. Not built — sketched.

- **Verify (2026-08-11):** the grief arc's lifecycle is in the logs —
  death → gossip → `arcs: settler 0x50976 grieves for 0x2f2a7 — they
  seek company.` → the bereaved's Social draining as it seeks company.
  The feud's *organic* appearance waits for 0.7.0's conflict source
  (nothing in 0.6.0 makes dispositions negative — enemy pairs cannot
  form yet; the arc is harness-verified).

### Stone 6 — Birth (experimental, INI-gated) — VERIFIED IN-GAME (2026-08-11)

- A spouse household has a child: **a sim-only mind** — there is no game
  actor, no form, no translator entry (`Birth::Create`). Seeded like any
  mind (needs, memory, goals) plus a warm relationship to both parents
  — and the parents know their child. Fed by the household
  (`Birth::FeedChildren`, every tick — hunger recovers directly,
  protected and fed, never walking to a market). Lives in the co-save
  like any mind.
- One birth per sim day, at most; the household the Rng draws. Gated by
  `sim.birth.enabled` (default 0) — off in the beta; on is a stress test.
- The census cannot evict it: `Lifecycle::Diff` classifies the *census*,
  never the registry, so a mind with no form is simply never scanned.
- **Verify (2026-08-11):** `birth: a child is born to 0x50976 and
  0x2f2a7 — a new mind, fed by the household.` fired in-game, and the
  wake line made the sim-only population visible: `The Commonwealth
  wakes up: 3 sim-only children restored too — fed by their
  households.` across two save/load cycles — the children ride the
  co-save. (A reload was briefly treated as a new day and re-birthed;
  the day gates now seed from the current day on restore — ADR-0017
  addendum 4.)

### Co-save — no v6 bump needed

Bonds ride v5 (form-id pairs + kind + since-Day) and households ride the
shared pouch. A child is an entity carrying existing components
(SpeciesTag, Needs, Memory, Goals, Relationships) — all already
serialized, so the child persists with no new record (the adapter's
serializer registration covers it; the child's parents restore by their
form refs). Gossip and arcs are *derived* from persisted components
(memory events, relationships, needs) — no record of their own
(ADR-0017).

### Tuning (INI additions)

Bond thresholds (friend / sweetheart / spouse / rival / enemy),
grief decay (`sim.arc.grief.decay`), mediation on/off
(`sim.arc.mediation`), births on/off (`sim.birth.enabled`), the wander
(`sim.wander.cooldown` seconds between commands, `sim.wander.radius`
in game units). Every new number is a `sim.*` or `bond.*` key the
adapter reads — same file, same rules (unknown keys ignored, broken
lines keep defaults).

## What the adapter wants from the engine

**The honest headline: the 0.5.0 boundary contract suffices for nearly all
of this.** The adapter builds lifecycle, bonds, arcs, gossip, and births
on the existing surface — `CreateEntity` / `DestroyEntity` / `Remember` /
`Update`, the `Relationships`, `Goals`, `Memory` components, the `Outcome`
channel, observation events, `WorldTime`, and the seeded `Rng`.

One stone would align the sim with the core's push-don't-poll philosophy:

### Request A — `RelationshipChanged` observation event (core stone 08 candidate)

The core already owns disposition/trust dynamics (drift, outcome shifts).
Let the core emit an observation event when a relationship crosses a
configurable threshold — so bond formation is an **event** the adapter
(and, through gossip, other minds) react to, instead of the adapter
polling `Relationships` every tick. The event carries:

```
subject (EntityId) · other (EntityId) · disposition · trust
threshold crossed (name from tuning) · world day
```

Thresholds come from tuning (`sim.bond.threshold.*`), so the core stays
world-agnostic — it knows nothing about "marriage", only that a
relationship crossed a line the world configured.

### Request B — optional, deferred: `GoalType` growth

If the core wants arcs first-class (e.g. `FindPartner`, `Avenge`, `Lead`)
rather than the adapter chaining the four generic intents
(`AcquireFood`, `ReachSafety`, `Socialize`, `Prosper`), add them. **Not
required for 0.6.0** — the adapter chains the existing four.

### What the adapter will NOT ask for

- No population logic, aging rules, or quest grammar in the core — those
  are world-specific and the adapter's job by design.
- No game knowledge of any kind — the core stays a stateless, world-free
  tick, exactly as the boundary contract promises.

## The 0.6.0 test plan

1. Two settlers become friends; one slights the other; the feud arc
   starts; gossip spreads it; a neutral settler mediates.
2. A couple courtships at the bench, marries, and their pouch is shared —
   save, reload, the bond and pouch survive. *(Verified in-game
   2026-08-11 — the marriage chain and the family bench.)*
3. Kill the spouse: the survivor grieves (Social drains at
   `sim.arc.grief.decay`; they seek company — the built grief arc), then
   the vengeance/comfort fork follows in a later stone.
4. Let the settlement starve: the famine arc opens the market late,
   dispositions fray, someone leaves. *(Not built — sketched.)*
5. A birth (with `sim.birth.enabled = 1`): a child mind is born,
   protected, fed, bonded to its parents — and returns from the co-save.
6. The meal-cadence wander: a fed mind mills around its settlement
   between meals (no more standing frozen at the bench), and the same
   pair's meals stay on the cooldown cadence.

Everything observable in the log; every behaviour driven by needs, bonds,
memory, and the seeded RNG — never by a script.

## Beyond 0.6.0 — the author's ideas (2026-08-10)

Three pillars the author wants after the life core is real. They live
here as sketches; each becomes a designed stone when its turn comes.

### Identity — settlers get names

The sim talks about people by formid hex. Names make it human: logs,
gossip, bonds, and the radio all name people — "Marcy and Jun became
friends", "a child was born to the Lees", "Sturges is feuding with the
provisioner".

- FO4's generic settlers are mostly named "Settler" in-game, so the
  adapter assigns **procedural Commonwealth names** at mind-seed time
  from a name list (INI-selectable), persisted in the co-save (a name
  registry keyed by form id) — a settler is "Sturges" in Sanctuary and
  still "Sturges" after reload and after a fast-travel despawn.
- **Verify:** the log greets the world by name; save, reload, same names;
  a name collision never happens (the registry dedupes).

### Agency — settlers act on the world: build, move items, destroy

Today settlers walk and trade. This pillar gives them **hands**: their
intents produce real world changes, not just movement.

- **Move items** — a hauler transfers stock between containers
  (workshop → stall, stall → workshop) using the game's item APIs; the
  stall actually sells what was stocked.
- **Build** — a builder places objects at the settlement through the
  workshop placement path (position, rotation, resources); the bench
  grows a stall, a fence, a chair the sim built.
- **Destroy** — a destroyer clears clutter (disable refs); decay, raids,
  and spite take things apart.
- **Engine ask (tentative):** the core's four `GoalType`s don't cover
  labour. The adapter can map `Prosper` → labor intents with the
  existing surface (no core change needed), but if the core wants
  labour first-class — `Construct`, `Haul`, `Demolish` — that's a
  growth of Request B.
- **Verify:** a stocked stall sells stocked goods; a builder's fence
  survives reload; a destroyer's clearing stays cleared.

### The Player Window — a radio, and MCM

The world is alive even when the player isn't watching. Give the player
ears and hands.

- **The radio** — a news channel that synthesizes world events into
  plain speech for the player, in three steps:
  1. **The news feed (first):** the adapter turns events (bonds, births,
     deaths, feuds, famine, market openings) into one-line news, pushed
     as in-game notifications and written to a news log the player can
     open — "A feud has begun in Sanctuary", "A child was born at
     Tenpines".
  2. **The radio object:** a transceiver in settlements (an in-game
     radio marker) plays the news as on-screen captions while the
     player is near — the settlement tells its own story.
  3. **Real audio (deferred):** voice lines need assets; the radio
     becomes a real station only when the author ships or licenses
     audio. Not a blocker for 1 and 2.
- **MCM + Settings Manager** — a Mod Configuration Menu page exposing
  the tuning keys (hunger rhythm, market hours, bond thresholds, births
  gate, gossip radius, population cap) so players tune the world
  in-game; the INI stays the source of truth, MCM reads and writes it
  (or an override file), with a restore-defaults button. Requires the
  MCM mod as a soft dependency and a small Papyrus surface on the
  adapter side.
- **Verify:** news appears for a feud without the player nearby; MCM
  changes survive reload; defaults restore cleanly.
