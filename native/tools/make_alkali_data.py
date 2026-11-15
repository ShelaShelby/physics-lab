#!/usr/bin/env python3
"""0.46: the measured levels and lines of the one-valence-electron atoms (Li, Na, K, Rb, Cs) for their photon jumps, from the NIST ASD pages saved as received in
data/asd/alkali (README there: query, date). Writes src/atom/alkali_data.inc.

    python3 native/tools/make_alkali_data.py

Kept: the levels of the valence orbitals the app draws (tools/dhf/valence.py VALENCE: the core plus one electron in n l), each J separately, as ASD gives them (no bracketed values);
the lines between two kept levels that ASD lists as electric dipole (Type empty), with ASD's transition probability Aki where it gives one (NAN where not). Nothing is computed or filled in here: every number is ASD's text.
"""
import csv, gzip, os, sys
HERE = os.path.dirname(os.path.abspath(__file__)); REPO = os.path.dirname(HERE)
sys.path.insert(0, os.path.join(HERE, 'dhf'))
from valence import VALENCE
VALENCE = dict(VALENCE)
# 0.47.2: francium (no frozen-core orbitals; its clouds come only from GRASP2018, tools/grasp): the orbitals of its 10 lowest NIST levels.
# NIST ASD Fr I (data/jumps/asd, fetched 9 Nov 2026) writes the configuration without the core ("7p").
VALENCE[87] = [(7, 0), (7, 1), (6, 2), (8, 0), (8, 1), (7, 2)]   # 9s left out: its GRASP cloud stayed 11-20 % off NIST (REPORT.md 0.47.2)
SPEC = {3: ('LiI', '1s2'), 11: ('NaI', '2p6'), 19: ('KI', '3p6'), 37: ('RbI', '4p6'), 55: ('CsI', '5p6'), 87: ('FrI', '')}
DIRS = {87: os.path.join('data', 'jumps', 'asd')}   # the others: data/asd/alkali
LET = 'spdfg'

def cells(path):
    if not os.path.exists(path) and os.path.exists(path + '.gz'): path += '.gz'
    f = gzip.open(path, 'rt', encoding='latin-1') if path.endswith('.gz') else open(path, encoding='latin-1')
    rows = list(csv.reader(f, delimiter='\t'))
    head = [h.strip() for h in rows[0]]
    for r in rows[1:]:
        if len(r) < 3: continue
        yield dict(zip(head, [c.strip().strip('"').lstrip('=').strip('"') for c in r]))

def orb(conf, core):
    """'2p6.3p' -> (3, 1) when the core is the given last closed shell; else None"""
    p = conf.split('.')
    if core == '': p = [''] + p
    if len(p) != 2 or p[0] != core or len(p[1]) != 2 or not p[1][0].isdigit() or p[1][1] not in LET: return None
    return int(p[1][0]), LET.index(p[1][1])

def j2(s):
    s = s.strip()
    if not s or ',' in s or not s.replace('/', '').isdigit(): return None
    return int(s.split('/')[0]) if '/' in s else 2 * int(s)

def main():
    lv = ['// Made by native/tools/make_alkali_data.py from data/asd/alkali (NIST ASD, fetched 8 Nov 2026) and data/jumps/asd (Fr, fetched 9 Nov 2026). Do not edit.',
          'struct AlkLevelRow { int Z, n, l, j2; double cm, unc; const char* cmText; };',
          'static const AlkLevelRow kAlkLevels[] = {']
    ln = ['struct AlkLineRow { int Z, nLo, lLo, j2Lo, nHi, lHi, j2Hi; double obsNm, ritzNm, aki; const char* acc; };', 'static const AlkLineRow kAlkLines[] = {']
    for Z, (tag, core) in sorted(SPEC.items()):
        want = set(VALENCE[Z]); have = {}
        for d in cells(os.path.join(REPO, DIRS.get(Z, os.path.join('data', 'asd', 'alkali')), tag + '_levels.tsv')):
            o = orb(d.get('Configuration', ''), core); J = j2(d.get('J', ''))
            if o is None or o not in want or J is None or d.get('Prefix') or d.get('Suffix'): continue
            if (o, J) in have: continue
            have[(o, J)] = d['Level (cm-1)']
            unc = d.get('Uncertainty (cm-1)', '') or 'NAN'
            lv.append(f'    {{{Z}, {o[0]}, {o[1]}, {J}, {d["Level (cm-1)"]}, {unc}, "{d["Level (cm-1)"]}"}},')
        missing = [(o, J) for o in want for J in ([2 * o[1] - 1, 2 * o[1] + 1] if o[1] else [1]) if (o, J) not in have]
        if missing: print(f'Z {Z}: levels missing in ASD: {missing}')
        nl = 0
        for d in cells(os.path.join(REPO, DIRS.get(Z, os.path.join('data', 'asd', 'alkali')), tag + '_lines.tsv')):
            a = orb(d.get('conf_i', ''), core); b = orb(d.get('conf_k', ''), core)
            if a is None or b is None or a not in want or b not in want: continue
            Ji, Jk = j2(d.get('J_i', '')), j2(d.get('J_k', ''))
            if Ji is None or Jk is None or d.get('Type'): continue
            if abs(a[1] - b[1]) != 1: continue
            obs = d.get('obs_wl_vac(nm)') or 'NAN'; ritz = d.get('ritz_wl_vac(nm)') or 'NAN'
            ln.append(f'    {{{Z}, {a[0]}, {a[1]}, {Ji}, {b[0]}, {b[1]}, {Jk}, {obs}, {ritz}, {d.get("Aki(s^-1)") or "NAN"}, "{d.get("Acc", "")}"}},'); nl += 1
        print(f'Z {Z}: {len(have)} levels, {nl} lines')
    lv.append('};'); ln.append('};')
    open(os.path.join(REPO, 'src', 'atom', 'alkali_data.inc'), 'w').write('\n'.join(lv + ln) + '\n')

if __name__ == '__main__': main()
