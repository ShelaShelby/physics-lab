#!/usr/bin/env python3
# 0.15 stage 5k part 1: reading pages (New UI On) scroll by the wheel and the keys whichever window has the focus, have a slim scrollbar, and the Contents list works again and again.
# Checked in the REAL app (Linux, software OpenGL, scripted events; NOT a real mouse, NOT Windows). The owner's PC showed a bug the cloud could not reproduce, so these cases copy what a real
# mouse does differently: the pointer arrives in one frame and the wheel / click comes many frames later, and another window (Contents list, Controls panel, Legend) holds the focus.
#   check_5k.py <PhysicsLab binary> <work folder> <part>      parts: pages, contents, log, about (0.74.0: the About page's last line, 'Free Palestine' and the drawn flag)
import os, re, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from shell_lib import run_app, prefetch, Bmp

SIZES = ("880x560", "2000x1127")   # 0.38.1: the narrow window (Contents menu) and the wide one (Contents rail); 1109x882 dropped
THEMES = ("dark",)                  # 0.38.1: scrolling does not depend on the theme (the look tests check the colours); light dropped
PAGES = (("about", {"PL_OPEN": "about"}), ("learn1", {"PL_LEARN": "1"}), ("timeline", {"PL_LEARN": "3"}), ("history", {"PL_OPEN": "history"}))
FULL = ("about", "timeline")                                    # these get every focus case in both themes; the others the plain wheel and key cases
fails = checks = 0

def check(ok, what):
    global fails, checks
    checks += 1
    if not ok:
        fails += 1
        print("FAIL", what)

def scroll(r):
    for n in r.note("pagescroll"):
        m = re.match(r"pagescroll y=(-?\d+) max=(-?\d+)", n)
        if m:
            return int(m.group(1)), int(m.group(2))
    return None

def pscrolls(r):
    out = []                                                     # (frame, y, max, [the scroll offset of each Contents entry at that moment])
    for ln in r.out.splitlines():
        if ln.startswith("PLSCROLL "):
            a, b = ln.split("|")
            f = a.split()
            out.append((float(f[1]), float(f[2]), float(f[3]), [int(v) for v in b.split()]))
    return out

def anchors(r):
    n = r.note("tocanchors")
    return [int(v) for v in n[0].split()[1:]] if n else []

def mid(rc):
    return rc[0] + rc[2] / 2, rc[1] + rc[3] / 2

def run(binary, work, name, env, sz, theme, frames):
    return run_app(binary, work, name, env, frames=frames, size=sz, theme=theme)

