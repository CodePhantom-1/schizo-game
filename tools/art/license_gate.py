"""The licence gate: every manifest row must carry an allowed licence (Stage V, research §7).

  python3 tools/art/license_gate.py                  # exit 1 on any problem or stale docs/credits.md
  python3 tools/art/license_gate.py --write-credits  # regenerate docs/credits.md

NC forbids sale, ND forbids the stylization pass, SA/GPL would spread their terms to the game.
QAL (Quaternius since 2026-08-28) and the Fab Standard licence allow a private repo shared with
collaborators but never redistribution of the files: keep them out of any public repo.
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import manifest  # noqa: E402

ALLOWED = {
    "CC0-1.0": "CC0 1.0 (public domain)",
    "CC-BY-4.0": "CC BY 4.0",
    "CC-BY-3.0": "CC BY 3.0",
    "LicenseRef-Original": "original to this game",
    "LicenseRef-QAL-1.0": "Quaternius Asset License 1.0 (no redistribution of the files)",
    "LicenseRef-Fab-Standard": "Fab Standard License (no redistribution of the files)",
    "LicenseRef-Sonniss": "Sonniss GDC bundle licence (royalty-free, no attribution)",
    "LicenseRef-PublicDomain": "public domain (no rights reserved: e.g. the Yale Bright Star Catalogue)",
}
ATTRIBUTION = {"CC-BY-4.0", "CC-BY-3.0"}
CREDITS = os.path.join(manifest.REPO, "docs", "credits.md")


def check(rows):
    problems, seen = [], set()
    for r in rows:
        rid = r["id"]
        if rid in seen:
            problems.append(f"{rid}: duplicate id")
        seen.add(rid)
        if r["license"] not in ALLOWED:
            problems.append(f"{rid}: licence {r['license']!r} is not allowed (allowed: {', '.join(sorted(ALLOWED))})")
        if r["license"] in ATTRIBUTION and not (r["author"] and r["url"]):
            problems.append(f"{rid}: {r['license']} needs an author and a url for the credits")
        if r["ai"] not in ("true", "false", ""):
            problems.append(f"{rid}: ai must be true or false, not {r['ai']!r}")
        if r["ai"] == "true" and not r["source_ref"]:
            problems.append(f"{rid}: AI-made, so source_ref must name the tool, model and prompt (Steam disclosure)")
    return problems


def credits_md(rows):
    out = ["# Credits", "",
           "Generated from `art/assets.csv` by `tools/art/license_gate.py --write-credits`. Do not edit by hand.", ""]
    attrib = [r for r in rows if r["license"] in ATTRIBUTION]
    if attrib:
        out += ["## Attribution (required)", ""]
        out += [f"- {r['id']} by {r['author']} ({r['url']}), {ALLOWED[r['license']]}"
                + (", modified" if r["kind"] not in ("reference",) else "") for r in attrib]
        out.append("")
    for lic in ("LicenseRef-QAL-1.0", "LicenseRef-Fab-Standard", "LicenseRef-Sonniss", "CC0-1.0", "LicenseRef-PublicDomain"):
        who = sorted({(r["author"] or "unknown", r["url"].split("/a/")[0] if "/a/" in r["url"] else r["url"])
                      for r in rows if r["license"] == lic})
        if who:
            out += [f"## {ALLOWED[lic]}", ""] + [f"- {a} ({u})" if u else f"- {a}" for a, u in who] + [""]
    ai = [r for r in rows if r["ai"] == "true"]
    out += ["## AI-generated content (Steam disclosure: pre-generated)", ""]
    out += [f"- {r['id']}: {r['source_ref']}" for r in ai] or ["- none"]
    return "\n".join(out) + "\n"


def main(argv):
    rows = manifest.load()
    problems = check(rows)
    for p in problems:
        print(f"FAIL {p}")
    text = credits_md(rows)
    if "--write-credits" in argv:
        with open(CREDITS, "w") as f:
            f.write(text)
        print(f"license_gate: wrote {os.path.relpath(CREDITS, manifest.REPO)}")
    elif not os.path.exists(CREDITS) or open(CREDITS).read() != text:
        problems.append("docs/credits.md is stale")
        print("FAIL docs/credits.md is stale: run tools/art/license_gate.py --write-credits")
    print(f"license_gate: {len(rows)} assets, {len(problems)} problems")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
