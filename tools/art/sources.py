"""Fetch the asset wishlist (art/sources.csv) from its CC0/permissive sources into art/source/<id>/.

  python3 tools/art/sources.py [id ...]     # fetch all (or the named) rows; skips what is already fetched
  python3 tools/art/sources.py --sounds [id ...]   # art/sounds.csv from Freesound (needs FREESOUND_API_KEY)

Adapters: polyhaven (API, texture maps), ambientcg (API v3, zip), kenney (pack page, zip),
git (a shallow clone, e.g. the KayKit packs on GitHub), url (a plain file; the row states its licence),
freesound (art/sounds.csv: a text search limited to CC0 and Attribution, the best-rated result whose duration
fits a loop or a cue, its HQ preview; the manifest records the sound's id, author and exact licence). Each checks the licence the source states
against the row's expected licence and fails on any mismatch, then records one manifest row per
fetched item (licence, author, url, date, a tree sha256). Plain Python + urllib; raw files are
gitignored (art/source/), only the manifest is tracked.
"""
import csv
import datetime
import hashlib
import io
import json
import os
import re
import subprocess
import sys
import time
import urllib.parse
import urllib.request
import zipfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import manifest  # noqa: E402

UA = "schizo-game-asset-pipeline/1.0 (+github.com/CodePhantom-1/schizo-game)"
WISHLIST = os.path.join(manifest.REPO, "art", "sources.csv")
SOUNDS = os.path.join(manifest.REPO, "art", "sounds.csv")
SOURCE = os.path.join(manifest.REPO, "art", "source")
PH_MAPS = {"diff": "Diffuse", "nor_gl": "nor_gl", "rough": "Rough", "ao": "AO", "disp": "Displacement"}


# ---- parsing (pure, tested against saved responses) ----

def polyhaven_files(files, res):
    """Poly Haven /files/<id> -> {map: url} for the jpg maps at res (png when no jpg)."""
    out = {}
    for key, name in PH_MAPS.items():
        entry = files.get(name, {}).get(res)
        if entry:
            out[key] = (entry.get("jpg") or entry.get("png"))["url"]
    return out


def polyhaven_author(info):
    return ", ".join(sorted(info.get("authors", {})))


def ambientcg_download(listing, asset, attrs):
    for a in listing.get("assets", []):
        if a["id"] == asset:
            for d in a.get("downloads", []):
                if d["attributes"] == attrs:
                    return d["url"], f"{asset}_{attrs}.{d['extension']}"
    raise ValueError(f"ambientCG {asset}: no {attrs} download")


def kenney_zip_url(html):
    m = re.search(r"href=['\"](https://kenney\.nl/media/pages/assets/[^'\"]+\.zip)['\"]", html)
    if not m:
        raise ValueError("Kenney page: no zip link")
    return m.group(1)


def kenney_license(html):
    return "CC0-1.0" if re.search(r"\bCC0\b", html) else ""


def license_from_text(text):
    t = text.lower()
    if "noncommercial" in t or "non-commercial" in t or "sharealike" in t or "noderivatives" in t:
        return ""
    if "cc0" in t or "creative commons zero" in t:
        return "CC0-1.0"
    if "cc by 4.0" in t or "attribution 4.0" in t:
        return "CC-BY-4.0"
    return ""


FREESOUND_API = "https://freesound.org/apiv2/search/text/"
FREESOUND_DURATION = {"1": (15.0, 300.0), "0": (0.3, 6.0)}  # a bed loops (15 s .. 5 min); a cue is short
FREESOUND_LICENSES = {"creativecommons.org/publicdomain/zero/1.0": "CC0-1.0", "creative commons 0": "CC0-1.0",
                      "creativecommons.org/licenses/by/4.0": "CC-BY-4.0", "creativecommons.org/licenses/by/3.0": "CC-BY-3.0"}


def freesound_license(value):
    """A Freesound licence (its URL, or the name) as SPDX; '' for anything else (NC, Sampling+, ...)."""
    v = value.strip().lower().rstrip("/")
    v = v.split("://", 1)[-1]
    return FREESOUND_LICENSES.get(v, "")


def freesound_search_url(row, token):
    """The search for a sounds.csv row: CC0 or Attribution only, duration fit for a loop or a cue, best rated."""
    lo, hi = FREESOUND_DURATION[row["loop"]]
    q = urllib.parse.urlencode({
        "query": row["query_or_url"],
        "filter": f'license:("Creative Commons 0" OR "Attribution") duration:[{lo} TO {hi}]',
        "sort": "rating_desc",
        "fields": "id,name,username,license,duration,previews,url",
        "page_size": "15",
        "token": token,
    })
    return f"{FREESOUND_API}?{q}"


