"""The asset manifest, art/assets.csv: one row per asset file, the licence gate's input.

Plain Python (no bpy), so Blender scripts, fetchers and CI all share it.
Rows are dicts keyed by COLUMNS; upsert() is keyed by id and keeps first-insert order.
"""
import csv
import hashlib
import os

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
PATH = os.path.join(REPO, "art", "assets.csv")
COLUMNS = ["id", "file", "kind", "license", "author", "url", "acquired", "sha256", "ai", "period",
           "source_ref", "tag"]


def load(path=PATH):
    if not os.path.exists(path):
        return []
    with open(path, newline="") as f:
        return [{c: r.get(c) or "" for c in COLUMNS} for r in csv.DictReader(f)]


def upsert(path, rows):
    existing = {r["id"]: r for r in load(path)}
    order = list(existing)
    for r in rows:
        if r["id"] not in existing:
            order.append(r["id"])
        existing[r["id"]] = {c: r.get(c) or "" for c in COLUMNS}
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", newline="") as f:
        w = csv.DictWriter(f, COLUMNS, lineterminator="\n")
        w.writeheader()
        for rid in order:
            w.writerow(existing[rid])


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def legacy(r6, acquired=""):
    """A generator's old positional row [id, file, kind, licence, source_ref, tag] as a manifest row."""
    rid, file, kind, lic, ref, tag = r6
    r = {"id": rid, "file": file, "kind": kind, "source_ref": ref, "tag": tag, "ai": "false"}
    if lic.startswith("original"):
        r.update(license="LicenseRef-Original", author="schizo-game contributors")
    elif "quaternius" in ref:
        # QAL v1.0 applies to anything fetched on or after 2026-08-28.
        r.update(license="LicenseRef-QAL-1.0", author="Quaternius", url=ref, acquired=acquired)
    elif "ambientcg" in ref and lic == "CC0":
        r.update(license="CC0-1.0", author="ambientCG", url=ref, acquired=acquired)
    else:
        r.update(license=lic, url=ref, acquired=acquired)
    if acquired and file and os.path.exists(os.path.join(REPO, file)):
        r["sha256"] = sha256(os.path.join(REPO, file))
    return r
