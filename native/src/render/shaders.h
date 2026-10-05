// The GLSL sources of the 2D renderer (OpenGL 3.3 core, "#version 330 core"). They live in their own header so the tests can compile every one of them with glslang
// (tests/check_shaders.py) without a graphics card, and the renderer uses exactly the same text.
#pragma once

namespace render {

// mode: 0 flat colour, 1 round dot, 2 lit ball, 3 Sun disc, 4 Sun glow, 5 soft fading dot
static const char* const kVertexShader = R"(#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aUV;
layout(location = 2) in vec4 aColor;
layout(location = 3) in vec4 aParam;
uniform vec2 uSize;
out vec2 vUV;
out vec4 vColor;
out vec4 vParam;
void main() {
    vUV = aUV; vColor = aColor; vParam = aParam;
    gl_Position = vec4(aPos.x / uSize.x * 2.0 - 1.0, 1.0 - aPos.y / uSize.y * 2.0, 0.0, 1.0);
}
)";

static const char* const kFragmentShader = R"(#version 330 core
in vec2 vUV;
in vec4 vColor;
in vec4 vParam;
out vec4 fragColor;
void main() {
    int mode = int(vParam.x + 0.5);
    if (mode == 0) { fragColor = vColor; return; }
    float d = length(vUV);                                   // 0 in the middle, 1 at the edge
    float aa = max(fwidth(d), 1e-4);
    float edge = 1.0 - smoothstep(1.0 - aa, 1.0 + aa, d);   // smooth edge, about one pixel wide
    if (mode == 1) { fragColor = vec4(vColor.rgb, vColor.a * edge); return; }
    if (mode == 2) {                                         // lit ball: Lambert shading, light from the Sun's direction on screen
        vec3 n = vec3(vUV.x, -vUV.y, sqrt(max(0.0, 1.0 - d * d)));
        vec3 L = normalize(vec3(vParam.y, -vParam.z, 0.35));
        float lit = max(dot(n, L), 0.0);
        vec3 c = vColor.rgb * (0.12 + 0.88 * lit) + vec3(0.2) * pow(lit, 12.0);
        fragColor = vec4(c, vColor.a * edge);
        return;
    }
    if (mode == 3) {                                         // the Sun: hot middle, darker edge (limb darkening)
        vec3 hot = mix(vColor.rgb, vec3(1.0, 1.0, 0.94), 0.6);
        vec3 rim = mix(vColor.rgb, vec3(0.47, 0.16, 0.0), 0.45);
        vec3 c = d < 0.55 ? mix(hot, vColor.rgb, d / 0.55) : d < 0.9 ? mix(vColor.rgb, mix(vColor.rgb, vec3(0.47, 0.16, 0.0), 0.2), (d - 0.55) / 0.35)
                                                       : mix(mix(vColor.rgb, vec3(0.47, 0.16, 0.0), 0.2), rim, (d - 0.9) / 0.1);
        fragColor = vec4(c, vColor.a * edge);
        return;
    }
    if (mode == 5) { fragColor = vec4(vColor.rgb, vColor.a * clamp(1.0 - d, 0.0, 1.0)); return; }   // soft dot fading to nothing at the edge
    // mode 4: glow that starts at 0.283 of the radius (the Sun's edge) and fades out
    float t = clamp((d - 0.283) / 0.717, 0.0, 1.0);
    float a = t < 0.25 ? mix(0.55, 0.18, t / 0.25) : mix(0.18, 0.0, (t - 0.25) / 0.75);
    if (d > 1.0) a = 0.0;
    fragColor = vec4(vColor.rgb, vColor.a * a);
}
)";

// Squares for instanced drawing: a unit square's corners (per vertex) moved to each centre (per instance)
static const char* const kSquareVS = R"(#version 330 core
layout(location = 0) in vec2 aCorner;
layout(location = 1) in vec2 aCentre;
uniform vec2 uSize;
uniform float uSide;
void main() {
    vec2 p = aCentre + aCorner * uSide;
    gl_Position = vec4(p.x / uSize.x * 2.0 - 1.0, 1.0 - p.y / uSize.y * 2.0, 0.0, 1.0);
}
)";
static const char* const kSquareFS = R"(#version 330 core
uniform vec4 uColor;
out vec4 fragColor;
void main() { fragColor = uColor; }
)";

