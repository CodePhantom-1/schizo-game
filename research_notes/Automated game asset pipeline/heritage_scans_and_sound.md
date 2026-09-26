# Open heritage 3D scans, reference imagery and free sound for a Bronze-Age Mesopotamia game (as of 2026-09-26)

Method note: most API claims below were checked with live, keyless `curl` calls on 2026-09-26 (marked **[live-probed]**). The object counts, UIDs and face counts are results from those calls. Where a claim comes only from docs or search snippets, the note says so.

License rule used throughout:
- **OK**: CC0, public domain, CC BY. CC BY needs attribution in the credits.
- **EXCLUDED**: any NC (NonCommercial) or ND (NoDerivs) license, and "research only" or "permission required" terms.
- **AVOID for shipped assets**: CC BY-SA (ShareAlike). A texture derived from an SA photo arguably has to be released under SA too. Use these as reference only.

---

## Q1. Smithsonian Open Access (CC0): API, 3D, and Mesopotamian holdings

### Takeaway
The API works and the 3D side needs no key. It is effectively useless for Mesopotamia: 3D searches for Mesopotamia, Sumer, Babylon, Uruk, Iraq-era objects, tablet and lyre returned nothing relevant. The 3D corpus is mostly natural history and space science. Use the Smithsonian for pipeline tooling and pattern reference only, not for content.

