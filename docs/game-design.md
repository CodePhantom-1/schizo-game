# GAME DESIGN — *schizo game* (untitled)

**Status:** Blueprint v1 (2026-09-24) · Built from the designer's concept notes ([world-bible.md](world-bible.md)) on the parent project's mechanics, adopted wholesale ([mechanics.md](mechanics.md)).
**Genre & tech:** unchanged from the parent — first-person 3D open-world survival / settlement / action RPG, UE5 (architecture §2).
**Related:** [world-bible.md](world-bible.md) (canon) · [endings.md](endings.md) · [mechanics.md](mechanics.md) (the audit of what carries over) · [../docs/game-design.md](../../docs/game-design.md) (the mechanics source)

---

## 1. Pitch

For almost 180 years the great Empire — raised by the Law Giver of the lost city of jewels — has stretched across the lands of the Taurissian people, taking tribute and gold for its patron deity, the lady of the skies. Now the hold of its dynasty is about to break: a drought is breaking the world, foreigners from the east raid and migrate into the Empire, and the south rises to tear it up in the name of patriotism and liberty. The Empire stands on its last pillars before the great reset of the world.

**You are a prisoner, taken from your homeland into this strange land ruled by two ancient peoples** — an immigrant from a conquered nation in an archaic foreign land. You shall decide the fate of these people and lay the foundations for new empires to come long after you, in a dying world.

Among the many groups fighting for their own piece of land you will come into contact with **a mysterious cult** — hidden, ancient, describing the Empire's deity as a demonic entity — and **the rebels of the south**, who wish to remove that deity and rebirth their own culture. You can make **your own city-state, do business, and forge alliances**. At the end, the Empire and the worship of its deity lie severely weakened by your hand — and the end of the story is a cliffhanger: the player going out to **become a king by establishing his own traditional kingdom**.

*(Everything above is canon — world-bible §3–6; the opening is the notes' own: "You are a prisoner taken from your homeland into a strange land ruled by two ancient peoples within this archaic foreign land.")*

## 2. The rules of this blueprint

1. **The notes are the game.** Everything here — world, story, factions, cities, pantheon, magick, endings, tone — comes from the designer's notes ([world-bible.md](world-bible.md)). From the earlier Bronze Age project this game takes **only the mechanical systems**, imported unchanged and audited in [mechanics.md](mechanics.md). **No story, setting, tone, character or content is imported from it.**
2. **Lore only from the notes.** World-bible is the canon; the notes are incomplete in places and say so.
3. **Gaps are decisions, not defaults.** §11 collects every open question. Nothing is filled in by anyone but the designer.

## 3. The player character

- **Backstory `[CANON]`:** an immigrant from a **Chaldean-like nation or state, conquered by the great Empire long ago**; he enters the game **as a prisoner taken from his homeland** into the strange land of two ancient peoples. The name of his homeland is `OPEN` (§11.5).
- **The outsider start fits the parent mechanic exactly:** social rank 0, the stateless outsider outside any city's protection (rpg-systems §2.4). The rank ladder is the game.
- **The past life `[CANON]`:** the City of Jewels is *"home of the player's previous incarnation"* (world-bible §5), and the Empire ending has the player *"accept their past life as their new identity and become the emperor of the empire they founded and supplant their descendant"* ([endings.md §3.1](endings.md)). **Read together, the notes imply the previous incarnation founded the Empire — that the player is the reincarnation of the Law Giver of the lost City of Jewels.** This is flagged, not decided: §11.3. How a past life works mechanically is §11.4 — no reincarnation mechanic exists in the parent systems, and none is invented here.
- **Voice `[CANON]` = parent decision:** the player is **optionally voiced** — a setting switches the player's voice on or off (the notes' first line; parent game-design §13.5, endings §5).
- **Backgrounds `OPEN` (§11.6):** the notes give one fixed backstory — that is the start. The background system is imported machinery; if the designer ever wants more than the notes' single start, the content of any additional start must come from the designer.

## 4. The four activity loops (imported systems)

The four loops are imported machinery ([mechanics.md](mechanics.md) §4); the notes say what the player does with them:

| Loop | The canon it serves |
|---|---|
| **Survive & craft** | the prisoner's start; the drought that is breaking the world |
| **Fight & conquer** | the raiders of the east; the rebellion of the south; a warrior people; the Retributors of Utu |
| **Explore & discover** | the strange land of two ancient peoples; the lost city of jewels; the mysterious land in the eastern mountains |
| **Build & settle** | *"the player can make his own city state"*; founding the traditional kingdom |