def part_pages(binary, work):
    # ---- pass 1: no script: where the page, the Controls panel and the Legend are ----
    pre = []
    for sz in SIZES:
        for theme in THEMES:
            for name, env in PAGES:
                if name in FULL or theme == "dark":
                    pre.append(((binary, work, f"k_pre_{name}_{sz}_{theme}", dict(env, PL_NEWUI="on")), {"frames": 20, "size": sz, "theme": theme}))
            pre.append(((binary, work, f"k_prel_{sz}_{theme}", {"PL_OPEN": "about", "PL_NEWUI": "on", "PL_ATOM": "1", "PL_ATOM_SET": "view=Chart of nuclides"}), {"frames": 20, "size": sz, "theme": theme}))
    prefetch(pre)
    info = {}
    for args, kw in pre:
        info[(args[2], kw["size"], kw["theme"])] = run_app(*args, **kw)
    runs = []                                                    # (tag, name, env, sz, theme, what, want)
    for sz in SIZES:
        for theme in THEMES:
            for name, env in PAGES:
                if not (name in FULL or theme == "dark"):
                    continue
                base = dict(env, PL_NEWUI="on")
                r0 = info[(f"k_pre_{name}_{sz}_{theme}", sz, theme)]
                col = r0.rects.get("reading.column")
                s0 = scroll(r0)
                check(col is not None and s0 is not None and s0[1] > 0, f"{name} {sz} {theme}: the page is long enough to scroll {s0}")
                if col is None:
                    continue
                cx, cy = mid(col)
                ex = col[0] + col[2] - 30                         # a place on the page that no open Contents list covers
                tag = f"{name} {sz} {theme}"
                nm = f"k_{name}_{sz}_{theme}"
                # the real-mouse order: the pointer arrives in one frame, the wheel / key comes many frames later
                runs.append((tag, nm + "_wheel", dict(base, PL_SCRIPT=f"6:move={cx:.0f},{cy:.0f};20:wheel=-3"), "the wheel, 14 frames after the pointer arrived, moved the page", "gt0"))
                runs.append((tag, nm + "_keys", dict(base, PL_SCRIPT="12:key=pagedown"), "PageDown moved the page", "gt0"))
                runs.append((tag, nm + "_end", dict(base, PL_SCRIPT="12:key=end;20:key=end;28:key=end"), "End went to the bottom", "max"))
                runs.append((tag, nm + "_updown", dict(base, PL_SCRIPT="12:key=pagedown;16:key=pageup"), "PageDown then PageUp came back to the top", "zero"))
                if col[0] >= 8:
                    runs.append((tag, nm + "_dim", dict(base, PL_SCRIPT=f"6:move={col[0] / 2:.0f},{cy:.0f};20:wheel=-3"), "the wheel over the dimmed picture moved the page", "gt0"))
                if name not in FULL:
                    continue
                if "reading.toc" in r0.rects:                    # wide window: the contents rail; focus it with a click on the first entry (page stays at the top), then wheel / key elsewhere
                    focus = "6:move=##toc0;10:click=##toc0;"
                else:
                    focus = "6:click=##readContents;"
                runs.append((tag, nm + "_tocwheel", dict(base, PL_SCRIPT=focus + f"18:move={ex:.0f},{cy:.0f};26:wheel=-3"), "with the Contents window focused the wheel moved the page", "gt0"))
                if "reading.toc" not in r0.rects:
                    runs.append((tag, nm + "_toclist", dict(base, PL_SCRIPT=focus + f"18:move={col[0] + 60:.0f},{col[1] + 200:.0f};26:wheel=-3"), "a wheel over the open Contents list leaves the page where it is", "zero"))
                runs.append((tag, nm + "_tockeys", dict(base, PL_SCRIPT=focus + "20:key=pagedown"), "with the Contents window focused PageDown moved the page", "gt0"))
                if sz != "880x560" or True:
                    pc = None
                    rc = info.get(("k_prel_" + sz + "_" + theme, sz, theme))
                    if name == "about" and rc is not None:
                        pc = rc.rects.get("panel.controls")
                        lg = next((v for k, v in rc.rects.items() if k.startswith("card.atom.legend")), None)
                        if lg is not None:                       # the Atom chart view: click in the Legend (it takes the focus), the pointer goes to the page, the wheel / key follows
                            lx, ly = mid(lg)
                            envl = {"PL_OPEN": "about", "PL_NEWUI": "on", "PL_ATOM": "1", "PL_ATOM_SET": "view=Chart of nuclides"}
                            runs.append((tag + " Legend", nm + "_legw", dict(envl, PL_SCRIPT=f"4:move={lx:.0f},{ly:.0f};8:clickxy={lx:.0f},{ly:.0f};18:move={ex:.0f},{cy:.0f};26:wheel=-3"), "with the Legend focused the wheel moved the page", "gt0"))
                            runs.append((tag + " Legend", nm + "_legk", dict(envl, PL_SCRIPT=f"4:move={lx:.0f},{ly:.0f};8:clickxy={lx:.0f},{ly:.0f};20:key=pagedown"), "with the Legend focused PageDown moved the page", "gt0"))
                    r1 = info[(f"k_pre_{name}_{sz}_{theme}", sz, theme)]
                    pc = r1.rects.get("panel.controls")
                    if name == "about" and pc is not None:       # the Controls panel is open: click in it (focus), pointer to the page, wheel / key
                        px, py = pc[0] + 3, pc[1] + pc[3] - 4
                        runs.append((tag + " Controls", nm + "_ctlw", dict(base, PL_SCRIPT=f"4:move={px:.0f},{py:.0f};8:clickxy={px:.0f},{py:.0f};18:move={ex:.0f},{cy:.0f};26:wheel=-3"), "with the Controls panel focused the wheel moved the page", "gt0"))
                        runs.append((tag + " Controls", nm + "_ctlk", dict(base, PL_SCRIPT=f"4:move={px:.0f},{py:.0f};8:clickxy={px:.0f},{py:.0f};20:key=pagedown"), "with the Controls panel focused PageDown moved the page", "gt0"))
    prefetch([((binary, work, nm, e), {"frames": 36, "size": nm_sz(nm), "theme": nm_theme(nm)}) for _, nm, e, _, _ in runs])
    for tag, nm, e, what, want in runs:
        r = run(binary, work, nm, e, nm_sz(nm), nm_theme(nm), 36)
        s = scroll(r)
        if want == "gt0":
            check(s is not None and s[0] > 0, f"{tag}: {what} {s}")
        elif want == "max":
            check(s is not None and s[1] > 0 and abs(s[0] - s[1]) <= 1, f"{tag}: {what} {s}")
        else:
            check(s is not None and s[0] == 0, f"{tag}: {what} {s}")
    # ---- THE cause found in 5k: the scroll number moved but the text did not. Here the PICTURE must change: the column's pixels after PageDown differ from the page at the top ----
    for sz in SIZES:
        a = Bmp(info[("k_pre_about_" + sz + "_dark", sz, "dark")].bmp)
        b = Bmp(run(binary, work, f"k_about_{sz}_dark_keys", {}, sz, "dark", 36).bmp)
        col = info[("k_pre_about_" + sz + "_dark", sz, "dark")].rects["reading.column"]
        n = tot = 0
        for y in range(int(col[1]) + 160, int(col[1] + col[3]) - 10, 3):
            for x in range(int(col[0]) + 20, int(col[0] + col[2]) - 40, 3):
                tot += 1
                n += sum(abs(p - q) for p, q in zip(a.px(x, y), b.px(x, y))) > 60
        check(n > tot * 0.05, f"about {sz}: after PageDown the text on the screen moved ({n} of {tot} sample pixels changed)")
    # ---- the scrollbar: its rect exists exactly when the page can scroll; a drag of its thumb moves the page ----
    seen_short = False
    for key, r in info.items():
        if not key[0].startswith("k_pre_"):
            continue
        s = scroll(r)
        if s is None:
            continue
        has = "reading.scrollbar" in r.rects
        check(has == (s[1] > 0), f"{key[0]}: the scrollbar exists exactly when the page scrolls (max {s[1]}, bar {has})")
        seen_short = seen_short or s[1] <= 0
        if has:
            tr, th = r.rects["reading.scrollbar"], r.rects["reading.scrollthumb"]
            check(5 <= th[2] <= 20 and th[1] >= tr[1] - 0.6 and th[1] + th[3] <= tr[1] + tr[3] + 0.6, f"{key[0]}: the thumb is slim and inside its track {th} {tr}")
    print(f"scrollbar: pages with nothing to scroll seen: {seen_short}")
    sz = "2000x1127"   # 0.38.1: was 1109x882, which is no longer in SIZES (its pass-1 run is reused here)
    r0 = info[(f"k_pre_about_{sz}_dark", sz, "dark")]
    tr, th = r0.rects["reading.scrollbar"], r0.rects["reading.scrollthumb"]
    x, y0 = mid(th)
    s0 = scroll(r0)
    r = run(binary, work, "k_drag", {"PL_OPEN": "about", "PL_NEWUI": "on", "PL_SCRIPT": f"6:move={x:.0f},{y0:.0f};10:drag={x:.0f},{y0:.0f},{x:.0f},{y0 + 80:.0f}"}, sz, "dark", 40)
    s = scroll(r)
    want = 80 * s0[1] / (tr[3] - th[3])
    check(s is not None and abs(s[0] - want) <= max(3.0, 0.03 * want), f"dragging the thumb 80 px moved the page by {s and s[0]} (expected about {want:.0f})")

