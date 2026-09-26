# THE WORLD MADE ALIVE — the art and world plan

**Status:** v1 (2026-09-26) · the designer's brief: *"vastly improve the entire map… nicer, more well-thought-out buildings with cooler designs, of all types… buildings for each profession and of all characteristics… improve the materials, textures, details so it's not just one monotone smooth plain block… the terrain, its textures, relief, stones, branches, bushes, trees, the variety of flora, animals of all kinds, contemporary and regional… really bring this world alive."*
**Works with:** [city-of-the-moon.md](city-of-the-moon.md) (the crescent layout, D-025) · [completion-plan.md](completion-plan.md) (this is **Stage V**, run together with Stage AA and replacing Stage S's art rows) · D-023 (the style) · the canon tables (`work_roles`, `callings`, `items`, `buildings`, `wild_places`).
**Tags:** everything new is `INVENTED` under D-018, grounded in the notes and in the real record of early Mesopotamia (`A` where attested). Anachronisms are kept out (see §10).

---

## 1. The look: what "Valheim-like" means here

D-023 says low-poly and a bit wacky, with the mood carried by light. That is not the same as bare. Valheim is low-poly *and* dense with handmade irregularity. The rules that get us there:

1. **Chunky, hand-made silhouettes.** No straight line is perfectly straight: walls lean 1–2°, courses wander, roofs sag, doorways are slightly off square. Every generator adds seeded jitter.
2. **Low-resolution painted surfaces, not smooth colour.** Textures are 128–512 px and slightly pixel-crisp, on trim sheets and atlases. Surfaces read as mudbrick courses, reed weave and plaster patches even from far away.
3. **Grime lives in vertex colour.** Ambient occlusion, soot, water stains and dust are baked or painted into vertex colour, so one material serves many buildings and each looks lived-in.
4. **Everything tells who lives there.** A building's look comes from its owner's profession, wealth, age and quarter. A smith's house has soot above the door and a slag heap; a scribe's has a clay bin and drying tablets on the sill.
5. **Light does the rest.** Low sun through dust, lamp-glow in windows, smoke columns, dawn marsh fog, moonlight by phase (already built).

## 2. The material library

One low-resolution set per material, each with **4 wear states**: fresh, worn, crumbling, and drought-cracked or salt-stained. It is driven by the kernel's drought stage and by a building's age. The textures are a mix of CC0 maps downscaled (ambientCG, already fetched) and stylized ones baked from Blender procedural nodes.

| Material | Where | Variants and details |
|---|---|---|
| **Sun-dried mudbrick** | most walls | visible courses, straw flecks, eroded bases, rain runnels, patched holes |
| **Fired brick** | footings, drains, the wealthy, the gate | reddish, with bitumen mortar in the elite quarter |
| **Mud plaster** | homes | smooth over brick; flaking patches show the courses beneath |
| **Whitewash / lime** | shrines, the rich, the temple precinct | brilliant in sun, grubby at hand height |
| **Glazed brick (lapis-blue, gold-yellow)** | the Moon Gate, the ziggurat shrine, the palace: **rare and sacred** | glossy, the only shiny wall in the city |
| **Clay-cone mosaic** | temple and elite façades (the Uruk cone-mosaic technique `A`) | red, black and white zigzag and lozenge patterns: a signature look nobody else has |
| **Bitumen** | damp courses, boat hulls, basket linings, mortar | black, tarry sheen, dripped edges |
| **Reed** | huts, mats, fences, roofs, the mudhif guest halls | bundled, woven (herringbone, check), frayed ends |
| **Palm** | beams, rafters, fences, rope | fibrous trunk sections, frond thatch |
| **Imported timber (cedar, tamarisk)** | rich doors, lintels, the palace, ships | warm red-brown, carved |
| **Stone (limestone, basalt, bitumen-stone)** | thresholds, door sockets, querns, weights, statue bases: **scarce in Sumer, so precious** | chipped edges, polish where hands touch |
| **Textiles** | awnings, curtains, clothing, banners | dyes of the period: madder red, indigo, saffron yellow, undyed cream, black goat hair |
| **Metal** | bronze, copper (with green patina), silver, gold, lead | shown mostly as accents; iron is a curiosity (§10) |
| **Clay and ceramics** | jars, pipes, tablets, cones, figurines | plain buff, red-slipped, and the rare glazed ware |
| **Leather, bone, shell** | goods, inlay, lamps (shell lamps) | |

**Surface detail, on every building:**
- trim sheets for edges, lintels and parapets;
- decals: cracks, soot, water stains, handprints, red-ochre bands, apotropaic marks (dog and owl engravings as the City-of-the-Dead influence, rarely), bird droppings, chalk tallies;
- instanced detail meshes: projecting beam ends, drain spouts, wall niches, peg holes, tethering rings, lamp niches, bricks drying on the ground.

## 3. The building grammar (house kit v2)

A **building is generated from its data row**: `typology × wealth tier × age × owner profession × quarter × seed(place id)`. The kit v2 pieces:

| Part | Choices |
|---|---|
| **Plinth** | none (poor) · packed earth · fired brick with bitumen · stone (elite, temple) |
| **Walls** | plain · buttressed with niches (temple style, the "recessed panel" façade `A`) · plastered · whitewashed · cone mosaic · painted ochre bands |
| **Openings** | doorways: palm-log lintel · cedar lintel · corbelled mudbrick arch · recessed double frame (rich). Windows: high slits, lattice grilles, clay-pipe vents. Doors: reed mat · plank · painted cedar with bronze studs |
| **Roof** | flat: palm beams + reed mat + packed clay, parapet; rooftop life: reed sun-shelters, drying racks, ladders or stairs, roof hatches, sleeping mats in summer |
| **Storeys** | 1 · 2 with timber gallery around a courtyard (merchants, elite) · 3 (only the palace and the lighthouse precinct) |
| **Courtyard** | none · shared (tenement) · private with a well, a hearth, a shrine niche and a palm or pomegranate · a garden (elite) |
| **Additions** | oven (tannur) · workshop lean-to · animal pen · granary dome · shop counter with awning · house shrine · stairs to the house-tomb below |

## 4. The typology catalogue: every kind of building, every trade

Each row is one generator preset. The props list is what makes it read *without signs*, since most people can't read (city-life §8). The anchors are the smart-object spots where NPCs work (Stage D6/F9).

### 4.1 Homes, by wealth
| Building | Look | Tells its owner by |
|---|---|---|
| **Reed hut / marsh dwelling** | arched reed-bundle hut on a mud platform; fish traps; a canoe pulled up | nets drying, fish on racks, a smoky hearth |
| **One-room poor house** | small, rough plaster, reed door, shared wall | a single jar, a quern outside, dung cakes for fuel stacked on the wall |
| **Tenement court** | several families round one court; laundry lines; many doors | children's toys (clay rattles, carts), shared oven, quarrels |
| **Artisan house-workshop** | shop in front, family behind, work spilling onto the street | the trade's tools and waste (see 4.2) |
| **Merchant courtyard house** | two storeys, cedar gallery, painted door, a storeroom of sealed jars | scales and stone weights, a seal on a cord, imported goods |
| **Elite house** | fired-brick plinth, whitewash or cone mosaic, garden court, private shrine, house-tomb, guard dog | carved cedar doors, glazed tiles, a musician's lyre, perfume jars |
| **Priest's house / the gipar** | inside the precinct: clean whitewash, offering tables, sacred garden | incense stands, votive figures, ritual basins |
| **Foreigners' lane** | Kaldunai and eastern hands: carpets hung out, different door shapes, painted ceramics | foreign pots, a different god's niche |
| **Suti and eastern migrant camps** | black goat-hair tents, cattle pens, dung fires, cart circles | saddle bags, felt, horse tack (the eastern horses, rare in the city) |

### 4.2 The trades (one per work role, calling and skill in the canon, plus the city's needs)
| Building | Look and props |
|---|---|
| **Bronzesmith's foundry** | furnace with goatskin bellows, crucibles, mould racks, ingots (the "oxhide" shape), charcoal heap, slag pile, soot-black front, hammering anvil stone |
| **Armourer** | racks of bronze scale plates being laced onto leather, leather caps, shields drying (hide over wicker), helmet forms |
| **Weaponsmith / bowyer-fletcher** | spear-shaft bundles, unstrung composite bows on pegs, feathers in baskets, arrowhead trays, sling pouches |
| **Tannery and leatherworks** | **downwind, at the edge**: soaking pits, hides stretched on frames, bark vats, flies, the smell cue |
| **Potter** | updraft kiln (domed, smoke), kick wheel, clay pits, drying rows of jars, sherd heaps, fuel of reeds |
| **Weaver and dyer** | upright and ground looms, dye vats in madder, indigo and saffron, dripping skeins on lines, the stained gutter |
| **Carpenter / wheelwright** | solid and spoked wheels, chariot frames, adzes, shavings, timber stacks (precious imported wood) |
| **Boatbuilder** | reed boats in bundles, bitumen pots on fires, the round quffa coracles, a slip into the lagoon |
| **Baker** | beehive tannur ovens, dough troughs, bread stacks, the morning queue |
| **Brewer / beerhouse** | big fermenting jars, drinking straws, benches, a hanging reed sign-by-shape, the rowdy evening |
| **Miller** | rows of saddle querns, grain sacks, flour dust on everything |
| **Butcher and fishmonger** | hooks, carcasses, fish on reed mats, dogs waiting, gulls |
| **Oil and perfume press** | beam press, sesame seed sacks, oil jars, a scented courtyard |
| **Jeweller / seal-cutter** | a small shady booth: lapis, carnelian, shell, cylinder seals and their rolled impressions, a bow drill |
| **Scribe's house / tablet school** | tablet shelves, clay bins, water trough for kneading tablets, reed styli, pupils' benches |
| **Physician (asû)** | drying herbs, mortars, bandages, honey jars, a patient on a mat |
| **Exorcist / diviner** | amulet racks, liver models of clay, incense, a sheep tethered for the reading, protective figurines buried at the threshold |
| **Astrologer's roof** | a roof platform, a gnomon, a star-watching mat: the Star Terrace (city §2) |
| **Weights house / customs house** | official scales, a bin of stone weights shaped like ducks, sealed bales, a scribe at a table |
| **Granaries and storehouses** | domed silos, sealed doors (clay sealings on pegs), guards |
| **Caravan yard / inn** | a walled court, donkey lines, fodder, water troughs, packers, a guest room gallery |
| **Barracks / watch post** | spear racks, a practice yard, a lookout platform, gongs or horns |
| **Well-houses and water-lifts** | shaduf sweeps, troughs, water-carriers' jars, the gossip spot |
| **Street shrines** | a niche with a figurine and a tiny offering bench, lamps at night |

### 4.3 The countryside
- **Farmstead:** a mudbrick house and reed barns; a domed granary; a **threshing floor** with a sledge; animal pens; a **shaduf** on the canal.
- **Fields:** the **seeder plough** and ox yoke (the Sumerian invention `A`), sheaves, and scarecrows made of reed.
- **Date-palm gardens:** climbers' ropes, and dates drying on mats.
- **Brick-making yard:** clay pits, brick moulds, and rows of bricks drying in the sun. A striking, very period sight.
- **Kilns and charcoal clamps** at the edges of the city.
- **Irrigation:** canal sluices made of reed-and-mud check gates, dykes, and water-wheels. The water-wheel is anachronistic, so it is replaced by the shaduf (§10).
- **Reed-cutting camps** in the marsh; fishing weirs; salt pans at the shore.

### 4.4 The monuments, done properly
- **The ziggurat.** Buttressed faces with recessed panels, reed layers between brick courses (as at Ur `A`), drainage slits, a triple stair, the lapis shrine on top, and the temple kitchens and workshops in the precinct.
- **The Great Lighthouse.** A mole of rubble and bitumen, the keepers' stair, the fuel store (reed bundles and bitumen), and the fire bowl.
- **The Moon Gate.** Glazed bands, guard rooms, and a toll table with scales.
- **The Prophet's Court.** A throne dais, the council hall's columns (palm trunks plastered and painted), and the audience court.
- **The Garden of Tombs.** Grave mounds, brick vaults, offering tubes to feed the dead (`A`), and the restless-dead mood.

## 5. Props and clutter: the world's texture of life

A library of **300+ low-poly props**, by kind:

- **Household:** storage jars (plain, spouted, ribbed), cooking pots, bowls, cups, lamps (clay and shell), braziers, reed mats, beds (wood frame with rope), stools, chests, looms, spindles, querns, water-skins, brooms.
- **Food as objects:** bread, dates, onion strings, fish, sheaves, sacks, jars sealed with clay.
- **Trade goods:** every row of `items.csv` gets a world mesh (Stage C2), so what a shop sells is what you see.
- **Religious:** altars, offering tables, standing worshipper statues with big inlaid eyes (the Tell Asmar style `A`), incense stands, divine emblems (Nanna's crescent, Utu's rayed disc, Inanna's reed-bundle ring post), foundation pegs.
- **Transport:** donkey packs, solid-wheel carts, sledges, reed boats, the round **quffa**, oars and poles.
- **Life:** laundry, children's toys, a dog asleep in shade, cats on walls, dung cakes drying, bricks drying, flies over refuse, a sleeping beggar, a game board (the Royal Game of Ur `A`) on a doorstep.

Placement is rule-based (Stage D/F data): props appear where their owner's trade and the hour of day put them. For example, the laundry is out in the morning, the lamps are lit at dusk, and the bread is gone by noon.

## 6. The terrain: land, water and relief

**Landforms**, built in `SimEnvironment` from data:
- river branches with **natural levees**;
- canals with sluices and silted edges;
- **fields in long strips** (the Mesopotamian field shape `A`);
- date groves on the levees;
- **reed marsh and lagoon** (the two waters);
- **tells** (mounds of older towns) on the horizon;
- dunes and desert pavement to the south;
- **salt flats** spreading as irrigation fails: salinization, the drought made visible;
- mudflats and a shell beach at the sea;
- the far mountains of the north and east.

**Materials:** a triplanar landscape material with low-resolution layers, painted by rules (height, water distance, slope, drought, footfall):
- alluvial silt, green irrigated earth, cracked dry mud, salt crust, sand, gravel;
- reed-mud, beach sand with shells, trodden paths (worn in by NPC traffic and roads), and rocky outcrops.

**Relief detail:**
- ditches, dykes, furrows and field boundaries;
- erosion gullies and cart ruts;
- flood lines on walls;
- scattered stones (river cobbles, imported limestone blocks, basalt grinders abandoned), pottery sherds everywhere (every tell glitters with them), bones, driftwood and dead branches.

## 7. The flora: regional and seasonal

It is generated by code (L-system and parametric trees in Blender) with wind animation, and it has **seasonal states tied to the calendar** (seasons.csv) and **wither states tied to the drought**.

| Group | Plants (lower-Mesopotamian `A`) |
|---|---|
| **Trees** | date palm (seedling offshoots, young, mature, very tall, dead trunk, fallen), tamarisk, Euphrates poplar, willow along canals, pomegranate, fig, apple (rare, in gardens), grape vines on arbours, the rare imported cedar in temple gardens |
| **Water and marsh** | giant reed (Phragmites), cattail, cyperus and papyrus-like sedges, water lilies in quiet canals, algae in stagnant drought pools |
| **Crops, by season** | barley (the staple: sprouting → green → gold → stubble), emmer wheat, flax (blue flowers), sesame, lentils, chickpeas, onion, garlic and leek beds, cucumbers, herbs (coriander, cumin) |
| **Wild and desert** | camelthorn, saltbush, liquorice scrub, desert grasses, thistles, and **spring wildflowers after the rains** (poppies, anemones) as a seasonal burst |
| **Dead and dry** | dry stalks, tumbleweed-like saltwort, bleached branches, brown-tipped fronds that deepen with the drought |

**Rules:** plants follow soil and water distance, reeds only where it's wet, palms on levees and in gardens. Gardens are planted by their owners' wealth. Instancing uses HISM/foliage with PCG scatter (plan.md rule 7) and impostors in the distance.

## 8. The fauna: the animals of the land

Each animal has a low-poly model with idle, walk, run, eat and sleep animations and its own sounds. Sources: CC0 animal packs (Quaternius animals, the same family as our humans), plus procedural models for the specifics.

| Group | Animals |
|---|---|
| **Herds and work** | fat-tailed sheep, goats, cattle (and the humped zebu type seen on seals `A`), donkeys, the **kunga** (the donkey-onager cross that pulled Sumerian battle-wagons `A`), dogs (mastiff and saluki types), cats, pigs (kept by Sumerians `A`), geese, ducks, doves and pigeons (Inanna's birds) |
| **Foreign and rare** | horses: the eastern barbarians' animals, rare and prized in the city (the notes' eastern warrior people) |
| **Wild land** | lions (the royal hunt), onagers, gazelles, wild boar in the marsh, jackals, foxes, striped hyena, wolves, ostriches in the desert, jungle cat, mongoose |
| **Birds** | herons, storks, pelicans, ibises, **flamingos in the lagoon**, vultures over the Garden of Tombs, eagles (the Anzu motif), crows, sparrows, kingfishers, and bats at dusk |
| **Water** | carp and catfish (seen jumping, in markets, and caught), turtles, frogs chorusing at night |
| **Small and dangerous** | snakes (vipers: the snakebite rites), scorpions, lizards on warm walls, **locust swarms** (an event), flies and mosquito clouds over stagnant water, fireflies in the marsh |
| **Mountains (off-map, for journeys and the Warrior Spirit)** | ibex, leopards, bears, mouflon |

**Behaviour:**
- Birds flock (boids), and each group keeps to its own habitat.
- Herds follow their herdsmen's schedules. The kernel's `herd_head` count shows as real animals, so a herd lost in a raid disappears from the field.
- Predators come at night (living-world §6), and a lion near a herd is an event.
- Animals react to the player: birds scatter, dogs bark at strangers (the reaction system, Stage G).

## 9. Atmosphere and life

- **Weather:** dust storms (a brown wall on the horizon, then darkness at noon), winter rain (wet surfaces, puddles, mud), dawn fog over the marsh, heat shimmer.
- **Signs of life:** oven smoke at dawn, cooking fires at dusk, incense from the precinct, and the lighthouse fire.
- **Sky:**
  - the stars, with **the zodiac constellations and the extra ones as a star field** (the astrology of D-025, so you can actually point at them);
  - the Milky Way;
  - the moon by phase (built);
  - comets and meteors as omen events (aeromancy).
- **The drought, visible:**
  - canals shrink and turn to cracked mud;
  - salt crusts creep across the fields;
  - palms brown and awnings fade;
  - dust thickens;
  - the market empties and prices rise (the kernel already drives these).
- **Festivals:** banners, garlands, a swept processional way, massed lamps, and crowds (Stage O).

## 10. Accuracy guard rails (what stays out)

- **No chickens:** they reach Mesopotamia late.
- **No camels in the city:** at most a rumour from the far south, and only if the designer wants it.
- **No water-wheels:** the shaduf and bucket lines instead.
- **No coins:** silver is weighed.
- **Iron is a rare curiosity**, never common tools.
- **No horses in ordinary city use:** they belong to the eastern peoples and the elite.
- **No glass windows.**

The world's own inventions (the Zusheng lore: the cities, the gods' visible signs, the magic of D-025) are welcome. The rule is only that nothing contradicts the setting's period feel unless the notes say so.

## 11. The other cities and regions (for journeys, vignettes and the codex)

Each city of the notes gets an **architectural identity kit**, used for journey scenes and codex illustrations, and later for walkable cities:

| City | Look |
|---|---|
| **City of Jewels** (the imperial capital) | glazed-brick splendour, lions and bulls in relief, lapis and gold, broad processional ways |
| **City of the Dead** | streets set with skulls, dog and owl engravings on every house, the temple dug into the earth, the colossal underworld queen |
| **City of the Sky** | the sacred hill, a white temple above, windswept |
| **City of Kings** | catacomb mouths, mummy processions, the Garden of the Gods (every plant and animal of creation) |
| **City of the Sun** | the Retributors' fortress: sun-discs, bronze, drilled ranks |
| **City of the Abyss** | the oldest temple of the two waters, the sacred pool, mystics |
| **City of the Warrior Spirit** | the tribal capital: palisades, horse lines, skull poles, the old sun god's ruined temple |

**The four directions (the notes' directions table):**
- **West:** the islands and mound builders.
- **North:** forest ziggurats "imitating the mountains", and the great mountains.
- **East:** river valleys and waterfalls.
- **South:** the desert of ruins and the Suti.

These are region kits for the wild lands and journeys.

## 12. How it gets built (all by code, like everything so far)

- **Blender headless generators** (the existing pipeline, `tools/art/`):
  - `house_kit_v2` + `assemble_building.py` (the §3 grammar, driven by `places.csv`);
  - `props_gen_v2` (the §5 library);
  - `flora_gen` (L-system palms, reeds, tamarisk and crops, with seasonal and wither variants);
  - `terrain_detail` (stones, sherds, debris).
  
  Every output has a contact sheet in `art/review/` for a visual check.
- **Textures:** the stylized bake (`texture_bake.py`: procedural nodes to 256 px maps) plus the downscaled CC0 set, packed into trim sheets and atlases.
- **Animals:** CC0 packs first, then procedural bodies where no pack covers an animal (kunga, zebu, fat-tailed sheep: variants of the base meshes).
- **In Unreal:**
  - `SimEnvironment` (Tommy's builder) grows the layout and landform code;
  - PCG and HISM scatter for flora, stones and props;
  - Niagara for smoke, dust, birds' far flocks and insects;
  - the drought, season and weather values from the kernel drive material parameters.
- **Data:** new canon tables `flora.csv` and `fauna.csv` (species, region, habitat, season, tag `A`/`INVENTED`); `buildings.csv` grows the typology columns (wealth tier, trade, parts).
- **Budgets:** 60 fps at 1080p medium on the RX 5700 XT (after the kernel fix) and 30+ fps on the iGPU. Instance counts, LODs and impostors are checked every step.

## 13. Stage V — the steps

| # | Step | Done when |
|---|---|---|
| V0 | **Art bible and reference board:** this document plus contact sheets of real Mesopotamian references (cone mosaics, Tell Asmar figures, mudhif halls, seeder ploughs), a style frame for each quarter, and the colour script (dawn, noon, dusk, night; wet and drought) | the designer approves the style frames |
| V1 | **The material library:** every material in §2 with 4 wear states, the trim sheets, the decal set, vertex-colour grime baking | a material ball sheet in `art/review/`, in engine |
| V2 | **House kit v2 and the building grammar (§3):** every part, jitter, generated from a data row | a contact sheet of 20 random buildings, none alike, all clearly Mesopotamian |
| V3 | **The typology catalogue (§4):** every home, trade and monument preset with its props and NPC anchors | each of the ~110 crescent buildings (Stage AA) is generated from its row, and a person can tell every trade apart without a sign |
| V4 | **The props library (§5):** 300+ props, one mesh per `items.csv` row | the contact sheets; every item visible in the world |
| V5 | **The terrain (§6):** landforms from data, the triplanar layers painted by rules, relief detail and scatter | a flight over the region reads as a real landscape at noon and at dusk |
| V6 | **The flora (§7):** the generators, seasonal and wither states, placement rules | each season looks different; the drought visibly withers the land |
| V7 | **The fauna (§8):** models, animations, behaviours, sounds hooks, herds tied to the kernel | a walk from the gate to the marsh meets herds, birds, dogs and, at night, a jackal |
| V8 | **Atmosphere (§9):** weather, smoke, sky, star field and zodiac, drought visuals, festival dressing | a dust storm, a rainy dusk and a festival night each look distinct |
| V9 | **The other cities and regions (§11):** the identity kits for journeys and the codex | each city of the notes has a vignette scene |
| V10 | **Performance and polish:** LODs, impostors, instancing, budgets met on both GPUs | measured and recorded |

**Order with the rest:**
- V0–V3 run *with* Stage AA, so the crescent is built in the new style from the start.
- V4 runs with Stage C (items).
- V5–V7 run with Stage Q (the wild lands).
- V8 runs with Stage O (weather, festivals).
- V9 runs with Q7 (journeys).
- V10 is continuous.
