#!/usr/bin/env python3
"""0.56.3 research (native/PLAN_MOVINGGAS.md): the three pictures for the owner, from the processor's reference (tools/movinggas/movinggas.cpp).

  python3 native/tools/movinggas_pictures.py <build>/movinggas native/docs/blackhole

Runs `movinggas --render` into a temporary folder (about 40 s on 4 cores) and draws moving_clump_strip.png, moving_clump_lightcurve.png and
disc_structure_options.png (each at most 1000 px wide, for a phone). Reads only what the app's own code computed; needs numpy and matplotlib.
The brightness shown is the light that reaches the camera per pixel, g^4 times the disc's flux (bolometric), in false colour on one fixed exposure per
picture: it is NOT the app's Real or Film colour.
"""
import os
import subprocess
import sys
import tempfile

import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt  # noqa: E402

FONT = 15


def load(d):
    meta = {}
    for line in open(os.path.join(d, "meta.txt")):
        k, v = line.split(None, 1)
        meta[k] = v.strip()
    W, H = int(meta["W"]), int(meta["H"])
    img = lambda n: np.fromfile(os.path.join(d, n + ".f32"), dtype=np.float32).reshape(H, W)
    return meta, img


def shrink(a, k):
    h, w = a.shape[0] // k * k, a.shape[1] // k * k
    return a[:h, :w].reshape(h // k, k, w // k, k).mean(axis=(1, 3))


def tone(a, expo):
    x = np.maximum(a, 0) / expo           # logarithmic: 1/30 of the exposure -> 0, 3 times it -> 1 (the Doppler-bright side does not swallow the clump)
    return np.clip(np.log10(np.maximum(x, 1e-9) * 30) / np.log10(90), 0, 1)


def minutes(meta, t):
    return t * float(meta["tgSec"]) / 60


def strip(meta, img, out):
    smooth = img("smooth")
    expo = np.percentile(smooth[smooth > 0], 99.0)
    n = 6
    fig, axes = plt.subplots(1, n, figsize=(10, 2.35), dpi=100)
    for i, ax in enumerate(axes):
        a = shrink(img("strip%d" % i), 3)
        ax.imshow(tone(a, expo), cmap="inferno", vmin=0, vmax=1, interpolation="lanczos")
        ax.set_xticks([]); ax.set_yticks([])
        for s in ax.spines.values():
            s.set_visible(False)
        ax.set_xlabel("%.1f min" % minutes(meta, float(meta["strip%d" % i]) - float(meta["strip0"])), fontsize=FONT)
    fig.suptitle("A clump going once round (spin 0.6, Sagittarius A*'s mass): far-away time under each", fontsize=FONT - 1, y=0.98)
    fig.subplots_adjust(left=0.005, right=0.995, top=0.86, bottom=0.17, wspace=0.03)
    fig.savefig(out)
    plt.close(fig)


def lightcurve(meta, d, out):
    lc = np.fromfile(os.path.join(d, "lightcurve.f32"), dtype=np.float32).reshape(-1, 5)
    Tc = float(meta["Tc"])
    t = lc[:, 0] / Tc
    norm = lc[:, 3].mean()
    fig, ax = plt.subplots(figsize=(10, 5.4), dpi=100)
    ax.plot(t, lc[:, 3] / norm, color="#1f2a44", lw=3.2, label="both images")
    ax.plot(t, lc[:, 1] / norm, color="#d9822b", lw=2.4, label="direct image")
    ax.plot(t, lc[:, 2] / norm, color="#2b8cbe", lw=2.4, label="secondary image (under the hole)")
    ax.plot(t, lc[:, 4] / norm, color="#888888", lw=1.8, ls="--", label="both, if light took no time")
    ax.set_xlabel("far-away time (orbits of the clump; one orbit = %.1f min for Sagittarius A*)" % minutes(meta, Tc), fontsize=FONT)
    ax.set_ylabel("clump's light (1 = its average)", fontsize=FONT)
    ax.tick_params(labelsize=FONT - 2)
    ax.set_xlim(0, 2)
    ax.set_ylim(bottom=0)
    ax.grid(alpha=0.3)
    ax.legend(fontsize=FONT - 2, loc="upper left", frameon=False)
    ax.set_title("The clump's light seen from 10 degrees above the disc (spin 0.6)", fontsize=FONT + 1)
    fig.tight_layout()
    fig.savefig(out)
    plt.close(fig)


def structure(meta, img, out):
    smooth = img("smooth")
    expo = np.percentile(smooth[smooth > 0], 99.0)
    tA, tB = float(meta["struct0"]), float(meta["struct1"])
    panels = [(smooth, "Today: smooth"),
              (img("struct0"), "Model: patches + clump"),
              (img("struct1"), "Half a clump orbit later (%.0f min)" % minutes(meta, tB - tA))]
    fig, axes = plt.subplots(1, 3, figsize=(10, 2.75), dpi=100)
    for ax, (a, title) in zip(axes, panels):
        ax.imshow(tone(shrink(a, 1), expo), cmap="inferno", vmin=0, vmax=1, interpolation="lanczos")
        ax.set_xticks([]); ax.set_yticks([])
        for s in ax.spines.values():
            s.set_visible(False)
        ax.set_title(title, fontsize=FONT)
    fig.subplots_adjust(left=0.005, right=0.995, top=0.88, bottom=0.01, wspace=0.03)
    fig.savefig(out)
    plt.close(fig)


def main():
    if len(sys.argv) != 3:
        print(__doc__)
        return 2
    exe, outdir = sys.argv[1], sys.argv[2]
    with tempfile.TemporaryDirectory() as tmp:
        d = os.environ.get("PL_MG_DATA")   # an earlier `movinggas --render <dir>`, to redraw without tracing again
        if not d:
            d = tmp
            subprocess.run([exe, "--render", d], check=True)
        meta, img = load(d)
        strip(meta, img, os.path.join(outdir, "moving_clump_strip.png"))
        lightcurve(meta, d, os.path.join(outdir, "moving_clump_lightcurve.png"))
        structure(meta, img, os.path.join(outdir, "disc_structure_options.png"))
    return 0


if __name__ == "__main__":
    sys.exit(main())
