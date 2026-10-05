#!/usr/bin/env python3
"""0.15 stage 2: lists every control found by the audit probe with its label (the text drawn inside its rectangle), per screen, in drawing order.
usage: list_controls.py <dir with <screen>.probe and <screen>.sN.probe> <screen>   -> lines 'window<TAB>label<TAB>w x h'"""
import sys, os, glob
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import analyze_probe as A
d, n = sys.argv[1], sys.argv[2]
first, more = A.screen_files(d, n)
seen, out = set(), []
for f in [first] + more:
    T, I, R, W = A.load(f)
    for i in I:
        if i["win"] in A.SKIP or i["id"] in seen: continue
        seen.add(i["id"])
        x0, y0, x1, y1 = i["r"]
        labs = [t["text"].strip() for t in T if t["win"] == i["win"] and x0 - 2 <= t["x"] <= x1 and y0 - 6 <= t["y"] <= y1 and t["text"].strip()]
        sy = W.get(i["win"], {}).get("sy", 0)
        out.append((i["win"].split("#")[0] or "?", round(y0 + sy), " ".join(labs), f"{x1-x0:.0f}x{y1-y0:.0f}"))
for w, y, l, s in sorted(out, key=lambda r: (r[0], r[1])):
    print(f"{w}\t{y}\t{l}\t{s}")