## 5. Factions

### 5.1 The four powers (canon: world-bible §4)

| Faction | What it is | The mechanic it sits on |
|---|---|---|
| **The Empire** | 180 years old; the Law Giver's dynasty; the lady of the skies' tribute empire; breaking under drought, eastern raiders and southern rebellion | a joinable faction with standing, rank and grants (rpg-systems §4); **the "Great King" of the one-oath rule** — `OPEN` §11.9 |
| **The Neo-Sumerian Rebellion** | the seer from Ur uniting the southern city states: push back the barbarians, fix the drought, rebirth Sumerian culture | joinable faction; the rebellion's city-state alliance populates the minor-faction web |
| **The Barbarians** | the eastern warrior people: tribal, scattered, bound loosely by the drive west | minor-faction system (living-world §6) — tribes with territory, needs, leaders; the raid formula drives them |
| **The Brotherhood of the Serpent** | the Ante-Diluvian priestly order, hidden, working in the background; met only by the worthy | the parent's hidden-faction pattern (parent endings §3.4): no standing meter shown, access by a **silent sequence of deeds** — the specific sequence is `OPEN` §11.12 |

### 5.2 The local web

The notes' *"many groups fighting for their own piece of land"* is the parent minor-faction system (living-world §6) populated from canon: the eastern tribes and their alliance (City of the Warrior Spirit), the southern rebel city-states, the **cults of the cities** (the dead cult, the cult of the Sky Father, the temple of sun and moon…), the **Retributors of Utu** paladin guild, bandits and raiders of the drought. Standing, raid logic, hire/join/destroy — all carried unchanged.

### 5.3 Alliances and business

*"Do business and forge alliances"* is the parent property system (rpg-systems §5: houses, fields, herds, workshops, ship shares, caravan partnerships, loans at the traditional rates) plus the faction system (standing tiers, sworn oaths, oath-curses). The Empire's **tribute** economy maps naturally onto the parent's *ilku* land-service mechanic — `OPEN` §11.16.

## 6. The story (canon) on the imported pacing machinery

The notes give the story. The pacing machinery — acts played live, time skips between them, the collapse running on schedule regardless of the player — is an imported system (living-world §10). The beats below are the notes' own; **the four-act arrangement is a proposal for the designer to confirm** (`OPEN` §11.14):

| Act | The horizon (canon) | The player's beat (canon) |
|---|---|---|
| **I** | the drought that is breaking the world; the eastern raids and migrations begin | the prisoner: taken from his homeland into the strange land of two ancient peoples; first contact among the many groups |
| **II** | the eastern migration swells; the south organises under the sage of the swamps | the web: business, alliances, contact with the mysterious cult and the rebels |
| **III** | the breaking: raids and rebellion; the Empire on its last pillars | the weakening: the player's work against the Empire and the deity's worship bears fruit |
| **IV** | the great reset at the door; the dynasty's hold breaking | the endings: side with a faction; found the kingdom ([endings.md](endings.md)); the cliffhanger |

**The scheduled horizon:** the clock is canon — *"the empire lies on its last pillars before the great reset of the world wipes their land from the face of the earth"* (world-bible §4.1) — and every ending the notes wrote has the Empire already broken or breaking. Whether the player can alter local outcomes only is `OPEN` §11.15; no note suggests the clock itself can be stopped.

**Time skips, heirs, and the past life:** the parent heir system (living-world §9) exists as a mechanic, but the notes tell a single protagonist's arc to kingship — and give him a *previous incarnation*. Whether this game uses generational heirs, a single life, or something built on the past-life device is a design decision the parent systems cannot answer: §11.4. Nothing is built until the designer answers.

## 7. What the world simulates

Unchanged from the parent, reskinned to canon. The full audit is [mechanics.md](mechanics.md); the headline:

- **The living world:** NPC lives, memory and rumour at walking speed, supply-driven shops, ~600 named NPCs on the map's scale, followers (3 companions, band of 30), crime and law, quests that fail and time out (living-world; city-life).
- **The cities:** the eight of world-bible §5 give the places-to-use their content — the underground temple and skull streets of the City of the Dead, the catacombs of the City of Kings, the Great Lighthouse, the Garden of the Gods — on the parent's city systems: the daily clock, the reaction pipeline, customs, festivals, death and burial, the event catalogue.
- **The map:** the parent pattern is one map + off-map ventures (world-map §1, §4); the notes themselves separate *"the whole world"* (§9 of the bible) from *"the game map"*. Which cities are on the map and at what scale is the biggest open decision in the project: §11.2.

