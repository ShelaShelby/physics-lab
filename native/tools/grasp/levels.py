"""The levels the app uses (src/atom/alkali_data.inc, made from NIST ASD by tools/make_alkali_data.py) and their NIST energies."""
import os, re
REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
SYM = {3: 'Li', 11: 'Na', 19: 'K', 37: 'Rb', 55: 'Cs', 87: 'Fr'}
Z_OF = {v: k for k, v in SYM.items()}

def nist_levels(X):
    """[(n, l, j2, cm)] in the order of the file"""
    txt = open(os.path.join(REPO, 'src', 'atom', 'alkali_data.inc')).read().split('kAlkLines')[0]
    return [(int(n), int(l), int(j), float(cm)) for Z, n, l, j, cm in re.findall(r'\{(\d+), (\d+), (\d+), (\d+), ([\d.]+),', txt) if int(Z) == Z_OF[X]]

def app_levels(X): return [(n, l, j) for n, l, j, _ in nist_levels(X)]
