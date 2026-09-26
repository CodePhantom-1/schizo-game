"""Fetch the animated animal bases for Stage V batch 4 from Quaternius's public Google Drive packs.

    python3 tools/art/fetch_fauna.py

Survey (2026-09-26, recorded in art/fauna_models.csv): the "Ultimate Animated Animal Pack" (July 2021,
glTF: Alpaca, Bull, Cow, Deer, Donkey, Fox, Horse, Horse_White, Husky, ShibaInu, Stag, Wolf) and the
"Farm Animal Pack" (FBX: Cow, Horse, Llama, Pig, Pug, Sheep, Zebra). No goat or cat exists: those are
variants (tools/art/fauna_variants.py): the goat of the deer (the farm sheep and pig carry only Idle and
Jump clips; the deer walks), the cat of the fox. Only the bases the fauna table uses are fetched, into
art/source/fauna/ (gitignored). The packs' bundled License.txt still says CC0, but quaternius.com's
licence page is the Quaternius Asset License v1.0 since 2026-08-28, which governs anything fetched now:
LicenseRef-QAL-1.0 (free in games, no redistribution of the files: fine for this private repo, not for a
public one). The licence page is checked on every run; a change of terms stops the fetch.
Download, retry and confirm-form handling are fetch_characters.py's.
"""
import datetime
import os
import sys
import urllib.request

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import fetch_characters as fc  # noqa: E402 -- the Drive download helpers
import manifest  # noqa: E402

SOURCE_DIR = os.path.join(manifest.REPO, "art", "source", "fauna")
LICENCE_URL = "https://quaternius.com/license.html"
LICENCE = "LicenseRef-QAL-1.0"
ANIMALS_URL = "https://quaternius.com/packs/ultimateanimatedanimals.html"
FARM_URL = "https://quaternius.com/packs/farmanimal.html"
# base id -> (file name, Drive file id, pack page)
BASES = {
    "donkey": ("Donkey.gltf", "1Buic-_4vNtmwN0rtMHcdWw3TEaSPz4iW", ANIMALS_URL),
    "fox": ("Fox.gltf", "1z-CWoUC2vJxrqgGFTYlMaywpE1ooV-bA", ANIMALS_URL),
    "husky": ("Husky.gltf", "1oYn47mfq9JAdkJJDcUftbhHNsgQ_CWdt", ANIMALS_URL),
    "shiba": ("ShibaInu.gltf", "1XWUVbmMbiG9E90OqrumueD_pBdnYdyHZ", ANIMALS_URL),
    "cow": ("Cow.gltf", "1lS3t1Sof0FVne1C1WXfdX48qaHES_pDG", ANIMALS_URL),
    "bull": ("Bull.gltf", "1M9gyr2UikIDW_Ynp-OfgiK-JjI7ziPd3", ANIMALS_URL),
    "deer": ("Deer.gltf", "1iGpXKrqYGyZCPGHPPSuDAoKnOXLhXJ0q", ANIMALS_URL),  # the goat's base: it walks
    "sheep": ("Sheep.fbx", "1SejVZugDFeAtIn16d_QhsvcJhBJEbKB5", FARM_URL),
    "pig": ("Pig.fbx", "1rdiu2AJFEbH4-n32jjOpnMKYorhpxR0G", FARM_URL),
}


def licence_ok():
    req = urllib.request.Request(LICENCE_URL, headers={"User-Agent": "schizo-game-art-pipeline/1.0"})
    text = urllib.request.urlopen(req, timeout=60).read().decode("utf-8", "replace")
    return "Quaternius Asset License (QAL) v1.0" in text


def download(file_id, out_path):
    """fetch_characters.download's retry loop, accepting a glTF (JSON) or a binary FBX payload."""
    import time  # noqa: PLC0415
    last = None
    for attempt in range(4):
        if attempt:
            time.sleep(15 * attempt)
        req = urllib.request.Request(fc.drive_url(file_id), headers={"User-Agent": "schizo-game-art-pipeline/1.0"})
        with urllib.request.urlopen(req, timeout=300) as resp:
            data, ctype = resp.read(), resp.headers.get("Content-Type", "")
        if "text/html" in ctype.lower():
            extra = fc._extract_confirm_fields(data.decode("utf-8", "replace"))
            if not extra:
                last = "HTML instead of the file (quota or interstitial)"
                continue
            req2 = urllib.request.Request(fc.drive_url(file_id) + f"&{extra}", headers={"User-Agent": "schizo-game-art-pipeline/1.0"})
            with urllib.request.urlopen(req2, timeout=300) as resp2:
                data = resp2.read()
        is_gltf = out_path.endswith(".gltf") and data.lstrip().startswith(b"{")
        is_fbx = out_path.endswith(".fbx") and data.startswith(b"Kaydara FBX Binary")
        if not (is_gltf or is_fbx):
            last = f"payload is not the expected file ({len(data)} bytes)"
            continue
        with open(out_path, "wb") as f:
            f.write(data)
        return len(data)
    raise RuntimeError(f"download failed after 4 attempts: {last}")


def main():
    if not licence_ok():
        sys.exit(f"fetch_fauna: {LICENCE_URL} no longer shows QAL v1.0 — review the terms before fetching")
    os.makedirs(SOURCE_DIR, exist_ok=True)
    today = datetime.date.today().isoformat()
    rows = []
    for base, (name, drive_id, page) in BASES.items():
        path = os.path.join(SOURCE_DIR, name)
        if os.path.exists(path) and os.path.getsize(path) > 0:
            print(f"[skip] {name}")
        else:
            n = download(drive_id, path)
            print(f"[fetch] {name}: {n / 1e6:.1f} MB")
        rows.append({"id": f"fauna_{base}", "file": os.path.relpath(path, manifest.REPO).replace(os.sep, "/"),
                     "kind": "source_fauna_model", "license": LICENCE, "author": "Quaternius", "url": page,
                     "acquired": today, "sha256": manifest.sha256(path), "ai": "false",
                     "source_ref": "Stage V batch 4 fauna base (art/fauna_models.csv)", "tag": "A"})
    manifest.upsert(manifest.PATH, rows)
    print(f"fetch_fauna: {len(rows)} bases in {os.path.relpath(SOURCE_DIR, manifest.REPO)} ({LICENCE})")


if __name__ == "__main__":
    main()