// Blobs: one instanced quad per blob. The fragment shader fills it like a canvas radial gradient
// (two circles: the focal circle and the blob's edge), or with the fuzzy-nucleon profile, or flat.
static const char* const kBlobVS = R"(#version 330 core
layout(location = 0) in vec2 aCorner;
layout(location = 1) in vec4 aPosKind;      // x, y, r, kind
layout(location = 2) in vec4 aFocal;        // fx, fy, f0, s1 (kind 5 and 6: fx is the colour group of the dot)
layout(location = 3) in vec4 aC0;
layout(location = 4) in vec4 aC1;
layout(location = 5) in vec4 aC2;
uniform vec2 uSize;
uniform int uPalWheel;                      // kind 5 / 6 dots (Renderer2D::blobsPalette): 1 = the group is a bin of the phase wheel, 0 = the group is Sign (0 = +, else -)
uniform int uPalLight;                      // the wheel's light-page colours
uniform vec4 uPalPlus;
uniform vec4 uPalMinus;
out vec2 vLocal;
flat out vec4 vPosKind;
flat out vec4 vFocal;
flat out vec4 vC0;
flat out vec4 vC1;
flat out vec4 vC2;

// Phase colours on the GPU (0.13 stage 1). phaseRgb() is a direct port of atom::phaseRgb() in src/atom/phase_wheel.h: the C++ function is the reference, this has the
// same steps and the same constants (CIELAB circle of constant lightness -> CIE XYZ, D65 -> linear sRGB -> sRGB gamma, clipped), in 32-bit floats instead of doubles.
// The phase of bin g is atom::phaseOfBin(g) = 2 pi g / kPhaseBins, with kPhaseBins = 24.
float finv(float t) { return t > 6.0 / 29.0 ? t * t * t : 3.0 * (6.0 / 29.0) * (6.0 / 29.0) * (t - 4.0 / 29.0); }
vec3 phaseRgb(float arg, bool lightPage) {
    float L = lightPage ? 52.0 : 72.0, C = lightPage ? 58.0 : 52.0;
    float a = C * cos(arg), b = C * sin(arg);
    float fy = (L + 16.0) / 116.0, fx = fy + a / 500.0, fz = fy - b / 200.0;
    float X = 0.95047 * finv(fx), Y = finv(fy), Z = 1.08883 * finv(fz);          // D65 white point
    vec3 lin = vec3(3.2406 * X - 1.5372 * Y - 0.4986 * Z, -0.9689 * X + 1.8758 * Y + 0.0415 * Z, 0.0557 * X - 0.2040 * Y + 1.0570 * Z);
    vec3 c;
    for (int i = 0; i < 3; i++) {
        float v = clamp(lin[i], 0.0, 1.0);
        c[i] = v <= 0.0031308 ? 12.92 * v : 1.055 * pow(v, 1.0 / 2.4) - 0.055;
    }
    return c;
}