def nm_sz(nm):
    for sz in SIZES:
        if f"_{sz}_" in nm:
            return sz
    return "1280x800"

def nm_theme(nm):
    return "light" if "_light" in nm else "dark"

def part_contents(binary, work):
    # every entry of the Contents list jumps to its section (dropdown at 880x560, rail wider), each time after a separate hover frame; the same entry twice; after a wheel scroll
    for sz in ("880x560", "1109x882"):
        for name, env in (("about", {"PL_OPEN": "about"}), ("timeline", {"PL_LEARN": "3"})):
            base = dict(env, PL_NEWUI="on")
            r0 = run(binary, work, f"kc_pre_{name}_{sz}", base, sz, "dark", 20)
            rail = "reading.toc" in r0.rects
            anc = anchors(r0)
            col = r0.rects.get("reading.column")
            check(len(anc) >= 3 and col is not None, f"{name} {sz}: the page has a Contents list ({len(anc)} entries)")
            if col is None or len(anc) < 3:
                continue
            cx, cy = mid(col)
            box = r0.rects.get("reading.toc") or r0.rects.get("reading.contents")
            # a list that is taller than its window (the dropdown) has the later entries out of sight: those are reached by scrolling the list, not tested here
            vis = [i for i in range(len(anc)) if f"reading.tocrow.{i}" in r0.rects]
            seq, f, expect, i0 = [], 6, [], None
            def entry(i, f):
                s = []
                if not rail:
                    s.append(f"{f}:click=##readContents")
                    f += 8
                s.append(f"{f}:move=##toc{i}")
                f += 8
                s.append(f"{f}:click=##toc{i}")
                f += 12
                s.append(f"{f}:pscroll")
                return s, f + 4
            order = vis if rail else [i for i in vis if r0.rects[f"reading.tocrow.{i}"][1] + r0.rects[f"reading.tocrow.{i}"][3] <= box[1] + box[3] - 4]
            # (the rows' rects are those of a rail, or of the list as it is drawn the frame before it is opened: the dropdown's rows only exist while open, so test every entry the rect says is inside)
            if not rail and not order:
                order = list(range(min(len(anc), 8)))
            plan = [("each", i) for i in order] + [("twice", order[min(1, len(order) - 1)])] * 2 + [("afterwheel", order[min(2, len(order) - 1)])]
            for kind, i in plan:
                if kind == "afterwheel":
                    seq.append(f"{f}:move={cx:.0f},{cy:.0f}")
                    seq.append(f"{f + 8}:wheel=-4")
                    f += 16
                    seq.append(f"{f}:pscroll")
                    f += 2
                    expect.append(("wheel", i))
                s, f = entry(i, f)
                seq += s
                expect.append((kind, i))
            r = run(binary, work, f"kc_{name}_{sz}", dict(base, PL_SCRIPT=";".join(seq)), sz, "dark", f + 6)
            ps = pscrolls(r)
            check(not r.scripted_missing(), f"{name} {sz}: every named control was found {r.scripted_missing()[:3]}")
            check(len(ps) == len(expect), f"{name} {sz}: {len(expect)} readings of the page's scroll came back ({len(ps)})")
            for (kind, i), (fr, y, mx, anow) in zip(expect, ps):
                if kind == "wheel":
                    check(y > 0, f"{name} {sz}: the wheel moved the page before the Contents choice {y}")
                    continue
                want = min(anow[i], mx)                        # where that heading is in the layout of that moment (the lines are measured as they are drawn: the first look at the page differs by about 1 %)
                check(abs(y - want) <= 2, f"{name} {sz}: entry {i} ({kind}) jumps to its section: scroll {y} wanted {want} (anchor now {anow[i]}, first look {anc[i]}, max {mx})")
                check(abs(y - anc[i]) <= 0.03 * max(anc[i], 200) + 3 or want == mx, f"{name} {sz}: entry {i} ({kind}) is near the section's first-look place {y} {anc[i]}")

