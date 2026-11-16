# Plan: the Black Hole Lab (0.54.2 research, 15 Nov 2026; no code yet)

Every formula and number below is copied from a paper saved under native/data/blackhole/ (README.txt there lists them) and names its equation, table or page.
Short names: **J15** = James, von Tunzelmann, Franklin & Thorne 2015 (the base paper); **BPT** = Bardeen, Press & Teukolsky 1972; **PT** = Page & Thorne 1974;
**L79** = Luminet 1979; **C75** = Cunningham 1975; **GLa / GLb** = Gralla & Lupsasca 2020 (null geodesics / lensing); **PeT** = Perlick & Tsupko 2022;
**R19** = Riazuelo 2019; **AF** = Abramowicz & Fragile 2013; **YN** = Yuan & Narayan 2014; **M87-I, M87-V, Sgr-I** = the Event Horizon Telescope papers;
**G22** = GRAVITY 2022; **MF12** = Müller & Frauendiener 2012; **B26** = Berens et al. 2026. Units: G = c = 1, lengths in M (= GM/c², "r_g"), spin a between 0 and M.
A value marked **(computed here)** is this job's own arithmetic from the cited formula (scratch script, not in the repo); the build job recomputes it in a test.

## 0. In plain words (for the owner)
1. **What can be shown truthfully.** The shape of the picture is settled physics: how a hole of a given mass and spin bends light is known exactly. The black
   "shadow", the ring of sky folded around it, the doubled stars and the way a flat disc of gas appears wrapped over and under the hole can all be calculated and tested against published numbers.
2. **A bright thin disc of gas** (the famous picture) is real physics too, with a standard model that says how bright and how hot it is at each distance. Such
   discs exist around quasars and around small black holes that feed from a star.
3. **What cannot be shown as fact.** Sagittarius A* and M87* do **not** have such a disc. Their gas is a thin, very hot, faint fog that hardly shines in
   visible light. The telescope pictures of them are radio pictures (1.3 mm waves) in made-up colours, blurred to about half the ring's width. Nobody knows
   what an eye next to them would see in detail, and their spins are poorly measured. The app must say this where it shows them.
4. So the honest lab is: **a calculated model**. The hole and the bending of light are exact; the disc is "the standard thin disc, as around a quasar"; for
   the two real holes the app shows their measured mass and size and says plainly that their real gas looks different.
