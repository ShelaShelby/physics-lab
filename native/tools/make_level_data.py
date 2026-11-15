#!/usr/bin/env python3
"""0.47.3: the measured levels and lines of the atoms whose jumps use level clouds (PLAN_JUMPS.md parts 2..), and the jump status of all 118 atoms.
From the NIST ASD pages saved as received in data/jumps/asd (README there) and data/jumps/elements.tsv. Writes src/atom/level_data.inc.

    python3 native/tools/make_level_data.py            (also imported by tools/grasp/run_two.py: the levels to compute)

Rules (owner decisions D1, D2 of 9 Nov 2026, PLAN_JUMPS.md section 11):
  - a level is kept when ASD names it (configuration, term, one J) and gives its energy. ASD writes some energies in brackets: values calculated by theory that NIST
    recommends over the measured ones (helium: Drake's calculations, matching the measured lines); they are kept and flagged ("calc").
  - D1 "measured": a level is measured when at least one OBSERVED classified line (ASD gives its observed wavelength) touches it; a jump is offered only between two
    measured levels.
  - a line is kept when ASD lists it as electric dipole (Type empty), both its levels are kept levels of the "cloud" set (below), with ASD's Aki and grade where it
    gives one. The wavelength shown is the observed one where ASD has it, else ASD's Ritz value (from the two levels).
  - D2 the levels that get a cloud: the ground level; both levels of each of the 30 strongest lines with an ASD Aki (by g_k Aki, g_k = 2 J_k + 1, the upper level)
    among measured levels; the lowest level of each of the 10 lowest configurations. A level above the first ionization limit (ASD's "Limit" row: autoionizing) or
    whose configuration is not two electrons outside the closed core (PART2 atoms; e.g. Zn 3d9.4s2.4p) is listed with "no cloud computed" and the reason.
Nothing is computed or filled in here: every number is ASD's text.
"""
import csv, gzip, os, re, sys
HERE = os.path.dirname(os.path.abspath(__file__)); REPO = os.path.dirname(HERE)
ASD = os.path.join(REPO, 'data', 'jumps', 'asd')
# the atoms of part 2 (0.47.3): two s electrons outside closed shells. Core subshells (ASD writes some of them in its configurations; they are dropped):
PART2 = {'He': (2, '1s'), 'Be': (4, '2s'), 'Mg': (12, '3s'), 'Ca': (20, '4s'), 'Sr': (38, '5s'), 'Ba': (56, '6s'), 'Ra': (88, '7s'),
         'Zn': (30, '4s'), 'Cd': (48, '5s'), 'Hg': (80, '6s'), 'Yb': (70, '6s')}
FULL = {'s': 2, 'p': 6, 'd': 10, 'f': 14, 'g': 18}
NSTRONG, NCONF = 30, 10
GAUGE_MAX = 50.0   # D4 (0.47.4): see rates()


def cells(path):
    rows = list(csv.reader(gzip.open(path, 'rt', encoding='latin-1'), delimiter='\t'))
    head = [h.strip() for h in rows[0]]
    for r in rows[1:]:
        if len(r) < 3: continue
        yield dict(zip(head, [c.strip().strip('"').lstrip('=').strip('"') for c in r]))


def j2(s):
    s = s.strip()
    if not s or ',' in s or not s.replace('/', '').isdigit(): return None
    return int(s.split('/')[0]) if '/' in s else 2 * int(s)


def num(s):
    s = s.strip().strip('[]()?+x ')
    try: return float(s)
    except ValueError: return None


def valence(conf, core_n):
    """the configuration's electrons outside closed shells: '3s.3p' -> [(3,0),(3,1)]; '2p6.3s2' -> [(3,0),(3,0)]; None when it is not two electrons outside full shells"""
    out = []
    for t in conf.split('.'):
        m = re.fullmatch(r'(\d+)([spdfg])(\d*)', t)
        if not m: return None
        n, l, k = int(m.group(1)), 'spdfg'.index(m.group(2)), int(m.group(3) or 1)
        if k == FULL['spdfg'[l]] and not (n == core_n and l == 0): continue   # a full core subshell written out (2p6, 3d10, 4f14)
        out += [(n, l)] * k
    return sorted(out) if len(out) == 2 else None