def part_log(binary, work):
    r = run(binary, work, "klog", {"PL_OPEN": "about", "PL_NEWUI": "on", "PL_SCRIPT": "6:move=640,400;10:wheel=-2;14:key=pagedown;18:key=ctrl+shift+f12"}, "1280x800", "dark", 40)
    path = os.path.join(os.path.abspath(work), "home_klog", ".local", "share", "PhysicsLab", "PhysicsLab", "physics-lab-input.txt")
    check(os.path.isfile(path), f"the hotkey wrote {path}")
    if not os.path.isfile(path):
        return
    t = open(path, encoding="utf-8").read()
    lines = t.splitlines()
    check(len(lines) < 200, f"the file is under 200 lines ({len(lines)})")
    for key in ("time:", "app window: 1280 x 800", "theme: dark", "motion: on", "hovered window:", "focused window:", "WantCaptureMouse=", "WantCaptureKeyboard=", "reading page: shown", "scroll y=", "max=",
                "flags=0x", "rect=", "dimmed layer: yes", "contents list:", "popups open:", "last wheel events", "wheel -2.00 at", "last key events", "PageDown down"):
        check(key in t, f"the file says \"{key}\"")
    check(any("Input log written to" in x[2] for x in r.texts), "the toast says where the log was written")

def worder(r):
    n = r.note("worder")
    return n[0].split()[1:] if n else []

def widx(order, key):
    for i, nme in enumerate(order):
        if key in nme:
            return i
    return -1

def write_png(path, w, h, rows):                                # rows: list of bytes (RGB), pure Python (no imaging library on the machine)
    import zlib, struct
    raw = b"".join(b"\x00" + r for r in rows)
    def chunk(t, d):
        c = struct.pack(">I", len(d)) + t + d
        return c + struct.pack(">I", zlib.crc32(t + d) & 0xffffffff)
    open(path, "wb").write(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0)) + chunk(b"IDAT", zlib.compress(raw)) + chunk(b"IEND", b""))