def freesound_pick(response, row):
    """The first result (best rated) whose licence the row accepts and whose duration fits."""
    lo, hi = FREESOUND_DURATION[row["loop"]]
    accepted = set(row["license"].split(";"))
    for r in response.get("results", []):
        lic = freesound_license(r.get("license", ""))
        if lic in accepted and lo <= float(r.get("duration", 0)) <= hi and r.get("previews", {}).get("preview-hq-ogg"):
            return {"sound_id": r["id"], "name": r["name"], "author": r["username"], "license": lic,
                    "preview": r["previews"]["preview-hq-ogg"], "url": r["url"], "duration": float(r["duration"])}
    raise ValueError(f"{row['id']}: no {'/'.join(sorted(accepted))} result fits {lo}-{hi} s for {row['query_or_url']!r}")


def expect_license(row, found):
    if found != row["license"]:
        raise ValueError(f"{row['id']}: the source states {found or 'no licence'!r}, the wishlist expects {row['license']!r}")


def wishlist(path=WISHLIST):
    with open(path, newline="") as f:
        return list(csv.DictReader(f))


# ---- network ----

def _get(url, tries=3):
    for i in range(tries):
        try:
            req = urllib.request.Request(url, headers={"User-Agent": UA})
            with urllib.request.urlopen(req, timeout=120) as r:
                return r.read()
        except OSError:
            if i == tries - 1:
                raise
            time.sleep(2 * (i + 1))


def _tree_sha(d):
    h = hashlib.sha256()
    for root, _, files in sorted(os.walk(d)):
        if "/.git/" in root.replace(os.sep, "/") + "/":
            continue
        for f in sorted(files):
            if f != ".fetched":
                p = os.path.join(root, f)
                h.update(os.path.relpath(p, d).replace(os.sep, "/").encode())
                h.update(manifest.sha256(p).encode())
    return h.hexdigest()


def _unzip(data, dest):
    with zipfile.ZipFile(io.BytesIO(data)) as z:
        z.extractall(dest)


def fetch_polyhaven(row, dest):
    asset = row["asset"]
    info = json.loads(_get(f"https://api.polyhaven.com/info/{asset}"))
    expect_license(row, "CC0-1.0")  # everything on Poly Haven is CC0 (polyhaven.com/license)
    for key, url in polyhaven_files(json.loads(_get(f"https://api.polyhaven.com/files/{asset}")), row["res"] or "2k").items():
        with open(os.path.join(dest, os.path.basename(url)), "wb") as f:
            f.write(_get(url))
    return polyhaven_author(info), f"https://polyhaven.com/a/{asset}"


def fetch_ambientcg(row, dest):
    asset = row["asset"]
    listing = json.loads(_get(f"https://ambientcg.com/api/v3/assets?id={asset}&include=downloads"))
    expect_license(row, "CC0-1.0")  # ambientCG publishes everything as CC0 (ambientcg.com/license)
    url, _ = ambientcg_download(listing, asset, row["res"] or "2K-JPG")
    _unzip(_get(url), dest)
    return "ambientCG", f"https://ambientcg.com/a/{asset}"


def fetch_kenney(row, dest):
    page = f"https://kenney.nl/assets/{row['asset']}"
    html = _get(page).decode("utf-8", "replace")
    expect_license(row, kenney_license(html))
    _unzip(_get(kenney_zip_url(html)), dest)
    return "Kenney", page


def fetch_git(row, dest):
    url = f"https://github.com/{row['asset']}"
    subprocess.run(["git", "clone", "--depth", "1", "--quiet", url, dest], check=True)
    texts = [open(os.path.join(dest, f), errors="replace").read()
             for f in os.listdir(dest) if f.upper().startswith(("LICENSE", "LICENCE", "README"))]
    found = next((lic for lic in map(license_from_text, texts) if lic), "")
    expect_license(row, found)
    return row["author"], url


def fetch_url(row, dest):
    """A plain file (e.g. the Yale Bright Star Catalogue, public domain): the row states the licence (checked by
    hand once, recorded in art/sources.csv); a .gz is unpacked beside itself."""
    import gzip  # noqa: PLC0415
    url = row["asset"]
    data = _get(url)
    name = os.path.basename(url)
    if name.endswith(".gz"):
        data, name = gzip.decompress(data), name[:-3]
    with open(os.path.join(dest, name), "wb") as f:
        f.write(data)
    return row["author"], url


