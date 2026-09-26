# Free / permissive 3D + texture asset sources with automatable retrieval (for a low-poly stylized Bronze-Age Mesopotamia UE 5.8 game, headless Linux)

Research date: 2026-09-26. Items marked "(live probe)" were verified by calling the endpoint with curl on that date; the counts are that day's values.

## Poly Haven: public API, relevant categories, resolutions, formats, terms

### Takeaway
Poly Haven has the cleanest automation story: no key and no auth, a JSON API that returns direct CDN file URLs with md5 checksums, and all assets are CC0. The ToS adds only a unique User-Agent/Referer requirement, plus a credit line if you surface the live API inside your own product. It is photoreal, though. For a stylized look it is best used for ground, plaster, mudbrick and sandstone *textures* (downsampled or stylized) and HDRIs, not for models.

### Cited Findings
- Endpoints (live probe): `GET https://api.polyhaven.com/types` → `["hdris","textures","models"]`; `GET /categories/{type}` → category→count map; `GET /assets?type={hdris|textures|models|all}&categories=a,b` → dict of asset_id → metadata (name, tags, categories, authors, dimensions, max_resolution, files_hash, description…); `GET /files/{asset_id}` → nested file tree with a `url`, `size` and `md5` per file. Docs are OpenAPI/Redoc at `https://api.polyhaven.com/api-docs/swagger.json`. — [Poly Haven Public-API README](https://github.com/Poly-Haven/Public-API/blob/master/README.md); live probe of https://api.polyhaven.com
- Totals on 2026-09-26 (live probe): 2,380 assets in total. Textures total 862, including plaster-concrete 133, wood 137, terrain 130, rock 124, brick 107, sand 60, plaster 32, roofing 23, sandstone 11, bark 24, collection: namaqualand 5. Models total 521, including plants 57, rocks 37, containers 68, vases 11, buildings 13, structures 26, trees 20, rigged 15, creature 1. — https://api.polyhaven.com/categories/textures, /categories/models (live probe)
- Near-East-relevant asset IDs that exist (live probe of `/assets?type=all`): `clay_plaster`, `patterned_clay_plaster`, `patterned_clay_wall`, `clay_block_wall`, `clay_floor_001`, `mud_cracked_dry_03`, `mud_cracked_dry_riverbed_002`, `dry_mud_field_001`, `dry_cracked_lake`, `brown_mud_dry`, `dry_ground_01`, `dry_riverbed_rock`, `red_sand`, `sand_01..03`, `cracked_red_ground`, `reed_roof_03`, `reed_roof_04`, `thatch_roof_angled`, `palm_bark`, `palm_tree_bark`, `ceramic_pot`, `planter_pot_clay`, `straw_rolls_field_01`, plus the sandstone bricks `sandstone_brick_wall_01`, `large_sandstone_blocks_01`, `white_sandstone_bricks` and others. Plant models are mostly South-African Namaqualand succulents and shrubs (for example `quiver_tree_01`, `searsia_lucida`, `wild_rooibos_bush`), with no date palm. — live probe https://api.polyhaven.com/assets?type=all
- Texture file tree (live probe, `/files/beige_wall_001`): map keys are `Diffuse, nor_dx, nor_gl, AO, Rough, Displacement, arm, blend, gltf, mtlx`. Each map comes at `16k, 8k, 4k, 2k, 1k`, and each resolution in `jpg`, `png` and `exr`. Example URL: `https://dl.polyhaven.org/file/ph-assets/Textures/jpg/1k/beige_wall_001/beige_wall_001_diff_1k.jpg`. `nor_dx` is the DirectX-convention normal that UE wants, and `arm` is a packed AO/Rough/Metal texture that maps directly onto UE ORM-style packing. — live probe https://api.polyhaven.com/files/beige_wall_001
- Model file tree (live probe, `/files/anthurium_botany_01`): keys include `gltf, fbx, usd, blend` plus texture maps, at `8k, 4k, 2k, 1k`. The `gltf` entry has an `include` dict listing the .bin and texture dependencies, each with its own URL and md5, so a script must fetch the includes too. — live probe
- HTTP: Cloudflare-cached JSON with `cache-control: max-age=43200` and `access-control-allow-origin: *`. The response carries a `terms-of-service:` header pointing to the ToS. No rate-limit headers were observed. — live probe headers
- ToS: "The API is free to access and use by anyone … including commercial use". "all API calls must be made with a unique 'Referer' header or user-agent that matches your software name". "CC0 assets carry no attribution requirement whatsoever … [but] if you use the live API inside your software … to surface Poly Haven content, you must make it clear to your users where that content comes from". Disrupting or degrading the API (DoS or spamming) is prohibited. Optional paid "Bulk Snapshot" arrangements exist for heavy production users. — [Poly Haven API ToS](https://github.com/Poly-Haven/Public-API/blob/master/ToS.md)

### Inferences
- An offline build-time downloader is not "surfacing content inside your software", so the credit clause should not apply to the shipped game. Set a UA such as `schizo-game-asset-sync/1.0`. The CC0 files can live in a public or private Git LFS repo with no restrictions.
- For a Valheim-like look, pull the 1k/2k JPG `Diffuse`, `nor_dx` and `arm` maps and posterize or palette-reduce them. Skip the 8k/16k tiers, which are huge. Verify the md5 from `/files` for idempotent syncs.

### Gaps
- No published numeric rate limit. The API also has no date palm, reed-bundle or mudbrick-specific models.

## ambientCG: API v2/v3 endpoints, filtering, resolutions, license, usage norms

### Takeaway
ambientCG is CC0 and has a keyless GET-only JSON API. v3 is current and v2 is frozen. Each asset has per-resolution ZIP downloads (1K–8K, JPG or PNG). It holds about 2,011 materials, 429 HDRIs and only 34 food-type 3D models, so it is a texture source and not a model source.

### Cited Findings
- v3 endpoints: `/api/v3/assets`, `/api/v3/categories`, `/api/v3/collections`, `/api/v3/rss`. All are GET. — [ambientCG API v3 docs](https://docs.ambientcg.com/api/v3/)
- `/api/v3/assets` params: `q` (keywords, AND), `type` (material, hdri, substance, decal, atlas, 3d-model, plain-image, brush, terrain, hdri-element; comma = OR), `technique` (e.g. surface-photogrammetry, surface-fully-procedural…), `sort` (popular, latest, downloads, oldest, random, alphabet), `id` (comma list), `date` (YYYY-MM-DD), `limit` 1–500 (default 100), `offset`, and `include` (type, releaseDate, … tags, colors, dimensions, maps, technique, downloads, relations, collections, previews, thumbnails). The site's own `ambientCG.com/list?...` params can be copied directly. — [ambientCG /api/v3/assets docs](https://docs.ambientcg.com/api/v3/assets/)
- Download entries (live probe, `?q=mud&type=material&include=downloads,tags,maps`) look like `{"attributes":"1K-JPG","extension":"zip","url":"https://ambientcg.com/get?file=Ground109_1K-JPG.zip","size":9101131}`. Variants are 1K/2K/4K/8K × JPG/PNG. Ground109 sizes were 1K-JPG 9.1 MB, 2K-JPG 31.8 MB, 8K-PNG 1.14 GB. The maps listed were color, displacement, normal, roughness, ambient-occlusion. — live probe
- v2 "does not receive updates anymore. New projects should use v3". v2 `/full_json` and `/downloads_csv` still respond. `downloadLink` logs statistics then redirects to `rawLink`, and the docs encourage using the downloadLink. — [ambientCG API v2 docs](https://docs.ambientcg.com/api/v2/)
- Counts on 2026-09-26 (live probe, `totalResults`): type=material 2011, hdri 429, 3d-model 34 (apples, breads, a tree stump and a stick). Keyword hits within materials: wood 294, ground 168, sand 83, rock 76, plaster 35, brick 31, clay 23, bark 15, straw 4, reed 0, thatch 0. — live probe
- Usage norms: the operator warns that the site is "operated and filled with content by just one person … built with hobbyists and educators in mind and the API is therefore potentially not as reliable or stable as needed for an 'enterprise-level' application." No numeric rate limit is published. — [ambientCG About the API](https://docs.ambientcg.com/api/)
- License: "All ambientCG assets are provided under the Creative Commons CC0 1.0 Universal License … You can include the raw files in your project, for example a video game." Credit is optional (suggested text: "Created using <asset name> from ambientCG.com, licensed under … CC0"). — [ambientCG License](https://docs.ambientcg.com/license/)

### Inferences
- Use v3 with `include=downloads,tags,maps` and pick `1K-JPG` or `2K-JPG`. Stylized games rarely need more, and it keeps LFS small. Throttle politely, for example with sequential downloads, because it is a one-person host.
- Reed and thatch surfaces are missing, so use the Poly Haven `reed_roof_*` and `thatch_*` assets or procedurally generated textures.

### Gaps
- I did not verify whether ambientCG ZIPs include both NormalGL and NormalDX. Check one ZIP before wiring the UE import (UE expects DX).

## Quaternius: packs, download automation, formats, animals/animations, and a license change (important)

### Takeaway
Quaternius is the best stylistic match for a Valheim-like low-poly look, but **the licensing changed**. quaternius.com now publishes the custom "Quaternius Asset License (QAL) v1.0", last updated 2026-08-28. It is free, commercial, and needs no attribution, but it forbids redistributing the assets as assets. The individual pack pages still say "License CC0", so the site contradicts itself. There is no API. Downloads are per-pack Google Drive folders (older packs) or itch.io pages (newer MegaKits).

### Cited Findings
- QAL v1.0 (updated 8/28/2026): "You can use these assets, free of charge, in personal, educational, and commercial games … with no credit required. You just can't resell or redistribute the assets themselves as assets."
  - The grant includes "Authorize contractors and collaborators to do the above on your behalf, solely for the purpose of working on your Product."
  - Restriction 3(a): you may not "redistribute the Assets (in original or modified form) as a standalone asset, asset pack … whether for free or for payment … regardless of how much the Assets have been modified."
  - Section 7: "Changes will not apply retroactively to Assets you've already obtained under an earlier version; the version in effect at the time you obtained the Assets governs."
  - [Quaternius License](https://quaternius.com/license.html)
- The pack pages still display "License CC0". For example, the Ultimate Animated Animal Pack shows "Models 12 Animated Textured Formats FBX OBJ Blend glTF License CC0", and the Stylized Nature MegaKit and Medieval Village MegaKit pages show "CC0". — [Ultimate Animated Animal Pack](https://quaternius.com/packs/ultimateanimatedanimals.html); [Stylized Nature MegaKit](https://quaternius.com/packs/stylizednaturemegakit.html); [Medieval Village MegaKit](https://quaternius.com/packs/medievalvillagemegakit.html). This **conflicts** with the license page above.
- Download mechanism per pack (live probe of the pack HTML):
  - Google Drive shared folders: Ultimate Nature `drive.google.com/drive/folders/1-Kl0L_Jg8awbh0S5T-z3zxh4mVlnxTpa`, Farm Animal `…/1is1ax2V0mIEDS7BbbzBV_x2Pabya1dVJ`, Ultimate Animated Animals `…/1uJ3N5HfB7jKTseJUNQr3N4YaN0UuEtHk`, Ultimate Crops `…/1uhbi-NWp7pwqOGtraBZurbphyxAvoABZ`, Survival `…/1NKfC95GMWWJquy6rRzwFVkVDoZ_QP7K_`, Animated Fish `…/1SvlOveJJjmhSn-FgCRyojc1T5QHjjGkF`, Ultimate Stylized Nature `…/1IV3bXHzkNvuNWFHPi4KPx-G4ghuxIuT-`.
  - itch.io pages: `quaternius.itch.io/stylized-nature-megakit`, `/medieval-village-megakit`, `/fantasy-props-megakit`, `/universal-animation-library`, `/universal-base-characters`.
  - Source: live probe of https://quaternius.com/packs/*.html
- The pack index includes buildings, farmbuildings, medievalvillage, medievalvillagemegakit, modularmedievalbuildings, fantasypropsmegakit, ultimatemodularruins, survival, ultimatecrops, ultimatenature, ultimatestylizednature, stylizednaturemegakit, texturedfantasynature, simplenature, farmanimal, ultimateanimatedanimals, animatedfish, universalanimationlibrary (1 and 2), universalbasecharacters, modularcharacteroutfitsfantasy and others. — [quaternius.com](https://quaternius.com/)
- Ultimate Animated Animals: 12 animated models "with more than 12 unique animations (Attack, Death, Kicks, Gallops, Walk, Jump and many more!)", in FBX, OBJ, glTF and Blend. — [pack page](https://quaternius.com/packs/ultimateanimatedanimals.html). The species listed by a search snippet (secondary, via the Poly Pizza bundle) are Donkey, Deer, Alpaca, Bull, Fox, Shiba Inu, Stag, Husky, Wolf, White Horse and Horse. — [Poly Pizza Animated Animal Pack bundle](https://poly.pizza/bundle/Animated-Animal-Pack-ILAPXeUYiS)
- Farm Animal Pack: 7 animated models in FBX, OBJ and Blend (no glTF listed). — [Farm Animal pack](https://quaternius.com/packs/farmanimal.html)
- Ultimate Nature Pack: 150 models in FBX, OBJ and Blend. — [Ultimate Nature](https://quaternius.com/packs/ultimatenature.html). Stylized Nature MegaKit: "110+ unique nature models", FBX, OBJ, Blend and glTF. — [pack page](https://quaternius.com/packs/stylizednaturemegakit.html). Medieval Village MegaKit: "over 300 modular environment pieces, all designed to snap perfectly to a grid". — [pack page](https://quaternius.com/packs/medievalvillagemegakit.html). Universal Animation Library: "120+ animations, created using a universal humanoid rig, which is compatible with Unreal Engine", in FBX, GLB and Blend. — [pack page](https://quaternius.com/packs/universalanimationlibrary.html)
- Quaternius palm trees appear on Poly Pizza (several "Palm Tree — Quaternius" entries). — [Poly Pizza search "palm tree"](https://poly.pizza/search/palm%20tree)

### Inferences
- Automation: `gdown --folder <url>` works on the Drive folders. gdown caps folder listings at around 50 files, so zipped packs are fine. The itch.io packs need the itch flow described in the Kenney/KayKit section. Pin checksums, because Drive links can change.
- **Repo implication under QAL:** a private GitHub/LFS repo shared only with collaborators working on the game fits the "contractors and collaborators" grant. A **public** repo containing the raw FBX/glTF would arguably be redistribution "as a standalone asset" and is best avoided. Record the download date for each pack, because Section 7 fixes the governing license version at acquisition. Anything obtained after 2026-08-28 should be treated as QAL, not CC0, despite the stale pack-page labels.
- The listed species give the donkey, bull (cattle), horses, deer/stag and dog (Husky, Shiba) fits. The farm pack probably covers sheep, cow and pig, but that is unverified here. I found no confirmed lion, goat, camel or onager in Quaternius.

### Gaps
- The exact species list of the Farm Animal pack and the exact animation clip names per animal could not be extracted, because the pages render them as images. I could not tell whether the Ultimate Nature or Stylized Nature MegaKit packs contain reeds or cattails; palms are confirmed only via Poly Pizza listings.
- It is unclear whether QAL applies retroactively to packs whose own page still says CC0. The license's Section 7 implies acquisition-date governance, but this has not been clarified with the author.

## Kenney (CC0): relevant packs and bulk download

### Takeaway
Kenney packs are all CC0, each downloadable as a single ZIP from kenney.nl with no login. The ZIP URL embeds a hash, so scrape it from the asset page. Each pack ships GLB, FBX, OBJ, DAE and STL, which makes Kenney the easiest source to script. The style is very clean and blocky "toy" low-poly, simpler than Valheim.

### Cited Findings
- Direct ZIP links scraped from the asset pages (live probe): nature-kit `https://kenney.nl/media/pages/assets/nature-kit/37ac38a37b-1677698939/kenney_nature-kit.zip` (330 files), survival-kit (80 files), castle-kit (75), fantasy-town-kit 2.0 (160), cube-pets 1.0 (24), mini-market (20). Every page states "Creative Commons CC0". — live probe of https://kenney.nl/assets/nature-kit and the other pack pages
- The Nature Kit 2.1 ZIP (10.5 MB, live download) contains 329 models each as `.glb`, `.fbx`, `.obj`, `.dae` and `.stl`, plus preview PNGs. The included License.txt says "License: (Creative Commons Zero, CC0) … free to use in personal, educational and commercial projects … crediting Kenney … (this is not mandatory)". The models include `tree_palm`, `tree_palmBend`, `tree_palmTall`, `tree_palmShort`, `tree_palmDetailedTall`/`Short`, `cactus_short`/`tall`, `crops_wheatStageA/B`, `crops_bambooStageA/B` (usable as reed stand-ins), `pot_large`/`small`, `tent_*`, `fence_*`, and many `cliff_*_rock`/`_stone` pieces. — live download of kenney_nature-kit.zip
- The 3D category spans 4 pages, including Fantasy Town Kit, Graveyard Kit, Pirate Kit, Mini Forest and Cube Pets. — [Kenney 3D assets](https://kenney.nl/assets/category:3D)
- Asset pages carry a "Get All-in-1" link. That is Kenney's bundle of all packs. — live probe of the pack pages

### Inferences
- Scripted approach: fetch `https://kenney.nl/assets/<slug>`, regex `https://kenney.nl/media/pages/assets/[^"]+\.zip`, download and unzip, then keep only `Models/GLTF format/*.glb` or the FBX folder. CC0 means public or private repos are both fine.
- Best Mesopotamian-useful Kenney packs are Nature Kit (palms, cactus, rocks, crops), Survival Kit, Castle Kit and Fantasy Town Kit for kitbash walls. Mudbrick would need retexturing.

### Gaps
- `kenney.nl/sitemap.xml` returned no asset URLs, so a full slug list needs scraping the paginated category pages. I did not verify the All-in-1 price or bundle format.

## KayKit (Kay Lousberg, CC0): relevant packs and download automation

### Takeaway
The core KayKit packs are CC0 and "Name your own price" (i.e. $0 allowed) on itch.io. Several packs are also mirrored on GitHub under the `KayKit-Game-Assets` org, which is the easiest route for automation (`git clone`). The style is a stylized low-poly gradient-atlas look, a good fit.

### Cited Findings
- Free packs listed on itch: Character Pack: Adventurers, Character Pack: Skeletons, Character Animations, Medieval Hexagon Pack, Forest Nature Pack, Platformer Pack, Dungeon Pack, Resource Bits and Block Bits. The paid items are Mystery Monthly Series 4–6 ($19.99 each), Bits Bundles 1/2 ($19.95) and The Complete KayKit ($150). — [kaylousberg.itch.io](https://kaylousberg.itch.io/)
- The Medieval Hexagon Pack has "over 200 stylised medieval he[xagon tiles]". Its description says "Free for personal and commercial use, no attribution required. (CC0 Licensed)", with files in FBX, GLTF and OBJ, and adds "please don't resell unmodified copies". The itch metadata says "Asset license Creative Commons Zero v1.0 Universal" and "No generative AI was used". — [KayKit Medieval Hexagon Pack](https://kaylousberg.itch.io/kaykit-medieval-hexagon)
- The Character Animations pack has animation sets per rig type (Rig_Medium, Rig_Large): general (idle, hit, death, interact), movement (walk, run, jump, crawl, sneak, dodge) and more. It works on other humanoids via retargeting. — [KayKit Character Animations](https://kaylousberg.itch.io/kaykit-character-animations)
- GitHub mirrors (GitHub search API, live) under `KayKit-Game-Assets`: KayKit-Character-Pack-Adventures-1.0, -Character-Pack-Skeletons-1.0, -Medieval-Hexagon-Pack-1.0, -Dungeon-Remastered-1.0, -City-Builder-Bits-1.0, -Restaurant-Bits-1.0, -Furniture-Bits-1.0, -Prototype-Bits-1.0, -Halloween-Bits-1.0, -Space-Base-Bits-1.0. The repos contain `LICENSE.txt` and an `addons/` folder (Godot-addon layout). — GitHub search, live probe
- The Adventurers repo LICENSE.txt says "License: (Creative Commons Zero, CC0) … free to use in personal, educational and commercial projects … crediting Kay Lousberg … (this is not mandatory)". The README lists 4 rigged and animated characters (+2 in the EXTRA tier), 75 animations, 25+ accessories, a single 1024² gradient atlas, and FBX/GLTF files. — [KayKit Adventurers repo](https://github.com/KayKit-Game-Assets/KayKit-Character-Pack-Adventures-1.0)

### Inferences
- Use `git clone --depth 1` for the GitHub-mirrored packs. The Forest Nature Pack and Character Animations are not in the GitHub list, so they need itch automation.
- KayKit has no desert, palm or animal pack among the free items. Its value here is humanoid characters and animations, crates, barrels and resource bits, and hex terrain.

### Gaps
- I did not confirm the Forest Nature Pack contents (palms or reeds?).
- **itch.io automation:** I found no official API for downloading free or name-your-price packs. Community tools exist, such as [itchio-downloader](https://github.com/Wal33D/itchio-downloader), which scrapes CSRF → download page → CDN URL, and the Rust [itch-downloader](https://crates.io/crates/itch-downloader), which needs an itch API key. Both are unofficial and fragile. The most robust option is a one-time manual download, then committing the ZIPs to LFS.

## Poly Pizza: API, license mix, search, attribution fields

### Takeaway
Poly Pizza hosts about 10k+ low-poly models, including the whole archived Google Poly library, Quaternius and others. The license mix is CC0 and CC-BY. The API needs a free account key sent as `X-Auth-Token`. It is very low-poly-friendly and one of the few places with date palms, reed boats, chariots and amphorae in low-poly form, but most "Poly by Google" items are CC-BY and need attribution.

### Cited Findings
- `https://api.poly.pizza/v1.1/search/palm` without a key returns HTTP 401 `{"error":"You need an API key to do that dingus"}`. `https://poly.pizza/settings/api` redirects to login. — live probe
- Per a third-party skill doc (secondary), the base is `https://api.poly.pizza/v1/` with header `X-Auth-Token: <key>`. Endpoints are `/search/{query}`, `/popular` and `/model/{id}`. Parameters are `limit` (default 12, max about 50), `cursor`, `format` (glb/fbx/obj), `animated` and `tricount` (e.g. "0-5000"). Response fields are ID, Title, Description, Creator (Username), Thumbnail, Download, DownloadFBX, Licence, Tags, TriangleCount, Animated and PublishedAt. HTTP 429 means rate-limited. The attribution format is `"[Title]" by [Username] (poly.pizza URL) CC-BY 4.0`. — [tiny-world-builder poly-pizza-api SKILL.md](https://github.com/jasonkneen/tiny-world-builder/blob/main/.agents/skills/poly-pizza-api/SKILL.md). The official page [poly.pizza/docs/api/v1.1](https://poly.pizza/docs/api/v1.1) is JS-rendered and I could not read it to confirm.
- The public HTML search works without a key (live probe). "date palm", "donkey", "goat", "sheep", "amphora", "clay pot", "mud hut", "reed boat", "chariot" and "cart" each returned a full first page of about 24–31 models. "camel" returned 10, "lion" 5, "ziggurat" 3 and "reed" 2. Search is fuzzy, so hits are not exact matches. Model pages show the license. For example, a Google "Palm tree" page reads "Poly by Google … OBJ /GLTF format • Creative Commons Attribution". — live probe https://poly.pizza/search/…
- The palm-tree search shows "Date palm tree — Poly by Google" and multiple "Palm Tree — Quaternius" results, plus paid promos (Synty etc.) mixed in. — [Poly Pizza search](https://poly.pizza/search/palm%20tree)

### Inferences
- Filter strictly on `Licence` in the API response. Store `{id, title, creator, licence, url}` in a manifest and auto-generate a CREDITS file for CC-BY. Both CC0 and CC-BY allow storage in a private or public repo, as long as attribution is kept.
- The Google Poly pieces vary a lot in style. Expect to re-material them to your palette.

### Gaps
- Official rate limits and ToS for the API are unconfirmed. The docs page did not render, and the limits above come from a secondary source.

## Sketchfab Data API v3: licensed search, download flow, restrictions

### Takeaway
Search is public and unauthenticated, and it filters by `downloadable=true&license=cc0|by`. The download itself requires an authenticated user (OAuth2 or API token), and the returned archive URLs expire after about 300 s. Sketchfab's developer guidelines require showing the license and author attribution, which "must follow the asset". Style fit is poor on average (mostly high-poly scans). Its main value here is CC0 museum scans of real Mesopotamian artifacts (reference or hero props) and a few CC-BY low-poly ziggurats and palms.

### Cited Findings
- Download: `GET /v3/models/{UID}/download` with an Authorization header. It returns JSON `{"gltf":{"url","size","expires":300},"usdz":{…}}`. The URL "already contains a token that has a short expiration", should not be cached, and needs no further auth. Formats are glTF archive, GLB and USDZ. Downloading requires an authenticated Sketchfab account. — [Sketchfab Downloading models](https://sketchfab.com/developers/download-api/downloading-models); [Castle Game Engine write-up](https://castle-engine.io/wp/2023/05/26/using-sketchfab-api-to-search-and-download-gltf-to-be-integrated-in-castle-game-engine/)
- Search (live probe, no auth): `https://api.sketchfab.com/v3/search?type=models&q=…&downloadable=true&license=cc0&count=24` returns `results[]` with uid, name, license.label, faceCount, isDownloadable and user.username, plus a `next` cursor. `GET /v3/licenses` slugs: by, by-sa, by-nd, by-nc, by-nc-sa, by-nc-nd, cc0, free-st ("Free Standard"), st ("Standard"), ed ("Editorial"). — live probe
- Guidelines: third-party apps using the Download API "must clearly display the Creative Commons license … and must provide attribution to the original creator", and attribution "must follow the asset everywhere it is used". — [Sketchfab Download API Guidelines](https://sketchfab.com/developers/download-api/guidelines) (via search snippet; the page is JS-rendered and I could not read it in full)
- Mesopotamian probes (live, downloadable):
  - `mesopotamia` + cc0: 7 hits, all museum scans, e.g. "Mesopotamian Cylinder Seal" (artsmia, 65k faces), "Cuneiform" (mkghamburg, 4M faces). `sumerian` + cc0: "Statue of Gudea" (clevelandart). `date palm` + cc0: "Assyrian Winged Genius, c. 883-859 BCE" (artsmia) and a Cleveland Assyrian relief.
  - `ziggurat` + by: 24+, e.g. "Ziggurat — a tower in Mesopotamia" (1,640 faces), "Ziggurat" (602 faces). `date palm` + by: "Date Palm" (evolveduk, 10k faces). `mud brick` + by: 9, including "Stylized Adobe Dwelling – Tier I". `lamassu` + by: 15 (danielpett British Museum scans).
  - `reed boat`: 0 in both licenses. `ziggurat` + cc0: 0.
  - Source: live probe of the api.sketchfab.com/v3/search results
- CC-BY results include obvious third-party IP, e.g. "Donkey (Pocket Shrek)". — live probe

### Inferences
- A CC-BY label on Sketchfab does not guarantee the uploader had the rights, so exclude fan art and trademarked characters. Museum-institution CC0 uploads (artsmia, clevelandart, WirtualneMuzeaMalopolski, TheHuntMuseum) are the trustworthy CC0 subset. They need decimation or stylization first, since they run 65k–4M faces.
- Headless automation needs a personal API token (Authorization: `Token <key>`, per Sketchfab's older token method) or OAuth2. Treat it as a curated, per-model pull rather than bulk scraping.

### Gaps
- I could not read the full text of the Sketchfab Terms of Use / developer terms on automated bulk download or rate limits (JS-rendered). The exact token header format also comes from memory rather than a fetched doc, so verify it.

## OpenGameArt: license mix, API/scraping feasibility

### Takeaway
OpenGameArt allows CC0, CC-BY, CC-BY-SA, GPL and OGA-BY. Everything must be filtered by license per item. There is no documented API, and the FAQ says nothing about scraping. It is a low-priority source for 3D in this style.

### Cited Findings
- Allowed licenses: CC0, CC-BY 3.0/4.0, CC-BY-SA 3.0/4.0, GPL 2.0/3.0, OGA-BY 3.0/4.0. OGA-BY is derived from CC-BY but "removes the restriction against technical measures that prevent redistribution (eg. DRM)". — [OpenGameArt FAQ](https://opengameart.org/content/faq)
- The FAQ does not address an API, scraping or bulk download. — [OpenGameArt FAQ](https://opengameart.org/content/faq)

### Inferences
- Avoid CC-BY-SA and GPL for game assets: share-alike and copyleft create obligations on derivatives. Only CC0, CC-BY and OGA-BY are safe. Any scraper would be an HTML scraper of Drupal pages.

### Gaps
- No verified API or rate-limit policy.

## Fab / Quixel Megascans: 2025–2026 licensing, automation, repo limits, style fit

### Takeaway
Free-for-all Megascans ended on 31 Dec 2024. Since 2025 most Megascans are paid per asset (from $0.99), though some remain free, and assets already acquired stay usable forever. Everything is under the Fab Standard License, which allows any engine and sharing through private repos with collaborators, but not public redistribution. Fab has no public download API: fab.com returned a Cloudflare challenge or 403 to scripted requests. The photoreal scans are a poor fit for low-poly stylized art.

### Cited Findings
- "The assets will remain free until the end of 2024, after which Epic will begin charging for most of the content." "When you acquire Quixel content on Fab – whether free or paid – you can use it forever." 2025 prices: individual 2D/3D assets from $0.99, procedural kits $4.99, packs $24.99. The Fab Standard license permits use "in all engines and tools". — [CG Channel, Oct 2024](https://www.cgchannel.com/2024/10/epic-games-has-made-megascans-free-to-all-but-only-until-the-end-of-2024/)
- "the majority of Megascans will no longer be available for free unlimited use in Unreal Engine projects", with some content remaining free. Legacy Megascans claimed via Bridge/Quixel.com between 22 Nov and 31 Dec 2024 got upgraded versions in My Library on Fab in January 2025. — [Fab Transition FAQs](https://support.fab.com/s/article/Fab-Transition-FAQs?language=en_US) (via search snippet; direct fetch failed on a TLS certificate error)
- Megaplants (April 2026 article) are "free to use under the Fab Standard License". — [Quixel news: Quixel on Fab](https://quixel.com/news/quixel-on-fab-new-megascans-and-megaplants)
- The Standard license has Personal (buyer ≤ $100k gross revenue) and Professional tiers with the same scope of rights. Fab also offers CC-BY as a free-listing license option. — [Fab docs: Licenses and Pricing](https://dev.epicgames.com/documentation/fab/licenses-and-pricing-in-fab)
- The Fab Standard License lets you share assets "via a private repository with your collaborators who are working on the project with you". — [Fab Standard License](https://www.fab.com/eula) (search snippet; direct fetch returned 403)
- A scripted request to fab.com listings returned a Cloudflare challenge page. — live probe

### Inferences
- **Repo rule:** Fab Standard assets are OK in a *private* GitHub+LFS repo limited to project collaborators, and NOT OK in a public repo. The CC-BY listings on Fab carry the normal CC-BY terms.
- Automation is effectively manual: acquire in the Fab web UI or the UE Fab plugin, then commit. For a headless Linux pipeline, treat Fab as an optional manual-import source.

### Gaps
- I could not read the full Fab EULA text (403) for exact clauses on public repos, AI training, or collaborator definitions. I also could not find a current list of which Megascans remain free in 2026.

## Other strong CC0 sources for stylized/low-poly (verified)

### Takeaway
Beyond the sources above, the verified extras are 3dtextures.me (CC0 textures) and museum CC0 scans on Sketchfab. I found no verifiable free CC0 "Polygon/Synty-style" set; Synty is paid and only appears as ads on Poly Pizza.

### Cited Findings
- 3dtextures.me: "All textures on this site are licensed as CC0." — [3dtextures.me About](https://3dtextures.me/about/)
- Synty "POLYGON" packs appear only as paid promos inside Poly Pizza search (e.g. "POLYGON Western Frontier Synty Studios $49"). — [Poly Pizza search](https://poly.pizza/search/palm%20tree)
- `sharetextures.com/license` and `cgbookcase.com/license` returned HTTP 404 on 2026-09-26, so their license pages were not verified. — live probe

### Inferences
- The priority order for this game's look is: Quaternius (with the QAL caveat) > KayKit > Kenney > Poly Pizza CC0 for models, and Poly Haven + ambientCG for surface textures and HDRIs, stylized down.

### Gaps
- I did not verify Smithsonian Open Access 3D (CC0, api.si.edu), the Blender Studio asset licenses, the Godot Asset Library, "Zsky" or "Pixel Frog" (Pixel Frog is 2D pixel art, so likely irrelevant). All are excluded here for lack of verification.

## Coverage of Mesopotamian / Near-Eastern specific items

### Takeaway
No single free source covers Bronze-Age Mesopotamia. Textures for mudbrick, clay plaster, cracked dry mud, reed roofs and sandstone are well covered, CC0, by Poly Haven and ambientCG. Low-poly palms, pots and generic animals come from Kenney, Quaternius and Poly Pizza. Ziggurats, reed boats, onagers, lions and oxcarts are thin or absent as permissive low-poly models and will need custom or procedural modelling.

### Cited Findings
- Mudbrick, plaster and ground textures (CC0): Poly Haven `clay_block_wall`, `clay_plaster`, `patterned_clay_wall`, `mud_cracked_dry_03`, `dry_mud_field_001`, `reed_roof_03/04`, `thatch_roof_angled`, `palm_bark`. ambientCG has 23 "clay", 35 "plaster" and 83 "sand" materials, but 0 reed or thatch. — live probes (see sections above)
- Date palms: Kenney `tree_palm*` (6 variants, CC0), Quaternius palm trees (via Poly Pizza), Poly Pizza "Date palm tree" by Google (CC-BY), Sketchfab "Date Palm" (CC-BY, 10k faces). — live probes
- Pottery and amphorae: Poly Haven `ceramic_pot` (CC0 scan), Kenney `pot_large`/`pot_small` (CC0), Poly Pizza amphora/clay pot results (mixed CC0/CC-BY), Sketchfab CC0 museum amphorae (high-poly). — live probes
- Ziggurat: Poly Pizza (3 hits) and Sketchfab CC-BY (24+, e.g. 602–1,640 faces); no CC0 ziggurat found. — live probes
- Reed boat: Sketchfab 0 hits. Poly Pizza returned fuzzy "boat" results only. — live probes
- Animals: Quaternius donkey, bull, horses, deer, dogs (animated). Poly Pizza donkey/goat/sheep hits, camel 10 and lion 5 (licenses mixed). No onager-specific model was found anywhere; a donkey is the closest proxy. — sources above
- Real-artifact references (CC0 museum scans on Sketchfab): cylinder seal, Statue of Gudea, cuneiform tablet, Assyrian reliefs. — live probe

### Inferences
- A practical pipeline: CC0 textures from the Poly Haven/ambientCG APIs, CC0 kit models from Kenney/KayKit (scripted), Quaternius packs downloaded once to a private LFS repo (QAL-safe), and curated CC-BY pulls from Poly Pizza/Sketchfab with an auto-generated credits manifest. Build ziggurats, mudbrick house modules, reed huts, reed boats and oxcarts in-house (procedural/Blender), textured with the CC0 mud and reed materials.

### Gaps
- I found no permissive source for onager, Mesopotamian lion or wild-ass animations, and no low-poly Bronze-Age oxcart or reed-bundle (mudhif) house. These are likely custom work.
