"""One element.  Modes:
  vis Z   : Visscher & Dyall's configuration, average level, their c and nucleus -> compare with their Table IV
  cont Z MIX MAXIT : continue a prod run from its saved orbitals (prod_Z.pkl) with other SCF mixing / iterations, when its
            last orbital step did not reach the tolerance (used for Lv, Z = 116: MIX 0.3, MAXIT 1000)
  point Z : the nonrelativistic limit with a POINT nucleus in NIST's configuration (average level): the model of Koga et al.'s Hartree-Fock orbitals, which the
            tests compare with it (data/dhf/koga_point_check.tsv; for the heavy closed-shell atoms, where the finite nucleus alone moves an s orbital by about 1e-4)
  prod Z  : NIST ground configuration (Smits et al. 2023 for Z = 109-118), average level -> CI in the open
            shells -> extended average level with the ground level's relativistic-configuration weights,
            repeated until the weights settle; c = CODATA 2022; plus the nonrelativistic limit (rel=False).
Writes out/<mode>_<Z>.pkl and prints a summary line.
"""
import sys, os, re, time, math, pickle, csv, numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from radhf import *
from ci import CI

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))
WORK = os.environ.get('DHF_WORK', os.path.join(REPO, '..', '..', 'dhf_work'))

def table_text(name):
    """a saved Visscher-Dyall page (data/dhf/visscher_tableN.html, as received) as plain text"""
    import html
    sys.path.insert(0, os.path.join(REPO, 'tools'))
    import pl_sources   # the saved pages are in the private sources repo (PL_SOURCES_DIR)
    s = open(pl_sources.path('dhf/' + name), encoding='latin-1').read()
    return html.unescape(re.sub(r'<[^>]+>', ' ', s)).splitlines()
CORE = {'He': '1s2', 'Ne': '1s2 2s2 2p6', 'Ar': '1s2 2s2 2p6 3s2 3p6',
        'Kr': '1s2 2s2 2p6 3s2 3p6 3d10 4s2 4p6', 'Xe': '1s2 2s2 2p6 3s2 3p6 3d10 4s2 4p6 4d10 5s2 5p6'}
CORE['Rn'] = CORE['Xe'] + ' 4f14 5d10 6s2 6p6'
CORE['Ba'] = CORE['Xe'] + ' 6s2'; CORE['Zn'] = CORE['Ar'] + ' 3d10 4s2'; CORE['Cd'] = CORE['Kr'] + ' 4d10 5s2'
CORE['Hg'] = CORE['Xe'] + ' 4f14 5d10 6s2'; CORE['No'] = CORE['Rn'] + ' 5f14 7s2'; CORE['Cn'] = CORE['No'] + ' 6d10'

def expand(s):
    s = s.strip().replace('.', ' ')
    m = re.match(r'\[?([A-Z][a-z]?)[\])]\s*(.*)', s)
    if m and m.group(1) in CORE: s = CORE[m.group(1)] + ' ' + m.group(2)
    return parse_config(s)

def visscher():
    """{Z: (A, config string, {(n,l,2j): <r> Gaussian})} from the saved Tables I and IV."""
    A = {}
    for line in table_text('visscher_table1.html'):
        m = re.match(r'\s*[A-Z][a-z]?\s+(\d+)\s+(\d+)\s+\S+E', line)
        if m: A[int(m.group(1))] = int(m.group(2))
    out = {}; Z = None
    for line in table_text('visscher_table4.html'):
        m = re.match(r'([A-Z][a-z]?)\s+(\d+)\s+(.*?)\s+(\d[spdf]-?)\s+(\S+)\s+(\S+)\s+(\S+)\s+(\S+)\s*$', line)
        if m:
            Z = int(m.group(2)); out[Z] = (A[Z], m.group(3), {}); orb, val = m.group(4), m.group(8)
        else:
            m2 = re.match(r'\s+(\d[spdf]-?)\s+(\S+)\s+(\S+)\s+(\S+)\s+(\S+)\s*$', line)
            if not (m2 and Z): continue
            orb, val = m2.group(1), m2.group(5)
        n = int(orb[0]); l = LET.index(orb[1]); j2 = 2 * l - 1 if orb.endswith('-') else 2 * l + 1
        out[Z][2][(n, l, j2)] = float(val)
    return out

def nist():
    rows = [r for r in csv.reader(open(os.path.join(REPO, 'data/hf/nist_ie_all.tsv')), delimiter='\t')][1:]
    return {int(r[0]): (r[5], r[6]) for r in rows if len(r) > 6 and r[2] == '0'}