def atom(X, runs=True):
    """{'levels': [dict], 'lines': [dict], 'limit': cm} for an atom of PART2; level dicts: conf, term, J2, odd, cm, text, calc, measured, cloud, why, val.
    runs: True for the app (cloud = computed, from data/jumps/grasp/<X>/levels.tsv); False for the GRASP drivers (cloud = to be computed)"""
    Z, vs = PART2[X]; core_n = int(vs[:-1])
    levels, limit = [], None
    for d in cells(os.path.join(ASD, X + 'I_levels.tsv.gz')):
        if d.get('Term') == 'Limit':
            if limit is None: limit = num(d['Level (cm-1)'])
            continue
        J = j2(d.get('J', '')); cm = num(d.get('Level (cm-1)', ''))
        conf, term = d.get('Configuration', ''), d.get('Term', '')
        if J is None or cm is None or not conf or not term or d.get('Suffix', '') not in ('', ']'): continue
        if any(L['conf'] == conf and L['term'] == term and L['J2'] == J for L in levels): continue
        levels.append({'conf': conf, 'term': term, 'J2': J, 'odd': term.endswith('*') or '*' in term, 'cm': cm,
                       'text': d['Level (cm-1)'].strip('[]'), 'calc': d.get('Prefix') == '[', 'measured': False, 'cloud': False, 'why': ''})
    key = {(L['conf'], L['term'], L['J2']): i for i, L in enumerate(levels)}
    raw = []
    for d in cells(os.path.join(ASD, X + 'I_lines.tsv.gz')):
        a = key.get((d.get('conf_i', ''), d.get('term_i', ''), j2(d.get('J_i', '') or '')))
        b = key.get((d.get('conf_k', ''), d.get('term_k', ''), j2(d.get('J_k', '') or '')))
        if a is None or b is None or d.get('Type'): continue
        obs = d.get('obs_wl_vac(nm)', ''); ritz = d.get('ritz_wl_vac(nm)', '')
        if num(obs) is not None: levels[a]['measured'] = levels[b]['measured'] = True
        raw.append({'lo': a, 'hi': b, 'obs': obs, 'ritz': ritz, 'A': d.get('Aki(s^-1)', ''), 'acc': d.get('Acc', '')})
    # D2
    want = {0}
    strong = sorted([r for r in raw if r['A'] and levels[r['lo']]['measured'] and levels[r['hi']]['measured']],
                    key=lambda r: -(levels[r['hi']]['J2'] + 1) * float(r['A']))
    for r in strong[:NSTRONG]: want |= {r['lo'], r['hi']}
    low = {}
    for i, L in enumerate(levels):
        if L['conf'] not in low or L['cm'] < levels[low[L['conf']]]['cm']: low[L['conf']] = i
    for c in sorted(low, key=lambda c: levels[low[c]]['cm'])[:NCONF]: want.add(low[c])
    for i in sorted(want):
        L = levels[i]; L['val'] = valence(L['conf'], core_n)
        if L['val'] is None: L['why'] = 'its configuration opens a closed shell (not two electrons outside closed shells)'
        elif limit and L['cm'] > limit: L['why'] = 'it lies above the ionization limit (autoionizing)'
        elif i and not L['measured']: L['why'] = 'no observed line touches it'
        else: L['cloud'] = True
    # what the GRASP runs gave (data/jumps/grasp/<X>/levels.tsv, tools/grasp/multipoles_two.py): a D2 level without a computed cloud says why
    done = os.path.join(REPO, 'data', 'jumps', 'grasp', X, 'levels.tsv')
    if not runs: pass
    elif not os.path.exists(done):
        for i in want:
            if levels[i]['cloud']: levels[i]['cloud'] = False; levels[i]['why'] = 'not computed yet'
    else:
        txt = open(done).read()
        have = {tuple(r.split('\t')[:2]) for r in txt.splitlines() if r and not r.startswith('#') and not r.startswith('level\t')}
        for i in want:
            L = levels[i]
            if L['cloud'] and (f'{L["conf"]} {L["term"]}', str(L['J2'] // 2)) not in have:
                L['cloud'] = False
                m = re.search(r'# no cloud: ' + re.escape(f'{L["conf"]} {L["term"]} J={L["J2"] // 2}') + r' \([^)]*\): (.*)', txt)
                L['why'] = 'the calculation failed for it' + (f' ({m.group(1)})' if m else '')
    # duplicates: one line per level pair (ASD lists a few twice, observed in two works): keep the first with an A, else the first
    lines, seen = [], {}
    for r in raw:
        if not (levels[r['lo']]['cloud'] and levels[r['hi']]['cloud']): continue
        k = (r['lo'], r['hi'])
        if k in seen:
            if not lines[seen[k]]['A'] and r['A']: lines[seen[k]] = r
            continue
        seen[k] = len(lines); lines.append(r)
    return {'Z': Z, 'levels': levels, 'lines': lines, 'limit': limit}


def status_rows():
    """[(Z, symbol, code, note)] from elements.tsv; code: 0 measured (ASD), 1 levels only, 2 measured not in NIST (Pa..Es), 3 a few laser lines (Fm, No), 4 nothing measured"""
    out = []
    for line in open(os.path.join(REPO, 'data', 'jumps', 'elements.tsv')):
        if line.startswith('#') or line.startswith('Z\t'): continue
        f = line.rstrip('\n').split('\t')
        Z, X, meas, other = int(f[0]), f[1], f[9], f[8]
        code = 0 if meas == 'yes' else 1 if meas == 'levels only' else 2 if meas == 'yes, not in NIST' else 3 if meas.startswith('yes, ') else 4
        out.append((Z, X, code, other))
    return out


def rates(X, keys=None):
    """D4: {(lower name, J, upper name, J): (A_len, gauge_%)} from data/jumps/grasp/<X>/rates.tsv (tools/grasp/rates_two.py), as C literals"""
    p = os.path.join(REPO, 'data', 'jumps', 'grasp', X, 'rates.tsv')
    out, gaps = {}, []
    if os.path.exists(p):
        for line in open(p):
            if line.startswith('#') or line.startswith('lower\t'): continue
            f = line.rstrip('\n').split('\t')
            # the calculation's typical error for this atom: |A_len - NIST| / NIST over every line both have (median; shown with each computed A)
            # (only the lines the app offers, between two levels with clouds: keys)
            if f[9] and num(f[9]) and (keys is None or (f[0], int(f[1]), f[2], int(f[3])) in keys): gaps.append(abs(float(f[7]) - num(f[9])) / num(f[9]) * 100)
            # a computed A whose length and velocity forms differ by more than GAUGE_MAX % is not used (Ca's 4s5d lines, 34-38 % apart, came out 82 % below NIST's)
            if abs(float(f[8])) > GAUGE_MAX: continue
            out[(f[0], int(f[1]), f[2], int(f[3]))] = (repr(float(f[7])), repr(float(f[8])))
    gaps.sort()
    med = (gaps[len(gaps) // 2] if len(gaps) % 2 else (gaps[len(gaps) // 2 - 1] + gaps[len(gaps) // 2]) / 2) if gaps else float('nan')
    return out, med, len(gaps)


def c(s): return '"' + s.replace('\\', '\\\\').replace('"', '\\"') + '"'


def main():
    out = ['// Made by native/tools/make_level_data.py from data/jumps/asd (NIST ASD, fetched 9 Nov 2026) and data/jumps/elements.tsv. Do not edit.',
           '// flags: 1 cloud computed for it (D2 set, two electrons outside closed shells, below the limit, measured), 2 NIST energy calculated (bracketed in ASD),',
           '//        4 measured (an observed line touches it, D1), 8 in the D2 set; why: the reason a D2 level has no cloud.',
           'struct LvlRow { int Z; const char* conf; const char* term; int j2, odd; double cm; const char* cmText; int flags; const char* why; };',
           '// akiCalc, gaugePct (D4, 0.47.4): this app\'s GRASP2018 A of the line (length form, NIST\'s wavelength) and how far the velocity form is from it, % (data/jumps/grasp/<X>/rates.tsv; NAN: none)',
           'struct LvlLineRow { int Z, lo, hi; double obsNm, ritzNm, aki; const char* acc; double akiCalc, gaugePct; };',
           '// rateMedPct, rateN (D4): the median of |computed A - NIST A| / NIST A (%) over the rateN lines of rates.tsv that have both (NAN, 0: none)',
           'struct LvlAtomRow { int Z; double limitCm; int first, count, firstLine, lineCount; double rateMedPct; int rateN; };']
    lv, ln, at = [], [], []
    for X, (Z, _) in sorted(PART2.items(), key=lambda kv: kv[1][0]):
        A = atom(X)
        first, fl = len(lv), len(ln)
        want = [i for i, L in enumerate(A['levels']) if 'val' in L]
        for L in A['levels']:
            fl_ = (1 if L['cloud'] else 0) | (2 if L['calc'] else 0) | (4 if L['measured'] else 0) | (8 if 'val' in L else 0)
            lv.append(f'    {{{Z}, {c(L["conf"])}, {c(L["term"])}, {L["J2"]}, {int(L["odd"])}, {L["cm"]!r}, {c(L["text"])}, {fl_}, {c(L["why"])}}},')
        lk = lambda r: (f'{A["levels"][r["lo"]]["conf"]} {A["levels"][r["lo"]]["term"]}', A['levels'][r['lo']]['J2'] // 2,
                        f'{A["levels"][r["hi"]]["conf"]} {A["levels"][r["hi"]]["term"]}', A['levels'][r['hi']]['J2'] // 2)
        calc, med, ncmp = rates(X, {lk(r) for r in A['lines']})
        for r in A['lines']:
            fv = lambda t: repr(num(t)) if t and num(t) is not None else 'NAN'   # (ASD marks some values with a sign such as '+' after the number: the number is kept)
            lo, hi = A['levels'][r['lo']], A['levels'][r['hi']]
            k = (f'{lo["conf"]} {lo["term"]}', lo['J2'] // 2, f'{hi["conf"]} {hi["term"]}', hi['J2'] // 2)
            ac, gp = calc.get(k, ('NAN', 'NAN'))
            ln.append(f'    {{{Z}, {r["lo"]}, {r["hi"]}, {fv(r["obs"])}, {fv(r["ritz"])}, {fv(r["A"])}, {c(r["acc"])}, {ac}, {gp}}},')
        at.append(f'    {{{Z}, {A["limit"]!r}, {first}, {len(A["levels"])}, {fl}, {len(A["lines"])}, {"NAN" if med != med else round(med, 1)!s}, {ncmp}}},')
        print(f'{X}: {len(A["levels"])} levels, D2 {len(want)}, clouds {sum(1 for L in A["levels"] if L["cloud"])}, lines between them {len(A["lines"])}, '
              f'with A {sum(1 for r in A["lines"] if r["A"])}')
    st = ['struct JumpStatusRow { int Z; const char* symbol; int code; const char* source; };', 'static const JumpStatusRow kJumpStatus[] = {']
    st += [f'    {{{Z}, {c(X)}, {code}, {c(o)}}},' for Z, X, code, o in status_rows()]
    out += ['static const LvlRow kLvlLevels[] = {'] + lv + ['};', 'static const LvlLineRow kLvlLines[] = {'] + ln + ['};',
            'static const LvlAtomRow kLvlAtoms[] = {'] + at + ['};'] + st + ['};']
    open(os.path.join(REPO, 'src', 'atom', 'level_data.inc'), 'w').write('\n'.join(out) + '\n')


if __name__ == '__main__': main()
