#!/usr/bin/env python3
"""0.54.0: the measured Lande g of the Atom Lab's levels, from the NIST ASD pages with the Lande column saved as received in data/jumps/asd_g
(tools/fetch_lande_asd.py, fetched 14 Nov 2026). Writes src/atom/lande_data.inc.

    python3 native/tools/make_lande_data.py

Kept: every level of H I, the alkalis (one electron outside the closed core, keyed n, l, J; the app looks up only the levels it has) and the level atoms (make_level_data.py's PART2, keyed by ASD's
configuration, term and J text as the level table holds it) for which ASD prints a Lande g. Nothing is computed here: the g is ASD's text, with ASD's reference code.
A level atom's level that is not in the lab's level table is left out (counted on the screen).
"""
import os, sys
HERE = os.path.dirname(os.path.abspath(__file__)); REPO = os.path.dirname(HERE)
sys.path.insert(0, HERE)
import make_level_data as mk_lvl
# the alkalis' spectra and closed cores as tools/make_alkali_data.py has them (that file needs numpy/scipy through tools/dhf, so they are repeated here)
SPEC = {3: ('LiI', '1s2'), 11: ('NaI', '2p6'), 19: ('KI', '3p6'), 37: ('RbI', '4p6'), 55: ('CsI', '5p6'), 87: ('FrI', '')}
LET = 'spdfg'
def orb(conf, core):
    """'2p6.3p' -> (3, 1) when the core is the given last closed shell; else None (as make_alkali_data.orb)"""
    p = conf.split('.')
    if core == '': p = [''] + p
    if len(p) != 2 or p[0] != core or len(p[1]) != 2 or not p[1][0].isdigit() or p[1][1] not in LET: return None
    return int(p[1][0]), LET.index(p[1][1])
G = os.path.join(REPO, 'data', 'jumps', 'asd_g')

def rows(tag):
    for d in mk_lvl.cells(os.path.join(G, tag + '_levels_g.tsv.gz')):
        g = d.get('Lande', '')
        if g and g.replace('.', '').replace('-', '').isdigit(): yield d, g

def main():
    out = ['// Made by native/tools/make_lande_data.py from data/jumps/asd_g (NIST ASD levels with the Lande column, fetched 14 Nov 2026). Do not edit.',
           '// conf/term empty: an alkali level keyed by n, l (the valence orbital); n = 0: a level atom keyed by ASD\'s configuration and term.',
           'struct LandeRow { int Z, n, l, j2; const char* conf; const char* term; double g; const char* gText; const char* ref; };',
           'static const LandeRow kLandeRows[] = {']
    for Z, (tag, core) in sorted(SPEC.items()):
        n = 0
        for d, g in rows(tag):
            o = orb(d.get('Configuration', ''), core); J = mk_lvl.j2(d.get('J', ''))
            if o is None or J is None: continue
            out.append(f'    {{{Z}, {o[0]}, {o[1]}, {J}, "", "", {g}, "{g}", "{d.get("Reference", "")}"}},'); n += 1
        print(f'{tag}: {n} levels with a measured g')
    for X, (Z, _) in sorted(mk_lvl.PART2.items(), key=lambda kv: kv[1][0]):
        have = {(L['conf'], L['term'], L['J2']) for L in mk_lvl.atom(X)['levels']}
        n = miss = 0
        for d, g in rows(X + 'I'):
            k = (d.get('Configuration', ''), d.get('Term', ''), mk_lvl.j2(d.get('J', '')))
            if k not in have: miss += 1; continue
            out.append(f'    {{{Z}, 0, 0, {k[2]}, "{k[0]}", "{k[1]}", {g}, "{g}", "{d.get("Reference", "")}"}},'); n += 1
        print(f'{X}I: {n} levels with a measured g' + (f' ({miss} not in the level table)' if miss else ''))
    n = sum(1 for _ in rows('HI'))
    print(f'HI: {n} levels with a measured g (the one-electron atom: none kept)' if n else 'HI: ASD lists no Lande g')
    out.append('};')
    open(os.path.join(REPO, 'src', 'atom', 'lande_data.inc'), 'w').write('\n'.join(out) + '\n')

if __name__ == '__main__':
    main()