def part_windows(binary, work):
    # Settings is open, then the Benchmark window: both stay inside the app window, the Start button is in view and starts the run; the Controls handle is below an open window (z order)
    asks = {}
    under, free = [], None
    for sz in ("880x560", "1109x882", "1920x1080"):
        env = {"PL_NEWUI": "on", "PL_OPEN": "settings", "PL_BENCH": "ask", "PL_BENCH_WARMUP": "0.3", "PL_BENCH_MEASURE": "0.3"}
        r = run_app(binary, work, f"kw_{sz}", env, frames=40, size=sz)
        w, h = r.window[0], r.window[1]
        top = r.rects["topbar"][3]
        for nm in ("win.settings", "win.benchmark"):
            b = r.rects.get(nm)
            check(b is not None, f"{sz}: {nm} is drawn")
            if b:
                check(b[0] >= -0.6 and b[0] + b[2] <= w + 0.6 and b[1] >= top - 0.6 and b[1] + b[3] <= h - 8 + 0.6, f"{sz}: {nm} is inside the visible area (below the top bar, 8 px above the bottom) {b} in {w}x{h}")
        st, bw = r.rects.get("bench.start"), r.rects.get("win.benchmark")
        check(st is not None and bw is not None and st[0] >= bw[0] and st[1] >= bw[1] and st[0] + st[2] <= bw[0] + bw[2] and st[1] + st[3] <= bw[1] + bw[3] and st[1] + st[3] <= h - 8 + 0.6, f"{sz}: the Start button is inside its window and the visible area {st} {bw}")
        a = r.note("benchstage")
        asks[sz] = a[0].split()[1] if a else None
        if st:
            sx, sy = mid(st)
            r2 = run_app(binary, work, f"kw_{sz}_go", dict(env, PL_SCRIPT=f"14:move={sx:.0f},{sy:.0f};24:click=bench.start"), frames=60, size=sz)
            b2 = r2.note("benchstage")
            check(bool(b2) and b2[0].split()[1] != asks[sz], f"{sz}: a scripted click on Start started the run: {a} -> {b2}")
        order = worder(r)
        hdl = r.rects.get("handle")
        check(hdl is not None and widx(order, "##shellHandle") >= 0, f"{sz}: the handle is shown")
        for key, nm in (("###settings", "win.settings"), ("###bench", "win.benchmark")):
            check(widx(order, "##shellHandle") < widx(order, key), f"{sz}: the handle is drawn BELOW the open window {key} ({order})")
            if hdl and r.rects.get(nm):
                print(f"  {sz}: handle {'overlaps' if overlap(hdl, r.rects[nm]) else 'does not overlap'} {nm}")
        # the handle's centre belongs to the window: where it lies under the Benchmark window the pixels are not the handle's pill.
        # 0.16 stage 4: the Benchmark window's text can lie under the centre, so "flat" is no test any more; the 5x5 block there is compared with the handle's own
        # block (pill and chevron) from a size where the handle lies free, and must differ from it (collected here, judged after the loop)
        if hdl:
            bm = Bmp(r.bmp)
            hx, hy = mid(hdl)
            blk = [bm.px(int(hx) + dx, int(hy) + dy) for dx in range(-2, 3) for dy in range(-2, 3)]
            if bw and overlap(hdl, bw): under.append((sz, (hx, hy), blk))
            elif not overlap(hdl, bw or hdl) and free is None: free = blk
    check(free is not None, "a size where the handle lies free of the Benchmark window gives the handle's own pixels")
    for sz, c, blk in under:
        if free:
            same = sum(1 for p, q in zip(blk, free) if max(abs(p[i] - q[i]) for i in range(3)) <= 12)
            check(same < 25, f"{sz}: the pixels at the handle's centre {c[0]:.0f},{c[1]:.0f} are the window's, not the handle's pill and chevron ({same} of 25 pixels equal the handle's)")

def overlap(a, b):
    return a[0] < b[0] + b[2] and b[0] < a[0] + a[2] and a[1] < b[1] + b[3] and b[1] < a[1] + a[3]

def part_toggle(binary, work):
    # the top bar's Settings button toggles: click 1 opens, 2 closes, 3 opens; the Theme list too; the gear icon is drawn (crops at 2x saved for a look)
    sz = "1280x800"
    seq = "10:click=top.settings;24:click=top.settings;38:click=top.settings"
    for frames, want, what in ((20, True, "click 1 opens Settings"), (34, False, "click 2 closes it"), (48, True, "click 3 opens it again")):
        r = run_app(binary, work, f"kt_{frames}", {"PL_NEWUI": "on", "PL_SCRIPT": seq}, frames=frames, size=sz)
        check(("win.settings" in r.rects) == want, f"{what} (frame {frames}, rect {'win.settings' in r.rects})")
    seq = "10:click=top.theme;24:click=top.theme;38:click=top.theme"
    for frames, want, what in ((20, True, "click 1 opens the Theme list"), (34, False, "click 2 closes it"), (48, True, "click 3 opens it again")):
        r = run_app(binary, work, f"kt_th_{frames}", {"PL_NEWUI": "on", "PL_SCRIPT": seq}, frames=frames, size=sz)
        check((widx(worder(r), "##Popup_") >= 0) == want, f"{what} (frame {frames}, windows {worder(r)[-3:]})")
    for theme in THEMES:
        r = run_app(binary, work, f"kt_icon_{theme}", {"PL_NEWUI": "on"}, frames=20, size=sz, theme=theme)
        ic = r.rects.get("topicon.settings")
        check(ic is not None, f"{theme}: the Settings icon's place is logged")
        if ic:
            bm = Bmp(r.bmp)
            x0, y0, x1, y1 = int(ic[0]) - 6, int(ic[1]) - 6, int(ic[0] + ic[2]) + 6, int(ic[1] + ic[3]) + 6
            rows = []
            for y in range(y0, y1):
                row = bytearray()
                for x in range(x0, x1):
                    for _ in (0, 1):
                        row += bytes(bm.px(x, y))
                rows += [bytes(row)] * 2                          # 2x, each pixel doubled
            png = os.path.join(os.path.abspath(work), f"gear_{theme}.png")
            write_png(png, (x1 - x0) * 2, (y1 - y0) * 2, rows)
            bg = bm.px(x0, y0)
            ink = sum(1 for y in range(int(ic[1]), int(ic[1] + ic[3])) for x in range(int(ic[0]), int(ic[0] + ic[2])) if sum(abs(a - b) for a, b in zip(bm.px(x, y), bg)) > 60)
            check(os.path.isfile(png) and ink > 20, f"{theme}: the gear crop was saved and has {ink} inked pixels ({png})")