SMITS = {109: ('[No] 6d7', '9/2'), 110: ('[No] 6d8', '4'), 111: ('[No] 6d9', '5/2'), 112: ('[No] 6d10', '0'),
         113: ('[Cn] 7p1', '1/2'), 114: ('[Cn] 7p2', '0'), 115: ('[Cn] 7p3', '3/2'), 116: ('[Cn] 7p4', '2'),
         117: ('[Cn] 7p5', '3/2'), 118: ('[Cn] 7p6', '0')}

def nist_J(level):
    m = re.search(r'<([\d/]+)>\s*$', level) or re.search(r'(\d+(?:/2)?)\s*$', level)
    if not m: return None
    v = m.group(1); return float(v.split('/')[0]) / 2 if '/' in v else float(v)

def default_A(Z):
    txt = open(os.path.join(REPO, 'src/atom/atom_data.inc')).read()
    N = [int(x) for x in re.search(r'kDefaultN\[119\]\s*=\s*\{(.*?)\}', txt, re.S).group(1).split(',')]
    return Z + N[Z]

def coefs(a, subs):
    """raw coefficients: P = sum dP_i r^(l+1) exp(-alpha_i r^2); Q = sum dQ_i ((l+1+kappa)/r - 2 alpha_i r) r^(l+1) exp(-alpha_i r^2)"""
    out = {}
    for s in subs:
        l, j2 = s[1], s[2]; B = a.basis(l, j2); r = a.r; al = a.alpha[:, None]
        graw = r ** (l + 1) * np.exp(-al * r * r)
        ng = np.sqrt((graw * graw * a.w).sum(1))
        v = a.vec[s]; nb = a.nb
        dP = v[:nb] / ng
        dQ = v[nb:] / (ng * 2 * a.c) if a.rel else np.zeros(nb)
        out[s] = (dP, dQ)
    return dict(amin=float(a.alpha[0]), ratio=float(a.alpha[1] / a.alpha[0]), nb=a.nb, c=a.c, coef=out)

def run_vis(Z):
    A, cs, ref = visscher()[Z]
    cfg = expand(cs)
    a = Atom(Z, A=A, c=137.0359895); subs, Q, W = al_weights(cfg)
    E = a.scf(subs, Q, W)
    mine = {s: a.rexp(s) for s in subs}
    worst = max(abs(mine[s] / ref[s] - 1) for s in ref)
    return dict(Z=Z, cfg=cs, E=E, conv=a.converged, rexp=mine, ref=ref, worst=worst)

def run_prod(Z, maxcyc=8):
    if Z <= 108:
        shells, level = nist()[Z]; Jref = nist_J(level); src = 'NIST ASD 5.12'
    else:
        shells, Jref = SMITS[Z][0], nist_J(SMITS[Z][1]); level = SMITS[Z][1]; src = 'Smits et al. 2023'
    cfg = expand(shells)
    assert abs(sum(cfg.values()) - Z) < 1e-9, (Z, cfg)
    A = default_A(Z)
    a = Atom(Z, A=A); subs, Q, W = al_weights(cfg)
    E_al = a.scf(subs, Q, W)
    res = dict(Z=Z, A=A, shells=shells, level=level, Jref=Jref, src=src, E_al=E_al, conv_al=a.converged)
    opn = [k for k in cfg if 0 < cfg[k] < 4 * k[1] + 2]
    if opn:
        prev = None
        for cyc in range(maxcyc):
            ci = CI(a, cfg); J, occ, confs, e = ci.solve()
            closed = {s: s[2] + 1 for s in a.vec if s[:2] in cfg and s[:2] not in opn}
            act = ci.act_sub
            cl = [(w, {**closed, **{s: nn for s, nn in zip(act, pat) if nn}}) for pat, w in confs.items() if w > 1e-14]
            subs, Q, W = eal_weights(cl)
            d = max(abs(occ[s] - prev[s]) for s in occ) if prev else 1
            prev = occ
            if d < 1e-7: break
            a.scf(subs, Q, W, guess=False)
        res.update(J=J, ndet=ci.ndet, levels=list(ci.levels()), occ_ci=occ, cycles=cyc + 1)
    else:
        res.update(J=0.0, ndet=1, levels=[0.0], occ_ci={}, cycles=0)
    res.update(E=a.E, conv=a.converged, subs=subs, Q={s: Q[s] for s in subs},
               r=a.r, U={s: a.radial(s) for s in subs}, rexp={s: a.rexp(s) for s in subs},
               eps={s: a.eps(s, subs, Q, W) for s in subs}, basis=coefs(a, subs), xi=a.xi)
    # nonrelativistic limit, same configuration (average level), same nucleus
    b = Atom(Z, A=A, rel=False); s2, Q2, W2 = al_weights(cfg)
    b.scf(s2, Q2, W2)
    res.update(nr=dict(E=b.E, conv=b.converged, Q={s: Q2[s] for s in s2}, U={s: b.radial(s)[0] for s in s2},
                       rexp={s: b.rexp(s) for s in s2}, basis=coefs(b, s2)))
    return res

