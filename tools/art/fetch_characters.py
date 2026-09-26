"""Download the CC0 low-poly animated characters (Quaternius "Ultimate
Animated Character Pack", Nov 2019) that replace the cylinder/sphere
programmer-art bodies of ASimNpc and ASimCharacter (D-023: low-poly, flat
vertex colours, a few thousand triangles — Valheim-like, NOT realistic).

Plain Python (stdlib only, mirrors fetch_textures.py), run before
tools/art/ue_import_characters.py:

    python3 tools/art/fetch_characters.py

Downloads self-contained .gltf files (mesh + skeleton + 17 embedded
animations each, ~2 MB apiece) into art/source/characters/ (gitignored —
fetched per machine, never committed). Idempotent: skips any model already
on disk. Every file is CC0 (quaternius.com, no attribution required); the
licence + pack URL is printed and recorded in art/assets.csv as
kind == character_model rows keyed by the UE asset id the importer uses
(Npc0..Npc8, Player), so ue_import_characters.py reads its work list from
the same manifest and the mapping is never hardcoded twice.

Why Drive ids: Quaternius serves packs from a public Google Drive folder
(https://quaternius.com/packs/ultimatedanimatedcharacter.html). The
usercontent endpoint below serves anonymous downloads without cookies for
files this small; the confirm-form dance is implemented anyway in case a
file grows past the virus-scan threshold.
"""
import csv
import json
import os
import re
import sys
import time
import urllib.error
import urllib.parse
import urllib.request

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
SOURCE_DIR = os.path.join(REPO_ROOT, "art", "source", "characters")

PACK_URL = "https://quaternius.com/packs/ultimatedanimatedcharacter.html"
PACK_LICENCE = "CC0"

# UE asset id -> model file stem + Google Drive file id (the glTF subfolder
# of the pack's Drive folder, enumerated 2026-09-25). The id (NpcN / Player)
# is the name ue_import_characters.py imports under /Game/Art/Characters
# and the name SimNpc.cpp / SimCharacter.cpp LoadObject at runtime.
#
# Cast rationale (see docs/proposals/invented-ledger-humans.md): Worker and
# Casual variants are the plainly-dressed townsfolk, OldClassy the elders
# (long robes), Kimono the robed priestly/scribal class, Wizard (blue/gold
# court robe, no hat) stands in for temple clergy such as the priestess.
# Player = Casual3_Male: sand/beige palette reads as sun-bleached wool.
CHAR_DEFS = {
    "Player": dict(stem="Casual3_Male", drive_id="1lu0NtOCZ8KAM3R9HU1spzWzqdK-_Fn-H"),
    "Npc0": dict(stem="Worker_Male", drive_id="10TWw1W-6aa4KmwKbQYewSG4-m67_4nof"),
    "Npc1": dict(stem="Worker_Female", drive_id="1rnm-vJlpnt0QFKOzwbV1-wUQKaFxJ-1T"),
    "Npc2": dict(stem="Casual_Male", drive_id="1rrj82IYNaSWj5ySxR6fW9oojyHFiW7_r"),
    "Npc3": dict(stem="Casual_Female", drive_id="1E79ks2jbMt5iIrI8Ag9lRgA0VRnfU4pk"),
    "Npc4": dict(stem="Casual2_Female", drive_id="1ou-CadwWpR_ita6L96e169IrVUFCqWBN"),
    "Npc5": dict(stem="OldClassy_Male", drive_id="1a1o9LoZDE6mjVz1JtDg9lV5qulfxALzy"),
    "Npc6": dict(stem="OldClassy_Female", drive_id="1xr7_81_CCdXpMy17wXOoLqQEefzOjSzZ"),
    "Npc7": dict(stem="Kimono_Male", drive_id="1F_QpqJz2vBV2_30ryafhuuOiJ9HLXMEF"),
    "Npc8": dict(stem="Wizard", drive_id="1kVJicV3OAdeL96Uif-3ud1bh8gsl7QGT"),
}


def drive_url(file_id):
    return f"https://drive.usercontent.google.com/download?id={file_id}&export=download&confirm=t"


def dest_path(stem):
    return os.path.join(SOURCE_DIR, f"{stem}.gltf")


def already_fetched(stem):
    p = dest_path(stem)
    return os.path.exists(p) and os.path.getsize(p) > 0


def _extract_confirm_fields(html_text):
    """Google's interstitial is a self-posting form of hidden inputs; return
    them as a query string to append for the confirmed retry."""
    fields = {}
    for name, value in re.findall(
        r'<input[^>]+type="hidden"[^>]+name="([^"]+)"[^>]+value="([^"]*)"', html_text
    ):
        fields[name] = value
    if not fields:
        for name, value in re.findall(
            r'<input[^>]+name="([^"]+)"[^>]+value="([^"]*)"[^>]+type="hidden"', html_text
        ):
            fields[name] = value
    return urllib.parse.urlencode(fields)


