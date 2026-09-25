# Invented ledger — the City of the Moon around the Moon Gate Quarter

Per D-018: every invented choice below fits theme and canon, contradicts no CANON/A row or the notes, and is subject to designer veto (a veto supersedes here). Art direction is D-023: low-poly, low-fi, Valheim-like, with the mood carried by light, fog and colour. No new canon rows are added. Everything here is scenery and lighting built by code (`SimEnvironment`, `SimDayNight`, `SimMeshKit`).

## Grounding

- **world-bible §5, City of the Moon:** the richest city, on the sea trade routes, home of the temple of sun and moon and the Great Lighthouse. This gives the harbour, the lighthouse and the ziggurat.
- **world-bible §2, the drought that is breaking the world:** this gives the dust haze, the withered and fallow fields, and the brown-tipped palm fronds.
- **world-bible §8, the northern mountains "rivalling the Himalayas":** these are far snow-capped silhouettes to the north, visible only as haze-blue shapes.
- **Ur as the real-site pairing (world-bible §11.7):** this gives the three-tier ziggurat with a triple stair and buttressed faces, and flat-roofed mudbrick houses with parapets.

## Layout (cm; +X east, +Y north; the Moon Gate at the origin)

| # | Invented choice | Where |
|---|---|---|
| 1 | The city wall is a 360 × 360 m rectangle (x 0..36000, y −14000..22000). It is 9.5 m high, with battlements and towers every 40 m. | `BuildWalls` |
| 2 | **The Moon Gate** is two 15 m towers and a lintel over a 6 m passage. A lapis-glazed band with gold rims runs across it, and a glowing crescent moon sits on a lapis field. The street's own gate place stands in the passage. | `BuildWalls`, `Build` |
| 3 | A sea gate in the east wall opens onto a stone quay. | `BuildWalls`, `BuildLighthouse` |
| 4 | **The temple of sun and moon** is a three-tier ziggurat, 46 × 64 m at the base and 23 m high. It has a triple stair, buttress ribs, and a lapis moon shrine on top with braziers. It stands in a walled precinct whose gate faces the quarter. | `BuildZiggurat` |
| 5 | **The Great Lighthouse** stands on a stone mole 85 m out to sea. It has a square base, an octagonal middle and a round top, with a bronze fire bowl. At night it has a fire and a slowly turning beam. | `BuildLighthouse` |
| 6 | The rest of the city is about 500 flat-roofed mudbrick houses on a jittered 10 m grid, with avenues every 60 m. Houses have parapets, some upper rooms and reed sunshades, dark doorways, and lamp-lit windows at night. The quarter's street, the gate plaza and the temple precinct are kept clear. | `BuildCity` |
| 7 | West of the gate there are irrigated barley fields in parcels: green, ripe, fallow and cracked dry (the drought). Canals cross them. A palm-lined road leads west. There are date-palm orchards. | `TerrainHeight`, `BuildCountryside` |
| 8 | A river runs south of the walls to the sea, with reed banks, palms and reed boats. | `TerrainHeight`, `BuildCountryside`, `BuildBoats` |
| 9 | Desert dunes lie beyond the fields and the river, with a rolling steppe north, rocks, and far mesas. | `TerrainHeight`, `BuildFar` |
| 10 | Merchant ships lie at the quay and at anchor. | `BuildBoats` |
| 11 | Lamps are lit from dusk to dawn: gate braziers, door lamps along the street, precinct and ziggurat braziers, lit windows. This follows `schedules.csv` `evening_meal_and_lamps`. | `Tick`, `BuildStreetDressing` |

## Sky model (`SimDayNight`)

- The sun rides a tilted circle. It rises in the east over the sea at 06:00, peaks at 62° in the south at noon, and sets in the west over the fields at 18:00. A pale moon takes the opposite circle.
- The sky is a physical SkyAtmosphere with dusty Mie scattering. The sky light is captured in real time. There are volumetric clouds, and a warm drought haze with volumetric fog that turns deep blue at night.
- The grade uses warm highlights, cool shadows, bloom, vignette and light film grain.
- On Linux (the designer's RX 5700 XT on RADV), the captured sky light and the clouds are skipped, exactly as before, because of the GPU hang recorded in DECISIONS. Lumen and virtual shadow maps are switched on only in `Config/Windows/WindowsEngine.ini`.

## Superseded

- **`SimPropsBuilder`'s 120 m painted sky dome and its post volume:** these walled off everything past the street. They are now only used with `-SimLegacySky`.
- **The street's own sun and ground cube:** the terrain is the ground (tag `SimGround`), and SimDayNight owns the sky.
- **The kit's small gate pillars and lintel:** these are used only when no `SimEnvironment` stands.
- **The imported photo-texture kit materials:** every slot is now a flat colour on `M_Flat`, with a per-house tint through ISM custom data. This follows D-023; the realistic path was already archived (A-1).
