# 0.47.1: fetches the NIST ASD levels and lines of every neutral atom that has an "X I" entry on the ASD holdings pages
# (data/asd/holdings/levels_pt.html), saved exactly as received but gzip-compressed, into data/jumps/asd/<X>I_levels.tsv.gz and <X>I_lines.tsv.gz.
# Run once, 9 Nov 2026: python3 native/tools/fetch_jumps_asd.py native/data/asd/holdings/levels_pt.html native/data/jumps/asd
# (0.54.4: the holdings page is in the private sources repo now: $PL_SOURCES_DIR/data/asd/holdings/levels_pt.html)
# Same request parameters as tools/fetch_alkali_asd.py, except: all wavelengths (no low/high limit) and no intensities.
import urllib.parse, urllib.request, sys, time, os, re, gzip
HOLD, OUT = sys.argv[1], sys.argv[2]
if not os.path.exists(HOLD): sys.exit("%s not found: the ASD holdings page is a saved page kept in the private repo ShelaShelby/physics-lab-sources (native/data/SOURCES.md); clone it and give $PL_SOURCES_DIR/data/asd/holdings/levels_pt.html" % HOLD)
UA = {"User-Agent": "PhysicsLab-native-data-tool/0.47.1 (one-off fetch of ASD levels and lines of neutral atoms)"}
def get(url, path):
    for a in range(5):
        try:
            with urllib.request.urlopen(urllib.request.Request(url, headers=UA), timeout=300) as r: data = r.read()
            with gzip.open(path, 'wb') as f: f.write(data)
            return len(data)
        except Exception as e:
            if a == 4: raise
            time.sleep(2 ** (a + 1))
syms = re.findall(r'title="([A-Za-z]+) I: \d+ levels', open(HOLD, encoding='latin1').read())
for s in syms:
    sp = s + " I"; tag = s + "I"
    lp = os.path.join(OUT, tag + "_levels.tsv.gz"); np_ = os.path.join(OUT, tag + "_lines.tsv.gz")
    if not os.path.exists(lp):
        lv = {"de": 0, "spectrum": sp, "submit": "Retrieve Data", "units": 0, "format": 3, "output": 0, "page_size": 15, "multiplet_ordered": 0,
              "conf_out": "on", "term_out": "on", "level_out": "on", "unc_out": 1, "j_out": "on", "biblio": "on", "temp": ""}
        u = "https://physics.nist.gov/cgi-bin/ASD/energy1.pl?" + urllib.parse.urlencode(lv)
        print(tag, "levels", get(u, lp), u, flush=True); time.sleep(1)
    if not os.path.exists(np_):
        q = {"spectra": sp, "low_w": "", "upp_w": "", "unit": 1, "format": 3, "line_out": 0, "en_unit": 0, "output": 0, "bibrefs": 1, "page_size": 15,
             "show_obs_wl": 1, "show_calc_wl": 1, "show_av": 3, "unc_out": 1, "order_out": 0, "max_low_enrg": "", "tsb_value": 0, "min_str": "", "A_out": 0,
             "max_str": "", "allowed_out": 1, "forbid_out": 1, "min_accur": "", "min_intens": "", "conf_out": "on", "term_out": "on", "enrg_out": "on", "J_out": "on",
             "submit": "Retrieve Data"}
        u = "https://physics.nist.gov/cgi-bin/ASD/lines1.pl?" + urllib.parse.urlencode(q)
        print(tag, "lines", get(u, np_), u, flush=True); time.sleep(1)