def download(file_id, out_path):
    """Fetch one Drive file. Google throttles anonymous bulk downloads
    ("Quota exceeded" HTML page) — retry with backoff and never write a
    non-glTF payload to disk, so an interrupted fetch can't masquerade as
    a fetched model (already_fetched() would then skip it forever)."""
    last_err = None
    for attempt in range(4):
        if attempt:
            time.sleep(15 * attempt)
        req = urllib.request.Request(
            drive_url(file_id), headers={"User-Agent": "schizo-game-art-pipeline/1.0"})
        try:
            with urllib.request.urlopen(req, timeout=300) as resp:
                data = resp.read()
                ctype = resp.headers.get("Content-Type", "")
        except urllib.error.HTTPError as e:
            last_err = f"HTTP {e.code}"
            continue
        if "text/html" in ctype.lower():
            text = data.decode("utf-8", "replace")
            if "quota" in text.lower():
                last_err = "Drive quota exceeded (anonymous rate limit)"
                continue
            # virus-scan / confirm interstitial — retry with the form's fields
            extra = _extract_confirm_fields(text)
            if not extra:
                last_err = "unexpected HTML interstitial"
                continue
            req2 = urllib.request.Request(
                drive_url(file_id) + f"&{extra}",
                headers={"User-Agent": "schizo-game-art-pipeline/1.0"})
            with urllib.request.urlopen(req2, timeout=300) as resp2:
                data = resp2.read()
        if not data.lstrip().startswith(b"{"):
            last_err = f"payload is not glTF JSON ({len(data)} bytes)"
            continue
        with open(out_path, "wb") as f:
            f.write(data)
        return len(data)
    raise RuntimeError(f"download failed after 4 attempts: {last_err}")


def verify_gltf(path):
    """The files must be self-contained (buffers embedded — no external .bin
    or textures for the importer to miss) and carry the animations the C++
    side expects (Walk/Idle; Run is a bonus)."""
    with open(path, encoding="utf-8") as f:
        g = json.load(f)
    for buf in g.get("buffers", []):
        uri = buf.get("uri", "")
        # data: URIs are embedded — self-contained. Anything else (a .bin
        # or http link) would dangle beside the single .gltf we ship.
        if uri and not uri.startswith("data:"):
            raise RuntimeError(f"{os.path.basename(path)}: external buffer uri {uri[:50]!r}")
    anims = [a.get("name", "") for a in g.get("animations", [])]
    if not ("Walk" in anims and "Idle" in anims):
        raise RuntimeError(f"{os.path.basename(path)}: missing Walk/Idle animations (has {anims})")
    if not g.get("skins"):
        raise RuntimeError(f"{os.path.basename(path)}: no skeleton (static mesh)")
    tris = sum(
        (g["accessors"][prim["indices"]]["count"] if "indices" in prim
         else g["accessors"][prim["attributes"]["POSITION"]]["count"]) // 3
        for mesh in g["meshes"] for prim in mesh["primitives"])
    return len(anims), tris


def _update_assets_csv(rows):
    """Upsert these rows into art/assets.csv (tools/art/manifest.py owns the format)."""
    import datetime  # noqa: PLC0415
    import manifest  # noqa: PLC0415
    today = datetime.date.today().isoformat()
    manifest.upsert(manifest.PATH, [manifest.legacy(r, today) for r in rows])


def main():
    os.makedirs(SOURCE_DIR, exist_ok=True)
    rows = []
    for asset_id, spec in CHAR_DEFS.items():
        stem = spec["stem"]
        if already_fetched(stem):
            print(f"[skip] {asset_id} ({stem}.gltf) already present")
        else:
            url = drive_url(spec["drive_id"])
            print(f"[fetch] {asset_id} <- {stem}.gltf ({url})")
            n = download(spec["drive_id"], dest_path(stem))
            print(f"  -> {dest_path(stem)} ({n / 1e6:.1f} MB)")
        n_anims, tris = verify_gltf(dest_path(stem))
        print(f"  licence: {PACK_LICENCE} -- source: {PACK_URL} "
              f"-- {tris} tris, {n_anims} animations, skeleton OK")
        rows.append([
            asset_id,
            os.path.relpath(dest_path(stem), REPO_ROOT),
            "character_model",
            PACK_LICENCE,
            PACK_URL,
            "A",
        ])
    _update_assets_csv(rows)
    total = sum(os.path.getsize(dest_path(s["stem"])) for s in CHAR_DEFS.values())
    print(f"\nfetch_characters: {len(CHAR_DEFS)} models verified "
          f"({total / 1e6:.1f} MB) in {SOURCE_DIR}")
    print("next: tools/art/ue_import_characters.py inside UnrealEditor-Cmd "
          "(see tools/art/README.md)")


if __name__ == "__main__":
    main()