void main() {
    float kind = aPosKind.w;
    vec4 c0 = aC0;
    if (kind > 4.5) {                                         // kind 5 / 6: a square / a soft dot whose colour is picked here from its group; the rest of the shaders see kind 2 / 4
        int g = int(aFocal.x + 0.5);
        vec3 rgb = uPalWheel != 0 ? phaseRgb(2.0 * 3.14159265358979323846 * float(g) / 24.0, uPalLight != 0) : (g == 0 ? uPalPlus.rgb : uPalMinus.rgb);
        c0 = vec4(rgb, aC0.a);
        kind = kind > 5.5 ? 4.0 : 2.0;
    }
    float r = aPosKind.z;
    float ext = kind > 1.5 && kind < 2.5 ? r : r + 1.0;   // squares are exact; round things get a pixel for the smooth edge
    vec2 p = aPosKind.xy + aCorner * 2.0 * ext;
    vLocal = aCorner * 2.0 * ext / max(r, 1e-6);
    vPosKind = vec4(aPosKind.xyz, kind); vFocal = aFocal; vC0 = c0; vC1 = aC1; vC2 = aC2;
    gl_Position = vec4(p.x / uSize.x * 2.0 - 1.0, 1.0 - p.y / uSize.y * 2.0, 0.0, 1.0);
}
)";
static const char* const kBlobFS = R"(#version 330 core
in vec2 vLocal;
flat in vec4 vPosKind;
flat in vec4 vFocal;
flat in vec4 vC0;
flat in vec4 vC1;
flat in vec4 vC2;
uniform float uFuzz[17];
out vec4 fragColor;
void main() {
    int kind = int(vPosKind.w + 0.5);
    if (kind == 2) { fragColor = vC0; return; }
    float d = length(vLocal);
    float aa = max(fwidth(d), 1e-4);
    float edge = 1.0 - smoothstep(1.0 - aa, 1.0 + aa, d);
    if (kind == 3) { fragColor = vec4(vC0.rgb, vC0.a * edge); return; }
    if (kind == 4) {                                         // soft round dot: a Gaussian falloff that reaches exactly 0 at the edge (the electron cloud's glow)
        float g = (exp(-4.0 * d * d) - 0.0183156) * 1.018657;    // exp(-4) = 0.0183156
        fragColor = vec4(vC0.rgb, vC0.a * g * step(d, 1.0));
        return;
    }
    if (kind == 1) {                                         // fuzzy nucleon: the tabulated column-density profile
        float x = clamp(d, 0.0, 1.0) * 16.0;
        int i = int(min(floor(x), 15.0));
        float a = mix(uFuzz[i], uFuzz[i + 1], x - float(i));
        fragColor = vec4(vC0.rgb, vC0.a * a * edge);
        return;
    }
    // two-circle radial gradient (as the canvas createRadialGradient): find the largest t with
    // |p - c(t)| = r(t), c(t) = F (1 - t), r(t) = f0 + t (1 - f0)
    vec2 F = vFocal.xy, q = vLocal - F;
    float f0 = vFocal.z, dr = 1.0 - f0;
    float A = dot(F, F) - dr * dr, B = dot(q, F) - f0 * dr, C = dot(q, q) - f0 * f0;
    float t;
    if (abs(A) < 1e-6) t = -C / (2.0 * B);
    else {
        float disc = max(B * B - A * C, 0.0), sq = sqrt(disc);
        float t1 = (-B + sq) / A, t2 = (-B - sq) / A;
        t = max(t1, t2);
        if (f0 + t * dr < 0.0) t = min(t1, t2);
    }
    t = clamp(t, 0.0, 1.0);
    float s1 = clamp(vFocal.w, 1e-4, 0.9999);
    vec4 c = t < s1 ? mix(vC0, vC1, t / s1) : mix(vC1, vC2, (t - s1) / (1.0 - s1));
    fragColor = vec4(c.rgb, c.a * edge);
}
)";
// The soft layer: its texture (premultiplied colours) stretched over the picture
static const char* const kCompositeVS = R"(#version 330 core
layout(location = 0) in vec2 aCorner;
out vec2 vUV;
void main() { vUV = aCorner + 0.5; gl_Position = vec4(aCorner * 2.0, 0.0, 1.0); }
)";
static const char* const kCompositeFS = R"(#version 330 core
in vec2 vUV;
uniform sampler2D uTex;
out vec4 fragColor;
void main() { fragColor = texture(uTex, vUV); }
)";

// The grey pass: the picture's colours turned into their brightness (CSS grayscale(1) uses these weights)
static const char* const kGrayFS = R"(#version 330 core
in vec2 vUV;
uniform sampler2D uTex;
out vec4 fragColor;
void main() { vec4 c = texture(uTex, vUV); float y = dot(c.rgb, vec3(0.2126, 0.7152, 0.0722)); fragColor = vec4(vec3(y), c.a); }
)";

// A turning planet: each pixel of the disc -> a point on the ball -> latitude and longitude on the
// planet (through the matrix uK) -> the Earth map, or a latitude/longitude grid. Then lit from the Sun.
static const char* const kPlanetFS = R"(#version 330 core
in vec2 vUV;
in vec4 vColor;
in vec4 vParam;
uniform mat3 uK;
uniform int uEarth;
uniform float uPa;
uniform float uLod;
uniform sampler2D uTex;
out vec4 fragColor;
const float PI = 3.14159265358979;
void main() {
    float d = length(vUV);
    float aa = max(fwidth(d), 1e-4);
    float edge = 1.0 - smoothstep(1.0 - aa, 1.0 + aa, d);
    if (edge <= 0.0) discard;
    vec3 n = vec3(vUV.x, -vUV.y, sqrt(max(0.0, 1.0 - d * d)));   // screen: right, up, towards you
    vec3 b = uK * n;                                              // the planet's own frame: meridian, east, pole
    float lat = atan(b.z, sqrt(max(0.0, 1.0 - b.z * b.z)));
    float lon = atan(b.y, b.x);
    vec3 c;
    if (uEarth == 1) {
        c = textureLod(uTex, vec2(lon / (2.0 * PI) + 0.5, 0.5 - lat / PI), uLod).rgb;
    } else {                                                      // grid every 30°, the equator, and the 0° meridian brighter
        float L30 = PI / 6.0, cl = cos(lat), al = abs(lat);
        float dlat = abs(lat - round(lat / L30) * L30), dlon = abs(lon - round(lon / L30) * L30) * cl;
        float f = max(0.0, 1.0 - dlat / uPa) * 0.3;
        if (al < 1.45) f = max(f, max(0.0, 1.0 - dlon / uPa) * 0.3);
        if (al < uPa) f = max(f, (1.0 - al / uPa) * 0.6);
        float m0 = abs(lon) * cl;
        if (al < 1.5 && m0 < uPa * 1.4) f = max(f, (1.0 - m0 / (uPa * 1.4)) * 0.8);
        c = vColor.rgb + (vec3(1.0) - vColor.rgb) * f;
    }
    vec3 L = normalize(vec3(vParam.y, -vParam.z, 0.35));
    float lit = max(dot(n, L), 0.0);
    fragColor = vec4(c * (0.12 + 0.88 * lit) + vec3(0.2) * pow(lit, 12.0), vColor.a * edge);
}
)";

