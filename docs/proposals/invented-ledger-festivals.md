# INVENTED LEDGER — K-2, festivals and per-person schedules

Per D-018: every choice below is authored glue tagged `INVENTED`. It fits theme and canon, contradicts no CANON/A row or the notes, and is subject to designer veto (a veto is appended to DECISIONS.md and supersedes only that line). No CANON row was changed. The three CANON rows of `people.csv` only gained two blank columns.

**Scope note.** The festival *days* were not decided here. `calendar.csv`'s `festival_days` row was already filled `INVENTED` under D-018 (see [invented-ledger-design.md](invented-ledger-design.md) #1: New Waters day 1, First-Cutting Procession day 181, Ishtar's Torch day 271, Feeding of the Dead day 360). K-2 turns that prose row into machine-readable rows and adds what the kernel needs to act on them. A veto of design #1 empties `festivals.csv`, and the mechanism then stands idle with no code change. The month names (`calendar.csv:month_names`) stay unwired in the kernel.

| # | Choice | Where |
|---|---|---|
| 1 | `festivals.csv`: the 4 D-018 festivals as rows, each with a deity (the_two_waters, nanna, inanna, underworld_queen) grounded in its source_ref | `festivals.csv` |
| 2 | Gathering windows: New Waters 8–13, First-Cutting 7–12, Ishtar's Torch 4–9 (the morning star is a dawn star), Feeding of the Dead 18–22 (an evening feast at home) | `festivals.csv` `attend_from`/`attend_until` |
| 3 | Gathering places: the three temple festivals go to `temple_front_place` (the temple of sun and moon). The kispum feast is kept `home` | `festivals.csv` `place` |
| 4 | Exempt roles: gatekeeper, watchman and lighthouse keeper keep their posts on every festival (the watch "works the empty streets" in `events.csv:festival_pickpockets`). Market traders are also exempt at First-Cutting | `festivals.csv` `exempt_roles` |
| 5 | Markets: closed on New Waters, Ishtar's Torch and Feeding of the Dead. Open on First-Cutting, when the new grain floods the square (`events.csv:harvest_market_surge`) | `festivals.csv` `market` |
| 6 | Gathering task texts (what a townsperson does at each festival) | `festivals.csv` `gathering` |
| 7 | Priest of the moon festival rows: receives the first portions (New Waters), leads the procession (First-Cutting), kindles Ishtar's torch and receives the tribute basket (Ishtar's Torch) | `schedules.csv` `moon_priest_*` |
| 8 | Tavern keeper "feast night" row on any festival day | `schedules.csv:tavern_feast_night` |
| 9 | `place` token for every existing schedule row (`work`/`home`/a place id; blank where the role has no slice-street place: fishermen, paladins, field hands, lighthouse, dockworkers) | `schedules.csv` `place` |
| 10 | `home_place`/`work_place` for the 29 INVENTED residents, read from their existing source_ref notes ("owns places.csv …", "lives at …"). The scribe works from his house (no tablet-house place exists). Gatekeepers, watchmen, brewer, tavern keeper, cook, traders, water carriers and the moon priest have no house on the slice street, so their `home_place` is blank | `people.csv` |
| 11 | The two gatekeepers split the day: Ur-Utu (dawn) goes home at 18, and Sin-iddinam (dusk) sleeps through the morning. This makes people.csv's existing "dawn/dusk gatekeeper" notes true | `person_schedules.csv` |
| 12 | Ishme-Dagan (the deep-watch man) sleeps at the watch post through the 21h patrol | `person_schedules.csv` |
| 13 | Enmenanna goes to Ea-nasir's grain stall at 10 to plead for time on her debt (from `quests.csv:the_priestess_debt`) | `person_schedules.csv` |

**Machinery (not content, documented in kernel/contracts/module_Schedule.md):** the per-hour rank order and the gathering-window semantics. They are the imported "festival overrides" of mechanics.md row 7 made concrete, with no tunable constants.
