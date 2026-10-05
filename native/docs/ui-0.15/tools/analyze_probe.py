#!/usr/bin/env python3
"""0.15 stage 1 audit: turns the .probe files written by the throwaway ImGui probe (see audit_probe.md) into one row per screen.
usage: analyze_probe.py <dir with *.probe> [--md]
Definitions (also written in REPORT.md):
  controls     distinct interactive items (buttons, tick boxes, sliders, combos, tabs, text inputs, selectables) in UI windows;
               visible = inside the window's clip rectangle at the first scroll position and not under a floating window
  boxes        distinct closed outlines >= 20 x 14 px (a frame, a card, a button border) that are on the screen
  sizes        distinct text pixel sizes (rounded to 0.5 px) of UI text; runs of one non-ASCII symbol are icons and not counted
  scroll       ScrollMax.y of the side column (how many pixels the user can scroll)
  long         wrapped paragraphs of more than 3 lines (all, and the ones that start inside the first screen)
  outside      visible controls that are not in the screen's own control window
"""
import sys, glob, os
PANELS = {"Controls", "Atom controls"}
BARS = {"##header", "Atom view bar"}
SKIP = {"Debug##Default"}          # ImGui's fallback window: here the labels and text drawn on the picture itself
def load(p):
    T, I, R, W = [], [], [], {}
    for line in open(p, encoding="utf-8", errors="replace"):
        f = line.rstrip("\n").split("\t")
        if f[0] == "T": T.append(dict(win=f[1], size=float(f[2]), wrap=float(f[3]), lines=int(f[4]), n=int(f[5]), x=float(f[6]), y=float(f[7]), w=float(f[8]), h=float(f[9]), text=f[10] if len(f) > 10 else ""))
        elif f[0] == "I": I.append(dict(win=f[1], r=tuple(map(float, f[2:6])), vis=f[6] == "1", id=f[7]))
        elif f[0] == "R": R.append(dict(win=f[1], r=tuple(map(float, f[2:6])), th=float(f[6]), a=int(f[7]), pts=int(f[8])))
        elif f[0] == "W": W[f[1]] = dict(x=float(f[2]), y=float(f[3]), w=float(f[4]), h=float(f[5]), sy=float(f[6]), smax=float(f[7]), ch=float(f[8]))
    return T, I, R, W