// ---- 0.13 stage 2: OpenGL 4.3 compute (GLSL 430 core) ----
// The colour group of every dot of a superposition or of a cloud jump, worked out on the GPU from the amplitudes stored with each dot when it was made. One kernel, two modes (uMode).
// Each function is a direct port of its C++ twin, named in its comment; the C++ functions stay the reference (tests/gl_compare.cpp counts every dot that differs, and each must be within
// float rounding of a threshold). The result is 8 bits per dot (255 = not shown, else the phase bin 0..23, or 0 / 1 for Sign), four dots to a 32-bit word, read back by the CPU.
static const char* const kGroupsCS = R"(#version 430 core
layout(local_size_x = 64) in;
layout(std430, binding = 0) readonly buffer AmpsA { vec4 ampA[]; };     // superposition: (A.re, A.im, B.re, B.im) of each dot (AtomState::sa); jump: the old dots (jA)
layout(std430, binding = 1) readonly buffer AmpsB { vec4 ampB[]; };     // jump: the new dots (jB)
layout(std430, binding = 2) writeonly buffer Groups { uint groups[]; };
uniform int uMode;      // 0: a superposition, 1: a cloud jump
uniform int uCount;     // dots to do (a jump: both populations, 2 jn)
uniform int uJn;        // a jump: dots in each population
uniform vec4 uP0;       // superposition: a, b, exp(-i theta) re, im.  jump: cos theta, sin theta, cos^2 theta, 0
uniform vec4 uP1;       // jump: exp(-i phi) re, im, exp(i phi u) re, im
uniform int uFlags;     // bit 0: the two states overlap (a superposition that does not beat); bit 1: Wheel colours (else Sign)

vec2 cmul(vec2 p, vec2 q) { return vec2(p.x * q.x - p.y * q.y, p.x * q.y + p.y * q.x); }

// port of atom::phaseBin() in src/atom/phase_wheel.h (kPhaseBins = 24, bin 0 centred on phase 0): t = fmod(arg / 2 pi + 0.5 / 24, 1), negative t + 1, min(23, int(t * 24))
int phaseBin(float arg) {
    float t = arg / 6.28318530717958647692 + 0.5 / 24.0;
    t = t - trunc(t);
    if (t < 0.0) t += 1.0;
    return min(23, int(t * 24.0));
}

// 64-bit arithmetic on (low, high) pairs of 32-bit words, for the lab's splitmix64
uvec2 mul64(uvec2 a, uvec2 b) { uint hi, lo; umulExtended(a.x, b.x, hi, lo); return uvec2(lo, hi + a.x * b.y + a.y * b.x); }
uvec2 add64(uvec2 a, uvec2 b) { uint c; uint lo = uaddCarry(a.x, b.x, c); return uvec2(lo, a.y + b.y + c); }
uvec2 xorShift(uvec2 z, int n) {        // z ^ (z >> n), 0 < n < 64
    uvec2 s = n >= 32 ? uvec2(z.y >> (n - 32), 0u) : uvec2((z.x >> n) | (z.y << (32 - n)), z.y >> n);
    return z ^ s;
}
// port of the fixed random number of a dot: splitmix64 of seed * 0x9E3779B97F4A7C15 + 0x632BE59BD9B4E019 (atom_sim.cpp: superShown and jumpUniform, which finish
// double(z >> 11) / 2^53). Here the top 24 bits of z (z >> 40), plus half a step, over 2^24: within 3e-8 of the 53-bit number.
float uniform01(uvec2 seed) {
    uvec2 z = add64(mul64(seed, uvec2(0x7F4A7C15u, 0x9E3779B9u)), uvec2(0xD9B4E019u, 0x632BE59Bu));
    z = mul64(xorShift(z, 30), uvec2(0x1CE4E5B9u, 0xBF58476Du));
    z = mul64(xorShift(z, 27), uvec2(0x133111EBu, 0x94D049BBu));
    z = xorShift(z, 31);
    return (float(z.y >> 8) + 0.5) / 16777216.0;
}

