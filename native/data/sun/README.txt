[0.54.4] The papers, preprints, journal scans, saved web pages and downloads of unclear terms that this README names are NOT in this public repo any more: they are in the private
repo ShelaShelby/physics-lab-sources at data/sun/ (same names; the other files named here are still in this folder). native/data/SOURCES.md lists each moved file with its
reference, URL, date read and sha256. Where a test reads one, its extract (<name>.extract.txt: the lines it needs, copied exactly) is in this folder.

The Sun's colour and why no glow is drawn round it (0.49.4, Orbit Lab).

astm_g173_pvlib.csv
    Saved as received on 11 Nov 2026 from https://raw.githubusercontent.com/pvlib/pvlib-python/main/pvlib/data/ASTMG173.csv
    (pvlib's copy of ASTM G173-03, "Reference Spectra Derived from SMARTS v. 2.9.2"). Used: the column "extraterrestrial" (W m^-2 nm^-1),
    the Sun's measured-based spectrum above the atmosphere, 280-4000 nm.
astm_g173_solcore.csv
    Saved as received on 11 Nov 2026 from https://raw.githubusercontent.com/qpv-research-group/solcore5/develop/solcore/light_source/astmg173.csv
    (solcore's copy of the same standard). Not used by the app: a second, independent copy. All 2002 rows of the extraterrestrial column agree
    with pvlib's exactly (tools/sun_colour.py checks it). ASTM's and NREL's own pages could not be opened from the session (the proxy refused).

    tools/sun_colour.py turns the extraterrestrial spectrum into CIE 1931 XYZ (2 degree observer, colour-science's table, checked against luxpy's
    copy: they agree to 5e-13), then into linear sRGB (IEC 61966-2-1, D65 white, colour-science), scaled so the largest channel is 1, then
    sRGB-encoded: (255, 242, 239), chromaticity x = 0.3234, y = 0.3327 (correlated colour temperature about 5,930 K, McCamy). This is the
    Sun's own colour on an sRGB screen, nearly white: the lab drew it as (247, 207, 72) from a typed table with no source until 0.49.3.
    The ETR sums to 1347.9 W/m^2 over the file's 280-4000 nm (a check that the right column is read).

nasa_spartan_the_corona.html
    Saved as received on 11 Nov 2026 from https://umbra.nascom.nasa.gov/spartan/the_corona.html (NASA Solar Data Analysis Center, SPARTAN 201-3,
    "The Corona"). It says: "the brightness of the white light corona ranges from one millionth to one billionth of the central solar disk brightness".
nasa_eclipse_photography.html
    Saved as received on 11 Nov 2026 from https://eclipse.gsfc.nasa.gov/SEhelp/SEphoto.html (NASA eclipse web site, solar eclipse photography).
    It says: "The corona has a surface brightness a million times fainter than the photosphere".
    So no corona or glow is drawn round the Sun: on any screen it would be far below one grey step next to the disc. Until 0.49.3 a yellow glow
    out to 3 solar radii, in the Sun's colour, was drawn (no source, no label).

Not done (ROADMAP): the Sun's limb darkening is still the lab's own blend (src/render/shaders.h, mode 3). The measured law (Neckel & Labs 1994,
Solar Physics 153, 91; or Neckel 2005, Solar Physics 229, 13) could not be opened from the session (ADS and Springer refused) and is not
reprinted on arXiv, so no coefficients were typed in.
