import subprocess, sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import analyze_probe as A
d, win, ts, th = sys.argv[1:5]
screens = sys.argv[5:]
for n in screens:
    first = os.path.join(d, n + '.probe')
    T, I, R, W = A.load(first)
    smax = max([w['smax'] for k, w in W.items() if k not in ('##header', 'Debug##Default', 'Atom view bar')] + [0])
    k = 1
    while k * 400 < smax + 400:
        env = dict(os.environ, PL_PROBE_SCROLL=str(k * 400), SFX='.s%d' % (k * 400))
        subprocess.run([os.path.join(os.path.dirname(os.path.abspath(__file__)), 'render_audit.sh'), d, win, ts, th, n], env=env, check=True)
        k += 1
    print(n, 'smax', smax, 'runs', k - 1)