// port of AtomLab::superGroup(i, theta) = superShown + phaseBin(superPhase) in src/atom/atom_sim.cpp
uint superGroup(uint i) {
    vec2 A = ampA[i].xy, B = ampA[i].zw;
    vec2 c = uP0.x * A + uP0.y * cmul(B, uP0.zw);                        // a A + b B e^(-i theta)
    if ((uFlags & 1) != 0) {                                             // overlapping states: static, every dot with a nonzero wave is shown
        vec2 s = uP0.x * A + uP0.y * B;
        if (!(dot(s, s) > 0.0)) return 255u;
    } else {                                                             // orthogonal: shown with the odds |c|^2 / q, q = (|A|^2 + |B|^2) / 2, against the dot's fixed random number (exact thinning)
        float q = 0.5 * (dot(A, A) + dot(B, B));
        if (!(q > 0.0)) return 255u;
        float w = dot(c, c) / q;
        if (!(uniform01(uvec2(i, 0u)) < w * 0.5)) return 255u;
    }
    float ph = (c.x == 0.0 && c.y == 0.0) ? 0.0 : atan(c.y, c.x);
    return uint(phaseBin(ph));
}

// port of AtomLab::jumpGroups(): jumpFractions(A, B, T) in src/atom/atom_core.cpp, the dot's fixed random number (jumpUniform: seed = i + (pop << 40)), jumpPhaseOf(A, B, T)
uint jumpGroup(uint k) {
    uint pop = k >= uint(uJn) ? 1u : 0u;
    uint i = k - pop * uint(uJn);
    vec2 A = (pop == 1u ? ampB[i] : ampA[i]).xy, B = (pop == 1u ? ampB[i] : ampA[i]).zw;
    float cth = uP0.x, sth = uP0.y, lam = uP0.z;
    vec2 em = uP1.xy, ep = uP1.zw;
    float a2 = dot(A, A), b2 = dot(B, B);
    vec2 cc = cth * A + sth * cmul(B, em);
    float rho = dot(cc, cc);
    float aFirstA = a2 > 0.0 ? min(1.0, rho / a2) : 0.0;                              // the old dots fill first
    float aFirstB = b2 > 0.0 ? max(0.0, (rho - a2 * aFirstA) / b2) : 0.0;
    float bFirstB = b2 > 0.0 ? min(1.0, rho / b2) : 0.0;                              // the new dots fill first
    float bFirstA = a2 > 0.0 ? max(0.0, (rho - b2 * bFirstB) / a2) : 0.0;
    float f = pop == 1u ? clamp(lam * aFirstB + (1.0 - lam) * bFirstB, 0.0, 1.0) : clamp(lam * aFirstA + (1.0 - lam) * bFirstA, 0.0, 1.0);
    if (!(uniform01(uvec2(i, pop << 8)) < f)) return 255u;
    vec2 z = cmul(ep, cc);
    if ((uFlags & 2) != 0) return uint(phaseBin((z.x == 0.0 && z.y == 0.0) ? 0.0 : atan(z.y, z.x)));
    return z.x >= 0.0 ? 0u : 1u;                                                      // Sign: cos(arg) >= 0 is Re >= 0
}

void main() {
    uint t = gl_GlobalInvocationID.x;
    if (t >= (uint(uCount) + 3u) / 4u) return;
    uint word = 0u;
    for (uint j = 0u; j < 4u; j++) {
        uint g = 4u * t + j, v = 255u;
        if (g < uint(uCount)) v = uMode == 0 ? superGroup(g) : jumpGroup(g);
        word |= v << (8u * j);
    }
    groups[t] = word;
}
)";

// ---- 0.13 stage 3: the 90 % outline on the GPU (compute, GLSL 430) ----
// kOutlineDensityCS: |psi|^2 of one state at Z = 1 on a 3D grid (mode 0, the 64^3 outline pass) or on a plane (mode 1, the Slice's contour), a direct port of atom::density and its parts
// (src/atom/atom_core.cpp: radial, angular), in 32-bit floats. The state arrives as a description (render/outline_compute.h, OutlineDensity), made by outlineDensityOf in src/atom/outline.cpp.
static const char* const kOutlineDensityCS = R"(#version 430 core
layout(local_size_x = 4, local_size_y = 4, local_size_z = 4) in;
layout(std430, binding = 0) writeonly buffer Values { float values[]; };
uniform int uMode;          // 0: the grid (uN^3 cells, index (k uN + j) uN + i for x = i, y = j, z = k), 1: a plane (uN x uN cells, index b uN + a)
uniform int uN;
uniform float uHalf;        // the grid covers [-uHalf, uHalf]; the cell centres are at -uHalf + (i + 1/2) h, h = 2 uHalf / uN
uniform int uHorizontal;    // plane: 1: z = uOffset (a = x, b = y), 0: y = uOffset (a = x, b = z)
uniform float uOffset;
uniform vec4 uLag;          // Laguerre polynomial coefficients lag[0] .. lag[3]
uniform float uNq;          // the principal quantum number n
uniform int uAngRe, uAngIm; // the real angular polynomials of the state (a complex state: Re and Im; a real one: uAngIm unused)
uniform vec2 uCw;           // (c of the Im part, weight): |A|^2 = weight (Re^2 + c^2 Im^2); real orbital: (0, 1), complex state: (c, 1/2)

