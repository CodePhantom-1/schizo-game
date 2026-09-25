# Invented ledger — W6-C, the street lives: townspeople and the sun (engine side)

Per D-018: every invented choice below fits theme and canon, contradicts no CANON/A row or the notes, and is subject to designer veto (a veto supersedes here).

Ported from the parked `wip/ue-4-townspeople-daynight` sketch, re-based on current main: the kernel has since grown per-person schedule resolution (`sim_world_npc_place_at` resolves role rows + person rows + `home`/`work` through people.csv, bent by season and festival), so the sketch's invented role-to-place table is GONE — the kernel is now the only truth for where a resident stands.

## What is still invented (all engine-side presentation glue)

- **The sun path** (`ASimDayNight`): hour 0..24 mapped to a directional-light pitch at 15 degrees/hour (`Pitch = Hour * 15 - 90`) — hour 6 sunrise, hour 12 zenith, hour 18 sunset. No canon sun-path model exists; this is the standard 24-hour linear sweep. Night boundary hour < 6 || hour >= 18 (dawn = the gatekeeper's `gates_open_at_dawn` row at hour 6; dusk just after `gates_close_at_dusk` at 18). Night light: directional 0.05, sky light 0.02 (real darkness, moonlight only); day 8.0 / 1.0.
- **The pre-street place fallback** (`ASimNpcDirector::ResolvePlaceLocation`, step 4): until the street registry is wired, a `FCrc::StrCrc32` hash of the place id maps deterministically onto the grey-box street's extent (X −1800..4300, Y −500..500, Z 100 — `SimGameMode`'s floor segments). Same id, same spot, every run. Once a registry/delegate is wired or the fallback is switched off (`bHashPlaceFallback`), an unknown place id means off-street (despawn), not a hash spot.
- **The cluster ring** (`ClusterOffsetFor`): residents who share one place (three watchmen at the gate post, a household at home) stand on a small deterministic ring (radius 40..120 cm, angle from the npc-id hash) instead of stacked on the exact place point.
- **Relevance defaults**: `sim.NpcRelevanceRadius` 30000 cm (0 = everyone), `sim.NpcMaxVisible` 48 (0 = no cap; canon has 41 people, so the cap is dormant today), walk speed 140 cm/s, capsule ACharacter default (34/88). The body is a scaled engine cylinder (no skeleton yet); the tint is a hash of the npc id into a hue on `/Engine/BasicShapes/BasicShapeMaterial`.
- **Display names are ids.** The C API exposes no npc name read (people.csv `name` exists but no `sim_world_npc_name`); actors and logs use the people.csv id until one is added (kernel code is out of scope for this wave).

## What is NOT invented

- Who is on the street this hour: kernel `sim_world_npc_schedule_at` / `npc_place_at` / `npc_fate` — the director only mirrors it (spawn on a placed schedule row, despawn on unplaced/unknown/`slain`/`captive:`/`sold_east`, walk to the resolved place). Shops closing at night is the kernel's own schedule rows moving residents home; the engine only reflects it (residents leave the stalls, the light dies).

## Registration

- `docs/DECISIONS.md`: no new decision — glue under D-018's existing creative-gap-filling authorization, not a new system.
- No kernel, canon or CSV changes.