### 7.1 Canon made playable

Every canon element below becomes content through an imported system — nothing new is built for any of them:

| Canon | Becomes, in the imported systems |
|---|---|
| Skull-filled streets; dog and owl engravings on houses (City of the Dead) | city architecture and art; a warding custom (city-life §4); the reaction system's backdrop |
| Ritual scrolls for speaking with underworld spirits (City of the Dead) | documents with real power (living-world §3.3); texts to read — Scripts and Rites knowledge (rpg-systems §2.1, §2.5) |
| The greatest temple built within the earth; the underworld queen's statue (City of the Dead) | places-to-use (living-world §4); interior world content |
| Mummified kings guarded; the royal catacombs (City of Kings) | the burial system's royal variant (city-life §6); tombs as places; ancestor rites |
| The Garden of the Gods — "every aspect of creation present" (City of Kings) | the largest temple place; festival site; deity-network content (rpg-systems §10.4) |
| The sacred hill of the Sky Father (City of the Sky) | cult-centre place; a pilgrimage venture (the world-map §4 pattern) |
| The temple of sun and moon; the Great Lighthouse (City of the Moon) | places; the sea-trade economy (ship shares, imports — rpg-systems §5.1); scholarship (the scribal-school slot) |
| The Retributors of Utu (City of the Sun) | a joinable minor faction: standing, contracts (living-world §6) |
| Sacrifices of fish and sea gems to the two waters (origin myth) | rite materials and trade goods (rpg-systems §10.1; living-world §3) |
| Mound builders (West); ziggurats imitating the mountains (North) | regional architecture kits per direction (world-bible §7) |
| The Suti of the South (world-bible §7) | a minor faction of the desert (living-world §6) |
| The War Age (world-bible §1) | the cosmological frame of every ending (endings §3) |

## 8. Magic in this world

### 8.1 The system, carried unchanged

The parent magic system (rpg-systems §10) needs no changes: **no mana** — a rite succeeds through the **five powers** (favour with the specific god · knowledge of the rite · materials · purity and place · time); effects come in the eight families (protection; healing and purification; divination; blessing; curse and binding; substitution; ancestors; divine intervention); the guard rails hold (no combat spells, omens never certainties, **magic cannot change the historical clock**). Mythic is the default mode; Chronicle mode stays optional (rpg-systems §0).

The system is unusually at home here: **purity** (rpg-systems §1.2) is load-bearing in a land of exorcisms and a cult of the dead; **divine intervention** is exactly the shape of the Prophet's final act ([endings.md §3.4](endings.md)); the **eṭemmu**-style restless dead and ancestor rites (city-life §6) meet the City of the Dead head-on.

### 8.2 The notes' forms mapped onto the existing system

| Note-form (world-bible §8) | Existing mechanic it lands on |
|---|---|
| Haruspicy / Barûtu; astragalomancy; pessomancy; aeromancy and its five sub-types; astrology (12 zodiacs, primitive) | the **Divination** skill and the divination family: omens with probabilities, never certainties (rpg-systems §10.3) |
| Exorcisms / zisurru; demonology (evil, neutral, benevolent demons); medicine as exorcism | the **Incantation** skill, the exorcist tradition-slot, healing/purification and protection families; the parent's physician-vs-exorcist split (rpg-systems §2.3, §10.2) already models "much of medicine is exorcism" |
| Amulets / talismans | the protection family (wards, amulets), carried as-is |
| Hymns (vibrating the names of deities) | the **Rites/Incantation** skills and the favour economy: rites performed correctly in the right language raise favour (rpg-systems §10.1) |
| Astrotheology (Sun, Moon, Saturn, Ishtar, Mercury-Enki) | the deity network's data (deities.csv): planetary associations per god — Utu=Sun, Nanna=Moon, Saturn (its god's face is unwritten; Ninurta, who stands in the notes' Enlil's pantheon, is the classic candidate), Enki=Mercury, Inanna=Ishtar=**Venus** — content, not a new system |
| Demonology's malevolent side | the curse-and-binding family, with the parent's rule intact: **sorcery is a crime** |

### 8.3 What the notes ask for that has **no existing mechanic** (opt-in list — nothing designed)

