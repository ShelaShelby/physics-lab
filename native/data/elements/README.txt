[0.54.4] The papers, preprints, journal scans, saved web pages and downloads of unclear terms that this README names are NOT in this public repo any more: they are in the private
repo ShelaShelby/physics-lab-sources at data/elements/ (same names; the other files named here are still in this folder). native/data/SOURCES.md lists each moved file with its
reference, URL, date read and sha256. Where a test reads one, its extract (<name>.extract.txt: the lines it needs, copied exactly) is in this folder.

Melting points, boiling points and densities of the elements (atom_data.inc kTempD, written by tools/make_temp_data.py).

mendeleev_1.3.0_*.csv: tables read from the SQLite database (elements.db) inside the mendeleev 1.3.0 Python package (L. M. Mentel,
  https://github.com/lmmentel/mendeleev; wheel mendeleev-1.3.0-py3-none-any.whl from PyPI, sha256 76f415767b654634e51dead755513ef431abf200e892eab8468aa659ff218d1c),
  saved 2026-11-05 exactly as stored (no rounding). Its own metadata (propertymetadata.csv) names the sources:
  melting and boiling points: W. M. Haynes (ed.), CRC Handbook of Chemistry and Physics, 97th ed. (CRC Press, 2016), stored in K (the CRC's °C + 273.15);
  densities: CRC Handbook 95th ed. (2014); for At, Fr and Es to Og: estimates from Wikipedia (the app leaves these out).
pubchem_periodictable.csv: PubChem's periodic table (https://pubchem.ncbi.nlm.nih.gov/rest/pug/periodictable/CSV, 2026-11-05), used only as a cross-check.
wiki/: Wikipedia wikitext (2026-11-05) that says which melting points are predictions: Astatine ("Many values have been predicted ... only for At2"),
  Francium ("estimated", "uncertain"), Template:Infobox fermium and lawrencium ("predicted"), Mendelevium ("estimated"), Nobelium ("predicted").