def inside(r, w): return r[0] < w["x"] + w["w"] and r[2] > w["x"] and r[1] < w["y"] + w["h"] and r[3] > w["y"]
def analyse(first, more=()):
    """first: the probe file at scroll 0; more: files of the same screen scrolled further down (the probe only sees what is drawn)."""
    T, I, R, W = load(first)
    floating = [k for k in W if k not in PANELS | BARS | SKIP]
    def covered(win, r):
        cx, cy = (r[0] + r[2]) / 2, (r[1] + r[3]) / 2
        return any(k != win and k not in PANELS | BARS | SKIP and W[k]["x"] <= cx <= W[k]["x"] + W[k]["w"] and W[k]["y"] <= cy <= W[k]["y"] + W[k]["h"] for k in W)
    own = floating
    mainwin = set(own) if own else PANELS
    seen, ctr_all, ctr_vis, outside = set(), 0, 0, 0
    for i in I:
        if i["win"] in SKIP or i["id"] in seen: continue
        seen.add(i["id"]); ctr_all += 1
        if i["vis"] and not covered(i["win"], i["r"]):
            ctr_vis += 1
            if i["win"] not in mainwin: outside += 1
    for f in more:                                  # items below the first screen
        for i in load(f)[1]:
            if i["win"] not in SKIP and i["id"] not in seen: seen.add(i["id"]); ctr_all += 1
    def collect(files):
        boxes, sizes, longp, longf, chars = set(), {}, set(), set(), {}
        for n, f in enumerate(files):
            T, I, R, W = load(f)
            def isfloat(k): return k not in PANELS | BARS | SKIP
            def covered(win, r):
                cx, cy = (r[0] + r[2]) / 2, (r[1] + r[3]) / 2
                return any(k != win and isfloat(k) and W[k]["x"] <= cx <= W[k]["x"] + W[k]["w"] and W[k]["y"] <= cy <= W[k]["y"] + W[k]["h"] for k in W)
            for r in R:
                if r["win"] in SKIP: continue
                w, h = r["r"][2] - r["r"][0], r["r"][3] - r["r"][1]
                if w < 20 or h < 14: continue
                win = W.get(r["win"])
                if win and not inside(r["r"], win): continue
                if covered(r["win"], r["r"]): continue
                sy = win["sy"] if win else 0
                boxes.add((r["win"], round(r["r"][0]), round(r["r"][1] + sy), round(r["r"][2]), round(r["r"][3] + sy)))
            for t in T:
                if t["win"] in SKIP: continue
                win = W.get(t["win"]); sy = win["sy"] if win else 0
                if win and not (t["y"] + t["h"] > win["y"] and t["y"] < win["y"] + win["h"]): continue
                if covered(t["win"], (t["x"], t["y"], t["x"] + t["w"], t["y"] + t["h"])): continue
                key = (t["win"], round(t["x"]), round(t["y"] + sy), t["text"])
                if key in chars: continue
                s_ = t["text"].strip()
                chars[key] = t["n"]
                if not (len(s_) <= 1 and any(ord(c) > 127 for c in s_)):
                    k = round(t["size"] * 2) / 2; sizes[k] = sizes.get(k, 0) + t["n"]
                if t["wrap"] > 0 and t["lines"] > 3:
                    longp.add(key[:3])
                    if n == 0: longf.add(key[:3])
        return boxes, sizes, longp, longf, sum(chars.values())
    boxes, sizes, longp, longf, nchars = collect([first] + list(more))
    boxes1 = collect([first])[0]
    scroll = max([W[k]["smax"] for k in PANELS if k in W] + [0])
    colh = max([W[k]["h"] for k in PANELS if k in W] + [0])
    fl = "; ".join(f"{k.split('#')[0]}: scroll {W[k]['smax']:.0f} px" for k in own)
    return dict(controls_all=ctr_all, controls_vis=ctr_vis, outside=outside, boxes=len(boxes1), boxes_all=len(boxes), sizes=sorted(sizes), nsizes=len(sizes),
                scroll=scroll, colh=colh, long_all=len(longp), long_first=len(longf), floating=fl, chars=nchars, size_chars=sizes)

def screen_files(d, n):
    first = os.path.join(d, n + ".probe")
    more = sorted(glob.glob(os.path.join(d, n + ".s*.probe")), key=lambda p: int(p.rsplit(".s", 1)[1].split(".")[0]))
    return first, more

NAMES = {"orbit": "Orbit Lab", "orbit_sel": "Orbit Lab, Earth selected", "atom_orbit": "Atom Lab: orbit view", "nucleus": "Atom Lab: nucleus", "cloud": "Atom Lab: Cloud 3D",
         "slice": "Atom Lab: Slice", "chart": "Atom Lab: chart", "landscape": "Atom Lab: landscape", "learn": "Learn tab", "ach": "Achievements tab", "settings": "Settings",
         "themes": "Theme picker", "about": "About", "history": "History (app changelog)", "lablog": "Lab changelog", "guide_orbit": "Guide (Orbit Lab)", "guide_atom": "Guide (Atom Lab)"}
if __name__ == "__main__":
    d = sys.argv[1]
    print("| Screen | Controls seen / in all | of those outside the main control area | Bordered boxes seen / in all | Text sizes (px) | Scroll of the side column or window, px | Paragraphs over 3 lines, first screen / all | Text characters |")
    print("|---|---|---|---|---|---|---|---|")
    for n, label in NAMES.items():
        first, more = screen_files(d, n)
        if not os.path.exists(first): continue
        a = analyse(first, more)
        T, I, R, W = load(first)
        own = [k for k in W if k not in PANELS | BARS | SKIP]
        scroll = max(W[k]["smax"] for k in own) if own else a["scroll"]
        where = " (window)" if own else ""
        sz = ", ".join(("%g" % s) for s in a["sizes"])
        print(f"| {label} | {a['controls_vis']} / {a['controls_all']} | {a['outside']} | {a['boxes']} / {a['boxes_all']} | {a['nsizes']}: {sz} | {scroll:,.0f}{where} | {a['long_first']} / {a['long_all']} | {a['chars']:,} |")
