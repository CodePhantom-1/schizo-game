# INVENTED LEDGER — the atmosphere (Stage V batch 5)

Per D-018 and D-023: every choice below is authored glue tagged `INVENTED`. It fits the theme and the canon, contradicts no CANON or A row, and the designer may veto it. A veto is appended to DECISIONS.md and supersedes only that line. No CANON row was changed.

The **weathers** are the kernel's (`weather.csv`: clear, hot, scorching, sandstorm, rain, fog), the **drought stages** the kernel's (0–4), the **festivals** `festivals.csv`'s. The **sky** is the real one: the Yale Bright Star Catalogue (public domain) at latitude 31° N. What each looks like on screen — the numbers below — is invented. Code: `ASimAtmosphere`, `ASimDayNight::SetWeatherBlend`, `tools/art/fx_gen.py`, `ue_make_fx_materials.py`, `star_dome.py`, `ASimEnvironment::CanalWaterZ`, the festival rows of `db/canon/flora.csv`.

## Weather

| # | Choice | Values |
|---|---|---|
| 1 | Each weather's target state (0..1) | hot: dust 0.12, shimmer 0.4 · scorching: dust 0.2, shimmer 1 · sandstorm: dust 1, overcast 0.3 · rain: rain 1, overcast 0.7 · fog: overcast 0.5, fog 1 at dawn lifting 8–11 h to 0.5 · clear: all 0 |
| 2 | The drought shows on a clear day | wither = stage / 4; dust ≥ 0.3 x stage / 4 |
| 3 | Changes blend, never snap | every value eases with a 60 s time constant; the world opens in its weather |
| 4 | Wet ground | soaks with the rain (60 s), dries linearly over two in-game hours; a skipped night is not a drying spell |
| 5 | How the sky shows it (`SetWeatherBlend`) | dust thickens Tommy's haze up to x8 in ochre; rain and cloud grey it and dim the sun; fog x4; the haze never starts nearer than 3 m; all-zero = his values exactly |
| 6 | Wet surfaces (M_Scatter, M_Terrain) | albedo x0.7, roughness to 0.4 at Wet 1 |

## Effects

| # | Choice | Values |
|---|---|---|
| 7 | Rain | 1,500 pale streaks (4 x 90 cm) in a 30 m box riding with the camera, falling with a slight wind slant; opacity 0.55 x Rain |
| 8 | Dust | 800 ochre motes blown 30 m across the box; opacity 0.5 x Dust |
| 9 | Smoke sources | every soot source the building grammar writes (`Content/Sim/smoke.csv`, 53): five grey puffs rising 6 m |
| 10 | When a fire smokes | ovens (bakery, temple kitchens, cookshop, potter) 05–08 h; foundry and armourer all day; every other hearth 17–20 h; nothing in the rain |

## The night sky

| # | Choice | Values |
|---|---|---|
| 11 | Which stars | every star brighter than magnitude 5.0 (1,604), 900 m out, following the camera |
| 12 | How bright | 2.8 m (0.18°) at magnitude 5, three times that at 0; brightness 10^(−0.2 (m − 1)) (gentler than the eye's 2.512 per magnitude), capped at 2.5; colour from B−V, half-way to white |
| 13 | The zodiac's figures | the twelve ecliptic constellations, each the minimum spanning tree of its stars brighter than 4.5 (Cancer: its four brightest) — drawn by nearness, not from any tablet; faint gold; `sim.Zodiac 1` shows them until the D-025 astrology UI |
| 14 | The Milky Way | 2,600 soft blobs about the galactic plane (σ 3.5°, thicker toward the centre in Sagittarius), the Great Rift dimmed; seeded |
| 15 | The turn | sidereal angle = day x 360.9856 + hour x 15.041 degrees about the pole, 31° up in the north; day 1 hour 0 is angle 0 |
| 16 | When they show | as the sun sinks below the horizon (fully by 6° under); cloud, dust (x0.85) and fog hide them |

## The drought and the festival

| # | Choice | Values |
|---|---|---|
| 17 | The canals drink the drought | the irrigation canals' water 30 cm lower each stage (120 cm at stage 4); the lagoon, river and sea stay |
| 18 | Festival dressing | banners (a 4.5 m cedar pole, madder cloth, lapis hem, gold fringe), garlands (flowers on a 4 m string), lamp stands (three oil lamps on a platter): round the temple of sun and moon (10 m) and along the Sacred Mound's fronts; 64 in all; up only on a festival day |
| 19 | Festival lamps | the flames (palette `flame_0`) glow while MPC_World.Festival is up |
