[0.54.4] The papers, preprints, journal scans, saved web pages and downloads of unclear terms that this README names are NOT in this public repo any more: they are in the private
repo ShelaShelby/physics-lab-sources at data/sun/ (same names; the other files named here are still in this folder). native/data/SOURCES.md lists each moved file with its
reference, URL, date read and sha256. Where a test reads one, its extract (<name>.extract.txt: the lines it needs, copied exactly) is in this folder.

[0.84.1] Research for the outside review (native/PLAN_REVIEW.md); the app does not use these yet. Saved in the private repo at data/sun/:
neckel_labs_1994_SoPh153_91.pdf (ADS scan, legacy.adsabs.harvard.edu) and its typed lines (_typed.txt: abstract, section 3.2, Equations (5), (6), Table I).
Extract: neckel_labs_1994_SoPh153_91.extract.txt. Table I's 365.875 nm row as printed sums to 0.99800, not 1 (REPORT 0.84.1). pierce_slaughter_1977_SoPh51_25.pdf
(the older candidate, not used). spots/: the Royal Greenwich Observatory - USAF/NOAA sunspot group records 1874-2016 from NASA Marshall (RGO_NOAA1874_2013.zip,
g2014-g2016.txt, format.txt, MissingDays.txt, the page) and one NOAA Solar Region Summary with its format sheet. Extract: spots/format.extract.txt.

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

[0.85.0] The Sun's limb darkening is Neckel & Labs 1994 (Table I, from neckel_labs_1994_SoPh153_91.extract.txt) weighted by the ASTM G173 spectrum
and the CIE observer per screen channel: tools/make_sun_limb_data.py writes src/render/sun_limb_data.inc, src/render/sun_limb.h makes the coefficients.
Used by the Sun's ball (kPlanetFS uMode 4) and the eclipses. Alpha Centauri's stars keep the lab's own blend (shaders.h mode 3): their edges are not measured here.

spots/ (0.85.0, the owner's answer of 25 Nov 2026: the packed table is kept in this public repo)
    sunspot_groups.bin: the measured sunspot groups, one row per group per day (date, latitude, Carrington longitude, area), 1874-05-09 to 2026-11-24, 255,250
    rows; made by tools/make_sunspot_table.py from the Greenwich and USAF/NOAA group records (NASA Marshall) and NOAA's daily Solar Region Summaries
    (NCEI), all saved in the private repo at data/sun/spots/ (native/data/SOURCES.md). US government data (NASA, NOAA); the RGO part's terms are not stated
    on NASA's page. Read by src/orbit/sunspots.cpp (built into the program).
    Extracts: g1874, g1950, g1976, g2016 (rows the tests read back), noaa_srs_20240510 (part I), format, msfc_greenwch_page.

[0.94.0] The Sun's surface (render/sun_surface.h, orbit/sun_photo.h). Saved in the private repo at data/sun/ (native/data/SOURCES.md has each file's line):
spots/contrast/maltby_1986_ApJ306_284.pdf (ADS scan) and _typed.txt (Table 3 typed from page 286): the umbra/photosphere intensity ratio against wavelength,
    0.387-2.35 um, early, middle and late phase. The app uses the middle phase. Extract: spots/maltby_1986_table3.extract.txt.
spots/contrast/astro-ph_0701401_mathew_2007_text.txt (the text of the PDF saved in 0.85.0): Table 2, the penumbra's mean intensity against spot radius
    (0.8561 - 0.0016 r, r in arcsec above 10), SOHO/MDI near 676.8 nm. Extract: spots/mathew_2007_table2.extract.txt.
spots/area/arxiv1905.06888_jha_2019.pdf and _text.txt: the penumbra to umbra area ratio (about 6 above 100 millionths, about 4.5 below). Extract: spots/jha_2019_ratio.extract.txt.
spots/contrast/albregtsen_maltby_1981_SoPh71_269.pdf and solanki_2003_AARv11_153.pdf (+ _text.txt): checked, not used for numbers. Albregtsen & Maltby give the umbra
    per spot only ("a similar table for the penumbra intensities is not given here"). Solanki 2003 gives the umbra's and penumbra's contrasts only for the whole
    spectrum (section 5.2). No measured penumbral contrast against wavelength was found (4 searches, REPORT 0.94.0): the app uses Mathew's one wavelength in every channel.
granulation/arxiv0906.2451_mathew_2009.pdf and _text.txt: Table 2, the granulation's rms contrast after the stray-light correction, 0.215 (450.4 nm), 0.158
    (555.0 nm), 0.111 (668.4 nm), Hinode/SOT BFI. Extract: granulation/mathew_2009_table2.extract.txt.
granulation/arxiv1508.03022_hathaway_2015.pdf and _text.txt: the granular peak of the photospheric velocity spectrum at about 1200 km, lifetimes about 10
    minutes (quoting Bray et al. 1984). Extract: granulation/hathaway_2015.extract.txt.
hmi/sdo_data_rules_2026-11-27.html: NASA SDO's terms (not copyrighted unless noted; non-commercial and education use needs no authorization; the credit line).
hmi/helioviewer_api_v2_2026-11-27.html: the API used (getClosestImage, getJP2Header, downloadImage).
hmi/hmi_ic_156593381_s8.png, _header.xml, hmi_ic_closest_20240510.json: one HMI continuum picture of 2024-05-09 23:59:50 UT (512 x 512), its header and the reply.
    Extracts: hmi/hmi_ic_156593381_header.extract.txt, hmi/hmi_ic_closest_20240510.extract.txt. The test reads the picture itself with PL_SOURCES_DIR.
    Measured here: the PNG's grey is linear (its edge profile is Neckel & Labs's at 617.3 nm within 0.004 for mu 0.4-1), and region 3664's spots centre at
    Carrington 347.8, latitude -19.8 (NOAA SRS: 348, -19).
Sizes of one picture (downloadImage, PNG, 2024-05-09): 79 KB (512 px), 323 KB (1024), 1.3 MB (2048, the app's), 4.6 MB (4096). One a day from 6 Dec 2010
    (Helioviewer's first HMI continuum picture) is about 5,800 days: 0.46 GB to 27 GB. So nothing is built in: the app downloads the shown day's picture.