### Cited Findings
- Open Access covers millions of 2D and 3D items, released for reuse "without asking". The API is hosted via api.data.gov, and there is also a GitHub data repository — [SI Open Access](https://www.si.edu/openaccess); [SI Open Access FAQ](https://www.si.edu/openaccess/faq)
- 3D items come as glTF, GLB, OBJ (150k and full-res versions) and Voyager scenes — [CG Channel](https://www.cgchannel.com/2020/03/get-2000-free-3d-models-from-the-smithsonian-collection/); [3d.si.edu open source](https://3d.si.edu/open-source-resources)
- **[live-probed]** EDAN search endpoint: `GET https://api.si.edu/openaccess/api/v1.0/search?q=...&rows=N&api_key=KEY`. `DEMO_KEY` worked but hit `OVER_RATE_LIMIT` (HTTP 429) after about 12 calls, so get a free api.data.gov key for real use — [api.si.edu](https://api.si.edu/openaccess/api/v1.0/search)
- **[live-probed]** The working 3D filter is `q=online_media_type:"3D Models"`, which returned 1,937 records. `online_media_type:"3D Images"` returned 0. `unit_code:FSG` (Freer|Sackler, now the National Museum of Asian Art) combined with Mesopotamia returned 0 — [api.si.edu](https://api.si.edu/openaccess/api/v1.0/search)
- **[live-probed]** The keyless 3D file API is `GET https://3d-api.si.edu/api/v1.0/content/file/search?q=TERM&file_type=glb&rows=N`. Each row gives a `uri` to a Draco-compressed GLB plus quality tags, for example `...-20k-thumb.glb` and `...-150k-4096-high.glb` (150k tris with 4096 px textures). Queries for mesopotamia, cuneiform, tablet, lyre, Sumer, Babylon, Uruk and Persian all returned 0 rows. "seal" matched pinnipeds — [3d-api.si.edu](https://3d-api.si.edu/api/v1.0/content/file/search?q=Iraq&rows=5)
- **[live-probed]** A general (non-3D) search for "cuneiform" returned 474 records, but the top hits are Smithsonian Libraries book records, not objects — [api.si.edu](https://api.si.edu/openaccess/api/v1.0/search)
- A full bulk mirror of the Smithsonian Open Access metadata exists (Harvard LIL on Source Cooperative) — [source.coop](https://source.coop/harvard-lil/smithsonian-open-access)

### Inferences
- Smithsonian's LOD naming (20k thumb / 150k high) is a good template for this game's own pipeline output.
- If Smithsonian content is ever needed, pull the bulk metadata dump rather than paging the rate-limited API.

### Gaps
- I did not browse the National Museum of Asian Art's 2D Ancient Near East holdings (the DEMO_KEY was exhausted). A few CC0 photos of Near Eastern objects may exist.
- I could not render the EDAN docs page (JS-only), so the exact documented rate limits were not confirmed. api.data.gov usually defaults to 1,000 requests per hour for a registered key, but I did not verify that here.

---

## Q2. The Met Open Access (CC0 images): Ancient West Asian Art

### Takeaway
This is the best source of CC0 **reference and texture photography** for Sumerian and Akkadian objects. It is keyless, the department filter works, `isPublicDomain` is given per object, and original-resolution JPGs are available. Search counts include 772 cylinder seals, 301 "Sumerian" hits and Tell Asmar worshippers. I found no Met 3D models.

### Cited Findings
- **[live-probed]** The department ID is `3`, with display name **"Ancient West Asian Art"** (renamed from "Ancient Near Eastern Art"): `GET https://collectionapi.metmuseum.org/public/collection/v1/departments` — [Met API](https://collectionapi.metmuseum.org/public/collection/v1/departments)
- **[live-probed]** Search is `GET /public/collection/v1/search?departmentId=3&hasImages=true&q=TERM`, which returns `{total, objectIDs}`. Counts for dept 3 with images:

  | Query | Hits |
  |---|---|
  | cylinder seal | 772 |
  | Sumerian | 301 |
  | brick | 183 |
  | Ur | 165 |
  | weight | 72 |
  | Tell Asmar | 9 |
  | Khafajah | 8 |
  | cone mosaic | 8 |
  | game board | 6 |
  | lyre | 4 |

  — [Met API search](https://collectionapi.metmuseum.org/public/collection/v1/search?departmentId=3&hasImages=true&q=cylinder%20seal)
- **[live-probed]** Object detail is `GET /public/collection/v1/objects/{id}`. It returns `isPublicDomain` (the CC0 flag), `primaryImage` (full-res original), `additionalImages[]`, `culture`, `objectDate` and `objectURL`. For example, 323735 "Standing male worshiper", Sumerian, ca. 2900–2600 BCE, has `isPublicDomain: True`, primary image `https://images.metmuseum.org/CRDImages/an/original/DP-39988-001.jpg`, and 3 additional images — [Met object 323735](https://collectionapi.metmuseum.org/public/collection/v1/objects/323735)
- The Met's Open Access images of public-domain works are CC0 — [Met API docs](https://metmuseum.github.io/). This comes from docs I know and did not re-fetch. The docs ask clients to stay under about 80 requests per second; treat that number as unverified this session.

### Inferences
- Pipeline: search dept 3 by keyword → fetch each object → keep only `isPublicDomain==true` → download `primaryImage` plus `additionalImages` → save `objectURL` and `creditLine` for credits. This is fully automatable with no key.
- Multi-angle `additionalImages` sets (such as the 3 extra angles on the worshipper) can feed photogrammetry or image-to-3D experiments. Treat the output as a new asset rather than a scan.

### Gaps
- I did not confirm whether the Met publishes 3D scans (on Sketchfab or elsewhere). Its Sketchfab account, if any, did not show up in the CC0 or CC BY Mesopotamian searches.
- Standard of Ur, the Royal Game of Ur board and the Ur lyres are mostly held by the British Museum and Penn, not the Met (see Q3).

---

## Q3. Other museums: British Museum, Penn (Ur), Louvre, ISAC, Cleveland, Rijksmuseum

### Takeaway
Only **Cleveland (CMA)** is fully usable: a CC0 API with image links, and a `sketchfab_id` field that links objects to CC0 3D scans. The **British Museum and Penn/Ur Online are NC → EXCLUDED**. Penn's OPenn is **CC BY-SA 2.0 → avoid for shipped assets**. **Louvre and ISAC → EXCLUDED** (permission or payment required).

### Cited Findings
- **Cleveland Museum of Art: OK (CC0).**
  - **[live-probed]** `GET https://openaccess-api.clevelandart.org/api/artworks/?q=Sumerian&cc0=1&has_image=1&limit=N` returned 8 CC0 objects with images (Striding Goat, Bull Calf Polemount, Bull Procession Cup and others). `q=Mesopotamia` returned 14.
  - Each record has `share_license_status: "CC0"` and `images.{web,print,full}.url` — [CMA Open Access API](https://openaccess-api.clevelandart.org/api/artworks/?q=Sumerian&cc0=1&has_image=1&limit=3)
  - **[live-probed]** `GET /api/artworks/1963.154` (Statue of Gudea) returns `sketchfab_id: d7493b2c823f43bfa1cfc778f68d3ba4` and `sketchfab_url`. The API itself therefore bridges objects to downloadable 3D — [CMA 1963.154](https://openaccess-api.clevelandart.org/api/artworks/1963.154)
- **British Museum: EXCLUDED (NC).** Collection-online content is "mostly" CC BY-NC-SA 4.0. Commercial use requires a paid license via bmimages.com, and the Museum may count even non-profit uses as commercial — [BM images and photography](https://www.britishmuseum.org/terms-use/copyright-and-permissions/images-and-photography); [BM copyright](https://www.britishmuseum.org/terms-use/copyright-and-permissions)
- **Penn Museum / Ur Online: EXCLUDED (NC).** Ur Online (a joint BM–Penn resource for Woolley's 1922–34 Ur excavations) publishes data, text and images under CC BY-NC-SA 4.0 — [Ur Online about](http://www.ur-online.org/about/8/)
- **Penn Museum via OPenn: AVOID (BY-SA).** Penn Museum images on OPenn are CC BY-SA 2.0 — [OPenn Penn Museum](https://openn.library.upenn.edu/html/0016.html). ShareAlike is not in the allowed list, so use these as reference only.
- **Louvre: EXCLUDED.** Commercial use of images (including multimedia production) needs a written, paid request to RMN-GP. Only the text records are open (Etalab licence). The terms were updated 19 March 2026 — [Louvre collections CGU](https://collections.louvre.fr/en/page/cgu)
- **ISAC (Oriental Institute), Chicago: EXCLUDED.** Images are "solely for research purposes", and reproduction or distribution is prohibited without written permission — [ISAC photo requests](https://isac.uchicago.edu/research/photo-requests-and-permissions); [ISAC collections](https://isac-idb.uchicago.edu/)
- Note: the Art Institute of Chicago (a different institution from ISAC) is CC0, but it has little Mesopotamian material — [AIC open access](https://www.artic.edu/open-access-images)

### Inferences
- Ur-specific icons (Standard of Ur, Royal Game of Ur, the Queen's Lyre, Ram in a Thicket) cannot be sourced from their holding museums under commercial terms. The practical routes are:
  - Wikimedia Commons photos that photographers released as CC BY or CC0 (Q5). Photos of 4,500-year-old 3D objects are generally the photographer's own copyright to license, but check each file.
  - Modelling from CC0 Met or CMA analogues.
- Rijksmuseum: not researched (it holds little Mesopotamian material). This is low value.

### Gaps
- Some Penn Museum objects may also appear on Sketchfab under other licenses; I found none in the CC0/BY Ur searches.
- The British Museum's own Sketchfab account was not specifically checked. Its default licensing would make those models NC in any case.

---

## Q4. Sketchfab cultural-heritage models (CC0 / CC BY) and automated download

### Takeaway
Sketchfab is the main source of real Mesopotamian scans. The search API is **keyless** and filters by license, which makes discovery automatable. Download needs auth: `GET /v3/models/{uid}/download` returns 401 without it, and the documented method is an OAuth Bearer token. The response gives temporary glTF/USDZ links. Many heritage scans have **0.5–10M faces** and need decimation.

### Cited Findings
- **[live-probed]** Search: `GET https://api.sketchfab.com/v3/search?type=models&q=TERM&downloadable=true&license=cc0|by&count=24`.
  - License slugs from `GET /v3/licenses`: `cc0`, `by` (both OK); `by-sa` (avoid); `by-nd`, `by-nc`, `by-nc-sa`, `by-nc-nd` (EXCLUDED); plus `free-st`, `st`, `ed` (store or editorial licenses, not Creative Commons).
  - Results include `uid`, `user.username`, `faceCount` and `viewerUrl` — [Sketchfab search API](https://api.sketchfab.com/v3/search?type=models&q=mesopotamia&downloadable=true&license=cc0)
- **[live-probed]** `GET /v3/models/{uid}` returns `license.slug` and `license.requirements`. For example, CMA's Gudea gives "Credit is not mandatory. Commercial use is allowed." and `isDownloadable: True` — [Sketchfab model API](https://api.sketchfab.com/v3/models/d7493b2c823f43bfa1cfc778f68d3ba4)
- **[live-probed] CC0, downloadable, relevant:**

  | Object | Account | UID | Faces |
  |---|---|---|---|
  | Statue of Gudea (Neo-Sumerian, Lagash) | clevelandart | d7493b2c823f43bfa1cfc778f68d3ba4 | 66,586 |
  | Priest-King or Deity (1971.45) | clevelandart | 2f8f0eccc39842da81a78b3b0a142ee6 | 85k |
  | Mesopotamian Cylinder Seal | artsmia (Minneapolis Institute of Art) | bb5cd29ea64141c09ad62d6bb79fecdd | 65k |
  | Mesopotamian Cylinder Seal, c. 800–400 BCE | artsmia | f5ed8afaa1c5483ba13986878d3e49d7 | 65k |
  | Rollsiegel (cylinder seal) | mkghamburg | a5f1d11994384e36b8b9dc8e85a14ae4 | 612k |
  | Cuneiform tablet | mkghamburg | 43fd9934491847ae9945e1c106c13e0f | 4M |
  | Cuneiform tablet | mkghamburg | ab87c640d1dc4ebcb5c3d68539058905 | 4M |
  | Cuneiform fragment | mkghamburg | 32a00067e67a44eb85262735ec90a172 | 2M |
  | "Beeld van een heerser of prins" (ruler statue) | rmo_leiden | 02ba2904e94748d1bdb40a89e88ce880 | 306k |
  | Gudea plaster cast | WirtualneMuzeaMalopolski | a9735f4849a24e54a89213f56022d77a | 500k |

  The same searches also returned **later** CC0 material that fits for style only: Assyrian reliefs (artsmia Winged Genius; danielpett Lion Hunt and Lachish; mkghamburg Ashurnasirpal) — [Sketchfab search](https://api.sketchfab.com/v3/search?type=models&q=assyrian&downloadable=true&license=cc0)
- **[live-probed] CC BY (attribution), downloadable, relevant:**
  - Field Museum cylinder-seal series (fieldmuseumeducation 156630, 156636, 156668, 156705, 156612, 156616 and more; 30–220k faces)
  - varldskulturmuseerna cylinder seal (91k)
  - bentoncountymuseum cylinder seal
  - hendrikhameeuw MB.Gl.13 cylinder seal (1.15M)
  - Cuneiform tablets: hmane (2–10M), jason.herrmann tablet from Umma (2.3M), imagingcenter Old-Babylonian S264 (20k), BrynMawrCollege Old-Babylonian tablet and "Gudea clay nail" (≈220–240k)
  - "The Sumerian King List" (ingiringi)
  - "c. 4000 B.C. Sumerian Tablet" (usf-idex-access3d)
  - "Sumerian royal pendant" (zatamite)
  - Ziggurat reconstructions: giulien (13.8k), neosearch (1.6k), chaze (602), 28pigorowa (44.7k)
  - "Mudhif (test model)" (HusseinYaseen, 4.1M)

  — [Sketchfab search](https://api.sketchfab.com/v3/search?type=models&q=cylinder%20seal&downloadable=true&license=by)
- **[live-probed] Look-alikes that are EXCLUDED:** "Standard of Ur" (Digor: BY-NC-ND; ggunhouse: BY-NC-SA), Enannatum (Digor: BY-NC-ND), gnuchev Sumerian relief, vase and Louvre plate (NC), John Rylands cuneiform (BY-NC) — [Sketchfab search](https://api.sketchfab.com/v3/search?type=models&q=sumerian&downloadable=true&count=24)
- **[live-probed]** No CC0 or CC BY downloadable hits for "royal game of ur", "ur lyre" or "tell asmar". The "ziggurat" query returned 0 CC0 and many CC BY results — [Sketchfab search](https://api.sketchfab.com/v3/search?type=models&q=ziggurat&downloadable=true&license=by)
- **[live-probed]** `GET /v3/models/{uid}/download` without auth returns **401** — [Sketchfab download API](https://sketchfab.com/developers/download-api/downloading-models)
- Download auth is documented as `Authorization: Bearer <OAuth access token>`. The response contains temporary links to a glTF archive (and USDZ if available), with size and an expiry of about 300 s. Apps must show the license and the author's attribution (username plus model link) — [Sketchfab Downloading models](https://sketchfab.com/developers/download-api/downloading-models); [Download API guidelines](https://sketchfab.com/developers/download-api/guidelines) (per search summary; both pages are JS-rendered and could not be fetched directly)

### Inferences
- Automated pipeline:
  1. Search with `license=cc0` or `license=by` and `downloadable=true`.
  2. Re-check `license.slug` on the model endpoint, because license filters on search can be stale.
  3. Call `/download` with a Bearer token and fetch the `gltf.url` immediately (the link expires).
  4. Log `uid`, `user.username`, `license.label` and `viewerUrl` into a `CREDITS.csv`.
- The Cleveland API's `sketchfab_id` field allows a clean join: CC0 metadata from CMA plus the CC0 mesh from Sketchfab.
- Many personal scripts use `Authorization: Token <personal API token>` from Sketchfab settings for downloads. I could not confirm in docs that this is officially supported for `/download`. Assume OAuth (a one-time interactive login, then refresh the token) to be safe.

### Gaps
- Sketchfab's Terms on bulk or automated downloading were not retrieved, because the pages are JS-rendered.
- The `/v3/users?username=` filter did not work: every account returned the same count of 75. Per-museum model counts are therefore unknown.

---

## Q5. Wikimedia Commons API: images of Ur, the ziggurat, mudhif, the marshes and mudbrick

### Takeaway
Commons is the best keyless source for **architecture and landscape reference and textures**. Filter by license in the search itself with `haswbstatement:P275=<license QID>`, and always read `extmetadata` (LicenseShortName, Artist, LicenseUrl) for attribution. Expect a lot of **CC BY-SA**, which is to be avoided, so filter strictly.

### Cited Findings
- **[live-probed]** Generator search with license and attribution in one call:

  ```
  GET https://commons.wikimedia.org/w/api.php?action=query&format=json
      &generator=search&gsrsearch=mudhif filetype:bitmap&gsrnamespace=6&gsrlimit=50
      &prop=imageinfo&iiprop=url|extmetadata|size
      &iiextmetadatafilter=LicenseShortName|Artist|LicenseUrl|UsageTerms
      &iiurlwidth=2048
  ```

  This returned per-file licenses:
  - Public domain: "Iraqi mudhif interior.jpg", "Mudhif by Gertrude Bell 1918 or 1920", a DVIDS mudhif photo
  - CC BY 4.0: "Al-Mudhif Palace…" (6000×4000)
  - CC BY-SA 4.0 (avoid): "Roofing of reed Mudhif", "Sumerian Mudhif-guesthouse" and others

  — [Commons API](https://commons.wikimedia.org/w/api.php)
- **[live-probed]** License filter inside CirrusSearch: `srsearch=ziggurat Ur haswbstatement:P275=Q6938433` (P275 = copyright license; Q6938433 = CC0) returned 10 hits, including "Eanna Ziggurat West Corner Far View.jpg", "Borsippa Ziggurat.jpg" and "Model of a Mesopotamian ziggurat in the Pergamon Museum" — [Commons API](https://commons.wikimedia.org/w/api.php)
- **[live-probed]** Category enumeration: `list=categorymembers&cmtitle=Category:Mudhif` gave 12 members, `Category:Mesopotamian Marshes` gave 34, and `Category:Ziggurat of Ur` gave 1 file plus subcategories (so recurse into subcats) — [Commons API](https://commons.wikimedia.org/w/api.php)

### Inferences
- Use two passes:
  1. Search or category-crawl for candidates.
  2. `prop=imageinfo&iiextmetadatafilter=LicenseShortName|Artist|LicenseUrl` in batches of up to 50 titles, keeping only `LicenseShortName` in {Public domain, CC0, CC BY x.x}.
- Store `Artist` (HTML, so strip tags) and `LicenseUrl` for credits.
- Other license QIDs to OR into `haswbstatement` (standard Wikidata items; verify before use):
  - Q20007257: CC BY 4.0
  - Q19125117: CC BY 2.0
  - Q14946043: CC BY 3.0
  - Public domain: filter on the `P6216=Q19652` copyright status instead
- Send a descriptive User-Agent per Wikimedia API etiquette, and use `iiurlwidth` thumbnails to avoid pulling 6000 px originals when 2048 px is enough.

### Gaps
- The CC BY and PD QIDs listed above were not live-tested.
- I did not enumerate specific mudbrick, glazed-brick or cone-mosaic texture files. Suggested categories to crawl: "Mud bricks", "Uruk cone mosaics", "Ishtar Gate" (glazed brick, which is Neo-Babylonian and so period-late).

---

## Q6. Freesound API v2 and other commercially usable sound sources

### Takeaway
Use Freesound with the license filter `"Creative Commons 0"` or `"Attribution"`, never `"Attribution NonCommercial"`. Search and HQ previews work with a simple token. **Original-file download requires OAuth2** and has a stricter 30/min and 500/day limit. For bulk ambience, the **Sonniss GDC bundles** are royalty-free, commercial and need no attribution, but they are **not automatable via an API**. **BBC RemArc is NC → EXCLUDED.**

### Cited Findings
- **Auth:**
  - Token auth goes in `?token=KEY` or the `Authorization: Token KEY` header.
  - OAuth2 uses `Authorization: Bearer ACCESS_TOKEN`, with the flow at `https://freesound.org/apiv2/oauth2/authorize/` (client_id, response_type=code) then `POST https://freesound.org/apiv2/oauth2/access_token/`.
  - Auth codes last 10 min and are single-use. Access tokens last 24 h. Refresh with `grant_type=refresh_token`.

  — [Freesound auth docs](https://freesound.org/docs/api/authentication.html)
- **Search:**
  - `GET /apiv2/search/` with `query`, a Solr-syntax `filter`, `fields`, `page_size` (max 150) and `sort` (score, downloads_desc, rating_desc…).
  - License filter strings: `"Attribution"`, `"Attribution NonCommercial"`, `"Creative Commons 0"`. Example: `filter=license:"Creative Commons 0" duration:[1 TO 120]`.
  - Preview keys: `preview-hq-mp3` (~128 kbps), `preview-hq-ogg` (~192 kbps), plus `lq` variants.
  - Download: `GET /apiv2/sounds/<id>/download/` requires OAuth2.

  — [Freesound resources](https://freesound.org/docs/api/resources_apiv2.html)
- **[live-probed docs text]** The license field "The Creative Commons license under which the sound is available to you ('Attribution', 'Attribution NonCommercial', 'Creative Commons 0')". A new `gen_ai_preference` field exists — [Freesound resources](https://freesound.org/docs/api/resources_apiv2.html)
- **[live-probed docs text]** Throttling: standard rate **60 requests/min and 2,000/day**. Original-quality downloads (and uploads, ratings, etc.) are **30/min and 500/day**. Throttled requests get HTTP 429. Higher limits can be requested via the contact form — [Freesound overview](https://freesound.org/docs/api/overview.html)
- **Sonniss #GameAudioGDC: OK.** Worldwide, royalty-free, personal and commercial use, no attribution required. You may not resell the sounds individually or use them for AI/ML training. A new bundle is released yearly (the 2026 bundle is listed) — [Sonniss license](https://sonniss.com/gdc-bundle-license/); [GDC 2026 bundle](https://gdc.sonniss.com/)
- **BBC Sound Effects (RemArc): EXCLUDED.** Personal, educational or research use only. Commercial licensing goes through Pro Sound Effects — [Avosound RemArc summary](https://www.avosound.com/en-us/licensing/remarc-license); [Production Expert](https://www.production-expert.com/home-page/2018/4/23/free-sound-effects-download-16000-bbc-sound-effect-samples-from-bbc-archive)

### Inferences
- Minimal automated flow (no OAuth): search with `fields=id,name,license,username,url,previews,duration` → download `preview-hq-ogg` (192 kbps OGG is fine for game ambience loops) → log `username`, `license` and `url` in credits. Two caveats:
  - Previews are lossy transcodes. Whether shipping previews is within Freesound's terms was not checked (see Gaps).
  - For masters, do the one-time OAuth2 login and refresh the token every 24 h, keeping under 500 downloads/day.
- Query plan for the requested ambience, each with `filter=license:"Creative Commons 0"` first and falling back to `"Attribution"`:

  | Sound | Query |
  |---|---|
  | market crowd | `bazaar crowd`, `market walla` |
  | marsh frogs | `frogs marsh`, `frog chorus` |
  | wind | `wind desert` |
  | fire | `campfire` |
  | hammering | `hammer metal`, `anvil` |
  | donkey | `donkey bray` |
  | sheep and goats | `sheep`, `goat bleat` |
  | birds | `reed warbler`, `wetland birds` |
  | water | `river gentle`, `paddle boat water` |

  Use `sort=rating_desc` and `duration:[5 TO 300]` for loops.
- The Sonniss bundles should be a one-time manual download indexed locally (for example by filename keyword). They cover Foley and ambience at pro quality.

### Gaps
- OpenGameArt audio was not researched this session. It uses mixed licenses (CC0, CC BY, GPL, CC BY-SA), so filter per item.
- I did not confirm whether Freesound's terms allow redistributing preview files (as opposed to originals) in a commercial game.
- Freesound's CC BY 4.0 vs 3.0 distinction under the single "Attribution" string was not checked.

---

## Q7. Photogrammetry-to-low-poly considerations

### Takeaway
Measured scan sizes range from ~65k faces (Minneapolis Institute of Art seals, CMA Gudea) through 0.5–4M (mkghamburg, hmane tablets) up to ~10M. Game-ready props need about 1–20k tris with baked normal maps. Automate decimation, normal baking and texture downscaling. Smithsonian's own output tiers (20k thumb / 150k high, Draco GLB) are a sensible LOD template.

### Cited Findings
- **[live-probed]** Face counts:
  - Minneapolis Institute of Art cylinder seals: 65,412
  - CMA Gudea: 66,586
  - Leiden ruler statue: 306k
  - mkghamburg tablets: 2–4M
  - hmane cuneiform tablet: 10,019,605
  - Mudhif test model: 4.1M
  - Low-poly ziggurat reconstructions: 602–13.8k (chaze, giulien)

  — [Sketchfab search API](https://api.sketchfab.com/v3/search?type=models&q=cuneiform&downloadable=true&license=by)
- **[live-probed]** Smithsonian ships the same model at `20k-thumb` and `150k-4096-high` GLB tiers with Draco compression — [3d-api.si.edu](https://3d-api.si.edu/api/v1.0/content/file/search?q=Iraq&rows=5)

### Inferences (not separately sourced; standard practice)
- **Pipeline:**
  1. Run Blender headless (`blender -b -P decimate.py`).
  2. Import the glTF and apply the Decimate modifier (collapse) to a target of about 5–20k tris.
  3. Bake normal and AO maps from the original onto the low-poly mesh.
  4. Downscale textures to 1–2k.
  5. Export GLB, then import to Unreal. UE5 Nanite can take mid-poly (100–300k) scans directly for hero statues. Small props still benefit from decimation.
- **Stylisation:** a flat-shaded or reduced-palette look (posterise albedo, drop the photographic lighting baked into scan textures) hides mismatches between CC0 museum scans and hand-made assets. De-light scan textures, because museum scans bake in studio lighting.
- **Scale and orientation:** check that Sketchfab and museum scans use real-world units, because many are unit-less. Smithsonian flags `gltf_orientation_compliant`, while Sketchfab models vary.
- **Accuracy caveat:** most available CC0 or CC BY items are **Neo-Sumerian (Gudea), Old Babylonian (tablets) or later (Assyrian, 9th–7th c. BCE; the artsmia seal dated 800–400 BCE)**. Tag each asset with its period so first-millennium material is not presented as 3rd-millennium Sumer.

### Gaps
- There are no open CC0 or CC BY 3D scans of the Royal Game of Ur, the Ur lyres, the Standard of Ur or the Tell Asmar statues. These must be modelled from CC0 Met or CMA photos (the Met has 9 Tell Asmar objects and 6 game-board hits with images) or from CC BY Commons photos.
