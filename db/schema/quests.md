# quests

**Rows:** 41 · **Source:** canon rows authored from the designer's notes ([world-bible](../../docs/world-bible.md)), researched real-world rows, or reserved for later phases.

## Fields

```
id,name,giver,act,kind,deadline_days,tag,source_ref,reward_silver,reward_faction,reward_standing
```

## Rules

- every row carries `tag` (`CANON` / `A` / `INVENTED` / `OPEN`) and `source_ref` (`wb §x` and/or `notes L<n>` — [db/sources/notes.md](../sources/notes.md); `A` rows name their real-world source; `INVENTED` rows are authored glue shown to the player as such).
- `OPEN` rows mark missing canon; they cannot ship.
- `reward_silver` (W2-A, INVENTED): silver grains credited to the quest-taker's purse (`Property::credit_purse`) on `complete_quest`. Scaled by act depth and quest `kind`, rounded to 5.
- `reward_faction` / `reward_standing` (W2-A, INVENTED): when set, `complete_quest` also calls `Faction::add_standing(reward_faction, reward_standing)`. Blank when no faction fits the quest's giver/story.
