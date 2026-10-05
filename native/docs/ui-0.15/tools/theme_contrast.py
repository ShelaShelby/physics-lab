#!/usr/bin/env python3
"""0.15 stage 1: WCAG 2.x contrast ratios of the 7 flat colours of every theme (src/app/app_text.inc kThemes and src/app/theme_style.cpp kNativeThemes;
Light's accent is the native override #2d6ae3). W3C WCAG 2.x relative luminance and (L1 + 0.05) / (L2 + 0.05). Flat colours only: the glass, gloss
and texture of Aero and Y2K are style data on top and are tested by tests/test_theme_stage2.inc."""
T = {
 'light': ('#f7f7f5', '#1c1c1a', '#6b6b66', '#fff', '#dcdcd6', '#2d6ae3', '#fff'),
 'dark': ('#151517', '#eee', '#9a9a94', '#1e1e21', '#333338', '#6c9cff', '#fff'),
 'black': ('#000', '#ececec', '#8f8f8a', '#0b0b0c', '#28282c', '#6c9cff', '#fff'),
 'midnight': ('#0a1226', '#e6ecfa', '#8d9bbd', '#111c38', '#26365f', '#7fb0ff', '#06122b'),
 'solar': ('#1c1510', '#f1e6d3', '#a8967c', '#271d15', '#463729', '#e8a838', '#1d1408'),
 'paper': ('#f3ecdc', '#2b2419', '#6f6350', '#fbf6ea', '#d9ceb5', '#9a3b1d', '#fff'),
 'sage': ('#0f1715', '#e1ece8', '#8ea39b', '#16211e', '#2a3b36', '#54c8a4', '#05130e'),
 'contrast': ('#000', '#fff', '#d2d2d2', '#000', '#b5b5b5', '#ffd60a', '#000'),
 'synthwave': ('#120a26', '#f6ecff', '#ad97d6', '#1b1035', '#4a2a85', '#ff4fd8', '#1a0630'),
 'monochrome': ('#fff', '#000', '#595959', '#fff', '#b3b3b3', '#000', '#fff'),
 'aero': ('#8fd3ee', '#031f2e', '#0a3348', '#ffffff', '#4c9fbd', '#05501f', '#04260d'),
 'y2kice': ('#e4f1ff', '#06142e', '#1b3358', '#f4f9ff', '#6a8cba', '#16479a', '#06142e'),
 'y2knight': ('#060a16', '#eaf4ff', '#d5e2f7', '#0c1428', '#5878b8', '#a6cdff', '#031026'),
}
def rgb(h):
    h = h.lstrip('#')
    if len(h) == 3: h = ''.join(c * 2 for c in h)
    return [int(h[i:i + 2], 16) / 255 for i in (0, 2, 4)]
def lum(h):
    f = lambda c: c / 12.92 if c <= 0.03928 else ((c + 0.055) / 1.055) ** 2.4
    r, g, b = [f(c) for c in rgb(h)]
    return 0.2126 * r + 0.7152 * g + 0.0722 * b
def cr(a, b):
    la, lb = sorted((lum(a), lum(b)), reverse=True)
    return (la + 0.05) / (lb + 0.05)
if __name__ == '__main__':
    print('| theme | text/page | muted/page | muted/card | accent/card | line/page |')
    print('|---|---|---|---|---|---|')
    for k, (bg, fg, mu, card, line, acc, on) in T.items():
        print(f'| {k} | {cr(fg, bg):.2f} | {cr(mu, bg):.2f} | {cr(mu, card):.2f} | {cr(acc, card):.2f} | {cr(line, bg):.2f} |')
