# MODULE CONTRACT — Combat (W4-B)

**Status:** Wave 4, track W4-B. Health, stamina, zonal wounds with bleeding
and healing over days, knockout and death; arms and armour from canon; a
deterministic integer attack resolver; group skirmishes; morale, stance,
surrender, prisoners and ransom; looting; wear, repair and recasting; and
the justice side (assault, murder, self-defence, duels, slaying outlaws).

**Imported mechanics:** docs/mechanics.md row 20 (conditions: zonal wounds),
row 29 (weapons: damage type, reach, speed, weight, balance, durability,
quality; flint → copper → arsenical → tin bronze; iron a curiosity;
recasting), row 30 (armour: 4 zones, 2 layers, cultural shields and helmets,
weight/stamina/heat trade-offs). The "fight & conquer" loop of
docs/game-design.md. Open play (D-021): nothing reads act or quest state.
Integer maths only (D-022).

**Files:** `include/sim/Combat.hpp`, `src/Combat.cpp` (the module — writes
only `CombatState`); `include/sim/CombatActions.hpp`,
`src/CombatActions.cpp` (the caller layer — the `WorldState&` verbs that
touch Needs, Population, Events, Justice, Property and inventories, like
`Actions.hpp`). Tests: `tests/test_combat.cpp`, `tests/test_scenario_duel.cpp`,
`tests/test_capi_combat.cpp`.

**Owns:** `CombatState` (`WorldState::combat`).
**May read:** `Db` tables `arms`, `combat_styles` (module); plus `items`,
`people`, `places`, `laws` (caller layer). `Needs` (read-only, for penalties).
**Must never:** hold its own Rng or advance the world's (every attack forks
`w.rng` with a salt of both ids and `CombatState::seq`); use floating point in
anything that affects state or a draw; resolve an OPEN row; gate on story.

## Canon data

| Table | What | Tags |
|---|---|---|
| `arms.csv` (new) | 20 rows: stats for every weapon, shield and armour piece. Columns `slot` (melee/ranged/shield/armour), `skill`, `damage_type` (cut/pierce/blunt), `damage`, `reach_cm` (range for ranged), `speed` 1–10, `weight_g`, `balance` 1–10, `durability`, `quality` 0–100, `zones` (armour), `layer` (under/over), `prot_cut/pierce/blunt`, `block` (shields), `stamina`, `heat`, `material_tier`, `ammo`, `culture` | 15 `A` (object attested), 5 `INVENTED`; every stat number INVENTED |
| `combat_styles.csv` (new) | 11 fighting styles by faction or role: kit, skill, morale, flee/surrender thresholds, favoured zone | all `INVENTED` |
| `items.csv` | a W4-B block: the 20 arms as tradeable goods + `arrow`, `sling_bullet`, `healing_herbs`, `linen_bandage` | `A` / `INVENTED` |
| `laws.csv` | `self_defence` (INVENTED, dismissed), `slaying_a_robber` (A: CH §21–22, dismissed), `duel_killing` (INVENTED, compensation;exile) | |
| `people/places/names/schedules.csv` | the asû Urlugaledina (role `physician`, 4 schedule rows) and the coppersmith Nur-Ea (role `craftsman`), their houses | INVENTED (names: 1 A, 1 INVENTED) |

The skill tokens (`blades`, `axes`, `maces`, `spears`, `bows`, `slings`,
`shields`, plus `dodge`, `unarmed`) are the W4-A seam, and they are now wired
to W4-A's `skills.csv` in `CombatActions.cpp`:

| Token | W4-A skill |
|---|---|
| `blades` | `dagger` |
| `axes`, `maces` | `mace_and_axe` |
| `spears` | `spear` |
| `bows`, `slings` | `bow_and_sling` |
| `shields` | `shield` |
| `unarmed`, `dodge` | `wrestling` |

The module itself still takes plain numbers (`CombatInputs`).

## The body

- **Health** 0..100, capped at `100 − open wound damage / 2`. 0 = dead.
- **Stamina** 0..100. A blow costs `2 + 2×weapon.stamina + weight_g/500 +
  armour_load_g/4000`; a block `3 + shield_g/2000`, a parry 3, a dodge
  `4 + load_g/2000`. Short of stamina: the blow goes in at −30 ("winded").
  +20/h awake, +40/h resting, +5 per breather round.
- **Wounds** are kept one per blow: zone, type, severity, damage, remaining,
  bleed per hour, clot hours, treatment. Severity from net damage:
  scratch ≤3, light ≤8, serious ≤15, grave ≤24, mortal ≥25.
