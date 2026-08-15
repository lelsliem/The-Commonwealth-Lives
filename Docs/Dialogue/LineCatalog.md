# LineCatalog — Voiced Settler-Bank Lines

> Hand-curated from the game's own voice bank (see `VoiceBank.md` for the survey).
> Every line below is *voiced* — the game ships audio for it in the 8 settler
> voice types (Male/Female × Old/EvenToned/Rough/Boston). The `(nv)` tag is the
> per-line voice coverage out of 8. Lines are the sim's *ambient* layer: they
> can be triggered through the game's Say/PlayIdle system when the player is
> near. Memory-driven lines (names, feuds, grief for specific people) have no
> recording and stay caption-only — see `HybridArchitecture.md`.
>
> Source: Fallout4.esm quest dialogue + Fallout4_en.DLSTRINGS, resolved through
> the settler Voices BA2 (4,737 lines extracted, 8 voice types).
>
> History: the UNSORTED bucket (2,087 lines) was filtered by hard register
> rules (combat/pain/topic/player/tape/exposition/location/length/1v) and
> hand-curated — 43 survivors were merged into the pools below, ~2,044 were
> eliminated. A second pass cut the pools to their final sizes (duplicates,
> 1v coverage, response-only lines). Fight and Feud have no true voiced pool;
> their single lines are the closest recordings, borrowed into Row.

---

## Greet — 30 lines

The workhorse pool — said at crossings, by every mind. Includes stallkeeper
and vendor greetings that double for market moments.

| # | FormID | Vo | Line |
|---|--------|----|------|
| 1 | 03f6f7 | 8 | Hey... |
| 2 | 061be4 | 3 | Hey. |
| 3 | 06202b | 3 | Howdy. |
| 4 | 194324 | 1 | Hey there. |
| 5 | 123041 | 7 | Hey, how's it going? |
| 6 | 192340 | 1 | How's it going, friend? |
| 7 | 18e9f9 | 8 | Hey, thanks for stopping by. |
| 8 | 18f9db | 1 | Hey there! Welcome back. |
| 9 | 19e9e6 | 4 | Hello, neighbor. |
| 10 | 19ffcd | 1 | Hey, buddy. |
| 11 | 062028 | 1 | Hello, handsome. |
| 12 | 062029 | 2 | Hello, cutey. |
| 13 | 16ada3 | 1 | Hey bean, what's cookin'? |
| 14 | 1a95a5 | 1 | Howdy, friend. Grab a seat by the fire. Everyone's welcome. |
| 15 | 112040 | 8 | Hey, I just wanted to introduce myself. |
| 16 | 0dc91e | 1 | Hey there. Trading? |
| 17 | 08558b | 8 | Well hey there, pup. |
| 18 | 1a95f8 | 6 | Woah... is it morning already? |
| 19 | 1263b1 | 1 | Hey, thanks! |
| 20 | 020d39 | 1 | Hey again. Step right up. Little bit of everything from all over. |
| 21 | 123702 | 1 | Hey, he brought it up, not me. As I was saying. |
| 22 | 14ce79 | 1 | Hey, feel free to sit down or whatever. But, uh, well... I think I ate a bad can of meat. |
| 23 | 190dff | 1 | Hey, this paranoia's what keeps us alive. |
| 24 | 020d3c | 1 | Hey. Market'll be open in the morning. Come by then. |
| 25 | 1a4d29 | 1 | I know you're probably busy. |
| 26 | 04abfe | 1 | Hey, I'm getting over someone, okay? A little compassion over here. |
| 27 | 19df5d | 1 | Either way, good to see a new face. |
| 28 | 112043 | 8 | I don't think we've met. What can I do to help out? |
| 29 | 10c841 | 8 | By the way, feel free to use our workshop. It's the least we can do. |
| 30 | 1a95fa | 6 | Go away. It's the middle of the night... oh hi... |