// port of atom::angular(a, x, y, z) in src/atom/atom_core.cpp (the real cubic harmonics; they hold the r^l factor)
float angularPoly(int a, vec3 p) {
    float x = p.x, y = p.y, z = p.z, r2 = x * x + y * y + z * z;
    switch (a) {
        case 0: return 1.0;
        case 1: return x;
        case 2: return y;
        case 3: return z;
        case 4: return 3.0 * z * z - r2;
        case 5: return x * z;
        case 6: return y * z;
        case 7: return x * y;
        case 8: return x * x - y * y;
        case 9: return z * (5.0 * z * z - 3.0 * r2);
        case 10: return x * (5.0 * z * z - r2);
        case 11: return y * (5.0 * z * z - r2);
        case 12: return x * y * z;
        case 13: return z * (x * x - y * y);
        case 14: return x * (x * x - 3.0 * y * y);
        default: return y * (3.0 * x * x - y * y);
    }
}

// port of atom::density(orb, 1, x, y, z) = |psi|^2 at Z = 1: atom::radial (Horner on the Laguerre coefficients, times e^(-r/n); the sign is squared away) times the angular part.
// A real orbital: (R A)^2. A complex state (Re + i c Im) / sqrt(2): R^2 (Re^2 + c^2 Im^2) / 2, which is |atom::psiComplex|^2.
float density(vec3 p) {
    float r = length(p);
    float q = 2.0 * r / uNq;
    float L = ((uLag.w * q + uLag.z) * q + uLag.y) * q + uLag.x;
    float R = L * exp(-r / uNq);
    float A = angularPoly(uAngRe, p), B = uCw.x != 0.0 ? angularPoly(uAngIm, p) : 0.0;
    return R * R * uCw.y * (A * A + uCw.x * uCw.x * B * B);
}

void main() {
    ivec3 g = ivec3(gl_GlobalInvocationID);
    float h = 2.0 * uHalf / float(uN);
    if (uMode == 0) {
        if (g.x >= uN || g.y >= uN || g.z >= uN) return;
        vec3 p = vec3(-uHalf + (float(g.x) + 0.5) * h, -uHalf + (float(g.y) + 0.5) * h, -uHalf + (float(g.z) + 0.5) * h);
        values[(g.z * uN + g.y) * uN + g.x] = density(p);
    } else {
        if (g.x >= uN || g.y >= uN || g.z != 0) return;
        float u = -uHalf + (float(g.x) + 0.5) * h, v = -uHalf + (float(g.y) + 0.5) * h;
        values[g.y * uN + g.x] = density(uHorizontal != 0 ? vec3(u, v, uOffset) : vec3(u, uOffset, v));
    }
}
)";

// kOutlineLevelCS: the 90 % level, with no sort. ONE work group of 1024 threads bisects over the bit patterns of the (non-negative) float cell values: for each candidate level t it adds up value * scale over
// the cells with value >= t (every thread a stride of the grid, then a tree sum in shared memory), and keeps the largest t whose sum is still >= target. 31 halvings of the 31-bit range find it exactly:
// it is the value of a cell, the same cell the CPU's sort-and-add (atom::outlineLevelCpu) stops at (up to the float sum). 31 passes over 262,144 cells in one work group: a few tenths of a millisecond,
// against the 17 ms of the CPU's sort. Result: [0] the level (float bits), [1] the probability enclosed at that level (float bits).
static const char* const kOutlineLevelCS = R"(#version 430 core
layout(local_size_x = 1024) in;
layout(std430, binding = 0) readonly buffer Grid { float grid[]; };
layout(std430, binding = 1) writeonly buffer Level { uint result[2]; };
uniform int uCount;         // cells in the grid
uniform float uScale;       // dV / norm: a cell's share of the probability is value * uScale
uniform float uTarget;      // 0.9
shared float part[1024];
shared uint sLo, sHi;

// the probability enclosed by the cells whose value, as float bits, is >= key (all threads get the same answer)
float massAbove(uint key) {
    uint tid = gl_LocalInvocationIndex;
    float s = 0.0;
    for (uint i = tid; i < uint(uCount); i += 1024u) { float v = grid[i]; if (floatBitsToUint(v) >= key) s += v * uScale; }
    part[tid] = s;
    memoryBarrierShared(); barrier();
    for (uint stride = 512u; stride > 0u; stride >>= 1) {
        if (tid < stride) part[tid] += part[tid + stride];
        memoryBarrierShared(); barrier();
    }
    float total = part[0];
    memoryBarrierShared(); barrier();
    return total;
}

