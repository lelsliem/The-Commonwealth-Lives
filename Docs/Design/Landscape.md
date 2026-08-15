# The Landscape — what the Nexus already does (2026-08-11)

A survey of the Fallout 4 Nexus, made before we build 0.7.3+: check
what already exists so we never reinvent it, borrow what fits, and
know exactly what we own. Nexus pages block direct fetches, so the
sources are the SS2 wiki, search snippets, and the mod pages read
indirectly — good enough for a map, not a license review.

## The headline: nobody does what we do

The core of The Living Commonwealth — **settler minds**: needs drive
decisions, decisions drive walks, walks land at markets, trades move
caps, meals warm relationships, bonds form and feud, births and
deaths are remembered and grieved — **has no competitor.** Every mod
in the relationship/family space is *player-facing* (player romances
companions, player marries, player-affinity). **No mod makes
settler-to-settler friendships, sweethearts, marriages, rivalries,
feuds, grief, or legacy a living system.** That space is empty; we
own it. The name we built (sticky, gendered, species-aware,
co-saved, world-persistent) also outclasses the cosmetic renamers.

## The map

| Mod (Nexus id) | What it does | Verdict |
|---|---|---|
| **Sim Settlements / SS2** (21872) | Settlers are *plot workers*: S.P.E.C.I.A.L. stats feed building/economy plots; unique settlers recruited via quests; a city-building progression sim | **Complementary — coexist, never compete.** It's the *building* sim; we're the *mind* sim. Its settlers are labor; ours are people. No overlap in machinery (its state is game/quest-driven, ours is the co-save). |
| **Settler Sandbox Expansion** (20442) | Non-scripted AI: settlers re-equip from placed containers, expanded sandbox radius, use furniture | **Borrow the idea.** This is the "idle life" polish for what settlers do between sim commands. Test alongside ours; its container/furniture behavior could inform our Rest/Explore wander (0.7.x) without us writing anything. |
| **Barter – Vendor & Economy Overhaul** (72654) | Reworks what vendors buy/sell and their cap pools — a harder trade economy | **Borrow for 0.8.0.** Our Trade-with-anyone buys from real vendors; Barter changes their stock and caps, so our sim's purchases land in a deeper economy. Game-side state; our cap pouches are co-save state — no conflict. |
| **NPCs Travel** | 130+ random encounters on the roads — wandering settlers, scavengers, robots | **Ambience for 0.8.0's roads.** Its actors are game NPCs, not sim minds (consistent with our vendor-census rule). Road life our provisioners can share the world with. |
| **Settler & Companion Dialogue Overhaul** (41785) | 2,300+ new voiced lines for settlers, companions, enemies | **The audio path, later.** Our speech is captions now (post-0.8.6c audio is the plan); if we ever want *spoken* words, voiced settler lines exist to trigger. CK/asset territory — stays deferred. |
| **Baby Sim** (100934) | Visible babies/children, aging, grown children become settlers | **Permission pending** (message sent 2026-08-11) — the visible-child path (ReleasePlan). |
| **We Have Names** (74287) | Permanent names for ~300 fixed NPCs | **Complementary.** Names the traders our vendor census will find; our runtime naming stays the tool for random settlers. |
| **Crime and Punishment** (58429) | Karma/crime/consequences systems overhaul | **Skip** (assessed — a heavyweight gameplay overhaul, not our sim). |
| **Player Marriage / Make Anyone Romantic** (21932 / 75413) | Player-facing romance/marriage | **Not ours.** Player-focused; our marriages are settler-to-settler. |
| **Better Settlers** (4772), **Real Name Settlers** (44978), **Simple Settler Names** (100929), **eXofied Settlers** (63276), **Don't Call Me Settler**, **What's Your Name** | Cosmetic settler renames/looks/levels | **The crowded space we win.** They're cosmetic record edits that conflict with each other (Real Name is incompatible with Better Settlers). Ours are runtime display-names that are sticky, co-saved, species-aware. One caveat: our "read the game name first" rule means a renamer's names flow into our sim's display — fine, even good. |
| **Old World Radio** (9048), **Galaxy News Radio** (16339) | Music/DJ radio stations | **Ambience.** Our radio is a caption channel for sim news; content stations coexist. |
| **Companions also need to eat and sleep** (55908), **Survive the Wasteland** (10997), **Simple Survival** | Player/companion needs | **Proof of demand** for needs-driven life, but player-facing. We're the only one giving *settlers* needs. |

## Release-page companions — the final list (2026-08-11)

Recommendations for the release page (all suggestions — link + credit,
no permission needed; the player downloads from Nexus, nothing is
bundled):

- **Sim Settlements / SS2** (21872) — the building sim; we're the mind
  sim. Coexist, never compete.
- **Settler & Companion Dialogue Overhaul** (41785) — 2,300 voiced
  settler lines; the future audio path for speech. (Author has used it;
  sits in the mods folder.)
- **Barter – Vendor & Economy Overhaul** (72654) — depth for 0.8.0
  vendor trades. **Its listed Skyrim requirement (MXPF, Skyrim
  68617) is a patcher tool, not a game dependency** — install the
  Main File ESP, ignore MXPF unless re-running the patcher yourself.
- **Settler Sandbox Expansion** (20442) — idle sandbox life. Workshop
  Framework (35004, Kinggath) is the standard base if listed — have it
  anyway, SS2 needs it too.
- **NPCs Travel** (16987) — road life for the 0.8.0 provisioners.
- **Pet And Talk To Cats** (98576) — placeable cats, pet/talk; ambience
  for our cat species (owned → named, fed at market). Player-facing;
  no sim impact.
- **We Have Names** (74287) — NOT listed. We name better (runtime,
  sticky, co-saved, species-aware); we do it ourselves.

## What this means for the plan

1. **Keep the core ours, and say so.** The release story writes itself:
   "settlers live: they get hungry, walk to market, trade caps, fall in
   love, feud, grieve, and are remembered." No other mod claims that.
2. **Don't chase the crowds.** No building (SS2 owns it — the "hands"
   pillar stays cut), no cosmetic renaming, no player-survival.
3. **Borrow, don't build, the polish.** Sandbox idle-life (SSE),
   vendor economy depth (Barter), road life (NPCs Travel) are already
   done — note them as **optional companion mods** in the release docs
   and let their ideas inform ours (0.8.0's vendor census, the Rest/
   Explore wander).
4. **Compatibility notes for the release page:** our runtime naming
   coexists with renamers (their base-record names flow through our
   "game name first" rule); we touch no building/plot records (SS2
   safe); our cap pouches are co-save state (Barter safe). The one
   real dependency we're *pursuing* is Baby Sim, permission pending.
5. **The empty-space list — future territory nobody holds:** settler↔
   settler dialogue scenes (Talk/Rows are first), settler-to-settler
   romance that *means* something (marriage = shared pouch, bench,
   bed — already built), life stages (visible children via Baby Sim),
   a settlement that remembers its dead (grief, legacy — built), and
   the player's window into it (radio/news — built).