def part_calc(binary, work):
    # 0.49: the calculator. The top bar's button opens it; a typed line is worked out live; Enter keeps it in the history (and the settings file); the key 2 typed into it
    # does not switch labs; Esc closes it and Ctrl+= opens it again; a formula of the selected planet works out its numbers; a history line can be used again.
    seq = ("10:click=top.calc;20:text=100 km / 2 hr to km/hr;24:key=enter;30:key=2;34:key=escape;44:key=ctrl+plus;50:click=calc.clear;"
           "56:click=calc.recipe1;60:move=400,600;61:wheel=-20;66:click=calc.hist0")   # (the history is below the keypad: scrolled to first)
    env = {"PL_NEWUI": "on", "PL_SCRIPT": seq, "PL_SELECT": "Earth"}
    r = run_app(binary, work, "calc_40", env, frames=40, size="1280x800")
    check("panel.calc" not in r.rects, "Esc in the calculator's box closes it (frame 40)")
    r = run_app(binary, work, "calc_80", env, frames=80, size="1280x800")
    lines = [ln for ln in r.out.splitlines() if ln.startswith("PLCALC ")]
    def find(pred):
        return [ln for ln in lines if pred(ln)]
    check(find(lambda ln: ln.endswith("|100 km / 2 hr to km/hr") and " number=50 unit=km/hr " in ln), "100 km / 2 hr to km/hr is worked out as 50 km/hr while typing")
    check(find(lambda ln: " hist=1 " in ln and " ans=13.888888888888889 " in ln), "Enter keeps it: one history line, and ans = 13.89 m/s (50 km/hr in SI)")
    check(lines and all(" lab=0 " in ln for ln in lines), "typing (and the key 2) in the calculator never switches the lab")
    check("panel.calc" in r.rects, "Ctrl+= opens it again (frame 80)")
    rec = find(lambda ln: ln.endswith(" to km/s") and "|(" in ln)
    check(rec and " number=29." in rec[0] and " unit=km/s " in rec[0], f"Earth's formula: its speed round the Sun, 29.x km/s ({rec[:1]})")
    check(lines and lines[-1].endswith("|100 km / 2 hr to km/hr") and " number=50 " in lines[-1], f"a tap on the history puts the line back ({lines[-1:]})")
    saved = ""
    for root, _, files in os.walk(os.path.join(os.path.abspath(work), "home_calc_80")):
        for f in files:
            try:
                t = open(os.path.join(root, f), encoding="utf-8", errors="replace").read()
            except OSError:
                continue
            if "calc.0=" in t:
                saved = t
    check("calc.0=100 km / 2 hr to km/hr" in saved, "the history is in the settings file (calc.0=...)")
    # the keypad: the same line built with taps only (no typing); ⌫ takes off a whole unit name ("hr", not "h")
    keys = ["1", "0", "0", "km", "÷", "2", "hr", "to", "km", "÷", "hr", "⌫", "hr"]
    seq = "10:click=top.calc;" + ";".join(f"{14 + 4 * i}:click=calc.key.{k}" for i, k in enumerate(keys))
    r = run_app(binary, work, "calc_keys", {"PL_NEWUI": "on", "PL_SCRIPT": seq}, frames=14 + 4 * len(keys) + 8, size="1280x800")
    lines = [ln for ln in r.out.splitlines() if ln.startswith("PLCALC ")]
    check(lines and lines[-1].endswith("|100 km / 2 hr to km / hr") and " number=50 " in lines[-1], f"the keypad alone builds 100 km / 2 hr to km / hr = 50 ({lines[-1:]})")
    check(any(ln.endswith("|100 km / 2 hr to km /") for ln in lines), "⌫ takes off the whole name hr")
    # 0.50.1: the Rabi line: ħ (typed hbar) makes the answer an angular frequency; the note says so and gives the cyclic f = ω/2π = 1279.5 MHz
    seq = "10:click=top.calc;20:text=e*a0*1e5 V/m/hbar to MHz"
    r = run_app(binary, work, "calc_rabi", {"PL_NEWUI": "on", "PL_SCRIPT": seq}, frames=30, size="1280x800")
    lines = [ln for ln in r.out.splitlines() if ln.startswith("PLCALC ") and ln.endswith("|e*a0*1e5 V/m/hbar to MHz")]
    check(lines and " number=8039.617101 " in lines[-1] and "angular frequency" in lines[-1] and "1279.5" in lines[-1],
          f"the Rabi line shows 8039.617101 MHz with the angular-frequency note and f = 1279.5 MHz ({lines[-1:]})")
    # 0.50.2: the Larmor line μp B / h (ħ divided by h) is a cyclic frequency, so NO ω note (0.50.1 offered 3.388 MHz)
    seq = "10:click=top.calc;20:text=gp*e*hbar/(2*mp)*1 T/(2*hP) to MHz"
    r = run_app(binary, work, "calc_larmor", {"PL_NEWUI": "on", "PL_SCRIPT": seq}, frames=30, size="1280x800")
    lines = [ln for ln in r.out.splitlines() if ln.startswith("PLCALC ") and ln.endswith("|gp*e*hbar/(2*mp)*1 T/(2*hP) to MHz")]
    check(lines and " number=21.28873923 " in lines[-1] and "angular" not in lines[-1] and "ω/2π" not in lines[-1],
          f"the Larmor line μp B/h shows 21.28873923 MHz with no ω note ({lines[-1:]})")

