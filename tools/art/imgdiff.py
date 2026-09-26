"""imgdiff.py — the mean absolute difference between two screenshots, as a percentage of full scale.

    python3 tools/art/imgdiff.py before.png after.png [--max 1.0]     # exit 1 when above --max (%)
    python3 tools/art/imgdiff.py before_dir after_dir [--max 1.0]     # every same-named PNG

Stage V batch 5 uses it to prove the clear day with no drought still looks exactly as before (< 1 %).
The HUD's corner text changes with the clock; crop it with --crop-top (px, default 180).
"""
import os
import sys

import numpy as np
from PIL import Image


def diff(a, b, crop_top=180):
    x = np.asarray(Image.open(a).convert("RGB"), np.float32)[crop_top:]
    y = np.asarray(Image.open(b).convert("RGB"), np.float32)[crop_top:]
    if x.shape != y.shape:
        raise ValueError(f"{a} {x.shape} vs {b} {y.shape}")
    return float(np.abs(x - y).mean() / 255.0 * 100.0)


def main(argv):
    limit = float(argv[argv.index("--max") + 1]) if "--max" in argv else 1.0
    crop = int(argv[argv.index("--crop-top") + 1]) if "--crop-top" in argv else 180
    a, b = [x for x in argv if not x.startswith("--") and not x.replace(".", "").isdigit()][:2]
    pairs = [(os.path.join(a, n), os.path.join(b, n)) for n in sorted(os.listdir(a)) if n.endswith(".png")
             and os.path.exists(os.path.join(b, n))] if os.path.isdir(a) else [(a, b)]
    worst = 0.0
    for x, y in pairs:
        d = diff(x, y, crop)
        worst = max(worst, d)
        print(f"imgdiff: {os.path.basename(x)} {d:.3f} %")
    print(f"imgdiff: worst {worst:.3f} % (limit {limit} %)")
    return 1 if worst > limit else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