Requested by the notes, absent from the parent systems. Each needs the designer's explicit decision to become a mechanic; until then they are canon lore only:

1. **Sex magick** (tied to Inanna). Note: the parent project explicitly declined it (parent endings §6) — here it is the designer's call, with rating consequences flagged.
2. **Numerology.**
3. **Kabbalah / hermeticism, "as above, so below."**
4. **The Moon-list powers:** mind control (the notes' own warning: *"don't do it, it is bad karma"*), astral travel, telepathy, animal telepathy, psychometry.
5. **Pathworking** (Mercury list) and planetary rite-crafting built on the four completed power-lists (Earth, Moon, Mercury, and Venus — Venus written at the designer's request; world-bible §8.3). The notes wrote no Sun or Saturn lists.
6. **The Tree-of-Life / Pentagram / "Taurus D" columns** of the directions table (world-bible §7): lore, or a working magic system?

### 8.4 The gods: literal or ambiguous? `OPEN` §11.11

The notes give three views of the same deity — the Empire worships the lady of the skies; the Brotherhood calls her a **demonic entity**; the Sumerian ending warns that tearing her cult down only makes room for **"a new hidden evil… an ancient contemporary of Ishtar"** ([endings.md §3.2](endings.md)) — and their endings show real divine action (deals with Ishtar, a summoning). Whether this world's cosmology is **literal** (the demonic reading is true; the gods act openly — Mythic mode as the world itself) or **ambiguous** (different groups believe different things; the game never confirms) is a tone decision only the designer can make. The mechanics support both; the magic system does not change between them.

## 9. Content policy (the accuracy policy, adapted)

The parent's binding accuracy policy (parent game-design §9) adapts to a canon-first world:

1. **The content database still runs** (architecture §3 pattern). Every row carries its tag: `[A]` (sourced to the real Sumerian/Akkadian record, with citations, as the parent does), `[CANON]` (from the notes), `OPEN` (unresolved). The player-facing codex still tells the player which is which.
2. **Canon lint replaces anachronism lint:** nothing enters the game that contradicts the notes; contradictions *between* notes are listed as `OPEN`, never silently resolved.
3. **Real sources where the notes import them:** the pantheon names, the city lists, Barûtu, zisurru, astragalomancy, pessomancy, aeromancy and the rest of §8 are `[A]` and get the parent's full research treatment (the Mesopotamian law codes are the natural research target for the law table — a research pointer, not a decision).
4. **Content warnings:** war violence, famine, displacement, slavery-era economies, death-cult and demonic-deity themes, and the eradication content of the Sumerian ending — present as the notes wrote it, with its hidden price, never as spectacle. The parent's tone rule survives because it came **from these notes in the first place**: *each ending has a price* (parent endings §1.3).

## 10. Scope reality check

The parent's honesty section (parent game-design §11) applies twice over. The eight cities of world-bible §5 — several of them capitals — at the parent's true 1:1 scale would be an order of magnitude beyond one kingdom of 60 km². The parent's staging tools all carry over (the vertical slice first; data as the scalable art; lean on the content database), but **the map decision (§11.2) gates everything**: until the designer says which cities, at what scale, no world-building below the blueprint level should start.

## 11. Open decisions (for the designer)

The notes leave these open. Nothing below is decided anywhere in this blueprint set; the mechanics for any of them exist only if the parent systems already provide them.

1. **The title.**
2. **The map:** which of the eight cities are on it, at what scale (1:1 like the imported world-map pattern, or compressed), and whether the player's homeland is ever seen or only lore.
3. **The past life:** confirm the implication of world-bible §5 + endings §3.1 — that the player is the reincarnation of the Law Giver of the City of Jewels — and decide when and how the game reveals it.
4. **Reincarnation vs heirs:** the past-life device vs the parent's heir system (living-world §9). Generational time skips, a single life, or a past-life-flavoured take on the heir system? No new mechanic is built until this is answered.
5. **The homeland:** the Chaldean-like nation's name, people and culture (it is off-map canon; it still needs a dossier for the player's identity, starting skills and languages).
6. **Backgrounds:** one fixed prisoner start (the notes), or the parent's five-background system reskinned?
7. **City identities:** do the eight cities sit on the real Sumerian sites of world-bible §10; is *e-ab-kur-irkalla-ki* ("Gravestone") the City of the Dead's native name; is the lost City of Jewels Akkad (whose real location is itself lost)? The notes suggest all three; none is stated.
8. **The two ancient peoples** who rule the strange land (the notes' opening) — which two, and of the origin myth's peoples (southern boat folk, northern settlers)?
9. **The Empire as "Great King":** confirm the one-oath rule (rpg-systems §4.2) binds to the Emperor, and what treaties, embargoes or bans exist between the powers.
10. **Rank ladder labels:** the imported 7-tier ladder (0 outsider → 6 king) keeps its structure; the parent project's rank labels are **not** carried — this setting's ranks replace them.
11. **The cosmology:** literal (§8.4) or ambiguous? Ishtar = the lady of the skies is canon (world-bible §6); what remains is the "demonic" reading, and the identity of the "empire of the bull" (endings §3.2).
12. **The hidden ending's sequence:** the notes require "a very specific sequence of events" and don't give it. The parent's template (silent sequence, no markers, one rumour — parent endings §3.4) is the mechanic; the steps are the designer's.
13. **Endings 1–3:** unlock conditions (parent pattern: standing + Act IV actions) and where the player founds his city in each. The hidden ending's founding site is canon: a new city on the ruins of Ur, the old foundations razed ([endings.md §3.4](endings.md)).
14. **The act boundaries** of §6 — the beats are canon, the four-act arrangement is a proposal.
15. **The clock:** can the player alter local outcomes only, or does the great reset give local outcomes more reach?
16. **The Empire's tribute:** run it on the parent's *ilku*/land-grant mechanic (rpg-systems §5.2)?
17. **The wider world** (world-bible §9): lore only, off-map ventures, or future scope?
18. **Directions:** terrain scheme A or B, Plan 1 or Plan 2 per direction, and the meaning of "Taurus D"; are the Tree-of-Life/Pentagram sums lore or mechanics?
19. **Deity data:** domains, cult sites, festivals and taboos for the pantheon lists — plus the still-missing **8 deities of medicine** list, whether **Sun and Saturn** planetary power-lists get written (the notes give none; Venus was completed at the designer's request), and the garbled Enki-pantheon line.
20. **NPC voices:** subtitles plus barks, or voiced main NPCs (the imported budget default, or the costlier option).

## 12. The notes, covered

The completeness audit — every element of the designer's notes, and where it lives in this blueprint. Nothing in the notes is homeless; nothing in the blueprint lacks a notes home:

| Notes element | Blueprint home |
|---|---|
| Voiced / unvoiced player toggle | game-design §3 · endings §6 · mechanics row 34 |
| The immigrant-prisoner backstory | game-design §1, §3 |
| Contact with the groups; the mysterious cult; the rebels | game-design §5, §6 (Act II) |
| The end arc: weakened Empire and worship; his own nation; "needed elsewhere", initiation, sins cleansed | endings §5 |
| Own city-state, business, alliances | game-design §4, §5.3 · endings §2 |
| Four endings, each with a faction; hidden fourth behind a specific sequence | endings §1, §3 · sequence `OPEN` §11.12 |
| The Empire (faction lore) | world-bible §4.1 |
| The Neo-Sumerian Rebellion | world-bible §4.2 |
| The Barbarians | world-bible §4.3 |
| The Brotherhood of the Serpent | world-bible §2, §4.4 |
| The four ending canons | endings §3 |
| The eight cities | world-bible §5 · made playable in §7.1 |
| The origin myth of Urash | world-bible §3 |
| The pantheon lists | world-bible §6 · seeds the deity network (mechanics row 32) |
| The 8 deities of medicine | missing from the notes — `OPEN` §11.19 |
| e-ab-kur-irkalla-ki = "Gravestone" | world-bible §5 |
| The opening: 180 years; the drought; the invasions; the sage of the swamps; the prisoner | world-bible §4.1 · game-design §1, §6 |
| Element = terrain schemes | world-bible §7 · `OPEN` §11.18 |
| The directions table (Taurus D, Tree of Life, Pentagram) | world-bible §7 · `OPEN` §11.18 |
| The wider-world requirements | world-bible §9 · `OPEN` §11.17 |
| The real Sumerian city lists | world-bible §10 · `OPEN` §11.7 |
| The forms of magick | world-bible §8.1 · mapped in §8.2 · opt-ins §8.3 |
| The forms of divination | world-bible §8.2 · mapped in §8.2 |
| The planetary power-lists | world-bible §8.3 — Venus completed at the designer's request · opt-in §8.3 |
| The War Age | world-bible §1 · endings §3.4 |
| "Counteract these degenerate forces" | endings §5 |
