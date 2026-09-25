# PROPOSALS — round 2: creative gap-filling (D-018)

**Status:** authored, not designer-reviewed. Per D-018 the designer pre-authorised this whole batch ("i allow u to fill the gaps with ur own creativity as long as it fits the theme and canon"); it ships as `INVENTED` and is listed here and in [invented-ledger-design.md](invented-ledger-design.md) so any single item can still be vetoed (a veto supersedes only that entry, per DECISIONS.md's standing rule).

Every item below closes one of [game-design.md §11](../game-design.md)'s twenty open decisions, or an `OPEN` row that lived in a table I own (`db/canon/`). Nothing here touches a `CANON` row, invents a new mechanic outside the parent systems, or breaks D-004 (Ishtar's demonic nature stays ambiguous — see item 11). Every row is now tagged `INVENTED` or `A` with `source_ref` citing its canon anchor + `D-018`.

---

## §11 items, in order

### 1. The title
Still `OPEN` — a title is marketing, not lore; no notes-canon constrains it, and D-018's grant ("fits the theme and canon") doesn't force an answer where the notes are silent by design rather than by gap. Left for the designer.

### 2. The map
Confirmed at D-002/D-005 scale (one city + ~20 km² region); this round resolves what was still loose under that decision. `cities.csv` gets a `map_status` column: **City of the Moon is the only v1-map city** (D-005); the other seven sit off-map, known through people, caravans and rumour (README's own pattern) — growth is city-by-city if warranted, never all at once. **The player's homeland is never seen** — lore only (§11.5's own dossier explains why: exile is the point). *Canon: D-002/D-005; source: cities.csv `map_status` column.*

### 3–4. The past life & reincarnation vs. heirs
The implication stands confirmed, not merely flagged: **the player is the reincarnated Law Giver of the City of Jewels.** Mechanically it rides the imported heir system exactly as D-003 already locked it — **no rebirth mechanic is built.** The reveal is narrative, drip-fed: the player accumulates unexplained fragments (a name he half-remembers, a building he's never visited but can navigate) from Act II onward, culminating in a full recall available once the player reaches rank 4 (Gigir) or higher *and* has visited any City-of-Jewels-adjacent content (off-map rumour chain) — confirmed outright only in Ending 1's script, where accepting the identity IS the ending's action. This uses zero new systems: it's dialogue/quest content gated on existing rank and faction-standing values. *Canon: game-design §3; endings §3.1; D-003 (locked, not reopened). No DB row — this is doc-only content guidance for the quest-authoring storms.*

### 5. The homeland
**The Land of Kaldun; the people are the Kaldunai.** A marsh-and-lowland nation south of the two rivers, conquered by the Empire roughly two generations before the game's present. "Chaldean-like" is read as the notes intend — flavor, not identity (the notes say "like," never "is"): reed-built villages, herding and fishing beside grain, a plain Aramaic-adjacent tongue distinct from the setting's own Sumerian/Akkadian (this gives the player a real starting-language slot, per rpg-systems §2.5), and household ancestor-reverence in place of a lost state cult — which is why the player reads the City of the Dead's customs so readily. Never appears on the map (see item 2); exists through memory, a diaspora quarter in the City of the Moon, and a handful of Kaldunai names. *Tag INVENTED. Source: `world_lore.csv:homeland_kaldun`; `names.csv` (6 Kaldunai entries: naboraz, qashtan, marduka, seluhi, tabitu, narisha).*

### 6. Backgrounds
Confirmed as the single fixed prisoner start (D-009 already settled this in substance); the background *system* stays dormant machinery, unused unless the designer later wants more starts. No new content authored — nothing to invent where D-009 already ruled.

### 7. City identities
Six of the eight cities get real-Sumerian-site pairings; two don't, by design:
- City of Jewels = **Akkad** (the notes' own §11.7 candidate — the one real city whose location is itself lost, matching the fictional one)
- City of the Dead = Gravestone = **Kutha** (the real underworld/Nergal cult city — the closest real analogue to a city built on death)
- City of the Sky = **Uruk** (An/Anu's principal cult city; one of the oldest)
- City of Kings = **Nippur** (Enlil's real cult center and kingship's traditional legitimacy)
- City of the Sun = **Larsa** (a real Utu cult city, alongside Sippar; also one of the notes' own "five first cities")
- City of the Abyss = **Eridu** (the real oldest Sumerian city, Enki's cult center — matches "the oldest city in the lands" exactly)
- City of the Moon = **Ur** — this one was already effectively confirmed by the notes themselves ("a seer from the city of Ur," notes L19, paired with the Prophet's City of the Moon); this round elevates it from suggestion to `CANON`-by-delegation.
- City of the Warrior Spirit gets **no real-world pairing** — it's explicitly a *Taurissian* (the Empire's own people's) city, not Sumerian, so forcing a Sumerian site onto it would contradict its own canon.

*e-ab-kur-irkalla-ki* ("Gravestone") **is confirmed as the City of the Dead's native cultic name — one city, not two** (this also resolves the standing D-014 ambiguity in `deities.csv:underworld_queen` and `factions.csv:dead_cult`). *Tag INVENTED (site pairings) / CANON-by-delegation (Ur). Source: `cities.csv` (`real_site_correspondence` column), `deities.csv:underworld_queen`, `factions.csv:dead_cult`.*

### 8. The two ancient peoples
The origin myth (world-bible §3) already names them precisely — the southern boat-folk (founders of the first great city and the two-waters worship) and the northern settlers (who "lowered kingship"). **Confirmed: these ARE the two peoples of the opening line.** No invention needed; the notes already answer this, the ambiguity was only ever "is it these two or some other two," and no other pair appears anywhere in the notes. *Tag: confirmation only, no DB row touched (world-bible §3 stays CANON verbatim).*

### 9. The Empire as "Great King"
Confirmed: the Empire's Emperor holds Great King status by the one-oath rule (rpg-systems §4.2) — the Emperor is the polity every vassal-of-the-Empire's oath ultimately answers to. Treaties: the Empire and the Neo-Sumerian Rebellion hold no standing treaty (they're at war in all but name — the rebellion's entire premise is secession); the Empire and the Barbarians hold no treaty either (raiders, not diplomats) but individual imperial border-cities can strike local truces (mercenary-contract pattern, already carried). The Brotherhood, being hidden, signs nothing. *Doc-only ruling — `factions.csv:the_empire` is a CANON row I cannot touch; this stands as game-design.md guidance for the faction-content storms.*

### 10. Rank ladder labels
Already resolved at D-015 (round 1) — nothing left open here.

### 11. Cosmology: literal or ambiguous
**Explicitly NOT resolved by this round.** D-004 is a locked decision: the game must never authorially confirm the Brotherhood's "demonic" reading of Ishtar. D-018's creative license does not override a standing decision — it fills gaps, it doesn't reopen closed ones. What this round DOES resolve is the **sub-question left dangling in endings §3.2: the identity of "the empire of the bull."** Answer: **Gugalanna, the Bull of Heaven** — a real Mesopotamian mythic figure (Epic of Gilgamesh), consort of the underworld queen, an "ancient contemporary of Ishtar" in the truest sense (he predates her narrative even in the real corpus). His cult grows in the dead cult's own shadow (City of the Dead = Gravestone) and only takes the empty throne-room once Ishtar's cult is torn down in Ending 2. This is grounded, not asserted as true in-world — the game can present Gugalanna's rise as a rumour, an omen, a Brotherhood warning, without ever confirming it "really" happened, keeping D-004's ambiguity intact for Ishtar while still answering the notes' own unanswered noun phrase. *Tag A. Source: `deities.csv:gugalanna`.*

### 12. The hidden ending's silent sequence
Five steps, written directly into `endings.csv:ending_4_brotherhood`'s `unlock` field (see there for the full text). Summary:
1. Trigger the `serpent_rumours` event and follow it without reporting the stranger to any faction.
2. Reach standing 40+ with at least two of the three open factions (Empire / Rebellion / Barbarians) *without ever swearing an oath* to any of them — the Brotherhood tests neutrality, not loyalty (world-bible §2: "worthy enough to stand face to face").
3. Perform `barutu_haruspicy` or `zisurru_warding` at high purity at least once — proof of Sacred-group competence.
4. Never commit a death-verdict crime (`laws.csv`) — the Brotherhood does not recruit the guilty.
5. By the close of Act III, decline all three factions' Act-IV commitment offers.

No marker is ever shown for any step; missing one silently closes the path, per the parent's own silent-sequence template (parent endings §3.4) that endings.md §3.4 already names as the mechanic. *Tag INVENTED. Source: `endings.csv:ending_4_brotherhood`.*

### 13. Endings 1–3: unlock conditions and founding sites
All four `endings.csv` rows now carry concrete `unlock` values (standing-at-Oath-bound-by-Act-IV + a specific Act IV action each), following the parent's standing+Act-IV pattern the doc already named as the template. Founding sites for endings 1–3 stay loosely defined (Ending 1: wherever the Empire's throne already sits — City of Jewels/Akkad if reachable, otherwise a regency; Ending 3: no founding at all, a council seat instead, per the ending's own text) since the notes don't fix them and forcing false precision would invent scope the notes never asked for. Ending 4's site is already CANON (ruins of Ur). *Tag INVENTED. Source: `endings.csv`.*

### 14. The act arrangement
Confirmed as proposed — `story.csv`'s six rows (opening → act_i–iv → epilogue) were already authored as the working arrangement; this round formalizes them as the answer rather than a still-open proposal. *No DB change needed — `story.csv` rows already stood as CANON; `game-design.md` §11.14 now marks this closed.*

### 15. The clock: local outcomes only, or wider reach
**Local outcomes only.** The great reset's horizon is canon and fixed (world-bible §1, §4.1) — nothing in any of the four endings suggests the player stops the reset itself, only what rises after it in his own region. This is the conservative reading and the one every ending's own text already implies (each ending describes a *regional* outcome — an empire, a rebellion's valley, a warlord's council, a single new city — never a global one). *Doc-only ruling, no DB row to flip (the clock lives in kernel machinery, not canon data).*

### 16. Ilku/tribute
Confirmed: the Empire's tribute economy runs on the imported *ilku* land-grant mechanic (rpg-systems §5.2) exactly as proposed — the tribute basket items (`tribute_gems`, `tribute_fruits`, `tribute_white_liquid`, all already CANON from D-016) are what a vassal's ilku obligation pays in kind, alongside the Éren/Gigir rank tier's land-for-service basis already written into `ranks.csv` at D-015. *Doc-only ruling — `factions.csv:the_empire` is CANON, not touched; confirmed via `ranks.csv` (already existing) and `items.csv` tribute rows (already existing).*

### 17. The wider world
**Lore only for v1**, exactly matching the notes' own header distinction (world lore vs. game map, wb §9) and game-design §10's scope warning. It feeds the codex's world-map page and gives off-map trade/pilgrim content something real to point at (`sea_caravan_docks` already references Gulf trade). No playable off-map venture is built against it until the map itself grows past the City of the Moon. *Tag INVENTED. Source: `world_lore.csv:wider_world_scope`.*

### 18. Directions: scheme, plan, and the esoteric columns
**Scheme A in force for all four directions** (Fire=Mountains / Water=Ocean / Air=Forest / Earth=Plains) — it's the cleaner, non-parenthetical-question-marked option (Scheme B's own entries carry the designer's "(?)" hedge in the notes, reading as the less-committed draft). Per-direction environment plan:
- **West: Plan 1** (names the mound builders, a canon people — Plan 2 names no one)
- **North: Plan 1** (Plan 2's "most mountainous" claim would duplicate East's Plan 1, so Plan 1 is the non-redundant pick)
- **East: Plan 1** (matches the Barbarians' own "great range of mountains" origin, notes L21 — Plan 2's swamp-jungle would contradict it)
- **South: Plan 2** (already keyed by `factions.csv:suti`; carries the War Age's claw-mark line the endings and world-bible §1 lean on)

**Taurus D, Tree of Life and Pentagram stay lore, not mechanics** — per mechanics.md §2, no new system is invented without an explicit opt-in, and none of §11.18's phrasing constitutes one. They surface as ambient flavor only: ombre text in omens, subtle regional NPC dialect/superstition color, never a second elemental system players interact with. *Tag INVENTED. Source: `regions.csv` (`in_force_ruling` column).*

### 19. Deity data: medicine deities, Sun/Saturn, the garbled Enki line
**The 8 deities of medicine**, grounded rather than invented from nothing: Gula (chief healing goddess, her dog her emblem, oversees the City of the Moon's physicians), Ninisina (Lady of Isin, later merged with Gula), Ninkarrak (invoked on amulets and in incantations), Nintinugga (Nippur's own healing goddess, fitting Enlil's city), Damu (the dying-and-returning healing god), Ninazu (healing herbs, underworld ties — fits the dead cult's city), Baba/Bau (Lagash's healing goddess), Meme (a minor presence in Gula's circle). *Tag A. Source: `deities.csv` (8 new rows), `pantheons.csv:eight_deities_of_medicine`.*

**Sun and Saturn power-lists**, written in the same register as the notes' own Earth/Moon/Mercury/Venus lists: Sun (Utu) = kingship's legitimacy, oaths and their breaking exposed, the exposure of hidden things, vitality and healing, the paladin's righteous force. Saturn (Ninurta, "the classic candidate" the blueprint already named) = the weight of time and old age, boundaries and thresholds, endurance through hardship (the drought's own planet), the plough imposing structure on wild land, binding curses, and the price every ending in this story exacts. *Tag A. Source: `planetary_powers.csv:sun`, `planetary_powers.csv:saturn`.*

**The garbled Enki-pantheon line** (notes L93-94 repeats "Enki's Pantheon / Enki" after the list is already complete) is read as a transcription artifact, not content — `pantheons.csv:enkis_pantheon` already flags this in its source_ref; no new content invented for a line that's plainly a copy-paste duplication, not a second pantheon.

**Domains, cult sites, festivals and taboos** for the wider pantheon lists stay unauthored past what's already in `deities.csv` — writing full liturgical detail for every listed god is downstream content-storm work (per plan.md's table-storm pattern), not a §11 gap-fill; this round only closes the two explicitly-OPEN sub-items (medicine deities, Sun/Saturn).

### 20. NPC voices
Left `OPEN` — this is a budget/scope decision (subtitles-only vs. voiced main NPCs), not a lore gap; D-018's "fits the theme and canon" grant doesn't extend to production-budget calls that belong to the designer alone.

---

## Two supporting rulings not tied to a single numbered §11 item

**Callings (§11.10 sub-item) and Companions (§11.20):** both stay content-only guidance here, since `callings.csv` and `people.csv` are owned by other agents (not touched). Suggested calling names, offered as non-binding direction for whichever agent authors `callings.csv`: *Exorcist* (Incantation-focused, fits the demonology/medicine content directly), *Diviner* (the Sacred group's divination skill, Barûtu/aeromancy content already written), *the Old Woman* (herb-lore, midwifery — pairs naturally with the medicine-deity circle above), *Lector-Priest* (hymnody, temple liturgy — the Hymns rite already exists). Companion archetype suggestions for `people.csv`'s eventual roster: an Empire deserter (ties to Ending 1's price), a Kaldunai kinsman-in-exile (ties to §11.5's homeland), and a Brotherhood-adjacent scholar (ties to the hidden ending's sequence, item 12 above) — none authored as rows, since `people.csv` is off-limits to this agent.

**The undeciphered script** (mechanics.md row 25's open sub-item): the Brotherhood's own Ante-Diluvian script fits perfectly — the notes' own line that "these tales shall be forgotten... one order has made it their duty to preserve this old knowledge" (notes L24) already implies a script only they still read. Doc-only ruling; no languages/scripts table exists yet to hold it.

---

## What stays open on purpose

- **The title** (item 1) — marketing, not canon.
- **NPC voices** (item 20) — a budget call.
- **Cosmology's literal-vs-ambiguous core question** (item 11) — locked by D-004; only its "empire of the bull" sub-question is answered here.
- **Full per-god domains/cult-sites/festivals/taboos** beyond what's already seeded — downstream content-storm scope, not a gap D-018 was meant to plug in one pass.

See [invented-ledger-design.md](invented-ledger-design.md) for the flat, one-line-per-choice veto list.
