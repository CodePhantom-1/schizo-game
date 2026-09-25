# INVENTED LEDGER — W5-B: faction politics decides which bands raid

Per D-018 (creative gap-filling authorized; every invented item ledgered so the
designer can veto it) and D-021 (open play: faction interests and politics
built; content `INVENTED` and ledgered). One entry each: what, where, why it
fits theme and canon. None contradicts a CANON/A row, the notes, or
round-2-invented §9 (the Empire keeps no standing treaties with the Rebellion
or the Barbarians — **local truces are possible**, and that is all this wave
authors). Machinery: `kernel/src/WildTick.cpp` (`raid_politics`),
`kernel/src/Wild.cpp` (treaty loading, `live_treaty_between`),
`kernel/include/sim/Wild.hpp` / `WildActions.hpp`; contract:
[kernel/contracts/module_Wild.md](../../kernel/contracts/module_Wild.md).

## What is imported, not invented

- **The raid formula** is mechanics.md row 11 unchanged (hunger + opportunity
  − defence − fear → chance), W4-C's implementation; this wave only adds
  political points inside its existing components.
- **The faction column** of `wild_groups.csv` was already filled by W4-C where
  lore supports it — the eastern warband serves `the_barbarians`, the Sutean
  riders are `suti`, the swamp-sage's sons are `neo_sumerian_rebellion`
  (factions.csv ids, all CANON). **No cell was changed.** The Jackals of the
  Caverns and the Reed Men stay unaligned (runaway debt-slaves and local
  debtors; mechanics.md row 12's outlawry is exactly their condition) — that
  reading is the one verification this wave made.
- **Outlawry exists** (Faction.hpp `outlaw`/`is_outlawed`, W2-A) and
  **grudges exist** (wild_groups.csv `grudges`, W4-C); both are only read.

## Invented (W5-B)

### The treaty table's first sworn row

- **`db/canon/treaties.csv: southern_cause_pact` (new, INVENTED).** The sage
  of the swamps and the rebellious alliance of southern city states swear one
  cause: the rebellion's sworn bands lift no hand against the allied south's
  fields, herds, markets or caravans. Parties `neo_sumerian_rebellion` +
  `southern_city_states_alliance` — the alliance is the City of the Moon's own
  jurisdiction (cities.csv), and the rebellion's stated aim is to unite those
  city states (wb §4.2). It formalizes what the partisans' charter already
  says ("never on the fields of the south", invented-ledger-wild #11) as a
  political fact the kernel can read. The Empire signs nothing (round-2 §9).
  This is the *minimum* row the politics needs; more truces are content
  additions, not machinery.
- **"Live" reading of treaties.csv (the kernel's, ledgered):** a row is live
  between factions A and B when both appear in `parties` (';'-separated), the
  `terms` field carries the token `no_raids` (the one term the kernel reads;
  the rest of `terms` is codex prose), and **neither party is outlawed** — an
  outlawed party's law is broken, so its sworn peace is void. Treaties are
  open-ended (no day windows authored; nothing in row 26 expires them).
- **What a live treaty does:** defence **+15** against that holder's things,
  and **blocked outright** (chance 0; `assess_raid` skips the target) against
  the holder's *own* things — city targets for a treaty with the city, a
  caravan of the partner faction for a treaty with a caravan's faction. A
  pact with the city does not protect an imperial caravan.
- **Static, not state:** treaties load with the canon in `init_wild` and are
  never saved — a reload rebuilds them from `treaties.csv` exactly like the
  caravan table. **No new save section exists**, so Snapshot.cpp is untouched
  and old saves load unchanged (politics also reads `FactionState`, which the
  FACTION_* sections already save).

### The war's sides

- **The city's open enemies are `the_empire` and `the_barbarians`**
  (`kEmpireFaction`/`kBarbarianFaction`, kernel/src/WildInternal.hpp). wb
  §4.1–4.2: the south strikes the Empire; §4.1/§4.3: the eastern raiders raid
  the cities and migrate west — the rebellion's south included (events.csv
  `rebel_agents_agitate`, `eastern_raiders_pillage`). `war_stage ≥ 1` gates
  the whole war layer (at peace, politics adds nothing but treaties).
- **A live-treaty partner stands with the city**, so its bands inherit the
  city's enemies — the rebellion's partisans read as at war with the Empire's
  salt caravan without a second hardcoded list. An outlawed ally (treaty
  void) stands alone. The Empire↔Barbarians pair is deliberately **not**
  modeled (no band or caravan of the Empire rides; nothing needs it).
- **At war with the holder ⇒ opportunity +12** (`kWarOpportunity`). Chosen as
  an opportunity bonus (the task's alternative, fear reduction, was not
  needed); 12 points ≈ half a harvest window, visible against the 25/30/20
  scale of the existing target weights. Holder = the city's faction for
  fields/herds/market, the caravan's own faction for caravans.

### Outlawry

- **A band whose faction is outlawed raids anyone** (the task's rule, this
  reading ledgered): `FactionState::outlawed_by_faction` marks a faction's
  law as broken toward the player, so that faction's sworn peace is void and
  its bands owe nobody quarter — opportunity **+10**
  (`kOutlawOpportunity`), treaty blocks and penalties do not apply to it.
  Faction.cpp was not touched (W5-parallel owns its rites hooks); only the
  existing `is_outlawed` read is used.

### Grudges

- **A grudge that names the holder's faction sharpens the raid: opportunity
  +20** (`kGrudgeOpportunity`) — the W4-C caravan-grudge weight, now one
  holder-keyed rule (value unchanged, so current play is byte-identical at
  peace). **No canon row names the city's alliance or any player faction**,
  so against the city itself the path is dormant; it lights the moment a row
  names `southern_city_states_alliance` (proved by test with a synthetic
  grudge). Nothing was invented to force it live. "The player's faction" has
  no id in the data yet (the player holds standing, not membership); when one
  exists, the same holder-keyed rule covers it.

### The C API (`kernel/src/CApiFaction.cpp`, `include/sim/CApiFaction.h`)

- Faction was unexposed except standing query/add (already in CApi.h, kept
  there — CApi.cpp untouched). New, following the CApiWild.cpp conventions:
  the player's **tier** with a faction, the **oath list** (id; swearer;
  to_faction; day; broken), the **oath-breaker curse** flag, **outlawed**
  status, the **treaty table** reads (`sim_world_treaty_count`/`_treaty`,
  `_treaty_live_between`) and **`sim_world_band_politics`** — one call
  returning "faction;net_pts;blocked;note" for the UI (net = politics
  opportunity − politics defence vs the city; note is the fired rules,
  ','-joined: war | grudge | treaty | outlaw). These are the wave's
  "debug/status reads" (the count/record pattern the wild API already uses).

### Tunables (all in `Wild.hpp`, integer points on the formula's scale)

| Constant | Value | Meaning |
|---|---|---|
| `kWarOpportunity` | 12 | band faction at war with the holder |
| `kGrudgeOpportunity` | 20 | a grudge names the holder's faction |
| `kTreatyDefence` | 15 | a live no_raids treaty reins the band |
| `kOutlawOpportunity` | 10 | an outlawed faction owes nobody peace |

**Deliberately left out:** no per-faction war-status table (the war clock +
the treaty table + the two canon enemies carry it); no treaty expiry,
suspension or oath-swearing verbs (the Faction module's to build); no
Empire-signed treaties (round-2 §9); no change to the partisans'
kind-gated target choice (their charter is identity, not politics); no
hardcoded player-faction id (none exists in canon).
