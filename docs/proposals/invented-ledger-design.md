# INVENTED LEDGER — design (round 2, D-018)

One line per invented choice from [round-2-invented.md](round-2-invented.md). A veto on any line supersedes only that line (append the veto to DECISIONS.md; nothing here is CANON until unvetoed by silence or ratified explicitly).

| # | Choice | Where |
|---|---|---|
| 1 | Calendar: 12 months × 30 days, 12 named months (Rains-Coming → Dead-Fires), 4 festival days keyed to the seasons | `calendar.csv` |
| 2 | City of the Dead = Gravestone — one city, not two | `deities.csv:underworld_queen`; `cities.csv:gravestone`; `factions.csv:dead_cult` |
| 3 | The "empire of the bull" (endings §3.2) = Gugalanna, the Bull of Heaven, consort of the underworld queen | `deities.csv:gugalanna` |
| 4 | The 8 deities of medicine = Gula, Ninisina, Ninkarrak, Nintinugga, Damu, Ninazu, Baba, Meme | `deities.csv`; `pantheons.csv:eight_deities_of_medicine` |
| 5 | Sun planetary power-list (Utu: kingship, oaths, exposure, healing, righteous force) | `planetary_powers.csv:sun` |
| 6 | Saturn planetary power-list (Ninurta: time's weight, boundaries, endurance, the price clause) | `planetary_powers.csv:saturn` |
| 7 | All 4 endings' unlock conditions (standing-at-Oath-bound + a specific Act IV action each) | `endings.csv` |
| 8 | The hidden ending's 5-step silent sequence | `endings.csv:ending_4_brotherhood` |
| 9 | Terrain: Scheme A in force for all directions; Plan 1 for West/North/East, Plan 2 for South | `regions.csv` (`in_force_ruling`) |
| 10 | Taurus D / Tree of Life / Pentagram columns stay lore-only, never a mechanic | `regions.csv`; game-design §11.18 |
| 11 | Real-Sumerian-site pairings for 6 of 8 cities (Akkad, Kutha, Uruk, Nippur, Larsa, Eridu); City of the Moon = Ur confirmed; City of the Warrior Spirit left unpaired (Taurissian, not Sumerian) | `cities.csv` (`real_site_correspondence`) |
| 12 | Map status per city: only City of the Moon is v1-map; the rest are off-map/rumour-only; the player's homeland is never visited | `cities.csv` (`map_status`) |
| 13 | The homeland: the Land of Kaldun, the Kaldunai people (marsh-lowland nation, Aramaic-adjacent tongue, ancestor-reverence, no surviving state cult) | `world_lore.csv:homeland_kaldun` |
| 14 | 6 Kaldunai (homeland-culture) personal names | `names.csv` |
| 15 | The wider world (world-bible §9) stays lore-only for v1; no off-map venture built to it yet | `world_lore.csv:wider_world_scope` |
| 16 | The Great King binds to the Emperor; no Empire-Rebellion or Empire-Barbarian treaties exist; local border truces are possible via the mercenary-contract pattern | doc-only (game-design guidance; `factions.csv:the_empire` untouched — CANON) |
| 17 | Ilku/tribute confirmed to run on the parent's land-grant mechanic, already anchored by the Éren/Gigir rank tiers and the D-016 tribute items | doc-only (no CANON row touched) |
| 18 | The clock's reach: local outcomes only, never the great reset itself | doc-only |
| 19 | The undeciphered script = the Brotherhood's own Ante-Diluvian script | doc-only (no table exists yet to hold it) |
| 20 | Calling-name and companion-archetype suggestions (non-binding; other agents own those tables) | doc-only in round-2-invented.md |
| 21 | The two ancient peoples (opening line) confirmed as the origin myth's southern boat-folk and northern settlers | confirmation only — no row changed |
| 22 | The act arrangement (story.csv's opening/I–IV/epilogue) confirmed as final, not a still-open proposal | confirmation only — no row changed |

**Explicitly left open, not vetoed:** the title (§11.1), NPC voices (§11.20), cosmology's literal-vs-ambiguous core question (§11.11 — locked by D-004), and full per-god liturgical detail beyond the medicine circle and Sun/Saturn.
