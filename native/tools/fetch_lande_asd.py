# 0.54.0: fetches the NIST ASD levels WITH the Lande-g column (lande_out=on) of the atoms whose levels the Atom Lab has: H I, the alkalis Li..Fr and the level
# atoms of tools/make_level_data.py (PART2). Saved exactly as received, gzip-compressed, into data/jumps/asd_g/<X>I_levels_g.tsv.gz.
# Run once, 14 Nov 2026: python3 native/tools/fetch_lande_asd.py native/data/jumps/asd_g
# Same request parameters as tools/fetch_jumps_asd.py (levels), plus lande_out=on (ASD leaves the Lande column out when no level of the spectrum has a g).
import urllib.parse, urllib.request, sys, time, os, gzip
OUT = sys.argv[1]
UA = {"User-Agent": "PhysicsLab-native-data-tool/0.54.0 (one-off fetch of ASD levels with Lande g)"}
def get(url, path):
    for a in range(5):
        try:
            with urllib.request.urlopen(urllib.request.Request(url, headers=UA), timeout=300) as r: data = r.read()
            with gzip.open(path, 'wb') as f: f.write(data)
            return len(data)
        except Exception:
            if a == 4: raise
            time.sleep(2 ** (a + 1))
for s in ["H", "Li", "Na", "K", "Rb", "Cs", "Fr", "He", "Be", "Mg", "Ca", "Sr", "Ba", "Ra", "Zn", "Cd", "Hg", "Yb"]:
    p = os.path.join(OUT, s + "I_levels_g.tsv.gz")
    if os.path.exists(p): continue
    lv = {"de": 0, "spectrum": s + " I", "submit": "Retrieve Data", "units": 0, "format": 3, "output": 0, "page_size": 15, "multiplet_ordered": 0,
          "conf_out": "on", "term_out": "on", "level_out": "on", "unc_out": 1, "j_out": "on", "lande_out": "on", "biblio": "on", "temp": ""}
    u = "https://physics.nist.gov/cgi-bin/ASD/energy1.pl?" + urllib.parse.urlencode(lv)
    print(s, get(u, p), u, flush=True); time.sleep(1)
