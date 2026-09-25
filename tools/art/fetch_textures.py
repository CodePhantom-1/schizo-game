"""Download CC0 PBR texture sets (ambientCG) for the mudbrick house kit's
materials. Plain Python -- no Blender/bpy needed, run before the Blender
scripts.

Downloads 2K JPG map sets into art/source/<AssetId>/ (gitignored -- fetched
per machine, not committed; the ~167 MB total across 6 materials is well
over the repo's ~40 MB commit budget, so this script is the fetch path the
hard rules prefer for a set this size). Idempotent: skips any asset whose
maps are already on disk. Every source is CC0 (ambientCG, no attribution
required); the license + exact asset URL is printed for each one and
recorded in art/assets.csv.

Run:
    python3 tools/art/fetch_textures.py
"""
import csv
import io
import os
import sys
import urllib.request
import zipfile

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
SOURCE_DIR = os.path.join(REPO_ROOT, "art", "source")
RES = "2K"
MAP_TYPES = ("Color", "NormalGL", "Roughness", "AmbientOcclusion")

# material name -> ambientCG asset id + documented tile size (metres).
# Texel density = 2048 px / tile_m; finer for weave/grain detail (timber,
# reed), coarser for the big ground plane -- see tools/art/README.md.
TEX_DEFS = {
    "M_Mudbrick": dict(
        asset_id="Bricks100", tile_m=2.0,
        use="sun-dried mudbrick wall/masonry surfaces (corners, parapets, stairs)",
    ),
    "M_MudPlaster": dict(
        asset_id="Ground087", tile_m=2.0,
        use="mud plaster / earthen render (main wall faces)",
    ),
    "M_Timber": dict(
        asset_id="Bark012", tile_m=1.0,
        use="rough timber / palm-trunk lintels and awning poles",
    ),
    "M_Reed": dict(
        asset_id="Wicker010A", tile_m=1.0,
        use="woven reed mat (awning panel)",
    ),
    "M_Ground": dict(
        asset_id="Ground109", tile_m=3.0,
        use="packed earth / courtyard ground (courtyard floor tile)",
    ),
    "M_Plinth": dict(
        asset_id="Rock035", tile_m=2.0,
        use="bitumen / darker stone plinth footing course",
    ),
}


def asset_url(asset_id):
    return f"https://ambientcg.com/a/{asset_id}"


def zip_url(asset_id, res=RES):
    return f"https://ambientcg.com/get?file={asset_id}_{res}-JPG.zip"


def dest_dir(asset_id):
    return os.path.join(SOURCE_DIR, asset_id)


def map_filename(asset_id, map_type, res=RES):
    return f"{asset_id}_{res}-JPG_{map_type}.jpg"


def map_path(asset_id, map_type, res=RES):
    return os.path.join(dest_dir(asset_id), map_filename(asset_id, map_type, res))


def already_fetched(asset_id):
    return all(
        os.path.exists(map_path(asset_id, m)) and os.path.getsize(map_path(asset_id, m)) > 0
        for m in MAP_TYPES
    )


def fetch_one(name, spec):
    asset_id = spec["asset_id"]
    if already_fetched(asset_id):
        print(f"[skip] {name} ({asset_id}) already present -> {dest_dir(asset_id)}")
        return
    url = zip_url(asset_id)
    print(f"[fetch] {name} ({asset_id}) {RES} from {url}")
    req = urllib.request.Request(url, headers={"User-Agent": "schizo-game-art-pipeline/1.0"})
    with urllib.request.urlopen(req, timeout=180) as resp:
        data = resp.read()
    d = dest_dir(asset_id)
    os.makedirs(d, exist_ok=True)
    with zipfile.ZipFile(io.BytesIO(data)) as zf:
        names = set(zf.namelist())
        for m in MAP_TYPES:
            fname = map_filename(asset_id, m)
            if fname not in names:
                raise RuntimeError(f"{asset_id}: expected map {fname} missing from zip")
            with zf.open(fname) as src, open(os.path.join(d, fname), "wb") as out:
                out.write(src.read())
    print(f"  -> {d} ({', '.join(MAP_TYPES)})")


def _update_assets_csv(rows):
    path = os.path.join(REPO_ROOT, "art", "assets.csv")
    existing_ids = set()
    if os.path.exists(path):
        with open(path, newline="") as f:
            for r in csv.DictReader(f):
                existing_ids.add(r["id"])
    new_rows = [r for r in rows if r[0] not in existing_ids]
    if not new_rows:
        return
    is_new = not os.path.exists(path)
    with open(path, "a", newline="") as f:
        w = csv.writer(f)
        if is_new:
            w.writerow(["id", "file", "kind", "license", "source_ref", "tag"])
        for r in new_rows:
            w.writerow(r)


def main():
    os.makedirs(SOURCE_DIR, exist_ok=True)
    rows = []
    for name, spec in TEX_DEFS.items():
        fetch_one(name, spec)
        asset_id = spec["asset_id"]
        print(f"  license: CC0 -- source: {asset_url(asset_id)} -- used for: {spec['use']}")
        if not already_fetched(asset_id):
            print(f"FAIL: {name} ({asset_id}) missing maps after fetch", file=sys.stderr)
            sys.exit(1)
        for m in MAP_TYPES:
            rows.append([
                f"{name}_{m}",
                os.path.relpath(map_path(asset_id, m), REPO_ROOT),
                "texture_map",
                "CC0",
                asset_url(asset_id),
                "A",
            ])
    _update_assets_csv(rows)
    total_bytes = sum(
        os.path.getsize(map_path(spec["asset_id"], m))
        for spec in TEX_DEFS.values() for m in MAP_TYPES
    )
    print(f"\nfetch_textures: {len(TEX_DEFS)} materials, {len(rows)} maps verified "
          f"({total_bytes / 1e6:.1f} MB) in {SOURCE_DIR}")


if __name__ == "__main__":
    main()
