# Verb results — the refusal codes and reason keys (FND-05)

Every mutating verb in `sim/CApiItems.h` returns a code and leaves a reason key that `sim_world_last_reason` reads. The engine shows the key's string-table text, so no refusal is silent and no raw number reaches the player. **Codes and keys never change meaning**; new refusals get new codes.

| Code | Key | Meaning |
|---|---|---|
| ≥ 0 | `""` | Success; the value is what the verb documents (usually units moved) |
| -1 | `refuse.bad_argument` | A null or empty argument, a quantity ≤ 0, or an id already taken |
| -2 | `refuse.unknown` | No such item, stack, world item or container |
| -3 | `refuse.bound` | The stack is a quest item and cannot leave its holder |
| -4 | `refuse.too_heavy` | Not one unit fits under the 125% carrying line |
| -5 | `refuse.full` | Not one unit fits in the container |
| -6 | `refuse.locked` | The container is locked |

Rules:
- A verb sets `last_reason` to its key when it refuses and to `""` when it succeeds.
- A refusal changes nothing: no inventory, container, world item or crime record moves.
- Older verbs keep their own documented codes (`CApi.h`); `sim_world_progression_refusal` stays the progression channel.
