# MECHANICS — The Adoption Audit

**Status:** Blueprint v1 (2026-09-24) · The game is **100% the notes' content** ([world-bible.md](world-bible.md)). The parent blueprint set ([../docs/](../../docs/)) contributes **systems and nothing else** — no story, setting, tone, characters, map content or names cross over. The binding rule is **existing mechanics only**: every mechanic is imported from the parent set unchanged. This document is the audit: what carries, what gets reskinned with notes-content, what is open, and what the notes ask for that has **no** imported mechanic.
**Related:** [game-design.md](game-design.md) · [world-bible.md](world-bible.md) · [endings.md](endings.md)

**Status column:** **Unchanged** — the mechanic carries as-is · **Reskin** — the mechanic is unchanged, its content is replaced from canon ([world-bible.md](world-bible.md)) · **Open** — a designer decision gates it ([game-design.md §11](game-design.md)).

---

## 1. The audit

| # | System | Carried from | Status |
|---|---|---|---|
| 1 | **Engine: Unreal Engine 5** | architecture §2 | Unchanged (parent decision carries unless the designer reopens it) |
| 2 | **Repo & content database; the accuracy pipeline** | architecture §3 | Reskin — tag scheme becomes `[A]` / `[CANON]` / `OPEN` (game-design §9); the pipeline enforces canon lint |
| 3 | **WorldState daily tick** (prices, harvests, raids, disease, politics, the historical clock) | architecture §4.2 | Unchanged — the clock's horizon becomes the Empire's fall and the great reset |
| 4 | **Simulation layers L0–L2** (full / scheduled / statistical) | living-world §1 | Unchanged |
| 5 | **Population scale** (~600 named NPCs, ~2,500 light, crowds at density) | living-world §1 | Unchanged numbers; tuned to the map decision (`Open` §11.2) |
| 6 | **The NPC model** (identity, household, livelihood, rank, faction, faith, needs, goals, memory, relationships) | living-world §2 | Unchanged; name stocks Reskin (`Open` §11.5, §11.7) |
| 7 | **Schedules & the daily clock** (tasks, not positions; seasonal bend; festival overrides) | living-world §2; city-life §1 | Unchanged; festival calendar content `INVENTED` (D-018; `festivals.csv`), festival overrides + per-person schedules live in the kernel (K-2, module_Schedule.md) |
| 8 | **Supply-driven economy & shops** (every item from a producer; finite stocks; imports with ships and caravans; prices from supply, standing, skill, language) | living-world §3 | Unchanged; catalogue content Reskin from canon (ritual scrolls, demon goods, lighthouse trade, sea-gem offerings) |
| 9 | **Places you can use** (houses, workshops, fields, temples, palace, school, port, tombs, wells, caves, ruins — doors open, owners, things to do) | living-world §4 | Unchanged; contents per the eight cities (underground temple, catacombs, the Garden of the Gods, the Great Lighthouse) |
| 10 | **Followers** (3 companions with personal quests; hirelings; band of 5–30; household; animals) | living-world §5 | Unchanged; roster content `Open` (§11.20) |
| 11 | **Minor factions & the raid formula** (hunger + opportunity − defence − fear → raid chance) | living-world §6 | Unchanged — populated by canon: eastern tribes, the southern alliance, cults, the Retributors of Utu, drought raiders. **Live (W4-C):** five bands (`wild_groups.csv`: outlaw bandits, the eastern warband, the Suti, the rebellion's partisans) with camps that form, raid, move, feud and re-form while the drought or war persists; the formula runs daily in the kernel (kernel/contracts/module_Wild.md); content `INVENTED` (D-018/D-021, [proposals/invented-ledger-wild.md](proposals/invented-ledger-wild.md)) |
| 12 | **Crime & justice pipeline** (perceive → judge → feel → respond → remember; guards, pursuit, detention, hearing, sealed verdict tablets; detention not prisons; outlawry → rank 0) | city-life §2–3 | Unchanged; the law table's content `Open` (§11.15; research pointer: the Mesopotamian law codes) |
| 13 | **Customs** (greeting, hospitality, purity, weapons, dress, food, spaces, the dead, oaths, mourning, gifts) | city-life §4 | Unchanged mechanics; per-community content Reskin/`Open` |
| 14 | **Festivals & the ritual year** (processions, crowd AI, roles for the player, favour gains) | city-life §5 | Unchanged mechanics; calendar content `INVENTED` (D-018; `festivals.csv` — gathering, market, festival-day rite time power live via K-2) |
| 15 | **Death, burial & the dead** (bodies found, mourning, tombs, offerings, the restless dead; Mythic vs Chronicle hauntings) | city-life §6 | Unchanged — meets canon head-on: the City of the Dead, the underworld queen, ancestor catacombs |
| 16 | **Random events** (trigger-driven; participants, chains, persistence) | city-life §7 | Unchanged; event content Reskin (omens: astrology and aeromancy feed the omen category) |
| 17 | **Quests** (8 sources: history arc, background, companions, sourced side quests, systemic, faction contracts, emergent; no markers by default, guided mode optional; quests fail and time out) | living-world §8 | Unchanged |
| 18 | **Heir system & time skips** (choose an heir; property, debts, oaths, rank, reputation pass; skills don't; acts live 1–2 seasons, years skipped between) | living-world §9–10 | `Open` §11.4 — the notes' single protagonist and past-life device vs generational heirs; no reincarnation mechanic exists, none is invented |
| 19 | **Four acts on the historical clock** | game-design §7 (parent) | Unchanged machine; the canon beats arranged on it in game-design §6 (arrangement `Open` §11.14) |
| 20 | **Attributes** (6, range 1–10) & **conditions** (hunger, thirst, fatigue, heat/cold, zonal wounds, illness, **purity**, intoxication) | rpg-systems §1 | Unchanged — purity is load-bearing in this setting. **Attributes built (W4-A):** `attributes.csv` (INVENTED names), growth by exercise; kernel `sim/Character.hpp` (contract `kernel/contracts/module_Character.md`) |
| 21 | **Skills** (6 groups, 0–100, by use/teachers/texts) incl. the Sacred group (Rites per tradition, Divination, Incantation) | rpg-systems §2.1 | Unchanged — the notes' magick forms land here (game-design §8.2). **Built (W4-A):** `skills.csv` (27, INVENTED); growth by use (craft, rites, trade, eating, reading, work), teachers and texts (`skill_teachings.csv`), work for wages (`work_roles.csv`); no decay (the spec names none) |
| 22 | **Levels** (XP from skill points, cap 40, 1 talent/level) | rpg-systems §2.2 | Unchanged. **Built (W4-A):** `talents.csv` (41, INVENTED, each with a mechanical effect) |
| 23 | **Callings** (choose at 5, specialisation at 15, second calling at 25, +50% growth) | rpg-systems §2.3 | Unchanged mechanics; the calling list Reskin/`Open` §11.10 — Exorcist and Diviner specialisations fit canon directly; the Old Woman and Lector priest slots need this setting's answer; the Retributors of Utu: faction only, or a calling? **Built (W4-A):** `callings.csv`: 8 callings, 18 specialisations with perks (INVENTED; the Old Woman and Lector-priest slots filled; the Retributors stay a faction) |
| 24 | **Social rank** (7 tiers, 0 outsider → 6 king; per-polity; rises by deeds + a patron's act; ranks die with polities) | rpg-systems §2.4 | Unchanged structure; tier labels canon (D-015). **Built (W4-A):** rank per polity, raised by standing + a patron, dropped to 0 by outlawry |
| 25 | **Languages & scripts** (speaking unlocks dialogue and prices; reading unlocks texts; one undeciphered-script slot) | rpg-systems §2.5 | Unchanged; content Reskin (Sumerian/Akkadian cuneiform family); which script is undeciphered `Open`. Literacy built (W4-A: scribal arts 10 to read a text); speaking languages not yet |
| 26 | **Factions & standing** (0–100 per faction; Stranger → Oath-bound; sworn treaty oaths with the oath-breaker curse; **one Great King at a time**; mercenary contracts from anyone) | rpg-systems §4.2 | Unchanged; the Great King's identity and inter-power treaties `Open` §11.9 |
| 27 | **Property & business** (houses, fields, orchards, herds, workshops, ship shares, caravan partnerships, loans at traditional rates; deeds as tablets; steward reports) | rpg-systems §5 | Unchanged — the notes' "do business" |
| 28 | **Food & eating** (hunger/thirst, diet balance, cooking stations, spoilage & preservation, rations as wages, feasting, offerings, famine) | rpg-systems §6 | Unchanged; recipes Reskin from canon (wheat, barley, fish, dates…) |
| 29 | **Weapons** (damage type, reach, speed, weight, balance, durability, quality; material tiers flint → copper → arsenical → tin bronze; iron as a prestige curiosity; recasting) | rpg-systems §7 | Unchanged — same technological world as the parent. **Built (W4-B):** `db/canon/arms.csv` + kernel `sim/Combat.hpp` (integer resolver, wear, repair and recasting at the smith; [module_Combat.md](../kernel/contracts/module_Combat.md)) |
| 30 | **Armour** (4 zones, 2 layers, cultural shields and helmets, weight/stamina/heat trade-offs, armour as identity) | rpg-systems §8 | Unchanged; culture variants Reskin. **Built (W4-B):** 4 zones × 2 layers, cultural shields (`combat_styles.csv` by faction), weight → stamina, heat → fatigue; with row 20's zonal wounds (bleeding, healing over days, scars, lasting limps) |
| 31 | **Transport** (foot, donkey, ox-cart, ridden horse, chariot, boats, ships; animal care) | rpg-systems §9 | Unchanged — fits canon: sea trade at the City of the Moon, the migration by boat, chariot-age warfare. **Live (W4-C):** `transport_modes.csv` (foot, donkey, ox-cart, horse, chariot, reed boat) with integer speeds; caravans run scheduled routes (`caravans.csv`); animal care `Open` |
| 32 | **The magic of the gods** (no mana; the five powers; the eight effect families; the deity network with per-god favour, domains, cult sites, festivals, taboos; equated-god favour carryover; neglected gods; patron gods; guard rails; Mythic/Chronicle modes) | rpg-systems §0, §10 | Unchanged; deities.csv seeded from the canon pantheon lists; per-god data and the equations question `Open` §11.11, §11.19 |
| 33 | **Diegetic UI** (scales you read, the map you draw, the codex that fills only with what you learned, unreadable scripts shown real) | game-design §8 (parent) | Unchanged |
| 34 | **Voice** (the player optionally voiced — a setting switches it on or off; NPCs subtitle-first) | game-design §13.5 (parent); endings §5 | Unchanged — and it is the notes' own first line |
| 35 | **Immersion rules** (no fast travel by default, real darkness, sounds and smells as signals, touch almost everything, the world gets on without you) | city-life §8 | Unchanged **W4-C:** travel costs game-minutes by mode, terrain and weather; the kernel never moves the player without them (`sim_world_travel`). |
| 36 | **One map + off-map ventures + journeys** | world-map §1, §4 | Unchanged pattern; which cities on the map and at what scale is the project's biggest `Open` §11.2 **W4-C:** the region beyond the walls is a travel graph (`wild_places.csv`/`wild_links.csv`, ~19 km² per D-002), and three journeys lead off-map to the Cities of the Abyss, the Sun and the Warrior Spirit. |
| 37 | **Accessibility & difficulty** (needs severity, permadeath on Historical, disease toggles, remap) | game-design §8 (parent); rpg-systems §12 | Unchanged; permadeath ties to the heir decision §11.4 |
| 38 | **Roadmap pattern** (blueprint → vertical slice → shippable; solo-build rules) | roadmap.md | Drafted — [plan.md](plan.md) (2026-09-24): the solo-agent build plan; execution waits on the Phase 0 decision gate |

## 2. Gaps — the notes' requests with **no existing mechanic**

Canon lore until the designer opts in ([game-design.md §8.3](game-design.md)); nothing here is designed preemptively:

1. Sex magick (Inanna).
2. Numerology.
3. Kabbalah / hermeticism — "as above, so below".
4. The Moon-list powers: mind control ("bad karma" — the notes' own warning), astral travel, telepathy, animal telepathy, psychometry.
5. Pathworking; planetary rite-crafting on the four power-lists (Earth, Moon, Mercury, Venus — Venus completed at the designer's request, 2026-09-24; the notes wrote no Sun or Saturn lists).
6. The Tree-of-Life / Pentagram / "Taurus D" data as a working system rather than lore.
7. A paladin dimension for the Retributors of Utu beyond ordinary faction mechanics, if wanted.

## 3. What is *not* carried

**Nothing narrative crosses over.** The parent's story, setting, factions, characters, map, gods, laws and festivals stay in the parent project: the Ugarit map and its five sites, the five great powers and their dossiers, the five backgrounds' content, the Keepers of the Tablets lore, the KTU festival calendar, the Hittite/Assyrian law-table content, the Year 8 set piece, the specific companion rosters, and the real-history bibliography (kept only where this project's notes import `[A]` material). Only the systems in §1 are imported, and every content slot they have is filled from the notes — or held `OPEN` where the notes are silent.

## 4. The one-paragraph version

This game is the parent engine with a different world poured into it: same body (attributes, skills, callings, rank), same society (factions, oaths, property, law, custom), same living world (NPCs, shops, followers, events, heirs), same magic (favour, knowledge, materials, purity, place, time), same collapse on a clock (four acts, time skips, an ending per faction, one hidden) — and a different god, a different empire, and a player who arrives in chains and leaves as a king. Every difference from the parent is content; every system is the parent's. Where the notes outrun the parent's systems, the answer is a decision, not a design.
