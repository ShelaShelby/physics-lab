#!/usr/bin/env python3
"""0.15 stage 2: the least opacity of a glass panel so that text on it passes WCAG 2.x 4.5:1 over the WORST picture that can be behind it.
Glass = panel colour at opacity a over the picture. Worst picture for light text on a dark panel = pure white; for dark text on a light panel = pure black
(the lab pictures do contain both: white chart tiles, the Aero chart's white page, the Sun's glare, the black sky). Colours: the 7 flat colours of each theme
(theme_contrast.py). Text checked: fg, muted and accent (the colours text actually uses on a panel). a is searched in steps of 0.01.
Result column 'use' = the smallest step above the highest minimum, plus 0.05 of margin, never above 0.97 (below 1 so a hint of the picture shows)."""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from theme_contrast import T, cr, rgb, lum
def blend(panel, under, a):
    p, u = rgb(panel), rgb(under)
    return '#' + ''.join(f'{round((x * a + y * (1 - a)) * 255):02x}' for x, y in zip(p, u))
def least(panel, text, under):
    for k in range(0, 101):
        if cr(text, blend(panel, under, k / 100)) >= 4.5: return k / 100
    return None                                   # no opacity works
if __name__ == '__main__':
    print('| theme | panel | worst picture | min a: text | muted | accent | use |')
    print('|---|---|---|---|---|---|---|')
    for k, (bg, fg, mu, card, line, acc, on) in T.items():
        light = lum(card) > 0.4
        under = '#000000' if light else '#ffffff'
        m = [least(card, c, under) for c in (fg, mu, acc)]
        ok = [x for x in m if x is not None]
        worst = max(ok) if ok else None
        use = min(0.97, round(worst + 0.05, 2)) if worst is not None else None
        fmt = lambda x: 'never' if x is None else f'{x:.2f}'
        print(f'| {k} | {card} | {"black" if light else "white"} | {fmt(m[0])} | {fmt(m[1])} | {fmt(m[2])} | {"-" if use is None else f"{use:.2f}"} |')
