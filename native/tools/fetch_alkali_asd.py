# 0.46: fetches the NIST ASD levels and lines of Li I, Na I, K I, Rb I, Cs I into data/asd/alkali (run once, 8 Nov 2026: python3 tools/fetch_alkali_asd.py data/asd/alkali).
# tools/make_alkali_data.py turns them into src/atom/alkali_data.inc.
import urllib.parse, urllib.request, sys, time, os
OUT = sys.argv[1]
UA = {"User-Agent": "PhysicsLab-native-data-tool/0.46 (one-off fetch of ASD levels and lines)"}
def get(url, path):
    for a in range(4):
        try:
            with urllib.request.urlopen(urllib.request.Request(url, headers=UA), timeout=120) as r: data = r.read()
            open(path, 'wb').write(data); return len(data)
        except Exception as e:
            if a == 3: raise
            time.sleep(2 ** (a + 1))
for sp in ["Li I", "Na I", "K I", "Rb I", "Cs I"]:
    tag = sp.replace(" ", "")
    lv = {"de": 0, "spectrum": sp, "submit": "Retrieve Data", "units": 0, "format": 3, "output": 0, "page_size": 15, "multiplet_ordered": 0,
          "conf_out": "on", "term_out": "on", "level_out": "on", "unc_out": 1, "j_out": "on", "biblio": "on", "temp": ""}
    u = "https://physics.nist.gov/cgi-bin/ASD/energy1.pl?" + urllib.parse.urlencode(lv)
    print(tag, "levels", get(u, os.path.join(OUT, tag + "_levels.tsv")), u, flush=True)
    q = {"spectra": sp, "low_w": 200, "upp_w": 20000, "unit": 1, "format": 3, "line_out": 0, "en_unit": 0, "output": 0, "bibrefs": 1, "page_size": 15,
         "show_obs_wl": 1, "show_calc_wl": 1, "show_av": 3, "unc_out": 1, "order_out": 0, "max_low_enrg": "", "tsb_value": 0, "min_str": "", "A_out": 0, "intens_out": "on",
         "max_str": "", "allowed_out": 1, "forbid_out": 1, "min_accur": "", "min_intens": "", "conf_out": "on", "term_out": "on", "enrg_out": "on", "J_out": "on", "submit": "Retrieve Data"}
    u = "https://physics.nist.gov/cgi-bin/ASD/lines1.pl?" + urllib.parse.urlencode(q)
    print(tag, "lines", get(u, os.path.join(OUT, tag + "_lines.tsv")), u, flush=True)
