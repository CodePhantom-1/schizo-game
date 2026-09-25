# THE CITY OF THE MOON — the redesign (D-025)

**Status:** v1 design (2026-09-25) · replaces the 500-house grid of `SimEnvironment::BuildCity` (invented-ledger-environment row 6). Everything here is `INVENTED` under D-018, grounded in the notes, and open to the designer's veto.
**The designer's brief:** less dense, more creative and alive, but compact. No scenery houses: **every building is lived in and has something to do.**

## 1. The idea: a crescent around a sacred lagoon

The notes: *"One of the oldest cities and the richest for its direct access to trade routes to the seas… the temple of sun and moon, the Great Lighthouse… a hub for trade and scholarship. Capitol to the enigmatic Prophet and home to the rebellious alliance of southern city states."* The origin myth: the first great city was founded by boat-folk from the south who offered **fish and sea gems to the deities of salt and fresh water**.

So the city is shaped like its god's sign. It is a **crescent** of mudbrick wrapped around a **lagoon** where the river's fresh water meets the sea's salt: *the two waters*.

- The **ziggurat** stands on the belly of the crescent.
- The **Great Lighthouse** stands on its eastern horn.
- The **Moon Gate** is at its western horn.
- The lagoon is the city's heart, its harbour and its holy water at once.

From the top of the ziggurat the whole city reads as a moon.

**Scale:** the chord is ~260 m and the whole thing can be walked in 4–5 minutes, compared with 360 m × 360 m before. There are **~110 buildings**, every one of them simulated, and **~160 named people**. The streets curve, and life spills out onto roofs, courtyards, canals and boats, so the city feels fuller than it is dense.

## 2. The nine quarters (west horn → east horn)

| # | Quarter | What it is | Places (each has an owner, a door, and things to do) |
|---|---|---|---|
| 1 | **The Moon Gate & the Caravan Yard** | the western horn: the land road arrives | the Moon Gate and its toll house; the caravan yard (donkeys, wells, packers); the Road Inn and the beerhouse; the **quayside prison barracks** where the game begins; the gatekeepers' lodge |
| 2 | **The Crescent Market** | the inner arc facing the lagoon: awnings, noise, everything for sale | ~20 stalls on a curving terrace; the **House of Weights** (the city's true scales); the smiths' lane (forge smoke); the potters' kilns; the weavers and dyers (dye vats colouring the gutter); the perfumers and oil sellers; the herbalist-physician; the scribe's booth by the well |
| 3 | **The Sacred Mound** | the crescent's belly, raised: the gods' ground | the ziggurat of sun and moon (Nanna and Utu); the **gipar**, the high priestess's house; the temple stores; the diviners' and exorcists' houses (livers, lots, SAG.BA.SAG.BA); the **Moon Pool**, a stepped basin where lagoon salt water and canal fresh water meet, the rite site of the two waters (fish and sea gems); the sacred date grove |
| 4 | **The House of Tablets** | on the rise toward the lighthouse: the city's scholarship | the scribal school; the tablet archive; the **Star Terrace**, a rooftop observatory where the zodiac and the planets are read (astrology, astrotheology, the numbers of the gods, "as above, so below"); the astrologers' lodgings |
| 5 | **The Lighthouse Horn** | the eastern horn, pointing out to sea | the Great Lighthouse (its keepers, its fire, its beam); the keepers' house; the signal fires; a cliff shrine to Enki, lord of the waters and of magic |
| 6 | **The Lagoon Wharf** | the water's edge along the inner curve | the sea gate; the ship berths; the fish market; the **sea-gem divers' huts** (gifts of Enki washed ashore and dived for, D-016); the shipwrights' slip; the merchant houses of the sea trade; the customs house |
| 7 | **The Reed Quarter** | where the river enters the lagoon: houses of reed on stilts and islands | reed-arched guest halls (mudhif); fishers' and boatmen's homes, where boats are the streets; the whisperers of the **sage of the swamps**, the rebellion's hidden safehouses; a floating shrine |
| 8 | **The Prophet's Court** | the old governor's palace on the crescent's back, now the rebellion's seat | the council hall of the southern alliance; the envoys' house (rebel city-states come and go); the rebel barracks and training yard; the old imperial offices, half abandoned and half looted; the hearing court and its holding room (justice) |
| 9 | **The Newcomers' Terraces & the Garden of Tombs** | outside the north wall, and along the river | the eastern migrants' camp (tents, cattle, a warchief's envoy); the foreigners' lane (Kaldunai, Suti, merchants of the other cities); **the Garden of Tombs**, a riverside necropolis for those with no house-tomb, with a keeper and the restless dead |

**Between the quarters:** courtyard homes (every family has a house-tomb, a shrine niche and a roof to sleep on in summer), little gardens and date palms in every courtyard, three bridges over the canals, stairs down to the water, rooftop walkways, and shaded alleys. **Outside the walls:** date orchards, the barley fields and canals withering in the drought (Tommy's countryside stays), the river, the dunes, the sea.

## 3. What makes it alive (hooks into the systems)

- **The lagoon is a clock:** fishing boats go out at dawn and return at dusk; ships arrive and leave on the kernel's schedule; the lighthouse beam turns at night.
- **The Moon Pool follows the moon:** crowds gather on the eššešu days (1, 7, 15); the offering of fish and sea gems happens there; its water level rises and falls with the season.
- **Rooftops are a second city:** people sleep there in summer, and astrologers, lovers, thieves and gossips meet there.
- **The Crescent Market reflects the drought:** its stalls empty and fill with the caravan and ship supply (Stage C); fewer awnings go up as the drought deepens.
- **Every door belongs to someone** (Stage S1): a knock at any house gets a real answer from a real resident.

## 4. How it gets built (so the environment and the simulation never disagree)

1. **The layout is data.** A new `db/canon/city_districts.csv` holds the 9 quarters: id, name, centre, radius, character. `places.csv` grows to every building: district, kind, owner household, and footprint (x, y, yaw, size). The kernel reads the owners and kinds; the engine reads the geometry. They share one source.
2. **`SimEnvironment` builds the crescent from that data.** It keeps Tommy's terrain, water, sky, ziggurat, lighthouse and Moon Gate builders. `BuildCity`'s grid is replaced by placement from `places.csv`, with the crescent wall and lagoon shoreline as spline curves.
3. **`SimStreetBuilder` is generalised** from one street to all places: doors, interiors and registry for every building.
4. Tommy owns the look (the D-023 style, the materials, the lighting). The coordinator owns the data, the layout code, and the simulation. The change is agreed with Tommy before `BuildCity` is replaced.
