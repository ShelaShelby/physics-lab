# separate Python: recompute the shape references with the 0.42 point-proton mean square radius (Ong et al. 2010 eq. 8)
import math  # 0.42: shape references for test_atom.cpp kShapeRef (run: python3 -I native/tools/shape_ref_042.py)

def quad(f,a,b,n=4000):
    h=(b-a)/n; return h/3*(f(a)+f(b)+sum((4 if i%2 else 2)*f(a+i*h) for i in range(1,n)))
def brentq(f,a,b,xtol=1e-14):
    fa=f(a)
    for _ in range(200):
        m=(a+b)/2; fm=f(m)
        if (fm<0)==(fa<0): a,fa=m,fm
        else: b=m
        if b-a<xtol: break
    return (a+b)/2
rows=[(67,165,5.2022,0.434410959),(68,167,5.2560,0.417905897),(71,175,5.3700,0.374429877),(63,153,5.1115,0.421335135),(92,235,5.8337,0.344978011),(27,59,3.7875,0.244387227),
(83,209,5.5211,-0.038276560),(62,147,4.9892,-0.037153656),(55,126,4.7872,-0.523884804),(13,27,3.0610,0.364438408),(11,23,2.9936,0.572583488),(64,155,5.1319,0.387107540)]
DF=0.75*(197.3269804/938.27208943)**2
def own(Z,N): return 0.84075**2+N/Z*(-0.1155)+DF
def Y20(c): return math.sqrt(5/(16*math.pi))*(3*c*c-1)
def ratio(b):
    num=quad(lambda c:(3*c*c-1)*(1+b*Y20(c))**5,-1,1)
    den=quad(lambda c:(1+b*Y20(c))**5,-1,1)
    return num/den
for Z,A,rms,old in rows:
    N=A-Z; Q0=old*Z*(rms*rms-0.84*0.84); ms=rms*rms-own(Z,N); r=Q0/(Z*ms)
    try: b=brentq(lambda x:ratio(x)-r,-0.8,0.8,xtol=1e-14)
    except Exception: b=float('nan')
    first=math.sqrt(5*math.pi)*Q0/(3*Z*(5/3)*ms)
    bold=brentq(lambda x:ratio(x)-old,-0.8,0.8,xtol=1e-14)
    print(f"{{{Z}, {A}, {rms:.4f}, {r:.9f}, {b:+.8f}, {first:+.7f}}},  # old beta check {bold:+.8f}")

# the cloud table of testQuadrupole (Q0 / (Z <r^2>) with the 0.42 point-proton <r^2>); N-12 has no measured radius: the rule ball rms = sqrt(0.6) x 1.2 A^(1/3)
clouds=[(67,165,5.2022,0.434410959236),(68,167,5.2560,0.417905896953),(63,153,5.1115,0.421335134532),(92,235,5.8337,0.344978011370),(27,59,3.7875,0.244387227285),
(13,27,3.0610,0.364438407721),(55,126,4.7872,-0.523884804060),(64,155,5.1319,0.387107539569),(3,6,2.5890,-0.044797779987),(3,8,2.3390,0.768748492144),
(7,12,math.sqrt(0.6)*1.2*12**(1/3),0.373674093554),(7,14,2.5582,0.500103853759)]
print("clouds:")
for Z,A,rms,old in clouds:
    N=A-Z; Q0=old*Z*(rms*rms-0.84*0.84); print(f"{{{Z}, {A}, {Q0/(Z*(rms*rms-own(Z,N))):.12f}}},")

# the counts of testQuadrupole: which ground rows of data/quadrupole/elec_mom_recomm.csv (through quadrupole_data.inc) give a shape with |beta2| <= 0.6
import re, csv, os, sys
here=os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0,here)
import pl_sources   # the IAEA radii table is in the private sources repo (PL_SOURCES_DIR)
radii={}
for r in csv.DictReader(open(pl_sources.path('radii/charge_radii.csv'))):
    if r['radius_val']: radii[(int(r['z']),int(r['n']))]=float(r['radius_val'])
rowre=re.compile(r"\{(\d+), (\d+), (-?\d+), (\d), '(\w)', \d, ([-0-9.e+]+|NAN)")
used=too=nosign=spinno=notclean=0
for m in rowre.finditer(open(os.path.join(here,'../src/atom/quadrupole_data.inc')).read()):
    Z,A,s2,firm,kind,Q=int(m[1]),int(m[2]),int(m[3]),int(m[4]),m[5],m[6]
    N=A-Z
    if kind=='X': notclean+=1; continue
    if kind!='S': nosign+=1; continue
    if not firm or s2<2: spinno+=1; continue
    I=s2/2; Q0=float(Q)*(I+1)*(2*I+3)/(I*(2*I-1))*100
    rms=radii[(Z,N)] if A>1 and (Z,N) in radii else math.sqrt(0.6)*1.2*A**(1/3)
    ms=max(0.1,rms*rms-own(Z,N)); r=Q0/(Z*ms)
    if ratio(-0.6)<=r<=ratio(0.6): used+=1
    else: too+=1
print(f"counts: used {used}, need |beta2| > 0.6 {too}, no firm sign {nosign}, no firm spin {spinno}, not clean {notclean}")