5. **A real thin disc would be blinding and blue-white**, not orange: tens to hundreds of times brighter per patch of sky than the Sun's surface, and far hotter.
   The app has to turn the brightness down (like a camera's exposure) and say so. The film's orange disc is a cooled, made-up disc at 4,500 K.
6. **The film** (Interstellar) changed five things on purpose, all written down by its makers: a slower spin, no colour shift, no bright-side/dim-side
   difference, a camera-lens glow, and an artist's disc. The "Film" look can copy those choices and label each one.
7. **The sky behind the hole**: we only have the sky as seen from Earth. Recommended: show the hole as if it were placed in front of our own sky, and say so.
8. **Weak graphics cards**: the heavy calculation can be done once on the processor into a small table whenever the viewpoint changes; the card then only
   looks the table up. That runs on anything the app runs on today. Better cards calculate every pixel themselves for a sharper picture.
9. **Build steps** (each one session, each leaves something to look at): (1) the new lab with a non-spinning hole in front of the sky; (2) the gas disc;
   (3) spin; (4) the Real and Film looks, quality levels, guide; (5) moving cameras: orbiting, then falling in; (6) tables for the Data panel; (7) later, the "skin".
10. Nothing in steps 1 to 6 waits for a source: all needed papers are saved. Two old book chapters could not be fetched; their results are in the saved papers.
11. Section 10 lists eight questions for the owner, each with a recommendation.

## 1. The model
**Space around the hole.** The Kerr metric in Boyer-Lindquist coordinates with the functions ρ, Δ, Σ, α (lapse), ω (frame dragging), ϖ: J15 (A.1), (A.2).
Hovering observers ("FIDOs") and their unit vectors: J15 (A.3). a = 0 gives the Schwarzschild metric (B26 (5)); every formula below must reduce to it, and the tests check that.

**Special radii** (BPT, upper sign = orbiting with the spin, lower = against):
- Horizon r₊ = M + (M² − a²)^½ (2.6); static limit (edge of the ergosphere) r₀ = M + (M² − a² cos²θ)^½ (2.7).
- Circular orbits: energy and angular momentum per mass (2.12), (2.13); angular velocity Ω = ±M^½ / (r^{3/2} ± aM^½) (2.16).
- Photon orbit r_ph = 2M{1 + cos[⅔ cos⁻¹(∓a/M)]} (2.18): 3M at a = 0; M and 4M at a = M.
- Marginally bound r_mb = 2M ∓ a + 2M^½(M ∓ a)^½ (2.19): 4M at a = 0; M and 5.83M at a = M.
- Innermost stable orbit r_ms = M{3 + Z₂ ∓ [(3 − Z₁)(3 + Z₁ + 2Z₂)]^½}, Z₁ = 1 + (1 − a²/M²)^⅓[(1 + a/M)^⅓ + (1 − a/M)^⅓], Z₂ = (3a²/M² + Z₁²)^½ (2.21): 6M at a = 0; M and 9M at a = M.

**One light ray.** From the camera's position, its velocity relative to the FIDO and a direction on its sky: the ray's direction for the FIDO by relativistic
aberration J15 (A.8)-(A.10); its momenta and the two constants b (axial angular momentum) and q (Carter constant), J15 (A.11), (A.12). Then integrate the ray
equations J15 (A.15) backward (five first-order equations for r, θ, φ, p_r, p_θ; the "super-Hamiltonian" form that stays well behaved at turning points).
This is **numerical integration**; DNGR used an adaptive Runge-Kutta-Fehlberg step (J15 App. A.4). The app's reference code does the same in double precision.

**The dark shadow's edge needs no integration.** A ray comes from the horizon exactly when J15's test (v) says so: the constants b₀(r₀), q₀(r₀) of the trapped
light orbits (A.5), their radius range r₁..r₂ (A.6), and the conditions (A.13), (A.14) with the sign of p_r or the upper turning point (largest root of R(r), (A.4)).
This holds for any camera position and speed outside the horizon, so the shadow is exact per pixel.
- Seen from far away the edge is the "critical curve": λ̃(r̃), η̃(r̃) for r̃ between r̃₋ and r̃₊, GLb (38)-(40), drawn at screen coordinates α = −λ/sin θₒ,
  β = ±(η + a² cos²θₒ − λ² cot²θₒ)^½, GLb (55). At a = 0 it is a circle of radius 3√3 M (GLb (71); L79 p. 229: b_c = 5.19695 M, "apparent diameter about 10.38 M" — 3√3 = 5.19615: L79's decimal is a slip, REPORT 0.54.3; M87-I: R_c = √27 r_g).
- Non-spinning, hovering observer at radius r_O: sin α_sh = 3√3 M (1 − 2M/r_O)^½ / r_O, with α_sh > 90° inside 3M (Synge's formula, PeT (6), (7)); at r_O = 3M the shadow is exactly half the sky (R19 Fig. 20).
- Non-spinning, falling freely from rest far away: R19 (59)-(63): the shadow is about 84.2° across at the horizon, 180° at the singularity (64).

**Closed forms that exist.**
- a = 0: the path is planar; angle against impact parameter is an elliptic integral, L79 (6), (11)-(13), B26 (19); near the critical value b = 5.19695 M + 3.4823 M e^{−μ} (L79 (7)).
- Any spin: every light path outside the horizon as Legendre elliptic integrals and Jacobi functions: GLa (29)-(31), (38), (49) for the angle, section IV and
  App. B for the radius. B26 uses them for a far observer; MF12 uses the a = 0 forms per pixel in a shader.
- No closed form: the magnification of a point star (the ray-bundle equations J15 (A.17)-(A.23), App. A.2) and anything with a camera inside a moving frame
  that is easier to integrate than to invert. The app integrates these, or takes the magnification from the ratio of solid angles of neighbouring rays (R19 (45)-(51)).
- **Proposal:** the reference is the integrator of J15 (A.15) in double precision on the CPU (it handles every camera and spin, and its a → 0 limit is tested
  against the a = 0 closed forms). Closed forms are used where they are exact and cheap: the shadow test, the radii, the a = 0 deflection table (section 6).

**The camera.** Hovering: β = 0 in J15 (A.9). Circular orbit in the equatorial plane: B_φ = 1, β = (ϖ/α)(Ω − ω), Ω = 1/(a + r_c^{3/2}), J15 (A.7). Any motion:
the speed β and direction B relative to the FIDO (J15 A.1 step (i)). Colour and brightness of the sky for a moving camera: f_c/f′ = [(1 − β²)^½/(1 − βN_y)]·[(1 − bω)/α], J15 (A.16)
(a = 0, hovering: the blueshift (1 − 2M/r)^{−½}, R19 (B18)). **Falling in:** Boyer-Lindquist coordinates stop at the horizon (J15 footnote to (A.1): switch to
ingoing Kerr coordinates; DNGR did not, and the film's inside view was made by hand, J15 §4.2); R19 does a = 0 with Kruskal-Szekeres coordinates (its (27)-(36)).
**So the foundation must not hard-wire Boyer-Lindquist:** the integrator takes "metric functions + state", the camera is "an event and a 4-velocity", and
the disc and sky are asked "where does this ray end", so a second coordinate chart can be added in step 5 without touching the drawing.

## 2. The gas disc
**Model:** the standard thin disc (Novikov-Thorne; PT). Assumptions (PT §II): gas on circular equatorial orbits, thin, all heat radiated locally from the
two faces, nothing emitted inside r_ms, zero torque at r_ms.
- **Brightness against radius:** F(r) = (Ṁ/4π) e^{−(ν+ψ+μ)} f with e^{ν+ψ+μ} = r, PT (11b), (15d); f is PT (15n) with x = (r/M)^½, x₀ = (r_ms/M)^½ (15k),
  x₁, x₂, x₃ the roots of x³ − 3x + 2a* = 0 (PT p. 502, above (14)). F is zero at r_ms, rises to a maximum and falls like r⁻³ far out.
  The same in numbers: F = [7×10²⁶ erg cm⁻² s⁻¹] (ṁ/m) r*⁻³ B⁻¹ C^{−½} Q, m = M/M☉, ṁ = Ṁc²/L_Edd, AF §5.3 (the F line of each of the three regions; functions B, C, Q below them).
- **Check value, a = 0:** maximum at r ≈ 9.55 M with (8π/3MṀ)F_max ≈ 1.146×10⁻⁴ (L79 p. 233). PT (15n) reproduces both (computed here: 9.551, 1.1459×10⁻⁴).
  **L79's printed (15) has √3/3 before the logarithm; PT gives √3/2, and only √3/2 gives L79's own maximum. Use PT; do not code L79 (15).**
- **Inner edge:** r_ms (BPT (2.21)); inside it the gas falls in and the model emits nothing. **Outer edge:** not set by the model (a free parameter).
- **Temperature:** each face shines as a black body, σT⁴ = F, so T ∝ (ṁ/m)^¼ at a fixed r/M: at the same feeding rate (in Eddington units) a heavier hole has a
  **cooler** disc. YN §1: thin-disc gas is 10⁴ to 10⁷ K depending on the hole's mass.
- **Shifts on the way to the camera.** Along a ray I_ν/ν³ is constant (J15 Fig. 15 caption); a black body stays a black body at T_obs = T/(1 + z) (R19 (53); J15 App. A.6);
  total brightness per patch of sky goes as (1 + z)⁻⁴ (L79 p. 233). a = 0, camera far away: 1 + z = (1 − 3M/r)^{−½}(1 + (M/r³)^½ b sin θ₀ sin α), L79 (19).
  Any spin: g = 1/(1 + z) from the gas's 4-velocity (BPT (2.12), (2.13), (2.16)) and the ray's b; its extremes g_min, g_max are tabulated in C75 (Fig. 1, Table 1; a/M = 0 and 0.9981). Moving camera: times J15 (A.16).
- **What an eye would see (computed here from AF's F with σT⁴ = F, at L = 0.1 L_Edd, efficiency 1 − E(r_ms) from BPT (2.12)):** hottest ring about 4×10⁶ K for
  10 Suns (X-rays; the visible part is its faint tail), about 1.5×10⁵ K for Sagittarius A*'s mass, about 2.5×10⁴ K for M87*'s mass (a = 0; with spin 0.9 about 1.7 times hotter).
  All are hotter than the Sun's surface, so every patch of the disc is brighter than the Sun's disc: at 550 nm about 50 times (M87* mass) to 500 times (Sgr A* mass). **Blinding.**
  Colour: blue-white. Above a few 10⁴ K the eye sees only the long-wave tail, whose colour hardly changes with temperature, so the Doppler shift shows mostly as
  brightness (bright approaching side), not as red against blue. The build computes the colour from render/cie1931_data.inc exactly as tools/star_colour.py does for stars,
  and applies a stated exposure (a setting; the readout says how many times it was dimmed).
- **Free parameters:** mass M; spin a (sets r_ms); feeding rate Ṁ (shown as L/L_Edd); outer radius; which way the gas orbits (with the spin by default);
  the viewing angle (the camera); isotropic emission from each face (PT's assumption; no limb darkening); a pure black body (no "hardening factor", AF §3.4).
- **Defensible values per preset.**
  - *Sagittarius A\**: M = (4.297 ± 0.012)×10⁶ M☉, distance 8277 ± 9 pc (G22; Sgr-I Table 1). Real feeding near the horizon 10⁻⁹ to 10⁻⁷ M☉/yr, L_bol ≲ 10³⁶ erg/s,
    about 9 orders below L_Edd (Sgr-I §2; YN §4.1: 2×10⁻⁹ L_Edd). A thin disc needs roughly 1.5 % of L_Edd or more (YN §4.2), so **no thin-disc value is defensible for it**; a disc on this preset is "what if".
  - *M87\**: M = (6.5 ± 0.7)×10⁹ M☉, distance 16.8 ± 0.8 Mpc (M87-I Table 1), Ṁ/Ṁ_Edd ≈ 2.0×10⁻⁵ (M87-V), seen 17° from the jet axis (M87-I §6). Same verdict.
  - *A thin-disc hole* (no named object): L = 0.1 L_Edd is inside the range where YN say the model applies (below L_Edd, above about 1.5 %); the mass is the user's.

## 3. "Real" against "Film"
**What the film changed, from J15 itself:**
| # | The physics | The film's choice | Where |
|---|---|---|---|
| 1 | spin a/M ≃ 1 is needed for the story's time loss (the paper's own figures use 0.999) | drawn with a/M = 0.6, so the shadow's flat side and off-centre disc do not confuse | §4.1.1, §1.2, App. A.3.1 |
| 2 | approaching side bluer, receding side redder (factors about 1.5 and 0.4 at about 0.55 c, with about 20 % gravitational redshift) | no colour shift | §4.1.2, §4.2 |
| 3 | approaching side much brighter (I_ν ∝ ν³): "exceedingly lopsided, with the hole's shadow barely discernible" | no brightness shift (Fig. 15a, not 15c) | §4.2, Fig. 15 |
| 4 | no lens | "veiling" lens flare from the measured point-spread function of the IMAX lenses, by convolution | §4.2, §4.3.1 |
| 5 | a disc from the accretion equations | an artists' disc: "anemic", not accreting, cooled to one temperature T = 4500 K, black body, thin, marginally opaque; textures added; "with some embellishments" | §4.1.1, §4.3.2, App. A.6 |
| 6 | the view from inside the horizon | made by hand from DNGR images | §4.2 |
| 7 | the eye | film-stock colour response, white balance at 6500 K | App. A.6 |
(Not a departure: stars are points with lensing magnification, App. A.3.1; the disc's picture geometry in Fig. 13: camera at r_c = 74.1 M, θ_c = 86.56°, swatch disc from 9.26 M to 18.70 M.)

**Real look (the dot):** user's spin; the shadow, lensed sky and stars; the thin disc from section 2 with colour shift and brightness shift on, black-body colour by CIE 1931,
a stated exposure; no lens effects. Its caption: "A calculated model: the standard thin disc. Real for quasars and feeding stellar black holes."
**Film look:** spin fixed at 0.6 (said), a uniform 4500 K disc with no colour or brightness shift (said), a soft glow. The film's lens data and its artists' textures
are not published, so the glow is ours and labelled "our own glow, in place of the film's lens flare" (0.55.3). Caption: "The film's choices (James et al. 2015)", with table rows 1-5 in the guide.
**What is NOT known, and what the app says:**
- Sgr A* and M87* have no thin bright disc: the light is synchrotron light from a geometrically thick, optically thin hot flow (M87-I §1, M87-V; YN §4.1), near the virial
  temperature T ~ 10¹²/r K (YN (16)), brightest in radio to far-infrared. App text: "Its real gas is a faint hot fog, not this disc. No one knows exactly what an eye beside it would see."
- The telescope pictures are at 1.3 mm, blurred to 20 μas (ring 42 ± 3 μas for M87*, 51.8 ± 2.3 μas for Sgr A*), colour = brightness temperature (M87-I Fig. 3; Sgr-I Fig. 3); Sgr A* changes within an hour (Sgr-I abstract).
- Spin: M87*: zero spin is disfavoured by the jet power and the spin points away from Earth (M87-I §6); Sgr A*: models disfavour zero spin, a counter-rotating
  flow and a view more than 50° from the axis; the two best models have a* = 0.5 and 0.94 at 30° (Sgr-I abstract, §6). No measured value. The presets therefore set mass and distance only; spin stays the user's, with this sentence.
- A picture of the hot flow itself would need a magnetised gas simulation (the EHT model libraries, M87-V): not buildable from closed forms; a ROADMAP idea only.

## 4. The background sky
Today: ESA's Gaia map drawn per pixel (render/planet_maps.h Map::Sky, Renderer2D::skyMap, orbit_view.cpp drawSky) and 2,874 catalogue stars as discs (orbit/sky.cpp), all as seen from Earth.
- **Proposed answer (A):** the hole is shown **as if placed in front of our own sky**, and the lab says so in one line ("The sky is ours, seen from Earth: we have no map of the sky from the galactic centre"). Truthful about what is calculated, and the lensing is easy to read against familiar constellations.
- (B) a sky as from the galactic centre or M87: needs a 3D model of stars and dust we do not have; it would be invented. Not proposed.
- (C) a black background, disc only: honest but loses the lensing of the sky, which is the part that is exact. Offered as a switch ("Sky: on/off").
- **Bending the map.** Each pixel's ray ends at a direction (θ′, φ′) on the far sky (J15 A.1 step (vi)); the map is sampled there. Lensing keeps surface brightness, so the map's
  values are used unchanged except for the camera's own shift (A.16): colour temperature × f_c/f′ and brightness × (f_c/f′)⁴ (R19 (52), (53)). To avoid shimmer, the sample is filtered over
  the ray bundle's footprint (J15 A.3.1, third bullet): in practice a mip level chosen from the map's stretch between neighbouring pixels. The map has about 0.09° a pixel: strongly magnified
  parts near the Einstein ring will be soft, and the guide says so.
- **Bending the point stars.** Every star has a primary image, a secondary one on the other side inside the Einstein ring, and ever fainter ones hugging the shadow (J15 §3.1; R19 (44));
  each further half-turn is fainter by e^{−γ}, γ = π at a = 0 (GLb (43), (72), (105)); e^{−π} = 0.043 (computed here). Stars stay points, their brightness multiplied by the
  magnification = ratio of solid angles (R19 (51), (52): m_obs = m + 4 log(1+z) − log f as printed; J15 A.3.1), never stretched blobs (J15 A.7 names stretched stars as the fault of the "paint them on the map" method).
  a = 0: invert the deflection function exactly for each star (R19 §V.A): 2,874 stars × 2 or 3 images on the CPU. Spin: no simple inverse (J15 A.7): invert the pixel-to-sky table of section 6
  by finding the table cells that contain the star's direction; the cell's area ratio is the magnification. The catalogue stars must then be removed from the map or the map dimmed as today (drawSky's comment: the map holds the bright stars faintly).

## 5. Tests (written first; a part with no reference is not built)
| Part | Independent reference |
|---|---|
| special radii | BPT (2.6), (2.18), (2.19), (2.21) and the limits printed under them (3M, 4M, 6M; M, 4M, 5.83M, 9M); PT (15k) must agree with BPT (2.21) |
| a = 0 light path | critical b = 5.19695 M and b = 5.19695 M + 3.4823 M e^{−μ} (L79 p. 229, (7)); weak bending μ → 4M/P for large P (L79 p. 229); integrator against the elliptic form L79 (6), (13) |
| shadow, a = 0 | radius 3√3 M far away (GLb (71)); Synge at r_O = 3M: exactly 90°, and the general curve (PeT (6), (7), Fig. 2); falling observer: tan(δ/2) = 2/(3√3), cos δ = 23/31, about 84.2° (R19 (62), (63)) |
| shadow, spin | integrator's black pixels against the closed test J15 (A.5), (A.6), (A.13), (A.14); far-away edge against GLb (38)-(40), (55); the a → 0 limit against the circle |
| ray equations | constants b, q and the null condition conserved along J15 (A.15) (conservation law); time-reversed ray returns to the camera |
| repeated images | each further image thinner by e^{−γ}: γ = π at a = 0 (GLb (72)); arrival radius of a face-on disc ring b/M ≈ r_s/M + 1 within 10 % (GLb (1)) |
| disc brightness | PT (15n): zero at r_ms; maximum at r ≈ 9.55 M with (8π/3MṀ)F = 1.146×10⁻⁴ for a = 0 (L79 p. 233); AF's 7×10²⁶ form against PT (11b); total light = (1 − E(r_ms))Ṁ, E from BPT (2.12) |
| disc picture, a = 0 | isoradial curves r = 6, 10, 20, 30 M at 10° and 30° above the disc (L79 Figs 5, 6; direct image b = r(1 + tan²θ₀ cos²α)^{−½} far out, L79 p. 231); redshift L79 (19) and the z-curves of Fig. 7 |
| disc shifts, spin | g_min, g_max against C75 Fig. 1 / Table 1 (a/M = 0 and 0.9981) |
| camera motion | orbit speed from J15 (A.7); hovering blueshift −0.293, −0.423 at r = 4M, 3M (R19 Fig. 20 caption, (B19)); aberration (A.9) against the special-relativity formula |
| star brightness | two images on opposite sides, the primary outside the Einstein ring (J15 §3.1); magnification from the table equals the ratio of solid angles (R19 (51)); later: C75-style light curves of Cunningham & Bardeen 1973 Figs 9-13 |
| presets | the shadow's size seen from Earth: 2√27 θ_g with θ_g = 5.125 μas gives 53.3 μas beside the measured ring 51.8 ± 2.3 μas (Sgr-I Table 1); M87*: θ_g = 3.8 ± 0.4 μas, ring 42 ± 3 μas (M87-I Table 1). A comparison shown to the user, not a pass/fail (the ring "does not necessarily coincide" with the shadow's edge: M87-I §6) |
| GPU against CPU | the shader or table picture against the double-precision reference: shadow edge within a stated fraction of a pixel, disc radius and g within float rounding (DECISIONS "Tests and precision") |

## 6. Speed and weak graphics cards
**Today:** OpenGL 3.3 core shaders for everything drawn (render/shaders.h; the sky map and planets are already per-pixel fragment shaders), OpenGL 4.3 compute where the card has it,
CPU paths kept as the reference and as switches (DECISIONS "Graphics and speed"; target a 2013 Core i7-4770K). The UI tests run on Mesa's software renderer with one thread (CLAUDE.md), so anything heavy per pixel also slows the test suite.
| Way | Correctness | Cost |
|---|---|---|
| (a) step every pixel's ray in the fragment shader (J15 (A.15)) | single precision only; errors grow near the shadow edge, where DNGR needed the most steps (J15 App. A.5 footnote) | hundreds of steps × every pixel × every frame: fine on the owner's card, too slow on weak ones and in the software-rendered tests |
| (b) closed forms per pixel | exact; MF12: single precision "causes no significant artifacts" for an observer inside 500 r_s | MF12 (a = 0, disc, OpenGL 2.0 fragment shader): about 40 frames/s at 600 × 500 on a GeForce 8400M GT, over 400 at 1000 × 1000 on a GTX 480; B26: Kerr on a phone, far observer only. Elliptic functions must be hand-written in GLSL (MF12 §5) |
| (c) a table made on the CPU in double precision, looked up by the shader | as exact as the reference; needs care at the shadow edge and the disc's edges (solved by evaluating the closed shadow test J15 (A.13), (A.14) per pixel in the shader, which is a few arithmetic lines) | a = 0: one 1-D deflection function per camera distance serves all pixels (R19 §IV): negligible. Spin: a 2-D table per (camera r, θ, a); turning the head is free; moving or changing spin refills it |
| (d) draw small, scale up | blurs the thin repeated images and the shadow edge | cheap; acceptable only while the view is moving |
| (e) redraw only when the view changes | exact | the picture is static unless the camera, spin or disc changes: keep the last frame in a texture |
**Proposal.** Correctness lives in one CPU double-precision module (tested as in section 5). Drawing has levels:
- **Basic** (default on unknown or weak cards and in the tests): way (c) + (e): the table at a modest size, filled on the CPU (all cores, coarse first, then finer while the view rests), the closed shadow test per pixel. Needs only what the app needs today.
- **High:** way (b) for a = 0 and way (a) with the table as the starting guess for spin, at full resolution. **Ultra:** High with several rays per pixel near the shadow edge and the footprint filter.
- **Weakest card to support: the same as the rest of the app** (OpenGL 3.3 core; the 2013 target PC of DECISIONS), because Basic uses only texture look-ups and a short closed formula. No new minimum is needed.
- **The first build job must measure** (and put in REPORT.md): time to fill the table at three sizes for a = 0 and a near 1, one core and all cores; frame time of Basic and High at 1280 × 800 and at 4K
  on the owner's card and on the software renderer; the largest difference between the single-precision picture and the double reference at the shadow edge; table size needed for a sub-pixel shadow edge.

## 7. Fitting the app (what a fourth lab touches)
- **The lab list:** src/app/prefs.h `kLabCount = 3` and prefs.cpp `kLabNames`, `labName`, `labIndex` (line 109 keeps an unknown lab name at the default: an old app reading a new file opens the Orbit Lab; a new key prefix `blackhole.` is needed beside `orbit.`, `atom.`, `molecule=`).
- **Shell (src/ui/shell.cpp), places that list the labs by hand:** the Labs menu ids (line 156), the top-bar tabs `tabIds[kLabCount]` (1021), search (298-323), `controlsKey` and the panel-state prefix (98, 632: a two-way `?:` that sends any other lab to "atom."/"molecule."),
  the Controls panel dispatch and the "Most realistic" button (917-950), tools/toolbar (564), the Defaults question (1132-1133), the centring of the picture (1238-1264), Space = play/pause (529). Many tests are `app.lab == 1` / `< molecule::kLabIndex`: each needs a look, since "not Orbit, not Molecule" currently means Atom.
- **New files, following the Molecule Lab:** src/blackhole/ (the physics, no UI: metric, radii, integrator, disc, tables), src/ui/shell_blackhole.cpp (controls, bar, cards), a guide (as ui/molecule_learn.cpp), shaders in render/shaders.h (the tests compile every shader), a Renderer2D entry like `skyMap`.
- **"Most realistic":** ui/realest.h/.cpp: a fourth table `find("blackhole", …)` and `applyBlackHole`; the picks: look Real, shifts on, sky on.
- **Settings and setups:** ui/settings.cpp (`SettingRow.lab`, the setup text at lines 99, 165, 538 lists the three labs' settings), ui/shot.cpp (the lab's name in a screenshot's name).
- **Data panel:** ui/data_panel.cpp has `tableId[3]`, `std::clamp(app.lab, 0, 2)` (lines 42, 179, 663) and per-lab contexts: a fourth lab would silently show the Molecule Lab's tables until these are widened; analysis/lab_tables.h gets the new tables (step 6).
- **Facts and Learn:** ui/app_tabs.cpp: the fact masks are 1, 2, 4 with `lab <= 2` (lines 47-79), the Learn tab maps a card's lab name to an index (195); ui/learn_data.inc sections; About ("Where the facts come from": every paper of section 9's steps), README.md ("Three labs"), DECISIONS.md.
- **The Orbit Lab's Sagittarius A\* dot:** kind "bh" in orbit/sky_data.inc (line 324), drawn in orbit_view.cpp (1074), card in orbit/sky.cpp (259) and the Journey text (ui/journey_dates.cpp 42): the card gets "Open in the Black Hole Lab" (preset Sagittarius A*).
- **Tests:** tests/test_app.cpp (settings file round trip; an old file must still load), test_shell.inc, and the check scripts that name the tabs (check_022.py, check_023.py, check_024.py, check_025.py, check_layout020.py, check_audit19.py, check_orbit_ui.py, check_realistic.py);
  native/CMakeLists.txt: one new group `ui_blackhole` (one test per screen), a new quick program `blackhole_tests` (ctest name `blackhole_physics`).
- **What the Defense Lab left behind** (useful as the map of "what a removed/added lab touches"): the 0.19 comment at prefs.h:56, the fallback at prefs.cpp:109 with its test (test_app.cpp:126-135), changelog entries. No code of it remains.

## 8. The skin (later; owner's decision of 15 Nov 2026, DECISIONS.md)
The whole picture becomes white paper in place of black space and stars; black handwritten-style calculations, the real equations the lab uses, cover it and run
across it; near the hole the writing stretches, curls into arcs and wraps around it, bent by the same light-path calculation as the real view (the sky map of
section 6 applied to the page of writing instead of the Gaia map). It comes after the real view works. No embedding diagram, so no source is needed for it
(this section's earlier text guessed a Flamm's-paraboloid picture; that was wrong). Resca 2018 stays in data/blackhole/README.txt as background only.

## 9. Build steps
Each is one session, starts with its tests, and has a checkpoint push after the physics module and its tests pass (before any UI). Versions are a proposal (a new lab is a new X.Y, so the full UI suite runs at step 1).
| Step | The user sees | Sources (all saved) | Tests (section 5 rows) | Risks |
|---|---|---|---|---|
| 1a. Physics (done in 0.54.3) | nothing on screen: src/blackhole/ (radii for any spin, the integrator, the a = 0 closed forms, Synge's shadow, the bent-sky table, star images, presets), tests blackhole_physics, a reference picture in docs/blackhole/ | J15, BPT, PT, L79, GLa, GLb, PeT, R19, B26, G22, M87-I, Sgr-I | radii, a = 0 light path, shadow a = 0, ray equations (conservation, also at a ≠ 0), repeated images, star brightness, presets | done; measurements in REPORT 0.54.3 |
| 1b. The lab (done in 0.55.0) | a fourth tab; a black shadow in front of our lensed sky with doubled stars and the Einstein ring; turn around it, move closer or farther (hovering camera); mass control; presets Sagittarius A* and M87* (mass, size in km and AU, size seen from Earth beside the measured ring); the Orbit Lab's dot opens it; the "calculated model" line | (the 0.54.3 module) | GPU against CPU | every hand-written lab list of section 7 (largest part of the job); star flicker near the ring |
| 2. The thin disc, a = 0 (done in 0.55.1) | the disc wrapped over and under the hole, bright side and dim side, true colour with an exposure control; feeding-rate and outer-radius controls; readouts of temperature and "times dimmed" | PT, L79, AF, YN, C75 (a = 0 column) | disc brightness, disc picture a = 0, disc shifts (a = 0) | white-out without a good exposure rule; L79's misprint (use PT); the disc's sharp inner edge needs filtering |
| 3. Spin (done in 0.55.2; special radii left as a ROADMAP line) | a spin control; the shadow becomes D-shaped; the disc moves inward to r_ms; special radii shown as rings on request | J15 (A.1-A.16), BPT, GLa, GLb, PT, C75 | radii, shadow spin, ray equations, repeated images, disc shifts spin | integrator cost and step control near the shadow edge; the table's refill time when the camera moves; stars for a ≠ 0 (table inversion) |
| 4a. The looks (done in 0.55.3) | Real (dot) and Film looks with their captions; "Most realistic"; the guide's "What the film changed"; "The paper's view"; About's sources | J15 §1.2, §4.1-4.3, Figs 13-16, App. A.3.1, A.6 | the Film numbers against J15's text; one colour, no shift; the glow against its reference; Real to the pixel; the looks part per theme | done; measurements in REPORT 0.55.3 |
| 4b. Quality and the rest of the looks | quality levels High and Ultra; the Real look's eye glare; one true exposure for sky and disc | **source needed** for the glare (a published model of the eye's glare) and for a traceable sky brightness | GPU against CPU at each level | the fine table's fill time |
| 5. Moving cameras (0.56.0) | orbit the hole at speed (aberration, colour and brightness of the sky change); then fall in along a radial path, first for a = 0 | J15 (A.7)-(A.16), R19, PeT | camera motion; falling observer 84.2° | a second coordinate chart inside the horizon (R19 uses Kruskal for a = 0; for spin, ingoing Kerr coordinates are named by J15 but their equations are not in a saved paper: **source needed** before the spinning fall); time control |
| 6. Data panel tables (0.56.1) | tables and plots: special radii against spin, F(r) and T(r), efficiency against spin, shadow size against distance, the presets beside the EHT rings | BPT, PT, AF, PeT, M87-I, Sgr-I | the same closed forms as the picture; table values against the papers' printed limits | data_panel.cpp's three-lab arrays |
| 7. The skin (later) | white paper with the lab's equations written across it, bent around the hole by the real view's light paths (section 8) | none needed (the same bending as the real view) | the page's bending = the sky map's | design needed |

## 10. Open questions for the owner: ANSWERED 15 Nov 2026, all as recommended (DECISIONS.md, "Black Hole Lab")
1. **The sky.** Our own sky behind the hole, with one line saying so, plus a switch for a black background? *Recommended: yes.*
2. **Presets and the disc.** For Sagittarius A* and M87* the thin disc is not what they have. *Recommended: the two presets open with no disc and a sentence about their real faint gas; the disc is one click away and labelled "what a bright disc would look like here". Add a third preset "A feeding black hole (model)" where the disc is the real thing.*
3. **Brightness.** A true disc is blinding. *Recommended: automatic exposure with a slider, and a readout "shown N times dimmer than real".*
4. **The Film look's glow.** The film's lens data is not published. *Recommended: a simple soft glow labelled as our own; or leave it out and name the missing piece.*
5. **Spin of the presets.** Not measured. *Recommended: presets set mass and distance only; spin starts at 0 with the sentence "spin not measured"; no invented value.*
6. **Weakest card.** *Recommended: the same as today's app (the Basic level), no new minimum.*
7. **A "radio picture" look** (what the telescope saw at 1.3 mm) would need a gas simulation. *Recommended: not in these steps; one ROADMAP line.*
8. **Order.** Spin before the looks (as above), or the Film look early because it is the picture people know? *Recommended: as above; the Film look needs spin 0.6 anyway.*