void main() {
    uint tid = gl_LocalInvocationIndex;
    if (tid == 0u) { sLo = 0u; sHi = 0x7F800000u; }                         // lo: encloses >= target (everything), hi: +infinity, encloses nothing
    memoryBarrierShared(); barrier();
    for (int it = 0; it < 32; it++) {
        uint lo = sLo, hi = sHi;
        if (hi - lo <= 1u) break;
        uint mid = lo + (hi - lo) / 2u;
        float m = massAbove(mid);
        if (tid == 0u) { if (m >= uTarget) sLo = mid; else sHi = mid; }
        memoryBarrierShared(); barrier();
    }
    uint key = sLo;
    float enclosed = massAbove(key);
    if (tid == 0u) { result[0] = key; result[1] = floatBitsToUint(enclosed); }
}
)";

// ---- the chart of nuclides' retained mesh (0.13 stage 4 part B) ----
// Draws a mesh made once (render/mesh.h) in the chart's world coordinates: the same projection as atom::ChartProjection::map (chart_view.cpp), from uniforms. Triangle vertices are projected;
// a line vertex is two end points, projected, pushed half a point out along the line's screen normal (as Renderer2D::line does: nx = -dy / len * w / 2, with w = 1). The side faces of a block
// are dropped when the chart is flat or the block is too low to show them (ChartScene::build: sides = !flat && H sin(pitch) > 0.05). The fragment shader is kFragmentShader's flat colour (mode 0).
static const char* const kChartVS = R"(#version 330 core
layout(location = 0) in vec3 aP0;
layout(location = 1) in vec3 aP1;
layout(location = 2) in vec4 aColor;
layout(location = 3) in vec2 aMisc;          // corner, height
uniform vec2 uSize;
uniform vec4 uRot;                           // cos(yaw), sin(yaw), cos(pitch), sin(pitch)
uniform vec4 uCen;                           // cx, cy, K, fx   (the grid's centre, the scale, the fitted centre x)
uniform vec4 uFit;                           // fy, identity, flat, 0
out vec2 vUV;
out vec4 vColor;
out vec4 vParam;
// twin: atom::ChartProjection::map (chart_view.cpp)
vec2 projectPt(vec3 w) {
    if (uFit.y > 0.5) return w.xy;
    vec2 r;
    if (uFit.z > 0.5) r = w.xy;
    else {
        float u = w.x - uCen.x, v = w.y - uCen.y;
        float x = u * uRot.x - v * uRot.y, y = u * uRot.y + v * uRot.x;
        r = vec2(uCen.x + x, uCen.y + (y * uRot.z - w.z * uRot.w));
    }
    return vec2(uCen.w + uCen.z * (r.x - uCen.x), uFit.x + uCen.z * (r.y - uCen.y));
}
void main() {
    vUV = vec2(0.0); vColor = aColor; vParam = vec4(0.0);
    bool cull = aMisc.y >= 0.0 && (uFit.z > 0.5 || aMisc.y * uRot.w <= 0.05);
    vec2 s;
    if (aMisc.x < 0.0) s = projectPt(aP0);
    else {
        vec2 s0 = projectPt(aP0), s1 = projectPt(aP1);
        vec2 d = s1 - s0;
        float len = length(d);
        float side = aMisc.x >= 1.5 ? -1.0 : 1.0;
        vec2 n = len > 1e-9 ? vec2(-d.y, d.x) / len * 0.5 : vec2(0.0);
        s = (mod(aMisc.x, 2.0) > 0.5 ? s1 : s0) + side * n;
        if (len < 1e-9) cull = true;
    }
    gl_Position = cull ? vec4(2.0, 2.0, 0.0, 1.0) : vec4(s.x / uSize.x * 2.0 - 1.0, 1.0 - s.y / uSize.y * 2.0, 0.0, 1.0);
}
)";