def part_matrix(binary, work):
    # 0.54.2: matrices in the calculator. A Pauli product shows a 2x2 grid with the right entries; eig shows two values; a 4x4 complex answer (the widest case) is a
    # grid that fits the panel, at 880x560 and 1280x800 (the text and the panel's width differ with the window); a refusal shows its message. One app start per size:
    # each line replaces the one before (Ctrl+A, then typing).
    wide = ("kron([[1/3 + i/7, -2/3 - 5i/7], [1/7 - i/3, 3/7 + 2i/3]], [[1/9 + i/11, -1/3 + i/13], [2/7 - 3i/11, -5/9 - i/3]])")
    lines = ["sigmax * sigmay", "eig([[0.3/2, 0.4/2], [0.4/2, -0.3/2]] eV) to eV", wide, "kron(sigmay, sigmay)", "[[1, 2], [3, 4]] + 1"]
    seq = "10:click=top.calc;" + ";".join(f"{20 + 10 * i}:key=ctrl+a;{23 + 10 * i}:text={ln}" for i, ln in enumerate(lines))
    for size in ("880x560", "1280x800"):
        w, h = (int(v) for v in size.split("x"))
        r = run_app(binary, work, "calc_matrix_" + size, {"PL_NEWUI": "on", "PL_SCRIPT": seq}, frames=20 + 10 * len(lines) + 6, size=size)
        out = [ln for ln in r.out.splitlines() if ln.startswith("PLCALC ")]
        def last(line):
            got = [ln for ln in out if ln.endswith("|" + line)]
            return got[-1] if got else ""
        def field(ln, name):
            m = re.search(r" " + name + r"=(\S+)", ln)
            return m.group(1) if m else ""
        a = last(lines[0])
        check(" ok=1 " in a and " number=[[i, 0], [0, -i]] unit= " in a and " grid=2x2 gfit=1 gdigits=10 " in a, f"{size}: sigmax * sigmay shows the 2x2 grid [[i, 0], [0, -i]] ({a})")
        a = last(lines[1])
        check(" ok=1 " in a and " number=-0.25, 0.25 unit=eV " in a and " list=2 " in a and " grid=" not in a, f"{size}: eig of the two-level matrix shows two values, -0.25 and 0.25 eV ({a})")
        a = last(lines[2])
        fits = " ok=1 " in a and " grid=4x4 gfit=1 " in a and field(a, "gw").isdigit() and int(field(a, "gw")) <= int(field(a, "room"))
        check(fits, f"{size}: a 4x4 complex answer is a grid no wider than the panel ({a[:260]})")
        a = last(lines[3])
        check(" ok=1 " in a and " grid=4x4 gfit=1 gdigits=10 gsplit=0 " in a and " number=[[0, 0, 0, -1], [0, 0, 1, 0], [0, 1, 0, 0], [-1, 0, 0, 0]] " in a,
              f"{size}: kron(sigmay, sigmay) is a 4x4 grid with all its digits ({a[:260]})")
        a = last(lines[4])
        check(" ok=0 " in a and "identity" in a and " grid=" not in a, f"{size}: matrix + number is refused with its message ({a[:260]})")
        pc = r.rects.get("panel.calc")
        check(pc is not None and pc[0] >= -0.6 and pc[0] + pc[2] <= w + 0.6, f"{size}: the calculator panel is inside the window ({pc})")