- **Bleeding** (cut/pierce only): light 1/h (clots in 3 h), serious 2, grave
  4, mortal 7; a serious+ thrust +1. Serious+ never clot on their own —
  **a wound that is not bound bleeds until death.** Binding (torn cloth):
  stops up to serious, grave seeps 1, mortal 2; with a `linen_bandage`: grave
  stops, mortal seeps 1. Herbs (`healing_herbs`): all stop, mortal seeps 1.
  A healer (asû): all stop. A wound left seeping clots 6 h after binding
  (3 h with herbs).
- **Effects** (`combat_effects`): bleeding, concussed (open head ≥15),
  knocked_out, limp (open legs ≥20, or lasting), useless_arm (open arms ≥30),
  weak_arm (lasting), winded (stamina <15), surrendered, prisoner, outlaw,
  dead, `scar:<zone>`.
- **Knockout:** a blunt head blow of net ≥9 (1–3 h), or a collapse at
  health ≤15 (2 h). A knocked-out, yielded or captive man is struck
  unopposed (chance 100%, aim always lands, +3 damage).
- **Death:** health 0, or a mortal cut/thrust to the head. Cause `slain` or
  `bled_out`, with the killer (the last attacker).

## The attack (`resolve_attack`)

```
attack  = skill(weapon.skill) + agility×3 (ranged ×4) + balance×2 + (speed−5)×2
          + quality/10 + clamp((reach − defender reach)/10, −10, 10) [melee]
          − penalties (winded 30; open arm dmg/2 ≤40; open head dmg/2 ≤30;
            weak_arm 10; exhausted 15, starving 10, parched 10 [Needs]; broken weapon 10)
defence = best of
          block = skill(shields) + strength×2 + shield.block + quality/10 (+10 vs ranged)
          parry = skill(weapon)/2 + balance×2 + agility        [melee only; fists parry fists]
          dodge = skill(dodge) + agility×3 − 2×(armour kg) − 20 limping (−10 vs ranged)
          − penalties (open head dmg/2 ≤30; Needs; stamina <5: 25)
chance_bp = clamp(5000 + (attack − defence) × 60, 500, 9500)
roll      = rng.int_in(0, 9999);  hit when roll < chance_bp;  margin = chance − roll
```

A miss is `dodged` / `blocked` / `parried`. A hit lands on `zone_hint` when
the margin clears the aim cost (torso 0, arms/legs 1500, head 3000), else
where it falls (head 10%, torso 40%, arms 25%, legs 25%).

```
blow = damage × (75 + quality/2)/100  (÷2 if broken)
       + (strength − 5) [melee; ×2 for blunt] + margin/1000
armour on the zone = Σ over both layers: prot[type] × (75 + quality/2)/100
       (0 if broken; ÷2 when margin ≥ 4000 and the blow is not blunt — "a gap")
net  = (blow − armour) × zone% (head 150, torso 100, arms/legs 75);
       a blunt blow on armour always bruises (net ≥ 1); net ≤ 0 = "deflected".
```

Wear: every blow wears the weapon 1; a block wears the shield
`1 + damage/4` (**shields break**); armour that took the blow wears
`1 + min(its protection, blow)/3`. At 0 durability an item is broken (weapon
half damage, armour 0, shield cannot block) until repaired or recast.
Morale: the struck lose `net/2 + 1`, the striker gains 2.

Outcome codes: 0 dodged, 1 blocked, 2 parried, 3 deflected, 4 wounded,
5 knocked_out, 6 killed; Invalid (−1) with a refusal
(`attacker_incapacitated`, `defender_dead`, `same_actor`, `no_ammunition`).
`AttackResult::text` is `key=value;...` (see CApi.h).

## Stance, skirmish, styles

- `choose_stance`: `resolve = morale + 8×(allies − enemies) + (health − 50)/2`.
  Yields when `health ≤ surrender_at` and `resolve < 40` (never if
  `surrender_at` is 0); flees when `health ≤ flee_at` or `resolve < 15`
  — unless limping, then yields (or fights on if he never yields).
- `resolve_skirmish(side_a, side_b)`: sides interleave A0,B0,A1,B1…; each
  standing fighter takes a stance, fleeing/yielding (to the strongest enemy)
  or striking the weakest standing enemy at his favoured zone. A fallen
  comrade costs every ally 10 morale. +5 stamina per round. Ends when a side
  has nobody fighting (winner 0/1) or at `max_rounds` (−1).
- Styles (`combat_styles.csv`) by role first, then faction, else `commoner`.
  An npc is armed with his style's kit (given into his inventory, plus a
  quiver of 12 arrows / 3 javelins / a pouch of 12 bullets) the first time
  he fights (`ensure_combatant`).

