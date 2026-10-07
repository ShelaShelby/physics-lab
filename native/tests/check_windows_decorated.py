#!/usr/bin/env python3
# 0.14 stage 1.1 test: every Dear ImGui window and pop-up the app opens must be painted by the theme (panelDecor), because in a glass theme the
# window's own background is see-through or flat. Stage 1 missed the guides, the changelog, the benchmark windows and two pop-ups: the Learn cards showed through the text.
#   ImGui::Begin( / BeginPopup( / BeginPopupModal(  must have  panelDecor(  in the next 8 lines.
#   panelDecor(app::PanelKind::Docked)  must have  pushDockedBg()  in the 10 lines before it (docked windows are see-through on purpose, everything else is solid).
# 0.14 stage 2, two more rules:
#   ImGui::Begin( of a window WITH a title bar (no NoTitleBar / NoDecoration in its flags) must have pushFloatStyle() in the 8 lines before: that gives the title bar its text colour
#     and font (a styled title bar is dark in Aero, and the normal text colour would be unreadable on it).
#   ImGui::BeginTooltip / SetTooltip / BeginCombo / Combo are never called directly (outside ui/style_draw.cpp): ui::BeginTooltip / SetTooltip / BeginCombo / Combo give them the theme's face.
# A new window that really needs no decoration goes in ALLOWED with the reason (none yet).
import re, sys, pathlib
ALLOWED = {   # "file.cpp:Begin-argument-start": "reason"
    "reading.cpp:\"##readBlock\"": "0.15 stage 3: an invisible, transparent window that only catches a click on the dimmed picture behind the reading view; it has nothing to decorate",
    "reading.cpp:\"##readBar\"": "0.15 stage 5k: the reading page's slim scrollbar: a transparent window of 16 px that only holds the draggable thumb, which it draws itself; it has nothing to decorate",
}
root = pathlib.Path(sys.argv[1]) if len(sys.argv) > 1 else pathlib.Path(__file__).resolve().parent.parent / "src"
bad = []
opens = re.compile(r'ImGui::(Begin|BeginPopup|BeginPopupModal)\(([^,)]*)')
n = 0
for f in sorted(root.rglob("*.cpp")):
    if f.name in ("style_draw.cpp",): continue
    lines = f.read_text(encoding="utf-8", errors="replace").splitlines()
    for i, ln in enumerate(lines):
        code = ln.split("//")[0]
        m = opens.search(code)
        if m:
            n += 1
            key = f"{f.name}:{m.group(2).strip()[:30]}"
            if key in ALLOWED: continue
            if not any("panelDecor(" in x for x in lines[i:i + 9]):
                bad.append(f"{f.name}:{i + 1}: {m.group(0)}...) has no panelDecor( in the next 8 lines")
        if re.search(r'ImGui::(BeginTooltip|SetTooltip|BeginCombo|Combo)\(', code):
            bad.append(f"{f.name}:{i + 1}: use ui::{re.search(r'ImGui::(BeginTooltip|SetTooltip|BeginCombo|Combo)', code).group(1)} (ui/style_draw.h), not Dear ImGui's own")
        if m and m.group(1) == "Begin" and not any(k in " ".join(lines[i:i + 3]) for k in ("NoTitleBar", "NoDecoration")):
            if not any("pushFloatStyle()" in x for x in lines[max(0, i - 8):i]):
                bad.append(f"{f.name}:{i + 1}: a window with a title bar without pushFloatStyle() in the 8 lines before its Begin")
        if "panelDecor(app::PanelKind::Docked)" in code:
            if not any("pushDockedBg()" in x for x in lines[max(0, i - 10):i]):
                bad.append(f"{f.name}:{i + 1}: a docked panel without pushDockedBg() before its Begin")
print(f"{n} windows and pop-ups checked, {len(bad)} problems")
for b in bad: print("PROBLEM", b)
sys.exit(1 if bad else 0)
