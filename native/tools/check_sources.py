#!/usr/bin/env python3
"""Provenance check of the saved originals (native 0.54.4).

  python3 native/tools/check_sources.py [folder of a checkout of ShelaShelby/physics-lab-sources]      (or set PL_SOURCES_DIR)

The papers, scans, preprints, saved web pages and unclear-licence downloads are kept in the private repo ShelaShelby/physics-lab-sources, not in this public
repo (native/data/SOURCES.md lists them, one line each). This script checks, against a checkout of that repo:
  1. every line of native/data/SOURCES.md: the file data/<path> exists in the checkout and its sha256 is the one written there;
  2. every extract (native/data/**/*.extract.txt, the few lines a test reads, copied exactly): its header names the original and the original's sha256 (both
     must match the manifest line), and every block between a "#--- begin" line and its "#--- end" line is found in the original (a .gz original is
     decompressed first; line ends are compared as LF and trailing blanks of a line are ignored, as the extract is written);
  3. the sources checkout holds no file that the manifest does not list (apart from README.md and .gitattributes).
Without the folder it prints one line saying the check was skipped and exits with 0. Any failure is printed and the exit status is 1.
Python 3, standard library only."""
import gzip, hashlib, os, re, sys

HERE = os.path.dirname(os.path.abspath(__file__))
DATA = os.path.normpath(os.path.join(HERE, "..", "data"))


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for b in iter(lambda: f.read(1 << 20), b""):
            h.update(b)
    return h.hexdigest()


def norm(text):
    """LF line ends, no trailing blanks on a line (how an extract is written)"""
    text = text.replace("\r\n", "\n").replace("\r", "\n")
    return re.sub(r"[ \t]+(?=\n)", "", text)


def manifest():
    """{path in the sources repo (data/...): sha256} from native/data/SOURCES.md"""
    out, dup = {}, []
    with open(os.path.join(DATA, "SOURCES.md"), encoding="utf-8") as f:
        for line in f:
            if not line.startswith("| data/"):
                continue
            cells = [c.strip() for c in line.strip().strip("|").split("|")]
            path, digest = cells[0], cells[4]
            if not re.fullmatch(r"[0-9a-f]{64}", digest):
                dup.append("manifest line without a sha256: " + path)
            if path in out:
                dup.append("listed twice in the manifest: " + path)
            out[path] = digest
    return out, dup


def extracts():
    for r, _, fs in os.walk(DATA):
        for f in sorted(fs):
            if f.endswith(".extract.txt"):
                yield os.path.join(r, f)


def parse_extract(path):
    head, blocks, cur = {}, [], None
    with open(path, encoding="utf-8", newline="") as f:
        text = f.read().replace("\r\n", "\n")
    for line in text.split("\n"):
        if line.startswith("#--- begin"):
            cur = []
        elif line.startswith("#--- end"):
            blocks.append("\n".join(cur)); cur = None
        elif cur is not None:
            cur.append(line)
        else:
            m = re.match(r"# (Original|sha256 of the original|Reference|URL|Date read|Place in the original): (.*)", line)
            if m:
                head[m.group(1)] = m.group(2).strip()
    return head, blocks


def original_text(path):
    with open(path, "rb") as f:
        b = f.read()
    if path.endswith(".gz"):
        b = gzip.decompress(b)
    return norm(b.decode("utf-8", errors="replace"))


def main():
    folder = sys.argv[1] if len(sys.argv) > 1 else os.environ.get("PL_SOURCES_DIR", "")
    if not folder:
        print("check_sources: skipped (no sources folder: give the checkout of ShelaShelby/physics-lab-sources as the argument or set PL_SOURCES_DIR)")
        return 0
    if not os.path.isdir(os.path.join(folder, "data")):
        print("check_sources: FAILED: %s has no data/ folder (it must be a checkout of ShelaShelby/physics-lab-sources)" % folder)
        return 1
    man, bad = manifest()
    n_files = n_extracts = n_blocks = 0
    for path, digest in sorted(man.items()):
        p = os.path.join(folder, path)
        if not os.path.isfile(p):
            bad.append("missing in the sources checkout: " + path)
        elif sha256(p) != digest:
            bad.append("sha256 differs: " + path)
        n_files += 1
    for x in extracts():
        rel = os.path.relpath(x, os.path.join(DATA, "..")).replace(os.sep, "/")
        head, blocks = parse_extract(x)
        orig = head.get("Original", "")
        if not orig or not blocks or "sha256 of the original" not in head:
            bad.append("extract without Original / sha256 / blocks: " + rel)
            continue
        if man.get(orig) != head["sha256 of the original"]:
            bad.append("extract %s: its original %s is not in the manifest with this sha256" % (rel, orig))
            continue
        p = os.path.join(folder, orig)
        if not os.path.isfile(p):
            bad.append("extract %s: original missing in the sources checkout: %s" % (rel, orig))
            continue
        text = original_text(p)
        for i, b in enumerate(blocks, 1):
            n_blocks += 1
            if norm(b) not in text:
                bad.append("extract %s: block %d (%s...) is not found in %s" % (rel, i, b[:40].replace("\n", " "), orig))
        n_extracts += 1
    listed = set(man)
    for r, _, fs in os.walk(os.path.join(folder, "data")):
        for f in fs:
            rel = os.path.relpath(os.path.join(r, f), folder).replace(os.sep, "/")
            if rel not in listed:
                bad.append("in the sources checkout but not in the manifest: " + rel)
    if bad:
        print("check_sources: FAILED (%d problems)" % len(bad))
        for b in bad[:60]:
            print("  " + b)
        return 1
    print("check_sources: ok (%d manifest lines, %d extracts with %d blocks)" % (n_files, n_extracts, n_blocks))
    return 0


if __name__ == "__main__":
    sys.exit(main())
