# 0.47.1: builds data/jumps/elements.tsv (one row per neutral atom Z = 1..118) from the NIST ASD files in data/jumps/asd (tools/fetch_jumps_asd.py)
# and the other measured sources listed in data/jumps/sources/README.txt (column other_source / measured; written below by hand, each with its saved file).
#   python3 native/tools/make_jumps_table.py native/data/jumps
# Columns:
#   levels          ASD rows with a level energy, not the ionization limit
#   named_levels    of those, the rows with a configuration (a named level)
#   lines           ASD line rows (any)
#   classified_E1   lines with both levels named (conf_i and conf_k) and Type empty (= electric dipole, E1, in ASD)
#   E1_with_A       of those, the lines with an Aki value
#   A_B_or_better   of those, Aki accuracy grade AAA, AA, A+, A, B+ or B (<= 10 %: sources/asd_lines_help.html); a primed grade (split from a multiplet
#                   assuming LS coupling) is not counted
import sys, os, gzip, csv, io
D = sys.argv[1]
SYM = ("H He Li Be B C N O F Ne Na Mg Al Si P S Cl Ar K Ca Sc Ti V Cr Mn Fe Co Ni Cu Zn Ga Ge As Se Br Kr Rb Sr Y Zr Nb Mo Tc Ru Rh Pd Ag Cd In Sn Sb Te I Xe "
       "Cs Ba La Ce Pr Nd Pm Sm Eu Gd Tb Dy Ho Er Tm Yb Lu Hf Ta W Re Os Ir Pt Au Hg Tl Pb Bi Po At Rn Fr Ra Ac Th Pa U Np Pu Am Cm Bk Cf Es Fm Md No Lr "
       "Rf Db Sg Bh Hs Mt Ds Rg Cn Nh Fl Mc Lv Ts Og").split()
GOOD = {'AAA', 'AA', 'A+', 'A', 'B+', 'B'}
# Measured data outside NIST ASD (file names in data/jumps/sources). Only what the saved files state.
OTHER = {
    85: ('Rothe et al. Nat. Commun. 4, 1835 (2013): ionization potential and Rydberg levels (astatine_rothe2013_doi.html)', 'yes'),
    91: ('Blaise & Wyart 1992 compilation (osti_blaise_wyart_1992.html; review_block2021.txt Fig. 2); Naubereit et al.: about 1500 high levels near the limit (review_block2021.txt)', 'yes, not in NIST'),
    92: ('Blaise & Wyart 1992 and 17 later level papers (review_block2021.txt Fig. 2)', 'yes, not in NIST'),
    93: ('Blaise & Wyart 1992 (review_block2021.txt Fig. 2)', 'yes, not in NIST'),
    94: ('Blaise & Wyart 1992 and later papers (review_block2021.txt Fig. 2)', 'yes, not in NIST'),
    95: ('Blaise & Wyart 1992 and later papers (review_block2021.txt Fig. 2)', 'yes, not in NIST'),
    96: ('Blaise & Wyart 1992 (review_block2021.txt Fig. 2)', 'yes, not in NIST'),
    97: ('Blaise & Wyart 1992 (review_block2021.txt Fig. 2)', 'yes, not in NIST'),
    98: ('Blaise & Wyart 1992 (review_block2021.txt Fig. 2)', 'yes, not in NIST'),
    99: ('Blaise & Wyart 1992; Wyart, Blaise & Worden (review_block2021.txt Fig. 2)', 'yes, not in NIST'),
    100: ('Sewtz et al. PRL 90, 163002 (2003): seven transitions (review_block2021.txt); Warbinek et al. Nature 634, 1075 (2024): 5f12.7s7p 5G*5 near 25111.8 cm-1 (fm_warbinek2024_pmc.html)', 'yes, a few lines, not in NIST'),
    102: ('Laatiaoui et al. Nature 538, 495 (2016): 1S0-1P1 at 29961.5 cm-1; Chhetri et al. PRL 120, 263003 (2018): Rydberg states (review_block2021.txt)', 'yes, one line and Rydberg levels, not in NIST'),
    103: ('none found; review_block2021.txt section 6.2: first laser spectroscopy still to come', 'no'),
}
rows = []
for Z, s in enumerate(SYM, 1):
    lv = os.path.join(D, 'asd', s + 'I_levels.tsv.gz'); ln = os.path.join(D, 'asd', s + 'I_lines.tsv.gz')
    n_lv = n_named = n_ln = n_e1 = n_a = n_ab = 0
    if os.path.exists(lv):
        t = gzip.open(lv, 'rt', encoding='latin1').read()
        if not t.lstrip().startswith('<'):
            for r in csv.DictReader(io.StringIO(t), delimiter='\t'):
                e = (r.get('Level (cm-1)') or '').strip('"[]() ?')
                if not e or (r.get('Term') or '').strip('"') == 'Limit': continue
                try: float(e)
                except ValueError: continue
                n_lv += 1; n_named += bool((r.get('Configuration') or '').strip('"'))
    if os.path.exists(ln):
        t = gzip.open(ln, 'rt', encoding='latin1').read()
        if not t.lstrip().startswith('<'):
            for r in csv.DictReader(io.StringIO(t), delimiter='\t'):
                n_ln += 1
                if (r.get('conf_i') or '').strip('"') and (r.get('conf_k') or '').strip('"') and not (r.get('Type') or '').strip('"'):
                    n_e1 += 1
                    if (r.get('Aki(s^-1)') or '').strip('"'):
                        n_a += 1; n_ab += (r.get('Acc') or '').strip('"') in GOOD
    other, meas = OTHER.get(Z, ('', ''))
    if not meas: meas = 'yes' if n_e1 > 0 else ('levels only' if n_named > 1 else 'no')
    rows.append((Z, s, n_lv, n_named, n_ln, n_e1, n_a, n_ab, other, meas))
with open(os.path.join(D, 'elements.tsv'), 'w') as f:
    f.write('# Neutral atoms: NIST ASD holdings (retrieved 9 Nov 2026, data/jumps/asd) and measured data outside ASD (data/jumps/sources). Made by tools/make_jumps_table.py.\n')
    f.write('# measured: yes = ASD has E1 lines with both levels named; levels only = ASD has named levels but its lines carry no classification;\n# for Z >= 85 the saved source decides (other_source); no = nothing measured found.\n')
    f.write('Z\tsymbol\tlevels\tnamed_levels\tlines\tclassified_E1\tE1_with_A\tA_B_or_better\tother_source\tmeasured\n')
    for r in rows: f.write('\t'.join(map(str, r)) + '\n')
