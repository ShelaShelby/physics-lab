[0.54.4] The papers, preprints, journal scans, saved web pages and downloads of unclear terms that this README names are NOT in this public repo any more: they are in the private
repo ShelaShelby/physics-lab-sources at data/photometry/ (same names; the other files named here are still in this folder). native/data/SOURCES.md lists each moved file with its
reference, URL, date read and sha256. Where a test reads one, its extract (<name>.extract.txt: the lines it needs, copied exactly) is in this folder.

How the Moon is shaded (0.50, Orbit Lab; review item 3).

arxiv_2504.08252_html_2026-11-11.html
    Saved as received on 11 Nov 2026 from https://arxiv.org/html/2504.08252 (Driver, Vaughan, Cheng, Ansar, Christian and Tsiotras, "Stereophotoclinometry
    Revisited", arXiv:2504.08252, 2025; the PDF is 42 MB, so the HTML version is kept). Table 1, Eq. (23), the McEwen model:
    d = (1 - g(phi)) cos i + g(phi) 2 cos i / (cos i + cos e), g(phi) = exp(-phi/60), phi the phase angle in degrees; "the phase weighting function g(phi),
    which was fit to lunar imagery captured by the Galileo spacecraft [71, 72]" (McEwen 1991, Icarus 92, 298; McEwen 1996, LPSC 27, 841), the exponential
    being "the exponential approximation to McEwen's original polynomial weighting function proposed in [6]". McEwen's own papers could not be saved
    (ADS and the journals were not tried again: refused in earlier jobs); his polynomial's coefficients were not typed from memory.
    Used in src/render/shading.h (mcewenLunar) and the planet shader (uLunar) for the Moon only.

The Sun's V magnitude (0.55.4, Black Hole Lab, the true exposure).

willmer2018_arxiv1804.07788.pdf (+ .txt.gz, its pdftotext -layout text; _abs.html)
    Saved as received on 16 Nov 2026 from https://arxiv.org/pdf/1804.07788 (C. N. A. Willmer, "The Absolute Magnitude of the Sun in Several Filters",
    ApJS 236, 47 (2018), arXiv:1804.07788v1). Table 3, row Johnson V: absolute 4.81, apparent -26.76 (Vega system). Used in src/blackhole/light.h
    (kSunV, kSunAbsV); tests/test_blackhole.cpp testTrueLight reads them from willmer2018_arxiv1804.07788.extract.txt (this folder) and checks that the two
    differ by the distance modulus of 1 AU.

Johnson B and V passbands (0.97.0, part 2 of the outside review of 28 Nov 2026, second).
bessell_1990_pasp102_1181.pdf (sources repo)
    Saved as received on 28 Nov 2026 from https://articles.adsabs.harvard.edu/pdf/1990PASP..102.1181B (M. S. Bessell, "UBVRI passbands", PASP 102, 1181 (1990)).
    Table 2, columns B and V, typed into bessell1990_BV.txt here (checked against the SVO's copy, ../stars/README.txt), with the SVO's Vega zero points.
    Used by tests/test_orbit.cpp testStarTemperature097 and tools/bv_blackbody.py.