// ---- the Orbit Lab's belt rocks (0.13 stage 4 part A) ----
// One thread a rock: its place on its Kepler ellipse and on the picture. Each part is a direct port of its C++ twin (orbit/belts.cpp, orbit/belts.h), named in its comment; the C++ functions stay
// the reference (tests/gl_compare.cpp compares every rock of scenes 1 and 6 with them). Times and angles that grow without limit are DOUBLE (GLSL 4.00 double precision): M = M0 + n t is formed and
// brought into -pi..pi in double, and only that small angle goes into float. The Kepler equation and the position are float (a rock's place is then good to about 1e-7 of its distance).
static const char* const kRocksCS = R"(#version 430 core
layout(local_size_x = 64) in;
layout(std430, binding = 0) readonly buffer Elements { float el[]; };          // 12 floats a rock: M0, a (AU), e, libA, libPh, P (3), Q (3), 0; a still rock: x, y, z (m) in the P slots
layout(std430, binding = 1) readonly buffer MeanMotion { double nn[]; };       // rad/s, one a rock (orbit::beltMeanMotion, made on the CPU in double)
layout(std430, binding = 2) writeonly buffer Positions { float pos[]; };       // x, y on the picture, points, two floats a rock
uniform int uCount;
uniform int uStatic;                                                           // 1: the Oort cloud, positions that never move
uniform int uD3;                                                               // 1: the 3D view (mild perspective)
uniform vec2 uT;                                                               // the simulated time t = double(uT.x) + double(uT.y) seconds (hi + lo: exact)
uniform float uLibPhase;                                                       // t / year / 150 * 6.2832, brought into 0..2 pi on the CPU in double
uniform vec4 uBc;                                                              // the barycentre, m
uniform vec4 uCam;                                                             // ca, sa, cb, sb
uniform vec4 uView;                                                            // px (points per metre), W, H, D (m)
uniform vec4 uOff;                                                             // cd (m), ox, oy (points)
const float PI = 3.14159265358979;
const float AU = 1.495978707e11;                                               // units::kAU, the IAU astronomical unit, m

// twin: orbit::solveKepler (belts.cpp). M is already in -pi..pi. At least 3 steps, stopped when a step moves E by less than 5e-7 (one float step: E is below 3.2, its spacing 2.4e-7),
// at most 20 (a float cannot go below its own rounding for e near 1, so the limit keeps the loop finite). The C++ twin's bisection safety net is not ported: from +-pi, Newton's method
// does not run away for any e < 1 (it is the start the C++ uses for e > 0.8).
float solveKepler(float e, float M) {
    float E = e > 0.8 ? (M < 0.0 ? -PI : PI) : M + e * sin(M);
    for (int k = 1; k <= 20; k++) {
        float dE = (M - (E - e * sin(E))) / (1.0 - e * cos(E));
        E += dE;
        if (k >= 3 && abs(dE) < 5e-7) break;
    }
    return E;
}

// twin: orbit::beltPoint (belts.cpp): a point on the ellipse, in the ecliptic (m, Sun at the origin)
vec3 rockPoint(uint i) {
    uint b = i * 12u;
    if (uStatic != 0) return vec3(el[b + 5u], el[b + 6u], el[b + 7u]);
    double t = double(uT.x) + double(uT.y);
    double M = double(el[b]) + nn[i] * t;                                      // M = M0 + n t (double: t reaches 1e11 s and more)
    float libA = el[b + 3u];
    if (libA != 0.0) M += double(libA * sin(uLibPhase + el[b + 4u]));         // the Trojans' sway, 150-year period (the phase was reduced in double)
    const double TWO_PI = 6.283185307179586LF;
    M -= TWO_PI * floor(M / TWO_PI + 0.5LF);                                   // -pi .. pi, in double
    float e = el[b + 2u];
    float E = solveKepler(e, float(M));
    float a = el[b + 1u] * AU;
    float xp = a * (cos(E) - e), yp = a * sqrt(1.0 - e * e) * sin(E);
    vec3 P = vec3(el[b + 5u], el[b + 6u], el[b + 7u]), Q = vec3(el[b + 8u], el[b + 9u], el[b + 10u]);
    return xp * P + yp * Q;
}

// twin: orbit::beltScreen (belts.cpp), the camera of OrbitView::draw (base and proj)
vec2 screenOf(vec3 p) {
    p -= uBc.xyz;
    float ca = uCam.x, sa = uCam.y, cb = uCam.z, sb = uCam.w;
    float x1 = p.x * ca - p.y * sa, y1 = p.x * sa + p.y * ca, y2 = y1 * cb - p.z * sb, dep = y1 * sb + p.z * cb;
    float D = uView.w;
    float f = uD3 != 0 ? D / max(D - (dep - uOff.x), 0.05 * D) : 1.0;
    return vec2(uView.y * 0.5 + x1 * uView.x * f + uOff.y, uView.z * 0.5 - y2 * uView.x * f + uOff.z);
}

void main() {
    uint i = gl_GlobalInvocationID.x;
    if (i >= uint(uCount)) return;
    vec2 s = screenOf(rockPoint(i));
    pos[2u * i] = s.x; pos[2u * i + 1u] = s.y;
}
)";

}  // namespace render
