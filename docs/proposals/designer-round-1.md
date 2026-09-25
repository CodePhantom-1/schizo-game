# PROPOSALS — designer round 1 · **APPROVED 2026-09-25, applied as D-015**

Two content gaps are blocking live systems. Both are mechanics-level data ratified through the decisions process (like D-011's constants) — **nothing here becomes canon until you approve it**. Reply "approve both", or edit freely; edits get applied and logged as D-014.

---

## P1 — The rank ladder (D-007; blocks: the RPG layer's vocabulary, Phase 4 slice)

The imported 7-tier ladder keeps its structure (0 outsider → 6 king, per-polity, rises by deeds + a patron's act). Proposed labels, using plain English where the notes give no word and attested Sumerian terms `[A]` where they do:

| Tier | Proposed label | Canon anchor |
|---|---|---|
| 0 | **the Outsider** | the prisoner start — taken from your homeland, protected by no one (notes L3;L105) |
| 1 | **Sojourner** | a resident guest under a local protector |
| 2 | **Householder** | a free member of a community; owns, trades, sues |
| 3 | **Éren** `[A]` — *King's Man* | a palace dependant holding land for service (the Empire's tribute principle, notes L15;L104, made personal) |
| 4 | **Gigir** `[A]` — *Chariot Warrior* | the elite land-for-service tier (gigir = chariot) |
| 5 | **Sukkal** `[A]` — *Intimate of the King* | envoys and high officials (sukkal = envoy/vizier) |
| 6 | **Lugal** `[A]` — *King* | the right of kingship; founding the traditional kingdom (notes L9;L11;L39) |

## P2 — The calendar's seasons (game-design §11.18–19; blocks: 2 dormant season-gated events, the midday-rest schedule row, and all future seasonal content)

The kernel's calendar shape (12 months × 30 days = 360) stays as ratified machinery; **month names stay OPEN** (unnamed numbers) until you ever want them. Proposed: **four season ids**, which un-gate the dormant content:

| Season id | Starts (day of year) | Why |
|---|---|---|
| `rains` | 1 | the year opens with the rains on fallow fields |
| `sowing` | 91 | wheat and barley go into the earth (notes L57) |
| `harvest` | 181 | the reaping; the economy's yearly peak |
| `vintage` | 271 | the late-summer pressing; the drought bites hardest here |

(These four absorb the kernel test's placeholder ids; the drought curve can then key on harvest/vintage, which is where a breaking drought hurts.)

---

**On approval:** the rank labels fill `ranks.csv` (tag CANON-by-delegation, D-014) and the season ids fill `calendar.csv` — which immediately activates the dormant season-gated events and the midday-rest row.
