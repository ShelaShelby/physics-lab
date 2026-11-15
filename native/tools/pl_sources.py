"""Where the saved originals are (native 0.54.4).

The papers, scans, preprints, saved web pages and unclear-licence downloads are NOT in the public repo ShelaShelby/physics-lab: they are in the private repo
ShelaShelby/physics-lab-sources, at the same relative path under data/ (native/data/SOURCES.md lists them). A tool that reads one of them asks this module:

    import pl_sources
    text = open(pl_sources.path("ame/mass.mas20.txt")).read()      # a file    (relative to data/)
    folder = pl_sources.data_dir("be2")                           # a folder  (create=True for a tool that saves a download)

The folder to look in is the checkout of the sources repo named by the environment variable PL_SOURCES_DIR (the folder that holds data/). A file that is still
in native/data (it stays in the public repo) is found there. Python 3, standard library only. Needs sys.path to hold this folder (tools run with -I add it first)."""
import os, sys

HERE = os.path.dirname(os.path.abspath(__file__))
DATA = os.path.join(HERE, "..", "data")

def _stop(rel, why):
    sys.exit("%s\n  This tool needs data/%s, a saved original that lives in the private repo ShelaShelby/physics-lab-sources, not in the public one (native/data/SOURCES.md).\n"
             "  Attach that repo to the session (add_repo, push access), clone it, and run again with PL_SOURCES_DIR=<the clone> (the folder that holds data/)." % (why, rel))

def root():
    r = os.environ.get("PL_SOURCES_DIR", "")
    return os.path.join(r, "data") if r else ""

def path(rel):
    """the readable path of data/<rel> (a file or a folder): the sources checkout first, then native/data; stops with a message when it is in neither"""
    for base in (root(), DATA):
        if base and os.path.exists(os.path.join(base, rel)):
            return os.path.join(base, rel)
    _stop(rel, "PL_SOURCES_DIR is not set." if not root() else "Not found under PL_SOURCES_DIR (%s)." % os.environ.get("PL_SOURCES_DIR"))

def data_dir(rel, create=False):
    """the folder data/<rel>; create=True (a tool that saves a download) makes it inside the sources checkout (never inside this repo) and stops when PL_SOURCES_DIR is not set"""
    if create:
        if not root():
            _stop(rel, "PL_SOURCES_DIR is not set (a download is saved into the sources repo, never into this one).")
        d = os.path.join(root(), rel)
        os.makedirs(d, exist_ok=True)
        return d
    return path(rel)

def out_path(rel):
    """where a tool saves a download: data/<rel> inside the sources checkout (folders made); stops when PL_SOURCES_DIR is not set"""
    data_dir(os.path.dirname(rel), create=True)
    return os.path.join(root(), rel)
