[0.54.4] The papers, preprints, journal scans, saved web pages and downloads of unclear terms that this README names are NOT in this public repo any more: they are in the private
repo ShelaShelby/physics-lab-sources at data/hf/ (same names; the other files named here are still in this folder). native/data/SOURCES.md lists each moved file with its
reference, URL, date read and sha256. Where a test reads one, its extract (<name>.extract.txt: the lines it needs, copied exactly) is in this folder.

Many-electron data for the Atom Lab (0.39). Kept exactly as downloaded on 6 Nov 2026; tools/make_hf_data.py reads them and writes src/atom/hf_data.inc.

koga/light_neutral, koga/light_cation, koga/HFwfn.bib
    T. Koga, K. Kanayama, S. Watanabe and A. J. Thakkar, "Analytical Hartree-Fock wave functions subject to cusp and asymptotic constraints:
    He to Xe, Li+ to Cs+, H- to I-", Int. J. Quantum Chem. 71, 491-497 (1999). From k99light.zip on https://ajit.ext.unb.ca/download.htm
    (the anions in the zip are not used and not kept).
koga/heavy_neutral
    T. Koga, K. Kanayama, T. Watanabe, T. Imai and A. J. Thakkar, "Analytical Hartree-Fock wave functions for the atoms Cs to Lr",
    Theor. Chem. Acc. 104, 411-413 (2000). From k00heavy.zip on the same page.
    No licence is stated on the page; the files are offered there for download as research data.
nist_ie_all.tsv
    NIST Atomic Spectra Database ver. 5.12 (Kramida, A., Ralchenko, Yu., Reader, J., and NIST ASD Team, 2024; doi:10.18434/T4W30F), ionization energies
    of every spectrum, query:
    https://physics.nist.gov/cgi-bin/ASD/ie.pl?spectra=H-Og&units=1&format=3&order=0&at_num_out=on&sp_name_out=on&ion_charge_out=on&el_name_out=on&seq_out=on&shells_out=on&level_out=on&ion_conf_out=on&e_out=0&unc_out=on&biblio=on&submit=Retrieve+Data
    NIST marks: no brackets = from evaluated experimental data; [ ] = semi-empirical; ( ) = theoretical (NIST's ie.pl help page).
    NIST Standard Reference Data: cite it and acknowledge NIST as the source.