def run_point(Z):
    cfg = expand(nist()[Z][0]); b = Atom(Z, A=None, rel=False); s, Q, W = al_weights(cfg); b.scf(s, Q, W)
    return dict(Z=Z, cfg=nist()[Z][0], E=b.E, conv=b.converged, rexp={k: b.rexp(k) for k in s if k[2] == 2 * k[1] + 1})

def restore(r):
    """an Atom holding the orbitals saved by coefs() in a prod result"""
    a = Atom(r['Z'], A=r['A']); b = r['basis']
    assert abs(b['amin'] - a.alpha[0]) < 1e-12 and b['nb'] == a.nb
    for s, (dP, dQ) in b['coef'].items():
        l = s[1]; graw = a.r ** (l + 1) * np.exp(-a.alpha[:, None] * a.r * a.r); ng = np.sqrt((graw * graw * a.w).sum(1))
        a.vec[s] = np.concatenate([dP * ng, dQ * ng * 2 * a.c])
    return a

def run_cont(Z, mix, maxit):
    r = pickle.load(open(os.path.join(WORK, f'prod_{Z}.pkl'), 'rb'))
    a = restore(r); cfg = expand(r['shells']); opn = [k for k in cfg if 0 < cfg[k] < 4 * k[1] + 2]
    prev = None; ok = False
    for cyc in range(8):
        ci = CI(a, cfg); J, occ, confs, e = ci.solve()
        closed = {s: s[2] + 1 for s in a.vec if s[:2] in cfg and s[:2] not in opn}
        cl = [(w, {**closed, **{s: nn for s, nn in zip(ci.act_sub, pat) if nn}}) for pat, w in confs.items() if w > 1e-14]
        subs, Q, W = eal_weights(cl)
        d = max(abs(occ[s] - prev[s]) for s in occ) if prev else 1; prev = occ
        if d < 1e-7 and ok: break
        a.scf(subs, Q, W, guess=False, mix=mix, maxit=maxit); ok = a.converged
    r.update(J=J, ndet=ci.ndet, levels=list(ci.levels()), occ_ci=occ, cycles=r['cycles'] + cyc + 1, continued=dict(mix=mix, maxit=maxit))
    r.update(E=a.E, conv=a.converged, subs=subs, Q={s: Q[s] for s in subs}, r=a.r, U={s: a.radial(s) for s in subs},
             rexp={s: a.rexp(s) for s in subs}, eps={s: a.eps(s, subs, Q, W) for s in subs}, basis=coefs(a, subs), xi=a.xi)
    return r

if __name__ == '__main__':
    mode, Z = sys.argv[1], int(sys.argv[2])
    os.makedirs(WORK, exist_ok=True)
    t = time.time()
    r = run_vis(Z) if mode == 'vis' else run_point(Z) if mode == 'point' else run_cont(Z, float(sys.argv[3]), int(sys.argv[4])) if mode == 'cont' else run_prod(Z)
    r['time'] = time.time() - t
    pickle.dump(r, open(os.path.join(WORK, f"{'prod' if mode == 'cont' else mode}_{Z}.pkl"), 'wb'))
    if mode == 'point':
        print(f"point {Z} conv {r['conv']} t {r['time']:.0f}s", flush=True)
    elif mode == 'vis':
        print(f"vis {Z} conv {r['conv']} worst <r> rel.dev {r['worst']:.2e} t {r['time']:.0f}s", flush=True)
    else:
        print(f"prod {Z} conv {r['conv']} J {r['J']:.3f} ref {r['Jref']} ndet {r['ndet']} cyc {r['cycles']} nrconv {r['nr']['conv']} t {r['time']:.0f}s", flush=True)