**Cut:** 48 — duplicate "Hey." variants (2 cut: the 3v and 2v copies), duplicate
"Good to see a new face.", named NPCs ("Hey Nick.", "Hey, Daisy, what can I
get for this?"), player-directed ("Hey, you're that Vault Dweller."),
quest/location ("Hey, I know you! You're that minuteman, Garvey!"), holotapes,
and combat/alarm ("Hey! Security!").

---

## Gossip — 23 lines

The settlement's daily chatter — work, weather, supply, the quiet. Final
pool is the 8v/7v core: lines every voice type can say. Some double as Row
openers when delivered short.

| # | FormID | Vo | Line |
|---|--------|----|------|
| 1 | 034129 | 8 | Why's it gotta be so quiet? |
| 2 | 07bb01 | 8 | Got work to do, can't talk now. |
| 3 | 1153e8 | 8 | People are having to sleep in shifts. It's making everyone a bit cranky. |
| 4 | 1153e9 | 8 | Things are a bit crowded. A few more beds would lift everyone's spirits. |
| 5 | 192333 | 8 | Nothing, really. Pretty quiet. |
| 6 | 18baa4 | 8 | My heart goes out to those folks. |
| 7 | 111f58 | 8 | I heard we had a trader in today. |
| 8 | 18baaa | 8 | Almost out of stimpacks. We'll need more soon. |
| 9 | 15f083 | 8 | Nice to have some good news around here for a change. |
| 10 | 111f56 | 8 | What I wouldn't give for some real time off. |
| 11 | 1153e0 | 8 | I'm just so hungry all the time. |
| 12 | 15524d | 8 | Another day of hard work. It never changes. |
| 13 | 156a06 | 8 | Can't remember the last time I had clean fingernails. |
| 14 | 156a0c | 8 | Farming's as honest as honest work gets. |
| 15 | 1569fc | 8 | Crop's comin' in pretty good. |
| 16 | 1569fd | 8 | The farm's not much, but it's something. |
| 17 | 1569fe | 8 | Easy living, this ain't. |
| 18 | 1909c8 | 8 | It's not perfect, of course, but I think with some work we can make a go of it. |
| 19 | 05e5c3 | 8 | Well... I guess there's nothing for it but to do our best. |
| 20 | 11e29e | 8 | If we want things to get better, we've got to start helping each other. |
| 21 | 18ba82 | 8 | Well, hopefully tonight will be better. |
| 22 | 122ff3 | 7 | Better than working, right? |
| 23 | 123056 | 7 | I think I ate too much. |

**Cut:** 101 — the 6v-and-below lines (quest-y or weak: "This place ain't what
it used to be", "Feels like we've been on our own", "Sure could use a Mister
Handy", "Did we miss all the fun?", "I guess we're on our own now", "Amazing
what people will just leave lying around like junk"), the response-only 8v
lines ("Really? Well, that's the best news I've heard in a long time!", "Well,
now. That's some good news.", "It's going to be all right then, I'm sure of
it.", "I don't think so. Things are actually going pretty good.", "It's nice
to have a positive outlook on the future for once."), named rumors, faction
drama, and player-directed lines.

---

## Row — 20 lines

The verbal ramp before a fight. Early lines are dismissals; the last ones
earn the escalation.

| # | FormID | Vo | Line |
|---|--------|----|------|
| 1 | 0187b2 | 6 | Beat it. |
| 2 | 01f97d | 6 | Keep walkin'. |
| 3 | 03247b | 8 | Leave me alone! |
| 4 | 03593a | 5 | Back off. This is mine. |
| 5 | 0390ed | 4 | I'm not looking for trouble. Enough junk here for both of us. |
| 6 | 0711cf | 8 | Just move along. |
| 7 | 01f97c | 6 | You're botherin' me. |
| 8 | 022eb0 | 6 | Look, just leave me alone. |
| 9 | 05fc82 | 4 | Got a problem? |
| 10 | 156a02 | 8 | Ain't looking for trouble, I hope. |
| 11 | 1569ff | 8 | My back hurts, my feet hurt. Everything hurts. |
| 12 | 022eae | 6 | Don't talk to me. Might attract someone's attention. |
| 13 | 1989f9 | 1 | Don't talk to me. |
| 14 | 0711ce | 8 | Leave. Now. Or else. |
| 15 | 0d14a7 | 8 | Better not try that again. |
| 16 | 022ead | 6 | Just... stop. Okay? We got our own problems. |
| 17 | 1478e1 | 4 | Finders keepers: the law of the wasteland. |
| 18 | 18f9ff | 4 | Bullshit. |
| 19 | 1a89c3 | 6 | Asshole... |
| 20 | 05fc3d | 8 | All done. Any other complaints? |

**Sources:** #11 is the Fight pool's only voiced line (the row opener — see
Fight). #12–13 are the Feud pool's only voiced lines (the cold openers — see
Feud). **Cut:** 42 — duplicate dismissals ("Back off." 1v, "Leave me alone."
1v, "Don't want any trouble." 2v, "I don't want any trouble, but finders
keepers."), 1v gatekeeper lines ("This place is off limits", "You're
trespassing on private property"), and "I found it first." (5v).

---

## Grief — 15 lines

Rare and heavy — every line must land. All generic (no names); the sim's
*personal* grief (who died, who they were) is caption-only.

| # | FormID | Vo | Line | Note |
|---|--------|----|------|------|
| 1 | 032098 | 8 | She's... she's dead! | ⚠️ she-specific |
| 2 | 034127 | 8 | Here one second, gone the next. Who moves that fast? | |
| 3 | 03412a | 8 | Gone... gotta be... | |
| 4 | 19b41e | 4 | I've lost... everything. | |
| 5 | 192d32 | 1 | Jesus. They're dead... | |
| 6 | 1905ab | 1 | I can't believe they're dead... | |
| 7 | 17c949 | 1 | He... He's dead... | ⚠️ he-specific |
| 8 | 18ba91 | 8 | I'm truly sorry. | |
| 9 | 18ba92 | 8 | I'm sorry. I know how hard this has been. | |
| 10 | 18ba94 | 8 | Damn. I'm really sorry. | |
| 11 | 061ff0 | 3 | One way of looking at it. The other is to be thankful so many people passed. | |
| 12 | 070db4 | 1 | Oh... I'm sorry. I probably shouldn't have asked. I hope I haven't re-opened any old wounds. | |
| 13 | 18baa6 | 8 | I don't know how they carry on after something like that. | |
| 14 | 18baa5 | 8 | It's just horrible. | |
| 15 | 1abc3e | 6 | I should never have gotten my hopes up. | |

**Gender-selective:** #1 (she) and #7 (he) only fit a matching victim. Kept
for now because their 8v coverage is the best raw grief in the bank — but
flagged to circle back later unless replacements surface. **Cut:** the other
87 were named-NPC deaths ("Austin's... dead.", "Blake, I hope you know..."),
combat barks ("You're dead!", "Stay gone."), transactional apologies ("Sorry,
don't have anything to sell."), and quest exposition.

---

## Fight — 2 lines (no true voiced pool)

The settler bank's 16 Fight candidates were false positives on
"hurt/scrap/shove" — pleading lines, combat barks, vendor talk. The guard
bank is where the real escalation lives: guards are written with threats and
"stand down" lines settlers never got. 2 usable:

| FormID | Vo | Line | Source |
|--------|----|------|--------|
| 034143 | 3 | Don't think I won't hurt you! | Guard bank — the missing pre-fight threat |
| 03429e | 4 | Ain't my fight! | Guard bank — the dismissive step-back |

Plus the settler gripe borrowed into Row as its opener:

| FormID | Vo | Line | Where used |
|--------|----|------|-----------|
| 1569ff | 8 | My back hurts, my feet hurt. Everything hurts. | Row #11 |

Fight specifics (who started it, what they did) stay in our caption pools —
the sim already writes them: "You ripped me off.", "Go on, one more time."

---

## Guard additions — 14 lines (surveyed bank: 618 lines, 4 voices)

The guard bank (GuardMaleDiamondCity01/02, GuardFemaleVault81,
GuardMaleVault81; ~1,665 files) is location- and player-directed by nature —
its Greet/Gossip/Trade pools are nearly all Diamond City or Vault 81
specific ("Welcome to Diamond City, motherfucker..."). But its **Row register
is gold**: guards are the game's authority figures, so they own every
"back off / last warning / move along" line the settlers lack.

**Row +12 (guard register):**

| FormID | Vo | Line |
|--------|----|------|
| 0711cc | 4 | I'm warning you. Back off! |
| 0842ac | 4 | Last warning. |
| 085580 | 4 | Get out of here, you. |
| 0711cb | 4 | Keep moving. |
| 034141 | 4 | So that's it, huh? |
| 0179d3 | 2 | What? You got a staring problem? |
| 018221 | 2 | You cause trouble, there's gonna be trouble, got it? |
| 018791 | 2 | No loitering. |
| 0d3fad | 2 | Stay out of trouble. |
| 0d3fb1 | 2 | Don't try anything. |
| 153736 | 1 | You got bricks for ears? I said get the hell out of here. Now. |
| 15376d | 1 | Told you to move along. Find somewhere else to gawk. |

**Fight +2 (above, the guard threats).** **Cut:** ~440 — combat barks
("Get down!", "We're being surrounded!"), player-directed gatekeeper lines
("Keep moving, scavver.", "Stay safe, ma'am."), quest/NPC lines (Sheng,
Erin's cat, Travis, Bobrov brothers), and the grunt/hit-reaction block.

**Note:** `0711ce` "Leave. Now. Or else." and `0711cf` "Just move along." are
recorded by *both* the settler (8v) and guard (4v) banks — already in the
Row pool; the guard recordings just extend coverage. `034127`/`03412a` grief
lines are likewise shared (already in Grief).

---

## Child additions — 24 lines (surveyed bank: 192 lines, 2 voices)

Children are the surprise: a small bank (~256 files) that's mostly
player-directed, but the kids have a **real sim register** — the Diamond City
trader kid (Meg), the Bunker Hill farm kids, and the Vault 81 schoolkids all
have generic lines that work settler-to-settler. Child lines are (n/2) —
only MaleChild/FemaleChild can say them, so they slot into the pools as a
child-only subset the sim can pick when a mind is a child.

**Greet +3:**

| FormID | Vo | Line |
|--------|----|------|
| 04650f | 2 | Hello. |
| 05e553 | 1 | Hey, thanks for your help. |
| 1ac0d6 | 1 | Whatever, I got better stuff to do. |

**Gossip +7:**

| FormID | Vo | Line |
|--------|----|------|
| 1ac59e | 2 | I heard there's all sorts of scary monsters in the Commonwealth. |
| 1ac59f | 2 | Mom says it's too dangerous to play outside. |
| 1ac5a6 | 2 | My friend is all better now. Being sick sucks. |
| 159c5b | 2 | Working on a farm is really boring. |
| 159c5f | 2 | When I grow up, I'm gonna have my own farm and make other people do all the work. |
| 14e677 | 1 | Watch out around here. If they catch you having fun, they'll make you do boring science. |
| 1ac0e7 | 1 | Man, what I wouldn't give for some snack cakes right now. |

**Row +6 (kid cheek, not kid aggression):**

| FormID | Vo | Line |
|--------|----|------|
| 1ac039 | 1 | Leave us alone! |
| 1ac0d5 | 1 | Aw, you're no fun. |
| 159c60 | 2 | Just so you know, I'm keepin' my eye on you. |
| 122234 | 1 | Your loss. |
| 10744f | 1 | Tattle-tale! Tattle-tale! |
| 1ac0ef | 1 | No one messes with us and gets away with it! |

**Trade +5 (the kid trader — Meg and the Bunker Hill kid):**

| FormID | Vo | Line |
|--------|----|------|
| 1ac0df | 1 | Hey, wanna make a deal? |
| 1ac0e1 | 1 | Up for a trade? I got toys for all ages. |
| 122238 | 1 | Caps up front, thank you. |
| 1ac0e2 | 1 | Are you buyin'? Or just in the way? |
| 183ef0 | 1 | *chuckle* Easy caps. |

**Family +3:**

| FormID | Vo | Line |
|--------|----|------|
| 17fa6e | 2 | Kids gotta earn their keep, too. |
| 1a70ac | 1 | I think we should get a dog. A big furry one that could scare off bandits but would sleep next to me and keep me warm! |
| 1a70ad | 1 | Dad, can we leave a light on tonight? I like sleeping with a light on. |

**Cut:** ~168 — the school/holotape lines (Mister Zwicky, Miss Edna, pop
quizzes), player-directed questions ("Hi, mister."), quest kids (Austin,
Cedric, Gary), and the grunt block.

**Synth verdict:** the Gen3 bank (214 lines) is nearly all Institute quest
dialogue — "Good to see you, director", SRB/Courser/Father business — and its
escaped-synth lines ("We just want to live normal lives, that's all!") are 1v
and quest-tied. Nothing usable for the pools. Synth settlers already speak
through the settler pool (they're settler-race NPCs to the game), so nothing
is lost.

---

## Feud — 2 lines (no true voiced pool)

Feud is inherently memory-driven ("I saw what you did at the bench") — no
recording exists. The cold openers below are the closest the bank has;
borrowed into Row:

| FormID | Vo | Line | Where used |
|--------|----|------|-----------|
| 022eae | 6 | Don't talk to me. Might attract someone's attention. | Row #12 |
| 1989f9 | 1 | Don't talk to me. | Row #13 |

The feud itself stays caption-only.

---

## Trade — 30 lines

The market exchange — the stall-keeper's calls and the buyer's haggle. These
are the sim's *audible* trade moments; the actual trade (caps, goods) is
sim-state and caption-free.

| # | FormID | Vo | Line |
|---|--------|----|------|
| 1 | 076aad | 8 | Looking to buy? |
| 2 | 020d3b | 1 | Welcome back. Looking to trade? |
| 3 | 144f2e | 1 | Lookin' to make a deal? |
| 4 | 03d61a | 1 | It's late. Ya gonna buy somethin'? |
| 5 | 106d05 | 1 | If you're here to trade, let's trade. |
| 6 | 047813 | 1 | We've got some supplies I can offer, if you're interested. |
| 7 | 04c916 | 1 | Got some goods I can sell you, if you're lookin'. |
| 8 | 019a9b | 1 | It ain't a lot, but I've got a few basic supplies. Ammo, meds, that sort of thing. |
| 9 | 161f05 | 1 | Got a few supplies I can trade. The one thing we don't need is more junk. Got enough of that already. |
| 10 | 161f06 | 1 | If you're here to stock up, I've got some supplies. The junk you see laying around isn't for sale, though. |
| 11 | 16a792 | 1 | Got a few things for trade, if you're interested. Lord knows it wasn't always that way. |
| 12 | 16a793 | 1 | Things were pretty lean for a while, but now we've got plenty to trade. Interested? |
| 13 | 1642c8 | 1 | You know the score, friend. If you've got the caps, we can talk. |
| 14 | 1a95af | 1 | Yeah, sure. Whatever I've got left, I'm happy to trade for. |
| 15 | 1263da | 1 | Come back later, my stock's always changing. |
| 16 | 1263db | 1 | No caps, no room. |
| 17 | 0c065d | 8 | I can help, but not for free. You'll need to come back when you have the caps. You need anything else? |
| 18 | 11a084 | 1 | Come see me when you've got the caps. |
| 19 | 06857a | 2 | I'm just here for the caps. |
| 20 | 11532e | 1 | Just had to push, didn't ya? 50 caps, take it or leave it. |
| 21 | 1069f8 | 1 | I'm not here to haggle. You'll get whatever I feel like giving you, got it? Now what's it gonna be? |
| 22 | 182964 | 1 | So, do we have a deal? |
| 23 | 018f4b | 1 | Two caps each. |
| 24 | 16eb02 | 1 | A little, but buy the lot and I'll throw in a dose of Rad Away as a chaser. Deal? |
| 25 | 1830f8 | 8 | You're looking at 'em. But I'll sell you a stake in it. Hundred caps. Wait, no, fifty. Ten? |
| 26 | 16cc50 | 8 | What'll ya have? |
| 27 | 16cc4e | 8 | What're ya drinking? |
| 28 | 115ce5 | 8 | Step right up, all the clothing fit to wear. |
| 29 | 19234e | 8 | Need armor? |
| 30 | 05fb8c | 4 | Can never have enough ammunition. Pricey, though. |

**Cut:** 116 — named NPCs ("Myrna. Got any Fusion Cells?", "Bluejay does
sales."), quest haggles, the kidnapped/raid crop deals, merc job prices,
player-directed lines, and the 8v water/food scarcity lines (those are supply
*gossip*, already in the Gossip pool's register).

---

## Family — 16 lines

Warmth said at home — couple and household moments. The sim's bonds drive
*who*; these give the audible layer.

| # | FormID | Vo | Line |
|---|--------|----|------|
| 1 | 03459d | 3 | Running from something? Welcome home. |
| 2 | 047811 | 1 | It's a good life we've got here, and we're grateful for it. |
| 3 | 04ac25 | 1 | Hey, how's the family doing? |
| 4 | 06202a | 3 | Not a day goes by that I don't thank my lucky stars that I live here. |
| 5 | 106e3f | 1 | Yep, we're all still here, thank God. |
| 6 | 106ce4 | 1 | Yeah. It's gonna take my son a while to get off the chems, but we'll make it. We always do. |
| 7 | 133c08 | 1 | I just wanted to provide for my family. |
| 8 | 164456 | 1 | I'll do whatever it takes to earn back my family's trust. |
| 9 | 18f9d6 | 1 | Things have almost returned to normal. I'm grateful for that. |
| 10 | 1909c1 | 8 | This would feel a lot more like a home if everyone had their own bed. |
| 11 | 1909c6 | 8 | We just wanted to say how grateful we are for the opportunity. |
| 12 | 1909c9 | 8 | It's not everything we expected, but we're willing to work hard to make this a home we can be proud of. |
| 13 | 0a0076 | 1 | We knew the risks of having kids these days, and we took 'em. |
| 14 | 1ac7ed | 1 | Well, you're home now, and you should relax. |
| 15 | 1a02df | 1 | Good, when my husband actually gets around to doing it. That is, if no one raids the farm first. |
| 16 | 1909ab | 8 | We were looking for someplace to make a new life, so here we are. |

**Repurpose candidates** (flagged, not yet moved):
- **Family #3** "Hey, how's the family doing?" — doubles as a Greet variant.
- **16fc57** "He's your kid. Not mine. So you'd better get your shit together
  and come find us when you're done with this idiocy." — spouse argument,
  a strong Row line.

**Cut:** 89 — kidnapped family cries, named families (Abernathys, Warwick,
Jake, Austin, Emogene), chem-drama, player-directed thanks, religious chants,
and insults.

## Ghoul survey — 0.8.7

The ghoul banks (FemaleGhoul 1,416 files, MaleGhoul 1,442; ~1,495 unique
formids) were surveyed for a small dedicated ghoul pool — ghoul settlers
resolve to their own voice types (never the settler banks), so before this
pass they were caption-only. The finding: **the ghoul bank has no genuine
ghoul-exclusive generic register** — its own lines are nearly all
player-directed (chems, quests, the Slog's tarberry/Daisy/Wiseman business,
kidnapping cries) — but it **recorded the same 8v generic lines as the
settlers** (52 of the settler pool's 8v lines, same formids, verified in
the Voices BA2). So the ghoul pool is the shared lines, honestly: **64
curated lines now carry ghoul voice coverage** (bits 14–15 in the
coverage table), and a ghoul voice speaks them from the same table — "Looking
to buy?", "Why's it gotta be so quiet?", "Leave me alone!", "I'm truly
sorry.", etc. Guard-only ("No loitering.") and child-only lines stay
unreachable for ghouls. The 313-strong auto-classified ghoul catalog
(`Not for github/ghoul_catalog.md`) documents why the rest was cut
(quest/NPC/player-directed).

---

## Pool summary

| Pool | Lines | Voice-heavy | Notes |
|------|-------|-------------|-------|
| Greet | 33 | 8v ×5 | +3 child (2v/1v) |
| Gossip | 30 | 8v ×20, 7v ×2 | +7 child (2v/1v) |
| Row | 38 | 8v ×5 | +12 guard + 6 child |
| Trade | 35 | 8v ×6 | +5 child trader (1v) |
| Family | 19 | 8v ×4 | +3 child (2v/1v) |
| Grief | 15 | 8v ×6 | 2 gender-selective, revisit |
| Fight | 2 | 4v ×1, 3v ×1 | Guard threats; 1 settler gripe in Row |
| Feud | 2 | 6v ×1, 1v ×1 | Borrowed into Row (cold openers) |

**Grand total: 172 unique curated voiced lines** (Fight 2 and Feud 2 are
cross-listed in Row, not extra recordings). Sources: settler UNSORTED bucket
(2,087 lines → 43 keepers), guard bank (618 lines, 4 voices → 14: 12 Row +
2 Fight), child bank (192 lines, 2 voices → 24: 3 Greet + 7 Gossip + 6 Row +
5 Trade + 3 Family). Synth bank (214 lines, 4 Gen3 voices): **nothing usable**
— all Institute quest dialogue; synth settlers already speak via the settler
pool. Ghoul bank (~1,495 formids, 2 voices): no exclusive register — 64
curated lines shared with the settler pool carry ghoul coverage. ~2,760
total eliminated across all banks as quest/combat/pain/player/named/location
filler.

**Banks now fully surveyed:** settlers, guards, Gen3 synths, children,
ghouls. The voiced catalog is complete. Remaining dialogue work is the
**subtitle (caption) pools** — the sim's own memory-driven lines — and the
HybridArchitecture design (triggering these voiced lines via Say/PlayIdle vs.
captions).
