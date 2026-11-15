Saturn's rings as Cassini measured them (0.51.2, Orbit Lab).

UVIS_HSP_2008_231_BETCEN_I_TAU10KM.LBL, UVIS_HSP_2008_231_BETCEN_I_TAU10KM.TAB
    Saved as received on 14 Nov 2026 from the NASA PDS Ring-Moon Systems Node,
    https://pds-rings.seti.org/holdings/volumes/COUVIS_8xxx/COUVIS_8001/data/ (data set CO-SR-UVIS-HSP-2/4-OCC-V3.0, product created 2020-11-27).
    Cassini UVIS High Speed Photometer, occultation of beta Centauri, 18 Aug 2008 (day 231), orbit 81, ingress; 110-190 nm; binned to 10 km of ring-plane radius,
    72,833 to 151,675 km; ring seen at 66.7 deg elevation; highest detectable normal optical depth 10.73 (label). Method: Colwell et al. 2010, Icarus 205, 1 (the label's reference).
    Every row has NOTE_FLAG 16 ("data includes light from two stars": beta Cen is a double star). 343 bins are slightly below 0 (noise), 2 above the maximum detectable.
    tools/make_ring_profile.py turns columns 1 (radius) and 5 (normal optical depth) into src/orbit/saturn_profile.inc (below 0 -> 0, above the maximum -> the maximum).
    Against NASA's Saturnian Rings Fact Sheet (src/orbit/saturn_rings.h): mean optical depth of the C ring 0.104 (sheet 0.05-0.35), A ring 0.70 (0.4-1.0), but the B ring
    3.27 (sheet 0.4-2.5) and the Cassini Division 0.148 (sheet 0.0-0.1): the sheet's ranges are lower than this occultation for those two (REPORT.md 0.51.2).