def fetch_sound(row, token):
    """A sounds.csv row from Freesound -> art/source/sounds/<id>/<id>.ogg; returns its manifest row."""
    dest = os.path.join(SOURCE, "sounds", row["id"])
    done = os.path.join(dest, ".fetched")
    if os.path.exists(done):
        return None
    pick = freesound_pick(json.loads(_get(freesound_search_url(row, token))), row)
    os.makedirs(dest, exist_ok=True)
    path = os.path.join(dest, f"{row['id']}.ogg")
    with open(path, "wb") as f:
        f.write(_get(pick["preview"]))
    with open(os.path.join(dest, "sound.json"), "w", encoding="utf-8") as f:
        json.dump(pick, f, indent=1)
    open(done, "w").write(datetime.date.today().isoformat())
    return {"id": f"snd_{row['id']}", "file": os.path.relpath(path, manifest.REPO).replace(os.sep, "/"), "kind": "sound",
            "license": pick["license"], "author": pick["author"], "url": pick["url"],
            "acquired": datetime.date.today().isoformat(), "sha256": manifest.sha256(path), "ai": "false",
            "source_ref": f"{row['purpose']} (freesound #{pick['sound_id']} {pick['name']!r}, HQ preview)", "tag": "A"}


def main_sounds(ids):
    token = os.environ.get("FREESOUND_API_KEY", "")
    if not token:
        print("sources: FREESOUND_API_KEY is not set. A person must create the key (freesound.org/apiv2/apply) and set\n"
              "it in the environment; the pipeline never creates accounts or keys.", file=sys.stderr)
        return 2
    with open(SOUNDS, newline="") as f:
        rows = [r for r in csv.DictReader(f) if not ids or r["id"] in ids]
    new, failed = [], []
    for r in rows:
        try:
            m = fetch_sound(r, token)
            print(f"{'skip ' if m is None else 'fetch'} {r['id']} ({r['query_or_url']!r})")
            if m:
                new.append(m)
        except Exception as e:  # noqa: BLE001 -- report every failure, keep fetching the rest
            failed.append(r["id"])
            print(f"FAIL {r['id']}: {str(e).replace(token, '<key>')}", file=sys.stderr)
    if new:
        manifest.upsert(manifest.PATH, new)
    print(f"sources: {len(new)} sounds fetched, {len(rows) - len(new) - len(failed)} already present, {len(failed)} failed")
    return 1 if failed else 0


ADAPTERS = {"polyhaven": fetch_polyhaven, "ambientcg": fetch_ambientcg, "kenney": fetch_kenney, "git": fetch_git,
            "url": fetch_url}


def fetch(row):
    dest = os.path.join(SOURCE, row["id"])
    done = os.path.join(dest, ".fetched")
    if os.path.exists(done):
        return None
    os.makedirs(os.path.dirname(dest), exist_ok=True)
    if row["source"] != "git":
        os.makedirs(dest, exist_ok=True)
    author, url = ADAPTERS[row["source"]](row, dest)
    open(done, "w").write(datetime.date.today().isoformat())
    return {"id": row["id"], "file": os.path.relpath(dest, manifest.REPO).replace(os.sep, "/") + "/", "kind": f"source_{row['kind']}",
            "license": row["license"], "author": author or row["author"], "url": url,
            "acquired": datetime.date.today().isoformat(), "sha256": _tree_sha(dest), "ai": "false",
            "source_ref": row["purpose"], "tag": "A"}


def main(argv):
    if "--sounds" in argv:
        return main_sounds([a for a in argv if a != "--sounds"])
    rows = [r for r in wishlist() if not argv or r["id"] in argv]
    new, failed = [], []
    for r in rows:
        try:
            m = fetch(r)
            print(f"{'skip ' if m is None else 'fetch'} {r['id']} ({r['source']}:{r['asset']})")
            if m:
                new.append(m)
        except Exception as e:  # noqa: BLE001 -- report every failure, keep fetching the rest
            failed.append(r["id"])
            print(f"FAIL {r['id']}: {e}", file=sys.stderr)
    if new:
        manifest.upsert(manifest.PATH, new)
    print(f"sources: {len(new)} fetched, {len(rows) - len(new) - len(failed)} already present, {len(failed)} failed")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
