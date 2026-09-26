# INVENTED LEDGER — the fauna (Stage V batch 4)

Per D-018 and D-023: every choice below is authored glue tagged `INVENTED`. It fits the theme and the canon, contradicts no CANON or A row, and the designer may veto it. A veto is appended to DECISIONS.md and supersedes only that line. No CANON row was changed.

The **species** are attested for Sumer and its marshes (`[A]`, world-art-plan §8): fat-tailed sheep, goats and humped cattle in the herds, the donkey as the beast of burden, the kunga (the onager-donkey cross) with the great, mastiffs and salukis, cats in the stores, pigs kept by the poor, geese and ducks, doves, herons and flamingos, vultures, crows and sparrows, jackals, carp. §10 holds: **no chickens, no camels, no horses** in ordinary use (the kunga only at the envoys' house and the warchief's tent) — `tools/tests/test_art_fauna.py` enforces it. The **numbers** below are invented. Code: `db/canon/fauna.csv`, `tools/art/fauna_variants.py`, `tools/art/birds_gen.py`, `ASimFauna`.

## Models

| # | Choice | Values |
|---|---|---|
| 1 | Bases from Quaternius (QAL v1.0: fine in this private repo, never in a public one): the Ultimate Animated Animal pack (donkey, fox, husky, Shiba Inu, cow, bull, deer — Idle, Walk, Gallop, Eating, Death...) and the Farm Animal pack (sheep, pig — Idle and Jump only) | `fetch_fauna.py` |
| 2 | Breeds: zebu = cow + a hump over the withers (+10% of the length); fat-tailed sheep = sheep with rump and tail x2.2 wide; goat = deer (it walks) in black; kunga = donkey x1.1 darker; mastiff = husky girth x1.15; saluki = Shiba girth x0.8; jackal and cat = fox (0.9 m, 0.5 m) | `fauna_variants.py` |
| 3 | Coats: palette ramps per breed (dark / main / light / horn) | `RAMPS` |
| 4 | Body lengths: sheep 1.2 m, goat 1.1, zebu 2.0, donkey 1.6, kunga 1.8, mastiff 1.1, saluki 1.0, jackal 1.0, cat 0.5, pig 1.2 | `art/fauna_models.csv` |
| 5 | Birds code-built (<= 61 triangles): goose, duck, dove, heron, flamingo, vulture, crow, sparrow; flyers with spread wings, waders and fowl folded; the carp a jumping fish | `birds_gen.py` |

## Behaviour

| # | Choice | Values |
|---|---|---|
| 6 | One visible herd animal per ten head of the kernel's herd; shares sheep 0.55, goats 0.30, cattle 0.15; spread over the cattle pen and the pasture | `HerdScale`, `fauna.csv` |
| 7 | Hours: herds and beasts 06–19, salukis 06–20, dogs and cats all day, birds 05–19/20, vultures 08–17, jackals 20–05 (only near the tombs and the camps); outside its hours an animal sleeps inside half its home radius; the jackals are gone by day | `fauna.csv` |
| 8 | Groups without a place typology: 6 along the streets, 4 in yards, 3 at the wall foot, 3 at the tombs, 3 at the camps (x `per_place`) | `HabitatGroups` |
| 9 | Home ground: pens and pastures 40% of the smaller side (3–25 m); a building's animals 3.5 m round a spot 2.5 m out of its door; street animals 4.5 m; yards 2.2 m behind the house | `HomesFor` |
| 10 | Walk 120 cm/s, flee 380 cm/s from a player within 6 m (dogs bark: `OnAnimalCue`); idle 2–6 s, graze 4–10 s, walk to a point in its ground | `ASimFauna::Step` |
| 11 | Live budget: the 150 nearest animals within 120 m of the camera are drawn and animated | `LiveCap`, `CullCm` |
| 12 | Flocks: doves in 2 flocks over the precinct and 3 over the roofs, 3–15 m up; vultures soaring 30–60 m in 40 m circles; crows and sparrows pecking by the doors; herons, flamingos (20–40), geese and ducks in the lagoon; carp leaping every ~7 s. Separation 1.5 m, leash at 0.7 of the flock's radius; a standing flock lifts when the player is within 4 m and settles 20 s after; flyers roost outside their hours | `FlockKindOf`, `StepFlocks` |
| 13 | Budget 600 birds (today ~290 in 30 flocks); wing flap 4 Hz doves, 8 sparrows, 5 crows, 0.4 vultures, 2–3 for the lifted waders and fowl | `M_Bird` per-instance `FlapHz` |