## Time

- `advance_combat_hours`: bleeding, clotting, stamina, knockout, rest hours.
- `tick_combat` (daily, from `advance_days`): conscious npcs bind their own
  wounds; whoever still bleeds bleeds the day out (the player's hands are
  the engine's — bind him through the API); every open, non-bleeding wound
  closes by `1 + 1 rested (8 h) + 1 herbs + 1 healer` a day; health climbs
  `4 (+6 rested)` a day toward its cap; morale drifts back to 50. A closed
  serious+ wound leaves `scar:<zone>`; a grave leg/arm wound no healer saw
  leaves a lasting `limp` / `weak_arm`.

## The caller layer (`CombatActions.hpp`)

| Verb | World effects |
|---|---|
| `attack_in_world` | ammo checked and spent (javelins spend themselves); a kill → `on_killed` |
| `skirmish_in_world` | archers with no ammo fight bare-handed; ammo spent per shot; deaths → `on_killed` |
| `on_killed` | npc removed from `Population::npcs` (so from every schedule), `knows` edges cut after each acquaintance remembers "mourns X, killed by Y"; Needs dropped; his prisoners freed; `TriggeredEvent{"combat_death"}` logged. His inventory stays — the body can be looted |
| `advance_body_hours` / `rest_in_world` | Needs: fatigue +1/h per 25 open damage, +1/h per 4 armour heat (awake); thirst +1/h while bleeding. Rest also advances Needs as sleep |
| `treat_in_world` | bind (spends a linen_bandage if held), herbs (spends healing_herbs), healer (a living `physician`; 5 silver) |
| `file_combat_crime` | yielded/captive victim → murder/assault; outlaw victim (or the player outlawed by any faction) → slaying_a_robber; agreed duel → duel_killing (wounds: nothing); victim struck first → self_defence; else murder/assault. Filed through `Actions::commit_crime` (witness memory, hearing in 3 days) |
| `ransom_prisoner` | 30 silver captive → captor; shortfall as a loan (20%, 90 days) — Code of Hammurabi §32 as the model |
| `loot_body` | a dead body, or the looter's own prisoner: all goods move; wear carries over |
| `repair_at_smith` / `recast_at_smith` | a living npc whose `work_place` is a `smithy`; fees to the smith's purse. Broken copper/bronze must be recast; flint, iron, leather cannot be |
| `combat_inputs_for` | W4-A wired: strength/agility/endurance from `actor_attribute`. For each token, the skill is the best of: the style's drill, `actor_effective_skill`, and the untrained floor of 10 |
| skill growth | each blow calls `note_skill_use` (W4-A; the player's sheet only). The attacker's weapon skill gains 3 when the blow lands and 1 when it misses. The defender's shield skill gains 2 on a block, wrestling 1 on a dodge, and his weapon skill 2 on a parry. Treating wounds calls `note_use("verb:treat")` for medicine. Medicine ≥ 40 binds as well as linen |

## For W4-C (wild lands, bandits)

- One blow: `AttackResult attack_in_world(WorldState&, attacker, defender, zone_hint)`.
- A raid or ambush: `SkirmishResult skirmish_in_world(WorldState&, side_a, side_b, max_rounds)`.
- Pure what-ifs on a scratch state: `resolve_attack(db, rng, state, Fighter, Fighter, zone, day)`
  / `resolve_skirmish(db, rng, state, a, b, inputs, needs, day, rounds)`.
- Arm a bandit: `apply_style_in_world(w, id, "drought_bandit")` (or give him
  role `bandit`); make him fair game: `set_outlaw(w.combat, id, true)`.
- Bandit ids need not be Population npcs; a dead stranger just logs the
  event and leaves his body's inventory.

## Snapshot

Trailing optional sections after K-1's, recognised by tag (`Reader::peek_tag`),
so older saves load with nobody hurt: `COMBAT_ACTORS` (one row per combatant
+ armour/wound/lasting/durability sub-rows), `COMBAT_HOSTILITY`,
`COMBAT_DUELS`, `COMBAT_PRISONERS`, `COMBAT_DEATHS`, `COMBAT_SEQ`.

## Known limits (next steps)

- Durability is per actor per item id (two copies of the same item share wear).
- Item quality is per arms row, not per instance; recasting yields the row's quality.
- Morale shock, flight and pursuit are resolved inside the skirmish only; a
  fled fighter is not tracked afterwards (W4-C's wild lands can).
- Illness/infection of wounds is the illness track's (mechanics row 20).

**Definition of done:** every declaration in Combat.hpp / CombatActions.hpp
implemented; the three test files pass; same seed + same calls → same save bytes.