def part_data(binary, work):
    # 0.51: the Data panel. The top bar's button opens it on the lab's first table with its first worked analysis (Kepler's third law: the planets' periods against a,
    # a power law: n = 3/2 within the giants' mass ratio); the user's own data (the example: a pendulum with uncertainties) is fitted weighted; Send puts the parameters
    # into the calculator, which opens; Esc closes the calculator, then the panel. Then Menu > Copy setup / Paste setup brings back the Atom Lab and its element.
    def pldata(r):
        return [ln for ln in r.out.splitlines() if ln.startswith("PLDATA ")]
    seq = ("10:click=top.data;16:click=data.tables;20:click=data.table.user;24:move=400,500;25:wheel=-10;30:click=data.model;34:move=400,500;35:wheel=-10;"
           "40:click=data.model.power;46:click=data.send;52:key=escape;56:key=escape")
    r = run_app(binary, work, "data_fit", {"PL_NEWUI": "on", "PL_SCRIPT": seq}, frames=62, size="1280x800")
    lines = pldata(r)
    kep = [ln for ln in lines if " table=planets " in ln and " model=power " in ln and " ok=1 " in ln]
    p1 = float(re.search(r" p1=(\S+)", kep[0]).group(1)) if kep else 0
    check(kep and abs(p1 - 1.5) < 1e-3, f"the Data panel opens on the planets with Kepler's third law fitted: n = {p1} (3/2 within 1e-3)")
    usr = [ln for ln in lines if " table=user " in ln and " model=power " in ln and " ok=1 " in ln and " weighted=1 " in ln]
    check(usr, "the example data are fitted with a power law, weighted by their uncertainties")
    sent = [ln for ln in lines if "send=In the calculator now: fit_Apow" in ln]
    check(sent, "Send to calculator names fit_Apow")
    check(not r.scripted_missing(), f"every scripted control was there ({r.scripted_missing()[:2]})")
    check("panel.data" not in r.rects and "panel.calc" not in r.rects, "Esc closes the calculator, then the Data panel")
    seq = "10:click=top.menu;14:click=menu.copysetup;20:key=1;26:click=top.menu;30:click=menu.pastesetup;36:click=top.data"
    r = run_app(binary, work, "data_setup", {"PL_NEWUI": "on", "PL_SCRIPT": seq, "PL_ATOM": "1", "PL_ATOM_SET": "Z=26"}, frames=44, size="1280x800")
    lines = pldata(r)
    check(lines and " table=ionlines " in lines[-1] and " rows=91 " in lines[-1], f"Paste setup brings back the Atom Lab with iron (its one-electron ion's 91 NIST lines): {lines[-1:]}")
    check(not r.scripted_missing(), f"every scripted control was there ({r.scripted_missing()[:2]})")
    # 0.51.1: the moons' worked analysis holds n at 3/2 (A alone is fitted: Jupiter's GM within 0.2 % of the card's 1.2668e17 m^3/s^2); unticking the hold frees n again;
    # the log axes switch on (the drawing only: the fit stays the same)
    seq = "10:click=top.data;14:click=data.tables;18:click=data.table.moons;24:click=data.preset0;30:click=data.logx;34:click=data.logy;38:move=400,500;39:wheel=-10;44:click=data.hold.n"
    r = run_app(binary, work, "data_hold", {"PL_NEWUI": "on", "PL_SCRIPT": seq}, frames=52, size="1280x800")
    lines = pldata(r)
    held = [ln for ln in lines if " table=moons " in ln and " held1=1.5 " in ln and " ok=1 " in ln]
    gm = 0
    if held:
        a = float(re.search(r" p0=(\S+)", held[0]).group(1))
        gm = 4 * 3.141592653589793 ** 2 * 1e9 / (a * 86400) ** 2
    check(held and abs(gm / 1.2668e17 - 1) < 2e-3, f"Jupiter's moons with n held at 3/2: GM = {gm:.5g} m^3/s^2 (the card's within 0.2 %)")
    check(lines and " logx=1 logy=1 " in lines[-1] and " held1=" not in lines[-1] and " p1=1.5001" in lines[-1], f"log axes on, and n free again after Hold is unticked: {lines[-1:]}")
    check(not r.scripted_missing(), f"every scripted control was there ({r.scripted_missing()[:2]})")

def part_about(binary, work):
    # 0.74.0 (owner, 22 Nov 2026): the developer's paragraph of About ends with "Free Palestine" in bold and a small DRAWN flag of Palestine (html_view.cpp lrnFlagPS: 1:2, the height of the
    # line's letters, right after the words). One run: the page as it opens (the paragraph is in the first card), the words drawn and the flag's box in view.
    r = run(binary, work, "k_about_flag", {"PL_OPEN": "about", "PL_NEWUI": "on"}, "1280x800", "dark", 30)
    words = [t for t in r.texts if t[2].startswith("Free Palestine")]
    flag = r.rects.get("page.flagps")
    check(bool(words), "About: the words 'Free Palestine' are drawn")
    check(flag is not None and abs(flag[2] - 2 * flag[3]) < 0.5 and 8 <= flag[3] <= 30 and 0 <= flag[1] <= 800 - flag[3], f"About: the drawn flag is there, 1:2, about a line's letters high ({flag})")

if __name__ == "__main__":
    binary, work, part = sys.argv[1], sys.argv[2], sys.argv[3]
    os.makedirs(work, exist_ok=True)
    {"pages": part_pages, "contents": part_contents, "log": part_log, "windows": part_windows, "toggle": part_toggle, "calc": part_calc, "matrix": part_matrix, "data": part_data, "about": part_about}[part](binary, work)
    print(f"{checks} checks, {fails} failed")
    sys.exit(1 if fails else 0)
