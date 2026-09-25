# Invented ledger — ART-2, the people of the quarter get bodies

Per D-018: every invented choice below fits theme and canon, contradicts
no CANON/A row or the notes, and is subject to designer veto (a veto
supersedes here). Art direction D-023: low-poly, low-fi, Valheim-like —
explicitly NOT realistic.

## Model source + licence

- **Quaternius "Ultimate Animated Character Pack" (Nov 2019)** —
  https://quaternius.com/packs/ultimatedanimatedcharacter.html
- **Licence: CC0** (https://creativecommons.org/publicdomain/zero/1.0/ —
  the pack page states CC0; no attribution required). Fetched per file
  from the pack's public Google Drive folder
  (`https://drive.google.com/drive/folders/1sNi1AfenfPRrvRt5yfaj5QMMd6KKcUJ5`,
  glTF subfolder) by `tools/art/fetch_characters.py`; every file is
  recorded in `art/assets.csv` (`kind == character_model`).
- Per file: ~2 MB self-contained `.gltf` (embedded buffers), **1 skinned
  mesh, 23 joints, 4-8 flat-colour materials, NO textures** (vertex-colour
  flats — exactly the D-023 look), **17 embedded animations** (Idle, Walk,
  Run, Jump, Punch, SitDown, ...).
- **Poly counts** (triangles, from the glTF accessors): Worker_Male 2,524;
  Kimono_Male 2,540; Worker_Female 5,856; OldClassy_Male 5,432;
  Casual3_Male 4,920; Casual_Male 6,492; Casual_Female 6,624;
  Casual2_Female 6,752; OldClassy_Female 7,008; Wizard 6,482. Slightly
  above the "few hundred" brief, still firmly low-poly; these are the
  lowest-poly rigged+animated CC0 humans found in the survey (Kenney's
  rigged humans are GLB-with-external-textures and not per-character
  animated; Poly Haven has no characters).
- The sources carry a ~4x export scale (the models are ~4.1-5.0 units tall
  in their own coordinates); **the C++ bounds-normalizes every body to
  176 cm** (the capsule: 2 x 88 half-height), so the importer's scale
  decision cannot make giants.

## The cast (INVENTED, vetoable)

| UE asset id | Model | Role in the quarter | Tris |
|---|---|---|---|
| `Player` | Casual3_Male (sand/beige shirt, dark wrap, brown slippers) | the protagonist — sun-bleached wool, distinct silhouette from every NPC | 4,920 |
| `Npc0` | Worker_Male (terracotta tunic over white) | labourer | 2,524 |
| `Npc1` | Worker_Female | labourer | 5,856 |
| `Npc2` | Casual_Male | plain townsman | 6,492 |
| `Npc3` | Casual_Female (white tunic, brown apron) | plain townswoman | 6,624 |
| `Npc4` | Casual2_Female | plain townswoman | 6,752 |
| `Npc5` | OldClassy_Male (long coat) | elder / scribe | 5,432 |
| `Npc6` | OldClassy_Female (long dress) | elder | 7,008 |
| `Npc7` | Kimono_Male (crossed robe) | robed class — scribes, temple staff | 2,540 |
| `Npc8` | Wizard (blue/gold court robe, NO hat) | temple clergy — stands in for the priestess's order | 6,482 |

- Rationale: no knights/soldiers/ninjas/pirates/suits/zombies/cowboys/
  chefs/doctors/goblins/elves from the pack (anachronistic or
  non-human); the Worker/Casual/OldClassy/Kimono/Wizard subset reads as
  plain folk, elders and robed clergy at low-fi distance.
- **Identity mapping:** the npc-id hash (`FCrc::StrCrc32(id) % 9`) picks
  the variant — the SAME hash that used to pick the cylinder tint hue, so
  a resident keeps a stable look across sessions. The kernel still owns
  who walks where (this is presentation only).
- Rejected models stayed unfetched; adding one is a one-line change in
  `fetch_characters.py` + a `kNumBodyVariants` bump in `SimNpc.cpp`.

## Tint approach (the colour differentiator, revised)

- Cylinder fallback (assets not imported): the original hash-tint on
  `BasicShapeMaterial` still applies — unchanged behaviour.
- Imported bodies: **no tint** — the imported flat-colour materials are
  the look, and the per-npc differentiator is the hash-picked VARIANT
  (9 bodies vs the old 360 hues). Rationale: the imported materials have
  no `Color` parameter, so a MID tint would silently do nothing (and
  blanket-replacing with BasicShapeMaterial would repaint robes as
  featureless blobs). This is the "tint only when the base material is
  BasicShapeMaterial, else skip" rule from the brief.
- `Player` keeps the sun-bleached wool OVERRIDE only on the programmer-art
  fallback; the imported Casual3_Male already wears sand/beige.

## Animation approach (INVENTED)

- No anim blueprint in the slice: `SimNpc`/`SimCharacter` play a single
  looping `UAnimationAsset` — Walk when moving, Idle when not
  (`PlayAnimation`, switched in Tick with a current-clip guard). Quaternius
  ships Walk_Carry/Run_Carry (loaded strides) — deliberately NOT picked;
  the import normalizes to plain Walk/Idle.
- The player sprints (800 cm/s) on the Walk clip; Run exists in the source
  pack if the designer wants a sprint clip later.
- If the import lands meshes without animations, the bodies play the bind
  pose (a posed body that slides still beats a cylinder — accepted
  fallback per the brief).

## Import layout (what the coordinator's run produces)

`tools/art/ue_import_characters.py` (UNTESTED — writes to the live
Content; see tools/art/README.md for the command and the FBX fallback):

- `/Game/Art/Characters/<id>.<id>` — SkeletalMesh (the C++ also accepts a
  StaticMesh there as a second-chance fallback).
- `/Game/Art/Characters/<id>/Anims/Walk.Walk`, `.../Idle.Idle` — the two
  clips, renamed from whatever Interchange named them (exact-stem match,
  carry/run variants excluded).
- Skeletons/physics assets/materials stay under
  `/Game/Art/Characters/_staging/<id>` — referenced by object, not path.

## Known weaknesses / follow-ups

- All imported bodies normalize to exactly 176 cm — uniform height loses
  natural variation (bounds-driven, so a wrong import scale can never
  make giants); a per-variant height jitter is a one-line follow-up if
  the designer wants it.
- Orientation is Interchange's call: the code sets mesh rotation to zero
  expecting glTF +Z-forward to be baked to UE +X. If bodies face
  sideways, the fix is ONE constant (`SetRelativeRotation(FRotator(0.f,
  -90.f, 0.f))` in `ApplyCharacterBody` / `ApplyTravellerBody`).
- No anim blueprint, no root-motion, no head-tracking; NPCs slide through
  tight turns (straight-line steering is the existing slice rule).
- The glTF headless Interchange import is the riskiest untested link —
  the FBX conversion recipe in `tools/art/README.md` is the escape hatch.
- Wizard for clergy is a wink, not a claim; a dedicated priestly robe
  model would be a later art pass.
