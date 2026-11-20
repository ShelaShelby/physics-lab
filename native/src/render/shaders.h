// The GLSL sources of the 2D renderer (OpenGL 3.3 core, "#version 330 core"). They live in their own header so the tests can compile every one of them with glslang
// (tests/check_shaders.py) without a graphics card, and the renderer uses exactly the same text.
#pragma once

namespace render {

// mode: 0 flat colour, 1 round dot, 2 lit ball, 3 Sun disc, 4 (unused since 0.49.4), 5 soft fading dot
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
uniform vec3 uSpace;         // 0.50: the colour of empty space (sRGB); a lit ball's zero light (render/shading.h)
uniform sampler2D uMask;     // 0.56.0 (mode 6)
uniform int uMaskOn;
uniform ivec2 uMaskOrigin;
out vec4 fragColor;
// 0.50: IEC 61966-2-1 (sRGB) transfer functions; C++ twin render/shading.h (srgbDecode, srgbEncode)
vec3 srgbDecode(vec3 c) { return mix(c / 12.92, pow((c + 0.055) / 1.055, vec3(2.4)), step(vec3(0.04045), c)); }
vec3 srgbEncode(vec3 l) { l = clamp(l, 0.0, 1.0); return mix(12.92 * l, 1.055 * pow(l, vec3(1.0 / 2.4)) - 0.055, step(vec3(0.0031308), l)); }
void main() {
    int mode = int(vParam.x + 0.5);
    if (mode == 0) { fragColor = vColor; return; }
    float d = length(vUV);                                   // 0 in the middle, 1 at the edge
    float aa = max(fwidth(d), 1e-4);
    float edge = 1.0 - smoothstep(1.0 - aa, 1.0 + aa, d);   // smooth edge, about one pixel wide
    if (mode == 1) { fragColor = vec4(vColor.rgb, vColor.a * edge); return; }
    if (mode == 2) {                                         // lit ball: Lambert shading, light from the real direction of the Sun (right, down, towards you; 0.46.7)
        vec3 n = vec3(vUV.x, -vUV.y, sqrt(max(0.0, 1.0 - d * d)));
        vec3 L = normalize(vec3(vParam.y, -vParam.z, vParam.w));
        float lit = max(dot(n, L), 0.0);
        vec3 c = srgbEncode(srgbDecode(uSpace) + srgbDecode(vColor.rgb) * lit);   // 0.50: in linear light, no ambient floor, no highlight (render/shading.h)
        fragColor = vec4(c, vColor.a * edge);
        return;
    }
    if (mode == 3) {                                         // the Sun: hot middle, darker edge (limb darkening: the lab's own blend, NOT a measured law; the Sun's card says so; ROADMAP)
        vec3 hot = mix(vColor.rgb, vec3(1.0, 1.0, 0.94), 0.6);
        vec3 rim = mix(vColor.rgb, vec3(0.47, 0.16, 0.0), 0.45);
        vec3 c = d < 0.55 ? mix(hot, vColor.rgb, d / 0.55) : d < 0.9 ? mix(vColor.rgb, mix(vColor.rgb, vec3(0.47, 0.16, 0.0), 0.2), (d - 0.55) / 0.35)
                                                       : mix(mix(vColor.rgb, vec3(0.47, 0.16, 0.0), 0.2), rim, (d - 0.9) / 0.1);
        fragColor = vec4(c, vColor.a * edge);
        return;
    }
    if (mode == 5) { fragColor = vec4(vColor.rgb, vColor.a * clamp(1.0 - d, 0.0, 1.0)); return; }   // soft dot fading to nothing at the edge
    if (mode == 6) {                                         // 0.56.0: a round dot through the spinning hole's open-sky share (Renderer2D::maskedDisc)
        float m = 1.0;
        if (uMaskOn == 1) { ivec2 q = clamp(ivec2(gl_FragCoord.xy) - uMaskOrigin, ivec2(0), textureSize(uMask, 0) - 1); m = texelFetch(uMask, q, 0).r; }
        fragColor = vec4(vColor.rgb, vColor.a * edge * m);
        return;
    }
    fragColor = vColor;   // (mode 4, the Sun's glow, removed in 0.49.4: the corona is far too faint to show next to the disc)
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
    if (kind > 4.5 && kind < 6.5) {                           // kind 5 / 6: a square / a soft dot whose colour is picked here from its group; the rest of the shaders see kind 2 / 4
        int g = int(aFocal.x + 0.5);
        vec3 rgb = uPalWheel != 0 ? phaseRgb(2.0 * 3.14159265358979323846 * float(g) / 24.0, uPalLight != 0) : (g == 0 ? uPalPlus.rgb : uPalMinus.rgb);
        c0 = vec4(rgb, aC0.a);
        kind = kind > 5.5 ? 4.0 : 2.0;
    }
    float r = aPosKind.z;
    float ext = kind > 1.5 && kind < 2.5 ? r : r + 1.0;   // squares are exact; round things get a pixel for the smooth edge
    if (kind > 9.5 && kind < 10.5) ext = r * clamp(aFocal.y, 0.0, 1.0) + 1.0;   // 0.66.0, kind 10: the quad reaches only where its light is (fy, units of r)
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
uniform vec4 uSph[64];                      // 0.40, kind 9: every sphere of the draw (Renderer2D::spheres): x, y (points), z (points, towards the viewer), r (points)
uniform int uSphN;
out vec4 fragColor;
void main() {
    int kind = int(vPosKind.w + 0.5);
    if (kind == 9) {                                         // 0.40: a sphere with a true depth at every pixel (the Molecule Lab's Space-filling). The pixel is dropped where
        float r = vPosKind.z;                                // another sphere of the draw is nearer, so two balls meet along their real intersection curve whatever the
        float d2 = dot(vLocal, vLocal), d = sqrt(d2);        // drawing order (painting whole discs back to front made a ball jump in front of its neighbour at once)
        float aa = max(fwidth(d), 1e-4);
        float edge = 1.0 - smoothstep(1.0 - aa, 1.0 + aa, d);
        if (edge <= 0.0) discard;
        float nz = sqrt(max(1.0 - d2, 0.0));
        float zi = vFocal.y + r * nz;
        vec2 p = vPosKind.xy + vLocal * r;
        vec2 gi = -vLocal / max(nz, 1e-3);                   // the gradient of this sphere's depth on the screen (dz per point)
        int me = int(vFocal.x + 0.5);
        float seam = 1e9;                                    // points to the nearest curve where a sphere behind meets this one
        for (int j = 0; j < uSphN; j++) {
            if (j == me) continue;
            vec4 S = uSph[j];
            vec2 q = p - S.xy;
            float qq = dot(q, q), rr = S.w * S.w;
            if (qq >= rr) continue;
            float hj = sqrt(rr - qq), m = zi - (S.z + hj);
            if (m < 0.0 || (m == 0.0 && j > me)) discard;   // the other sphere is nearer here
            if (m < 4.0) seam = min(seam, m / max(length(gi + q / max(hj, 1e-3)), 1e-3));   // (only where the two surfaces nearly meet: far behind, the other's steep edge is no seam)
        }
        vec3 n = vec3(vLocal, nz);                           // screen x right, y down, z towards the viewer
        vec3 L = normalize(vec3(-0.45, -0.55, 0.70));        // the light, from the upper left in front (a drawing choice, as the old gradient)
        float diff = max(dot(n, L), 0.0);
        vec3 c = mix(vC2.rgb, vC1.rgb, smoothstep(0.0, 0.8, diff));
        c = mix(c, mix(vC1.rgb, vec3(1.0), 0.6), 0.75 * pow(max(dot(reflect(-L, n), vec3(0.0, 0.0, 1.0)), 0.0), 6.0));   // the broad shine (as the old gradient's light centre)
        c = mix(c, vec3(1.0), 0.35 * pow(max(dot(reflect(-L, n), vec3(0.0, 0.0, 1.0)), 0.0), 40.0));
        float line = min((1.0 - d) * r, seam), w = vFocal.z;  // the outline (vC0, w points wide): along the edge and along the meeting curves
        float o = 1.0 - smoothstep(w - 0.5, w + 0.5, line);
        fragColor = vec4(mix(c, vC0.rgb, o * vC0.a), edge);
        return;
    }
    if (kind == 2) { fragColor = vC0; return; }
    if (kind == 10) {                                        // 0.66.0: a glow stack, one quad for several glows round one centre (a nucleon's), added up as their own blobs
        float d = length(vLocal), fw = fwidth(d), a = 0.0;   // uSph[i] = (radius / r, relative strength, 1: kind 1's profile (strength vFocal.x) or 0: kind 4's
        for (int i = 0; i < uSphN; i++) {                    // Gaussian (strength vC0.a), -)
            vec4 S = uSph[i];
            float di = d / S.x;
            if (S.z > 0.5) {
                float aa = max(fw / S.x, 1e-4);
                float x = clamp(di, 0.0, 1.0) * 16.0;
                int j = int(min(floor(x), 15.0));
                a += vFocal.x * S.y * mix(uFuzz[j], uFuzz[j + 1], x - float(j)) * (1.0 - smoothstep(1.0 - aa, 1.0 + aa, di));
            } else a += vC0.a * S.y * (exp(-4.0 * di * di) - 0.0183156) * 1.018657 * step(di, 1.0);
        }
        fragColor = vec4(vC0.rgb, a);
        return;
    }
    if (kind == 7) {                                         // a ring (0.16 stage 5): a hard-edged band, half width vFocal.z round the circle of radius vFocal.x (units of r), as Renderer2D::ring's line pieces
        if (abs(length(vLocal) - vFocal.x) > vFocal.z) discard;
        fragColor = vC0; return;
    }
    if (kind == 8) {                                         // a line (0.16 stage 5): from -vFocal.xy to +vFocal.xy, half width vFocal.z, hard-edged and without caps, as Renderer2D::line
        vec2 a = vFocal.xy;
        float L2 = max(dot(a, a), 1e-12);
        if (abs(dot(vLocal, a)) > L2 || abs(vLocal.x * a.y - vLocal.y * a.x) > vFocal.z * sqrt(L2)) discard;
        fragColor = vC0; return;
    }
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
// 0.46 (C4): the Cloud 3D Glow dots straight from a GPU buffer of the dots' places (metres), kept between frames. This shader does per dot what atom_view and cloud_dots.cpp did on the CPU
// every frame (projectCloud, the GlowDot depth size and weight, softDot): the same formulas in the same order, in 32-bit floats; it hands the blob fragment shader (kBlobFS) a kind-4 soft dot.
// Hidden dots (group 255) and dots off the picture are sent outside the clip volume (nothing drawn), as the CPU skipped them.
static const char* const kCloudVS = R"(#version 330 core
layout(location = 0) in vec2 aCorner;
layout(location = 1) in vec3 aPos;
layout(location = 2) in float aGroup;
uniform vec2 uSize;
uniform vec4 uCam0;                         // cx, cy (points), pxm (points per metre), ext (the cloud's radius, m)
uniform vec4 uCam1;                         // cos yaw, sin yaw, cos pitch, sin pitch
uniform vec2 uView;                         // the picture's width and height (points): dots outside are not drawn
uniform float uLight;                       // base x exposure x 12,000 / dots: the light of a dot of depth weight 1 (cloud_light.h cloudDotLight before its depth weight)
uniform int uPalWheel;
uniform int uPalLight;
uniform vec4 uPalPlus;
uniform vec4 uPalMinus;
out vec2 vLocal;
flat out vec4 vPosKind;
flat out vec4 vFocal;
flat out vec4 vC0;
flat out vec4 vC1;
flat out vec4 vC2;
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
    vFocal = vec4(0.0); vC1 = vec4(0.0); vC2 = vec4(0.0);
    float x1 = aPos.x * uCam1.x - aPos.y * uCam1.y, y1 = aPos.x * uCam1.y + aPos.y * uCam1.x;
    float dep = y1 * uCam1.z - aPos.z * uCam1.w, up = y1 * uCam1.w + aPos.z * uCam1.z;
    float f = 1.0 / (1.0 + dep / (4.0 * uCam0.w));
    float X = uCam0.x + x1 * uCam0.z * f, Y = uCam0.y - up * uCam0.z * f, D = dep / uCam0.w;
    int g = int(aGroup + 0.5);
    if (g == 255 || X < 0.0 || X > uView.x || Y < 0.0 || Y > uView.y) { vLocal = vec2(0.0); vPosKind = vec4(0.0); vC0 = vec4(0.0); gl_Position = vec4(2.0, 2.0, 2.0, 1.0); return; }
    float t = max(-1.0, min(1.0, D)), near = 0.5 - 0.5 * t;
    float r = 4.6 * (0.8 + 0.4 * near), wt = 0.3 + 0.9 * near;
    float a = clamp(min(1.0, uLight * wt), 0.0, 1.0);
    vec3 rgb = uPalWheel != 0 ? phaseRgb(2.0 * 3.14159265358979323846 * float(g) / 24.0, uPalLight != 0) : (g == 0 ? uPalPlus.rgb : uPalMinus.rgb);
    float ext = r + 1.0;
    vec2 p = vec2(X, Y) + aCorner * 2.0 * ext;
    vLocal = aCorner * 2.0 * ext / max(r, 1e-6);
    vPosKind = vec4(X, Y, r, 4.0); vC0 = vec4(rgb, a);
    gl_Position = vec4(p.x / uSize.x * 2.0 - 1.0, 1.0 - p.y / uSize.y * 2.0, 0.0, 1.0);
}
)";
// 0.46 (C4): the exposure's grid on the GPU: each shown dot is one point in the cell of 8 x 8 points it falls in, adding its footprint (depth weight x r^2 x 0.785), into a small float picture
// of the grid's size (additive blending): the grid glowBlobs built on the CPU, summed in another order. The CPU then reads the few thousand cells back and picks the exposure as before.
static const char* const kCloudGridVS = R"(#version 330 core
layout(location = 1) in vec3 aPos;
layout(location = 2) in float aGroup;
uniform vec4 uCam0;
uniform vec4 uCam1;
uniform vec2 uView;
uniform vec2 uGrid;                         // the grid's cells across and down (int(w / 8) + 2, int(h / 8) + 2)
out float vVal;
void main() {
    float x1 = aPos.x * uCam1.x - aPos.y * uCam1.y, y1 = aPos.x * uCam1.y + aPos.y * uCam1.x;
    float dep = y1 * uCam1.z - aPos.z * uCam1.w, up = y1 * uCam1.w + aPos.z * uCam1.z;
    float f = 1.0 / (1.0 + dep / (4.0 * uCam0.w));
    float X = uCam0.x + x1 * uCam0.z * f, Y = uCam0.y - up * uCam0.z * f, D = dep / uCam0.w;
    int g = int(aGroup + 0.5);
    vVal = 0.0;
    if (g == 255 || X < 0.0 || X > uView.x || Y < 0.0 || Y > uView.y) { gl_Position = vec4(2.0, 2.0, 2.0, 1.0); return; }
    float t = max(-1.0, min(1.0, D)), near = 0.5 - 0.5 * t;
    float r = 4.6 * (0.8 + 0.4 * near), wt = 0.3 + 0.9 * near;
    vVal = wt * r * r * 0.785;
    int cx = max(0, min(int(uGrid.x) - 1, int(X / 8.0))), cy = max(0, min(int(uGrid.y) - 1, int(Y / 8.0)));
    gl_Position = vec4((float(cx) + 0.5) / uGrid.x * 2.0 - 1.0, (float(cy) + 0.5) / uGrid.y * 2.0 - 1.0, 0.0, 1.0);
    gl_PointSize = 1.0;
}
)";
static const char* const kCloudGridFS = R"(#version 330 core
in float vVal;
out vec4 fragColor;
void main() { fragColor = vec4(vVal, 0.0, 0.0, 0.0); }
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
// planet (through the matrix uK) -> its picture map (uMode 1, the map's middle at longitude uLon0), or its
// plain colour (uMode 0; 0.26: no grid, a planet without a map is drawn in its measured colour). Then lit from the Sun.
// uMode 2 (0.26): the sky photo. vUV is the offset from the middle of the picture in radians (right, down):
// the angle from straight ahead is its length (the night sky's map, orbit/sky.cpp skyScreen), so the direction
// is (sin th along vUV, -cos th in depth); uK turns it into galactic coordinates, and the Gaia map has
// galactic longitude growing to the left (render/planet_maps.cpp skyUV, the C++ twin). It fades out over the
// last 0.2 rad before 100 degrees, where the night sky's map ends.
static const char* const kPlanetFS = R"(#version 330 core
in vec2 vUV;
in vec4 vColor;
in vec4 vParam;
uniform mat3 uK;
uniform int uMode;
uniform float uLod;
uniform float uLon0;
uniform sampler2D uTex;
uniform int uRingN;          // 0.26: ring bands that shadow the ball: (inner, outer radius in planet radii, optical depth); 0.51.2: up to 32 (Saturn: Cassini's profile)
uniform vec3 uRingB[32];
uniform int uOccN;           // 0.51.2: round bodies that can eclipse the Sun here: (centre in the screen frame, right, up, towards you; radius), in units of this body's radius
uniform vec4 uOcc[4];
uniform float uSunAng;       // 0.51.2: the Sun's angular radius at this body, radians
uniform int uAir;            // 0.51.2: 1 = Earth's air (Rayleigh single scattering, render/shading.h earthAirLight), from the table uAirT
uniform vec3 uAirT[48];      // airTransmit at 48 path lengths, evenly spaced in ln m from uAirK.y to uAirK.z (render/shading.h airTableM)
uniform vec3 uAirK;          // (k = R / H, ln m of the first row, ln m of the last row)
uniform vec2 uShape;         // 0.27: the radii along the 90° E axis and the pole over the equatorial radius (1, 1: a ball)
uniform vec3 uSpace;         // 0.50: the colour of empty space (sRGB); a lit ball's zero light (render/shading.h)
uniform int uLunar;          // 0.50: 1 = the Moon: McEwen's lunar photometric function instead of Lambert (render/shading.h mcewenLunar)
out vec4 fragColor;
const float PI = 3.14159265358979;
// 0.50: IEC 61966-2-1 (sRGB) transfer functions; C++ twin render/shading.h (srgbDecode, srgbEncode)
vec3 srgbDecode(vec3 c) { return mix(c / 12.92, pow((c + 0.055) / 1.055, vec3(2.4)), step(vec3(0.04045), c)); }
vec3 srgbEncode(vec3 l) { l = clamp(l, 0.0, 1.0); return mix(12.92 * l, 1.055 * pow(l, vec3(1.0 / 2.4)) - 0.055, step(vec3(0.0031308), l)); }
// 0.51.2: C++ twins render/shading.h sunCovered and sunlightPast (eclipse shadows; the Sun evenly bright)
float sunCovered(float a, float b, float th) {
    if (th >= a + b) return 0.0;
    if (th <= abs(a - b)) return b >= a ? 1.0 : (b * b) / (a * a);
    float p = acos(clamp((th * th + a * a - b * b) / (2.0 * th * a), -1.0, 1.0)), q = acos(clamp((th * th + b * b - a * a) / (2.0 * th * b), -1.0, 1.0));
    float lens = a * a * p + b * b * q - 0.5 * sqrt(max(0.0, (-th + a + b) * (th + a - b) * (th - a + b) * (th + a + b)));
    return clamp(lens / (PI * a * a), 0.0, 1.0);
}
float sunlightPast(vec3 s, vec4 o, vec3 L) {
    vec3 d = o.xyz - s;
    float dist = length(d);
    if (dist <= o.w) return 0.0;
    float along = dot(d, L);
    if (along <= 0.0) return 1.0;
    return 1.0 - sunCovered(uSunAng, asin(o.w / dist), atan(length(cross(d, L)), along));
}
// C++ twins render/shading.h airMass and airTableLookup
float airMass(float mu) { mu = clamp(mu, 0.0, 1.0); float k = uAirK.x; return sqrt(k * k * mu * mu + 2.0 * k + 1.0) - k * mu; }
vec3 airT(float m) {
    float u = clamp((log(max(m, 2.0)) - uAirK.y) / (uAirK.z - uAirK.y) * 47.0, 0.0, 47.0);
    int i = min(int(u), 46);
    return mix(uAirT[i], uAirT[i + 1], u - float(i));
}
void main() {
    if (uMode == 2) {
        float th = length(vUV);
        if (th > 1.75) discard;
        vec2 d = th > 1e-6 ? vUV / th : vec2(0.0);
        vec3 v = vec3(sin(th) * d.x, -sin(th) * d.y, -cos(th));   // view frame: right, up, depth (the viewer looks towards -depth)
        vec3 g = uK * v;
        float l = atan(g.y, g.x), b = asin(clamp(g.z, -1.0, 1.0));
        vec3 c = textureLod(uTex, vec2(0.5 - l / (2.0 * PI), 0.5 - b / PI), uLod).rgb;
        float fade = 1.0 - smoothstep(1.55, 1.75, th);
        fragColor = vec4(c, vColor.a * fade);
        return;
    }
    // 0.27: the body is an ellipsoid with radii 1, uShape.x, uShape.y along its meridian, east and pole axes (a ball: 1, 1).
    // The pixel's line of sight, P0 + t D in the body frame, meets it where Q(t) = A t^2 + 2 B t + C = 0; the nearest point to you is the
    // larger root. d = sqrt(1 + min Q) is 1 on the outline and, for a ball, the distance from the middle. C++ twin: render/ellipsoid.h.
    vec3 S = vec3(1.0, 1.0 / (uShape.x * uShape.x), 1.0 / (uShape.y * uShape.y));
    vec3 P0 = uK * vec3(vUV.x, -vUV.y, 0.0), D = uK * vec3(0.0, 0.0, 1.0);
    float qa = dot(D * S, D), qb = dot(P0 * S, D), qc = dot(P0 * S, P0) - 1.0;
    float d = sqrt(max(0.0, 1.0 + qc - qb * qb / qa));
    float aa = max(fwidth(d), 1e-4);
    float t = (-qb + sqrt(max(0.0, qb * qb - qa * qc))) / qa;
    vec3 b = P0 + t * D;                                          // the point on the body, in its own frame: meridian, east, pole
    float lat = atan(b.z, length(b.xy));                          // planetocentric latitude (the maps' own: assets/maps/README.md)
    float lon = atan(b.y, b.x);
    // 0.50.3 (outside review): the mip level from this pixel's own footprint on the map, not one level for the whole disc (that was right only at the middle of the equator:
    // too sharp towards the poles, where a map row is 1/cos(lat) longer than its circle, and towards the limb). The derivatives are taken here, before any discard
    // (they need the 2x2 block), and the longitude's jump of a whole turn at the map's seam is taken out. uLod stays the floor (no sharper than before).
    vec2 uv = vec2((lon - uLon0) / (2.0 * PI) + 0.5, 0.5 - lat / PI);
    vec2 ux = dFdx(uv), uy = dFdy(uv);
    ux.x -= floor(ux.x + 0.5); uy.x -= floor(uy.x + 0.5);
    vec2 tsz = vec2(textureSize(uTex, 0));
    float lodHere = max(uLod, log2(max(max(length(ux * tsz), length(uy * tsz)), 1e-6)));
    float edge = 1.0 - smoothstep(1.0 - aa, 1.0 + aa, d);
    if (edge <= 0.0) discard;
    vec3 n = normalize(transpose(uK) * (b * S));                  // its surface normal on the screen: right, up, towards you
    vec3 c;
    if (uMode == 1) {   // 0.50: the planet maps are GL_SRGB8_ALPHA8 textures: the GPU returns linear light
        c = textureLod(uTex, uv, lodHere).rgb;
    } else {
        c = srgbDecode(vColor.rgb);
    }
    vec3 L = normalize(vec3(vParam.y, -vParam.z, vParam.w));   // 0.46.7: the real 3D direction of the Sun (was the screen direction plus a fixed 0.35 towards you)
    float lit = max(dot(n, L), 0.0);
    if (uLunar == 1) {   // 0.50: McEwen's lunar function (arXiv:2504.08252 Eq. 23); the viewer looks along -z, so cos e = n.z and the phase angle is the angle between L and +z
        float ci = dot(n, L), ce = n.z, phi = degrees(acos(clamp(L.z, -1.0, 1.0)));
        float g = exp(-phi / 60.0);
        lit = (ci > 0.0 && ce > 0.0) ? (1.0 - g) * ci + g * 2.0 * ci / (ci + ce) : 0.0;
    }
    if (uRingN > 0 && lit > 0.0) {   // the rings' shadow: the ray to the Sun crosses the ring plane (the equator, b.z = 0) through a band of optical depth tau
        vec3 Lp = uK * L;
        if (abs(Lp.z) > 1e-4) {
            float t = -b.z / Lp.z;
            if (t > 0.0) {
                float rho = length(b.xy + t * Lp.xy), tau = 0.0;
                for (int i = 0; i < 32; i++) if (i < uRingN && rho >= uRingB[i].x && rho <= uRingB[i].y) tau += uRingB[i].z;
                lit *= exp(-tau / abs(Lp.z));       // the slanted path through the ring: tau / mu0
            }
        }
    }
    float sunF = 1.0;   // 0.51.2: the share of the Sun's disc seen from this point (eclipses); the air above the point is taken to be in the same shadow
    if (uOccN > 0 && dot(n, L) > 0.0) {
        vec3 sp = vec3(vUV.x, -vUV.y, t);                       // the point in the screen frame (uK * sp = b), units of the body's radius
        for (int i = 0; i < 4; i++) if (i < uOccN) sunF *= sunlightPast(sp, uOcc[i], L);
    }
    if (uAir == 1) {   // 0.51.2: Earth's air, render/shading.h earthAirLight (the ring shadow's factor and the eclipse's dim both the surface's light and the air's)
        float mu0 = dot(n, L), mu = n.z;
        vec3 airL = vec3(0.0);
        if (mu0 > 0.0 && mu > 0.0) {
            float X0 = airMass(mu0), X = airMass(mu), cs = -L.z;
            vec3 T = airT(X0 + X);
            airL = c * mu0 * T + 0.75 * (1.0 + cs * cs) / 4.0 * X / (X0 + X) * (vec3(1.0) - T);
        }
        float ringF = mu0 > 0.0 ? lit / mu0 : 0.0;
        fragColor = vec4(srgbEncode(srgbDecode(uSpace) + airL * ringF * sunF), vColor.a * edge);
        return;
    }
    fragColor = vec4(srgbEncode(srgbDecode(uSpace) + c * lit * sunF), vColor.a * edge);   // 0.50: Lambert in linear light, then encoded; no ambient floor, no highlight (render/shading.h)
}
)";

// 0.55.0, the Black Hole Lab: our sky seen past a non-spinning black hole at the middle (uCentre, window pixels, y up) of the picture, uKpx pixels per radian, by the
// night sky's projection (the angle from the middle is the distance from it). Each pixel's ray: the angle from the shadow's edge, x = th - alpha_sh, is formed
// without cancelling, x_px = (r^2 - e^2) / (r + e) with r^2 exact (half-integer offsets from an integer centre) and e^2 given as two floats (uEdge = e, e^2 hi, lo);
// the bending dphi(x) from the CPU's double-precision table at s = ln x (uTable, uN values; t = (ln x_px - uT.x) / uT.y): the same 4-point cubic as its C++ twin
// blackhole/schwarzschild.cpp LensTable::lookupX. The far-sky direction cos(dphi) e_r + sin(dphi) t (bentDirection) through uK into galactic coordinates and the
// Gaia map (render/planet_maps.cpp skyUV), filtered by the map's stretch between neighbouring pixels (textureGrad). The shadow's edge is anti-aliased by the
// pixel's covered part (x_px + 1/2). uBend 0: the plain sky by kPlanetFS mode 2's own rule (its fixed mip level uLod), for comparison.
// uDebug 1 (tests): the far direction (view frame) and x_px as floats instead of a colour.
// 0.55.1: the gas disc (uDisc 1; blackhole/disc.h). For each pixel and each of the first four crossings of the disc's plane (phi_k = phi_0 + k pi, phi_0 from the
// disc's axis uDA on the view frame: cos(phi) A + sin(phi) B = 0), u = 1/r from the CPU's table uDU by the same 4-point cubic in s = ln|x| + |x|/xc and in
// the column phi / Phi (DiscTable::lookup), Phi from uDP; the ray must not have ended (phi <= Phi). The shift g = (1 - 3/r)^(1/2) / (1 - r^(-3/2) b Lz/b)
// (Luminet 1979 (16), (19)), the temperature T = uDT.x F(r)^(1/4) with F Page & Thorne's a = 0 flux in Luminet's (15) form with 3^(1/2)/2 (disc.h), the colour of
// a black body at g T from uDC (linear sRGB in units of the Sun's surface, against log10 T), over the screen's white (uDT.y = 1/white), scaled down whole if a
// channel is over 1, laid over the sky in linear light and encoded (IEC 61966-2-1) at the end. Opaque: the crossings are laid front to back, each covering the part of the pixel whose radius range (r +- fwidth/2) overlaps the disc's
// [rIn, rOut] (the anti-aliasing of the edges and of the thin rings); what is left shows the sky or the shadow behind. uSky 0: black behind.
// uDebug 2 (tests): the first crossing whose pixel centre lies on the disc: (order, r, g, x_px); order -1 for none.
static const char* const kLensFS = R"(#version 330 core
in vec2 vUV;
uniform sampler2D uTex;
uniform sampler2D uTable;
uniform mat3 uK;
uniform vec2 uCentre;
uniform float uKpx;
uniform vec3 uEdge;
uniform vec2 uT;
uniform int uN;
uniform int uBend;
uniform int uDebug;
uniform float uBright;
uniform float uLod;
uniform int uSky;
uniform int uDisc;
uniform sampler2D uDU;
uniform sampler2D uDP;
uniform sampler2D uDC;
uniform ivec3 uDN;
uniform vec4 uDS;
uniform vec3 uDA;
uniform vec4 uDR;
uniform vec4 uDT;
layout(location = 0) out vec4 fragColor;
layout(location = 1) out vec4 discOut;                    // 0.55.4: the disc's own light (linear; uRaw 1: before its colour is scaled into the screen) and the sky
layout(location = 2) out vec4 skyOut;                     // behind it (linear, its covered part), for the Film glow and the eye's glare (unused otherwise)
uniform int uRaw;
uniform int uExp;                                     // 0.56.0: the exposure's sample at (uSamp.x + i uSamp.y, uSamp.z - j uSamp.w): (the first disc image's
uniform vec4 uSamp;                                   // brightest channel, its Y) before the white; uWhiteMode 1: the white from uWhite (the card's own)
uniform sampler2D uWhite;
uniform int uWhiteMode;
float gInv = 1.0;
const float PI = 3.14159265358979;
float tab(int i) { return texelFetch(uTable, ivec2(clamp(i, 0, uN - 1), 0), 0).r; }
float bendAt(float xpx) {
    float t = (log(xpx) - uT.x) / uT.y;
    if (t <= 0.0) { float v0 = tab(0); return v0 + t * (tab(1) - v0); }
    int i = clamp(int(t), 1, uN - 3);
    float u = t - float(i);
    return tab(i - 1) * (-u * (u - 1.0) * (u - 2.0) / 6.0) + tab(i) * ((u + 1.0) * (u - 1.0) * (u - 2.0) / 2.0)
         + tab(i + 1) * (-(u + 1.0) * u * (u - 2.0) / 2.0) + tab(i + 2) * ((u + 1.0) * u * (u - 1.0) / 6.0);
}
vec2 mapUV(vec3 v) {
    vec3 g = uK * v;
    float l = atan(g.y, g.x), b = asin(clamp(g.z, -1.0, 1.0));
    return vec2(0.5 - l / (2.0 * PI), 0.5 - b / PI);
}
vec4 cubicW(float u) { return vec4(-u * (u - 1.0) * (u - 2.0) / 6.0, (u + 1.0) * (u - 1.0) * (u - 2.0) / 2.0, -(u + 1.0) * u * (u - 2.0) / 2.0, (u + 1.0) * u * (u - 1.0) / 6.0); }
int stencil(float q, int n, out vec4 w) { q = clamp(q, 0.0, float(n - 1)); int i = clamp(int(q), 1, n - 3); w = cubicW(q - float(i)); return i - 1; }
float dU(int row, int j) { return texelFetch(uDU, ivec2(j, row), 0).r; }
float dP(int k) { return texelFetch(uDP, ivec2(k, 0), 0).r; }
vec3 encS(vec3 l) { l = clamp(l, 0.0, 1.0); return mix(12.92 * l, 1.055 * pow(l, vec3(1.0 / 2.4)) - 0.055, step(vec3(0.0031308), l)); }   // IEC 61966-2-1
vec3 decS(vec3 e) { return mix(e / 12.92, pow((e + 0.055) / 1.055, vec3(2.4)), step(vec3(0.04045), e)); }
uniform float uAll;                                   // 0.58.0: 0 the eye's light; > 0 All light, 1 / T_sun (blackhole/disc.h allLightLinear)
vec3 allLight(vec3 c, float s, float T) {             // the eye's colour c (times s) of a black body at T scaled so its luminance is (T / T_sun)^4 (times s);
    float y = dot(c, vec3(0.2126, 0.7152, 0.0722)), q = T * uAll;   // called only when uAll > 0 (the eye's picture keeps 0.57's operations)
    q *= q;
    return y > 0.0 ? c * (q * q * s / y) : vec3(0.0);
}
vec3 discColourRaw(float r, float g) {
    float sr = sqrt(r), s3 = 1.7320508, s6 = 2.4494897;
    float F = 3.0 / (8.0 * PI) / ((r - 3.0) * r * r * sr) * (sr - s6 + 0.8660254 * log((sr + s3) * (s6 - s3) / ((sr - s3) * (s6 + s3))));
    float T = uDT.x * pow(max(F, 0.0), 0.25) * g;
    if (!(T > 0.0)) return vec3(0.0);
    float q = (log(T) * 0.43429448 - uDT.z) / uDT.w;
    if (q < 0.0) return vec3(0.0);
    q = min(q, float(uDN.z - 1) - 0.001);
    int i = int(q);
    vec3 c = mix(texelFetch(uDC, ivec2(i, 0), 0).rgb, texelFetch(uDC, ivec2(i + 1, 0), 0).rgb, q - float(i)) * gInv;
    if (uAll > 0.0) c = allLight(c, gInv, T);
    return c;
}
vec3 scaledColour(vec3 c) { float m = max(c.r, max(c.g, c.b)); return m > 1.0 ? c / m : c; }   // (a colour brighter than the screen: scaled down, its hue kept)
void main() {
    discOut = vec4(0.0); skyOut = vec4(0.0);
    float wt = texelFetch(uWhite, ivec2(0, 0), 0).r;
    gInv = uExp == 1 ? 1.0 : (uWhiteMode == 1 ? (wt > 0.0 ? 1.0 / wt : 1.0) : uDT.y);
    vec2 o = (uExp == 1 ? vec2(uSamp.x + floor(gl_FragCoord.x) * uSamp.y, uSamp.z - floor(gl_FragCoord.y) * uSamp.w) : gl_FragCoord.xy) - uCentre;   // right, up, pixels
    float d2 = dot(o, o), r = sqrt(d2), th = r / uKpx;
    vec2 dn = r > 0.0 ? o / r : vec2(0.0);
    if (uBend == 0) {
        vec3 v = vec3(sin(th) * dn.x, sin(th) * dn.y, -cos(th));
        if (uDebug == 1) { fragColor = vec4(v, 1e6); return; }
        if (th > PI || uSky == 0) { fragColor = vec4(0.0, 0.0, 0.0, 1.0); return; }
        fragColor = vec4(textureLod(uTex, mapUV(v), uLod).rgb * uBright, 1.0);
        skyOut = vec4(decS(fragColor.rgb), 1.0);
        return;
    }
    float xpx = (d2 - uEdge.y - uEdge.z) / (r + uEdge.x);
    float D = bendAt(max(xpx, 1e-3));
    vec3 v = vec3(sin(D) * dn.x, sin(D) * dn.y, cos(D));
    if (uDebug == 1) { fragColor = xpx > 0.0 ? vec4(v, xpx) : vec4(0.0, 0.0, 0.0, xpx); return; }
    vec2 uv = mapUV(v);
    vec2 ux = dFdx(uv), uy = dFdy(uv);                   // (taken by every pixel, before any choice: the derivatives need the 2 x 2 block)
    ux.x -= floor(ux.x + 0.5); uy.x -= floor(uy.x + 0.5);   // the map's seam at a whole turn of longitude
    ux = clamp(ux, vec2(-0.25), vec2(0.25)); uy = clamp(uy, vec2(-0.25), vec2(0.25));
    vec3 c = textureGrad(uTex, uv, ux, uy).rgb;
    float cover = clamp(xpx + 0.5, 0.0, 1.0);
    vec3 back = (th > PI || uSky == 0) ? vec3(0.0) : c * uBright * cover;
    if (uDisc == 0) { fragColor = vec4(back, 1.0); skyOut = vec4(decS(back), 1.0); return; }
    // the disc: the radius at each of the four crossings (every pixel, before any choice: fwidth needs the 2 x 2 block)
    float x = xpx / uKpx, ax = max(abs(x), 1e-30);
    int side = x > 0.0 ? 0 : 1;
    float q = (log(ax) + ax / uDS.w - uDS.x) / (side == 0 ? uDS.y : uDS.z);
    vec4 wr;
    int i0 = stencil(q, uDN.x, wr), row0 = side * uDN.x + i0;
    float P = dot(wr, vec4(dP(row0), dP(row0 + 1), dP(row0 + 2), dP(row0 + 3)));
    float A = uDA.z, B = dot(dn, uDA.xy), LzB = dn.y * uDA.x - dn.x * uDA.y;
    float phi0 = atan(-A, B);
    if (phi0 <= 0.0) phi0 += PI;
    float rk[4];
    for (int k = 0; k < 4; k++) {
        float phi = phi0 + float(k) * PI, rr = 1e9;
        if (phi <= P && th <= PI) {
            vec4 wc;
            int j0 = stencil(phi / P * float(uDN.y - 1), uDN.y, wc);
            float u = 0.0;
            for (int a = 0; a < 4; a++) u += wr[a] * dot(wc, vec4(dU(row0 + a, j0), dU(row0 + a, j0 + 1), dU(row0 + a, j0 + 2), dU(row0 + a, j0 + 3)));
            if (u > 0.0) rr = 1.0 / u;
        }
        rk[k] = rr;
    }
    float fw[4] = float[4](fwidth(rk[0]), fwidth(rk[1]), fwidth(rk[2]), fwidth(rk[3]));
    float b = uDR.y * sin(th);
    if (uExp == 1) {
        for (int k = 0; k < 4; k++)
            if (rk[k] >= uDR.z && rk[k] <= uDR.w) {
                vec3 c = discColourRaw(rk[k], sqrt(1.0 - 3.0 / rk[k]) / (1.0 - pow(rk[k], -1.5) * b * LzB));
                fragColor = vec4(max(c.r, max(c.g, c.b)), dot(c, vec3(0.2126, 0.7152, 0.0722)), 0.0, 1.0);
                return;
            }
        fragColor = vec4(0.0);
        return;
    }
    if (uDebug == 2) {
        for (int k = 0; k < 4; k++)
            if (rk[k] >= uDR.z && rk[k] <= uDR.w) { float g = sqrt(1.0 - 3.0 / rk[k]) / (1.0 - pow(rk[k], -1.5) * b * LzB); fragColor = vec4(float(k), rk[k], g, xpx); return; }
        fragColor = vec4(-1.0, 0.0, 0.0, xpx);
        return;
    }
    vec3 acc = vec3(0.0), accRaw = vec3(0.0);
    float tr = 1.0;
    for (int k = 0; k < 4; k++) {
        float w = max(fw[k], 1e-7);
        float cov = clamp((min(rk[k] + 0.5 * w, uDR.w) - max(rk[k] - 0.5 * w, uDR.z)) / w, 0.0, 1.0);
        if (cov <= 0.0) continue;
        float rc = clamp(rk[k], uDR.z, uDR.w);           // (a pixel that only partly covers the disc: its colour at the nearest radius on it)
        float g = sqrt(1.0 - 3.0 / rc) / (1.0 - pow(rc, -1.5) * b * LzB);
        vec3 cr = discColourRaw(rc, g);
        acc += tr * cov * scaledColour(cr);
        accRaw += tr * cov * cr;
        tr *= 1.0 - cov;
    }
    fragColor = vec4(encS(acc + tr * decS(back)), 1.0);    // (laid over each other in linear light; the map's colours are sRGB-encoded)
    discOut = vec4(uRaw == 1 ? accRaw : acc, 1.0);
    skyOut = vec4(tr * decS(back), 1.0);
}
)";

// 0.55.2, the Black Hole Lab round a SPINNING hole (blackhole/spin_sky.h; the non-spinning hole keeps kLensFS). Each pixel's ray: th = its distance from the middle
// over uKpx (the angle from the hole's direction), chi its azimuth on the camera's sky (chi = atan2(dn . uKO.zw, dn . uKO.xy): the screen directions of e_theta
// and e_phi); the shadow's edge psi_e(chi) from the processor's closed test (uKE: nEdge samples, the periodic 4-point cubic of SpinTable::edge), x = th - psi_e,
// the shadow where x < 0 (anti-aliased by the pixel's covered part x_px + 1/2); then the table (uKA: far direction, b; uKB: u = 1/r at the first four crossings
// of the equator) by the same 4 x 4 point cubic as SpinTable::lookup: the column from s = ln|x| + |x| / xc between sLo and the side's own end, the row from
// tau(chi) (uKT, straight lines between nEdge + 1 samples), periodic. The sky: the far direction (normalised) through uKs (the chart's axes for the camera at
// phi = 0, turned by the camera's phi, onto the view and into galactic coordinates) and the Gaia map as kLensFS. The disc: the first crossing with
// rIn <= r <= rOut, g = C75 (A9) from r and b (spin.h discShiftKerr: alpha, omega, varpi of J15 (A.2) on the equator, Omega = 1 / (r^(3/2) + a)), T = uDT.x
// F(r)^(1/4) with F^(1/4) from uDF (Page and Thorne's flux at this spin, against ln r), the colour as kLensFS. uDebug 1: (far direction on the chart's axes,
// x_px), uDebug 2: (order, r, g, x_px).
static const char* const kKerrFS = R"(#version 330 core
in vec2 vUV;
uniform sampler2D uTex;
uniform sampler2D uKA;
uniform sampler2D uKB;
uniform sampler2D uKE;
uniform sampler2D uKT;
uniform sampler2D uDC;
uniform sampler2D uDF;
uniform mat3 uKs;
uniform vec2 uCentre;
uniform float uKpx;
uniform ivec3 uKN;
uniform vec4 uKS;
uniform vec4 uKO;
uniform int uDebug;
uniform float uBright;
uniform int uSky;
uniform int uDisc;
uniform vec4 uDR;
uniform vec4 uDT;
uniform ivec3 uDN;
uniform vec4 uDFix;
layout(location = 0) out vec4 fragColor;
layout(location = 1) out vec4 discOut;                    // 0.55.4: the disc's own light (linear; uRaw 1: before its colour is scaled into the screen) and the sky
layout(location = 2) out vec4 skyOut;                     // behind it (linear, its covered part), for the Film glow and the eye's glare (unused otherwise)
uniform int uRaw;
const float PI = 3.14159265358979;
vec4 cubicW(float u) { return vec4(-u * (u - 1.0) * (u - 2.0) / 6.0, (u + 1.0) * (u - 1.0) * (u - 2.0) / 2.0, -(u + 1.0) * u * (u - 2.0) / 2.0, (u + 1.0) * u * (u - 1.0) / 6.0); }
int stencil(float q, int n, out vec4 w) { q = clamp(q, 0.0, float(n - 1)); int i = clamp(int(q), 1, n - 3); w = cubicW(q - float(i)); return i - 1; }
float edgeAt(float chi) {
    int n = uKN.z;
    float q = chi / (2.0 * PI) * float(n);
    q -= floor(q / float(n)) * float(n);
    int i = int(q);
    vec4 w = cubicW(q - float(i));
    float e = 0.0;
    for (int k = 0; k < 4; k++) e += w[k] * texelFetch(uKE, ivec2((i - 1 + k + n) % n, 0), 0).r   /* (GLSL: % of a negative number is undefined) */;
    return e;
}
float tauAt(float chi) {
    int n = uKN.z;
    float q = chi / (2.0 * PI) * float(n);
    float turns = floor(q / float(n));
    q -= turns * float(n);
    int i = min(int(q), n - 1);
    float t0 = texelFetch(uKT, ivec2(i, 0), 0).r, t1 = texelFetch(uKT, ivec2(i + 1, 0), 0).r;
    return turns + t0 + (q - float(i)) * (t1 - t0);
}
float sOf(float ax) { return log(ax) + ax / uKS.y; }
// the table's 4 x 4 cubic on one side at (|x|, chi): A (dir, b) and B (u0..u3)
void tableAt(int side, float ax, float pe, float chi, out vec4 A, out vec4 B) {
    float sHi = sOf(side == 0 ? PI - pe : pe);
    float t = (sOf(max(ax, uKS.z)) - uKS.x) / (sHi - uKS.x) * float(uKN.y - 1);
    vec4 wc;
    int j0 = stencil(t, uKN.y, wc);
    float q = tauAt(chi) * float(uKN.x);
    q -= floor(q / float(uKN.x)) * float(uKN.x);
    int i = min(int(q), uKN.x - 1);
    vec4 wr = cubicW(q - float(i));
    A = vec4(0.0); B = vec4(0.0);
    for (int r = 0; r < 4; r++) {
        int row = side * uKN.x + (i - 1 + r + uKN.x) % uKN.x;
        for (int c = 0; c < 4; c++) {
            float w = wr[r] * wc[c];
            A += w * texelFetch(uKA, ivec2(j0 + c, row), 0);
            B += w * texelFetch(uKB, ivec2(j0 + c, row), 0);
        }
    }
}
vec2 mapUV(vec3 v) {
    vec3 g = uKs * v;
    float l = atan(g.y, g.x), b = asin(clamp(g.z, -1.0, 1.0));
    return vec2(0.5 - l / (2.0 * PI), 0.5 - b / PI);
}
vec3 encS(vec3 l) { l = clamp(l, 0.0, 1.0); return mix(12.92 * l, 1.055 * pow(l, vec3(1.0 / 2.4)) - 0.055, step(vec3(0.0031308), l)); }   // IEC 61966-2-1
vec3 decS(vec3 e) { return mix(e / 12.92, pow((e + 0.055) / 1.055, vec3(2.4)), step(vec3(0.04045), e)); }
float shiftKerr(float r, float b) {                       // C75 (A9) on the equator
    float a = uKS.w, D = r * r - 2.0 * r + a * a, S2 = (r * r + a * a) * (r * r + a * a) - a * a * D;
    float al = sqrt(r * r * D / S2), om = 2.0 * a * r / S2, vp = sqrt(S2) / r;
    float Om = 1.0 / (r * sqrt(r) + a), V = (Om - om) * vp / al;
    return al * sqrt(max(1.0 - V * V, 0.0)) / (1.0 - Om * b);
}
uniform float uAll;                                   // 0.58.0: 0 the eye's light; > 0 All light, 1 / T_sun (blackhole/disc.h allLightLinear)
vec3 allLight(vec3 c, float s, float T) {             // the eye's colour c (times s) of a black body at T scaled so its luminance is (T / T_sun)^4 (times s);
    float y = dot(c, vec3(0.2126, 0.7152, 0.0722)), q = T * uAll;   // called only when uAll > 0 (the eye's picture keeps 0.57's operations)
    q *= q;
    return y > 0.0 ? c * (q * q * s / y) : vec3(0.0);
}
vec3 discColourRaw(float r, float g) {
    if (uDFix.w > 0.0) return uDFix.rgb;                 // 0.55.3, Film: one colour all over the disc, no shift (blackhole/looks.h)
    float q = (log(r) - uDR.z) / uDR.w;
    if (q < 0.0) return vec3(0.0);
    q = min(q, float(uDN.x - 1) - 0.001);
    int i = int(q);
    float f4 = mix(texelFetch(uDF, ivec2(i, 0), 0).r, texelFetch(uDF, ivec2(i + 1, 0), 0).r, q - float(i));
    float T = uDT.x * f4 * g;
    if (!(T > 0.0)) return vec3(0.0);
    float c = (log(T) * 0.43429448 - uDT.z) / uDT.w;
    if (c < 0.0) return vec3(0.0);
    c = min(c, float(uDN.y - 1) - 0.001);
    int k = int(c);
    vec3 col = mix(texelFetch(uDC, ivec2(k, 0), 0).rgb, texelFetch(uDC, ivec2(k + 1, 0), 0).rgb, c - float(k)) * uDT.y;
    if (uAll > 0.0) col = allLight(col, uDT.y, T);
    return col;
}
vec3 scaledColour(vec3 c) { float m = max(c.r, max(c.g, c.b)); return m > 1.0 ? c / m : c; }   // (a colour brighter than the screen: scaled down, its hue kept)
void main() {
    discOut = vec4(0.0); skyOut = vec4(0.0);
    vec2 o = gl_FragCoord.xy - uCentre;                  // right, up, pixels
    float r = length(o), th = r / uKpx;
    vec2 dn = r > 0.0 ? o / r : vec2(0.0, 1.0);
    float chi = atan(dot(dn, uKO.zw), dot(dn, uKO.xy));
    if (chi < 0.0) chi += 2.0 * PI;
    float pe = edgeAt(chi);
    float xpx = r - pe * uKpx, x = th - pe;
    vec4 A0, B0;
    tableAt(0, max(x, 0.0), pe, chi, A0, B0);             // the sky (outside; a pixel just inside takes the edge's ray for its covered part)
    vec3 v = A0.xyz / max(length(A0.xyz), 1e-30);
    if (uDebug == 1) { fragColor = x > 0.0 ? vec4(v, xpx) : vec4(0.0, 0.0, 0.0, xpx); return; }
    vec2 uv = mapUV(v);
    vec2 ux = dFdx(uv), uy = dFdy(uv);
    ux.x -= floor(ux.x + 0.5); uy.x -= floor(uy.x + 0.5);
    ux = clamp(ux, vec2(-0.25), vec2(0.25)); uy = clamp(uy, vec2(-0.25), vec2(0.25));
    vec3 c = textureGrad(uTex, uv, ux, uy).rgb;
    float cover = clamp(xpx + 0.5, 0.0, 1.0);
    vec3 back = (th > PI || uSky == 0) ? vec3(0.0) : c * uBright * cover;
    if (uDisc == 0 && uDebug != 2) { fragColor = vec4(back, 1.0); skyOut = vec4(decS(back), 1.0); return; }
    vec4 A = A0, B = B0;
    if (x <= 0.0) tableAt(1, -x, pe, chi, A, B);
    float rk[4];
    for (int k = 0; k < 4; k++) rk[k] = (B[k] > 0.0 && th <= PI) ? 1.0 / B[k] : 1e9;
    float fw[4] = float[4](fwidth(rk[0]), fwidth(rk[1]), fwidth(rk[2]), fwidth(rk[3]));
    float b = A.w;
    if (uDebug == 2) {
        for (int k = 0; k < 4; k++)
            if (rk[k] >= uDR.x && rk[k] <= uDR.y) { fragColor = vec4(float(k), rk[k], shiftKerr(rk[k], b), xpx); return; }
        fragColor = vec4(-1.0, 0.0, 0.0, xpx);
        return;
    }
    vec3 acc = vec3(0.0), accRaw = vec3(0.0);
    float tr = 1.0;
    for (int k = 0; k < 4; k++) {
        // the part of the pixel's radius range on the disc; a range below float's own step at r (the camera close above the disc: every neighbour meets it at
        // the same r) counts as a point
        float w = fw[k];
        float cov = w > 1e-5 * rk[k] ? clamp((min(rk[k] + 0.5 * w, uDR.y) - max(rk[k] - 0.5 * w, uDR.x)) / w, 0.0, 1.0) : ((rk[k] >= uDR.x && rk[k] <= uDR.y) ? 1.0 : 0.0);
        if (cov <= 0.0) continue;
        float rc = clamp(rk[k], uDR.x, uDR.y);
        vec3 cr = discColourRaw(rc, shiftKerr(rc, b));
        acc += tr * cov * scaledColour(cr);
        accRaw += tr * cov * cr;
        tr *= 1.0 - cov;
    }
    fragColor = vec4(encS(acc + tr * decS(back)), 1.0);
    discOut = vec4(uRaw == 1 ? accRaw : acc, 1.0);
    skyOut = vec4(tr * decS(back), 1.0);
}
)";

// 0.55.3: the Black Hole Lab's Film look: the app's own glow (blackhole/looks.h glowReference is the same in double; no source: the film's flare is a
// measured point-spread function that is not published). Down: each 4 x 4 block of the disc's own light (0.55.4: the lens pass's second output, linear, the
// disc and its images only; until 0.55.3 the whole picture, sky included) -> the mean of max(light - uThr, 0).
// Blur: a Gaussian of uSig blocks over -uR..uR along uDir, light outside the picture counted as none, the weights summing to 1. Add: the picture plus uStr x the
// blur (bilinear between the blocks' centres, indices clamped at the edges) in linear light, encoded again: it only adds light.
static const char* const kGlowDownFS = R"(#version 330 core
uniform sampler2D uTex;
uniform ivec2 uSize;
uniform float uThr;
out vec4 fragColor;
vec3 decS(vec3 e) { return mix(e / 12.92, pow((e + 0.055) / 1.055, vec3(2.4)), step(vec3(0.04045), e)); }
void main() {
    ivec2 b = ivec2(gl_FragCoord.xy);
    vec3 acc = vec3(0.0);
    int n = 0;
    for (int j = 0; j < 4; j++)
        for (int i = 0; i < 4; i++) {
            ivec2 p = b * 4 + ivec2(i, j);
            if (p.x < uSize.x && p.y < uSize.y) { acc += max(texelFetch(uTex, p, 0).rgb - vec3(uThr), vec3(0.0)); n++; }
        }
    fragColor = vec4(acc / float(max(n, 1)), 1.0);
}
)";
static const char* const kGlowBlurFS = R"(#version 330 core
uniform sampler2D uTex;
uniform ivec2 uN;
uniform ivec2 uDir;
uniform int uR;
uniform float uSig;
out vec4 fragColor;
void main() {
    ivec2 p = ivec2(gl_FragCoord.xy);
    vec3 acc = vec3(0.0);
    float sum = 0.0;
    for (int k = -uR; k <= uR; k++) {
        float w = exp(-0.5 * float(k * k) / (uSig * uSig));
        sum += w;
        ivec2 q = p + uDir * k;
        if (q.x >= 0 && q.y >= 0 && q.x < uN.x && q.y < uN.y) acc += w * texelFetch(uTex, q, 0).rgb;
    }
    fragColor = vec4(acc / sum, 1.0);
}
)";
static const char* const kGlowAddFS = R"(#version 330 core
in vec2 vUV;
uniform sampler2D uTex;
uniform sampler2D uGlow;
uniform ivec2 uN;
uniform float uStr;
out vec4 fragColor;
vec3 encS(vec3 l) { l = clamp(l, 0.0, 1.0); return mix(12.92 * l, 1.055 * pow(l, vec3(1.0 / 2.4)) - 0.055, step(vec3(0.0031308), l)); }   // IEC 61966-2-1
vec3 decS(vec3 e) { return mix(e / 12.92, pow((e + 0.055) / 1.055, vec3(2.4)), step(vec3(0.04045), e)); }
vec3 G(int x, int y) { return texelFetch(uGlow, ivec2(x, y), 0).rgb; }
void main() {
    ivec2 sz = textureSize(uTex, 0);
    ivec2 px = clamp(ivec2(vUV * vec2(sz)), ivec2(0), sz - 1);
    vec3 base = decS(texelFetch(uTex, px, 0).rgb);
    vec2 q = (vec2(px) + 0.5) / 4.0 - 0.5;
    ivec2 i0 = ivec2(floor(q));
    vec2 f = q - vec2(i0);
    ivec2 a = clamp(i0, ivec2(0), uN - 1), b = clamp(i0 + 1, ivec2(0), uN - 1);
    vec3 g = mix(mix(G(a.x, a.y), G(b.x, a.y), f.x), mix(G(a.x, b.y), G(b.x, b.y), f.x), f.y);
    fragColor = vec4(encS(base + uStr * g), 1.0);
}
)";

// 0.55.4: the eye's glare in the Real look (blackhole/light.h: the CIE general disability glare equation). Down: each uB x uB block of the disc's own light
// (the lens pass's second output, before its colour is scaled into the screen) -> its mean (pixels outside the picture not counted); read back, the veil is
// made on the processor (light.cpp glareVeil). Add: the disc's light x uKeep (the share each source keeps; scaled down when brighter than the screen, its hue
// kept, as the lens pass does), the sky behind it (the lens pass's third output), and the veil, bilinear between the blocks' centres (indices clamped).
static const char* const kGlareDownFS = R"(#version 330 core
uniform sampler2D uTex;
uniform ivec2 uSize;
uniform int uB;
out vec4 fragColor;
void main() {
    ivec2 b = ivec2(gl_FragCoord.xy);
    vec3 acc = vec3(0.0);
    int n = 0;
    for (int j = 0; j < uB; j++)
        for (int i = 0; i < uB; i++) {
            ivec2 p = b * uB + ivec2(i, j);
            if (p.x < uSize.x && p.y < uSize.y) { acc += texelFetch(uTex, p, 0).rgb; n++; }
        }
    fragColor = vec4(acc / float(max(n, 1)), 1.0);
}
)";
static const char* const kGlareAddFS = R"(#version 330 core
in vec2 vUV;
uniform sampler2D uDisc;
uniform sampler2D uSky;
uniform sampler2D uVeil;
uniform ivec2 uN;
uniform int uB;
uniform float uKeep;
out vec4 fragColor;
vec3 encS(vec3 l) { l = clamp(l, 0.0, 1.0); return mix(12.92 * l, 1.055 * pow(l, vec3(1.0 / 2.4)) - 0.055, step(vec3(0.0031308), l)); }   // IEC 61966-2-1
vec3 V(int x, int y) { return texelFetch(uVeil, ivec2(x, y), 0).rgb; }
void main() {
    ivec2 sz = textureSize(uDisc, 0);
    ivec2 px = clamp(ivec2(vUV * vec2(sz)), ivec2(0), sz - 1);
    vec3 d = uKeep * texelFetch(uDisc, px, 0).rgb;
    float m = max(d.r, max(d.g, d.b));
    if (m > 1.0) d /= m;
    vec2 q = (vec2(px) + 0.5) / float(uB) - 0.5;
    ivec2 i0 = ivec2(floor(q));
    vec2 f = clamp(q - vec2(i0), 0.0, 1.0);
    ivec2 a = clamp(i0, ivec2(0), uN - 1), b = clamp(i0 + 1, ivec2(0), uN - 1);
    vec3 g = mix(mix(V(a.x, a.y), V(b.x, a.y), f.x), mix(V(a.x, b.y), V(b.x, b.y), f.x), f.y);
    fragColor = vec4(encS(d + texelFetch(uSky, px, 0).rgb + g), 1.0);
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

// port of AtomLab::jumpGroups() (0.49.3): an old dot is shown with odds uP0.x, a new one with uP0.y, against the dot's fixed random number (jumpUniform: seed = i + (pop << 40));
// its colour is the phase of its own level's wave (the old level's A at an old dot, the new level's B at a new dot)
uint jumpGroup(uint k) {
    uint pop = k >= uint(uJn) ? 1u : 0u;
    uint i = k - pop * uint(uJn);
    vec4 v = pop == 1u ? ampB[i] : ampA[i];
    vec2 own = pop == 1u ? v.zw : v.xy;
    float f = pop == 1u ? uP0.y : uP0.x;
    if (!(uniform01(uvec2(i, pop << 8)) < f)) return 255u;
    if ((uFlags & 2) != 0) return uint(phaseBin((own.x == 0.0 && own.y == 0.0) ? 0.0 : atan(own.y, own.x)));
    return own.x >= 0.0 ? 0u : 1u;                                                    // Sign: cos(arg) >= 0 is Re >= 0
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
// 0.49.2: the camera in DOUBLE, and the projection below in double: in float, D - depth cancelled for dots far off the picture (the Oort cloud: depth up to 1.5e16 m),
// which put them up to 4e-3 of their coordinate from the CPU's place. Now only the rock's own float position and the final float store round.
uniform dvec4 uBc;                                                             // the barycentre, m
uniform dvec4 uCam;                                                            // ca, sa, cb, sb
uniform dvec4 uView;                                                           // px (points per metre), W, H, D (m)
uniform dvec4 uOff;                                                            // cd (m), ox, oy (points)
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
vec2 screenOf(vec3 pf) {
    dvec3 p = dvec3(pf) - uBc.xyz;
    double ca = uCam.x, sa = uCam.y, cb = uCam.z, sb = uCam.w;
    double x1 = p.x * ca - p.y * sa, y1 = p.x * sa + p.y * ca, y2 = y1 * cb - p.z * sb, dep = y1 * sb + p.z * cb;
    double D = uView.w;
    double f = uD3 != 0 ? D / max(D - (dep - uOff.x), 0.05LF * D) : 1.0LF;
    return vec2(float(uView.y * 0.5LF + x1 * uView.x * f + uOff.y), float(uView.z * 0.5LF - y2 * uView.x * f + uOff.z));
}

void main() {
    uint i = gl_GlobalInvocationID.x;
    if (i >= uint(uCount)) return;
    vec2 s = screenOf(rockPoint(i));
    pos[2u * i] = s.x; pos[2u * i + 1u] = s.y;
}
)";

static const char* const kKerrTableCS = R"(#version 430 core
// 0.55.6: the spinning hole's light paths on the graphics card (blackhole/spin_card.h): the rays, the step and the stored values of the processor's table
// (geodesic.cpp KerrBL::launch, rhs, ended, farDirection and trace; spin_sky.cpp spinRay and the node's psi; spin.cpp shadowEdge), all in double precision.
// GLSL has no double sine, cosine, arctangent, exponential or logarithm: they are written here (Taylor series after range reduction, the arctangent by Newton
// from the float one) and tested against the processor's. The camera's constants come from the processor (Pa[]), so they are its numbers to the bit.
layout(local_size_x = 64) in;
layout(std430, binding = 0) readonly buffer ParB { double Pa[]; };
layout(std430, binding = 1) readonly buffer InB { double In[]; };
layout(std430, binding = 2) buffer EdgeB { double Ed[]; };
layout(std430, binding = 3) writeonly buffer OutB { double Ou[]; };
uniform int uMode;    // 0: edges at azimuths In[i] into Ed[i]; 1: rays at (psi, chi) In[2i], In[2i+1] with the end state; 2: probes (edge Ed[i], azimuth In[i]); 3: nodes; 4: tests
uniform int uN;
uniform int uFirst;
uniform int uNChi;
uniform int uNS;
#define SPA Pa[0]
#define RC Pa[1]
#define THC Pa[2]
#define RTOL Pa[3]
#define ATOL Pa[4]
#define RFAR Pa[5]
#define HZ Pa[6]
#define ALPHA Pa[7]
#define OMEGA Pa[8]
#define VARPI Pa[9]
#define RHO Pa[10]
#define SQD Pa[11]
#define COST Pa[12]
#define SINT Pa[13]
#define TLO Pa[14]
#define THI Pa[15]
#define B1 Pa[16]
#define B2 Pa[17]
#define SLO Pa[18]
#define XC Pa[19]
#define PIO2A Pa[20]
#define PIO2B Pa[21]
#define PIO2C Pa[22]
#define LN2A Pa[23]
#define LN2B Pa[24]
#define PI Pa[25]
#define TWOOPI Pa[26]
#define INVLN2 Pa[27]
#define SQH Pa[28]
#define LN2C Pa[29]
#define MAXSTEPS 200000
#define Z5 double[5](0.0LF, 0.0LF, 0.0LF, 0.0LF, 0.0LF)

void dsincos(double x, out double s, out double c) {
    double k = floor(x * TWOOPI + 0.5LF);
    double r = ((x - k * PIO2A) - k * PIO2B) - k * PIO2C, z = r * r;
    double ps = r + r * z * (-0.16666666666666666LF + z * (0.008333333333333333LF + z * (-0.0001984126984126984LF + z * (2.7557319223985893e-06LF
                + z * (-2.505210838544172e-08LF + z * (1.6059043836821613e-10LF + z * (-7.647163731819816e-13LF + z * 2.8114572543455206e-15LF)))))));
    double pc = 1.0LF + z * (-0.5LF + z * (0.041666666666666664LF + z * (-0.001388888888888889LF + z * (2.48015873015873e-05LF + z * (-2.755731922398589e-07LF
                + z * (2.08767569878681e-09LF + z * (-1.1470745597729725e-11LF + z * (4.779477332387385e-14LF + z * -1.5619206968586225e-16LF))))))));
    int n = int(k - 4.0LF * floor(k * 0.25LF));
    if (n == 0) { s = ps; c = pc; } else if (n == 1) { s = pc; c = -ps; } else if (n == 2) { s = -ps; c = -pc; } else { s = -pc; c = ps; }
}
double dexp(double x) {
    if (x < -745.0LF) return 0.0LF;
    double k = floor(min(x, 709.0LF) * INVLN2 + 0.5LF);
    double r = ((min(x, 709.0LF) - k * LN2A) - k * LN2B) - k * LN2C;
    double p = 1.0LF + r * (1.0LF + r * (0.5LF + r * (0.16666666666666666LF + r * (0.041666666666666664LF + r * (0.008333333333333333LF + r * (0.001388888888888889LF
               + r * (0.0001984126984126984LF + r * (2.48015873015873e-05LF + r * (2.7557319223985893e-06LF + r * (2.755731922398589e-07LF + r * (2.505210838544172e-08LF
               + r * (2.08767569878681e-09LF + r * 1.6059043836821613e-10LF))))))))))));
    return ldexp(p, int(k));
}
double dlog(double x) {
    int e = 0;
    double m = frexp(x, e);
    if (m < SQH) { m = 2.0LF * m; e = e - 1; }
    double f = (m - 1.0LF) / (m + 1.0LF), z = f * f;
    double p = 1.0LF + z * (0.3333333333333333LF + z * (0.2LF + z * (0.14285714285714285LF + z * (0.1111111111111111LF + z * (0.09090909090909091LF + z * (0.07692307692307693LF
               + z * (0.06666666666666667LF + z * (0.058823529411764705LF + z * (0.05263157894736842LF + z * 0.047619047619047616LF)))))))));
    return double(e) * LN2A + (double(e) * LN2B + (double(e) * LN2C + 2.0LF * f * p));
}
double datan2(double y, double x) {
    double m = max(abs(x), abs(y));
    if (m == 0.0LF) return 0.0LF;
    double xs = x / m, ys = y / m, t = double(atan(float(ys), float(xs))), s = 0.0LF, c = 0.0LF;
    for (int it = 0; it < 2; it++) { dsincos(t, s, c); t = t - (xs * s - ys * c) / (xs * c + ys * s); }
    return t;
}
double dacos(double x) { double v = clamp(x, -1.0LF, 1.0LF); return datan2(sqrt((1.0LF - v) * (1.0LF + v)), v); }
double dcbrt(double v) {
    if (v == 0.0LF) return 0.0LF;
    int e = 0;
    double m = frexp(abs(v), e);
    int q = e >= 0 ? e / 3 : -((2 - e) / 3);
    m = ldexp(m, e - 3 * q);
    double y = double(pow(float(m), 0.33333334));
    for (int it = 0; it < 2; it++) y = y - (y * y * y - m) / (3.0LF * y * y);
    y = ldexp(y, q);
    return v < 0.0LF ? -y : y;
}
double dpowm02(double x) { return dexp(-0.2LF * dlog(x)); }   // x^-0.2
double dmax(double a, double b) { return a < b ? b : a; }        // std::max's rule (NaN in b: a)

// KerrBL::launch for a hovering camera looking at (psi, chi) (spin.h lookAt): y = (r, theta, phi, p_r, p_theta) and the constants b, q (J15 (A.11), (A.12))
void launch(double psi, double chi, out double y[5], out double b, out double q) {
    double sp = 0.0LF, cp = 0.0LF, sc = 0.0LF, cc = 0.0LF;
    dsincos(psi, sp, cp); dsincos(chi, sc, cc);
    double nr = cp, nth = -(sp * cc), nph = -(sp * sc);
    double EF = 1.0LF / (ALPHA + OMEGA * VARPI * nph);
    y[0] = RC; y[1] = THC; y[2] = 0.0LF; y[3] = EF * RHO / SQD * nr; y[4] = EF * RHO * nth;
    b = EF * VARPI * nph;
    q = y[4] * y[4] + COST * COST * (b * b / (SINT * SINT) - SPA * SPA);
}
// the ray equations J15 (A.15) (KerrBL::rhs)
void rhs(double b, double q, double y[5], out double dy[5]) {
    double s = 0.0LF, c = 0.0LF;
    dsincos(y[1], s, c);
    double r = y[0], pr = y[3], pt = y[4], a = SPA;
    double rho2 = r * r + a * a * c * c, D = r * r - 2.0LF * r + a * a, dD = 2.0LF * r - 2.0LF;
    double P = r * r + a * a - a * b, Kc = (b - a) * (b - a) + q, R = P * P - D * Kc, dR = 4.0LF * r * P - dD * Kc;
    double Th = q - c * c * (b * b / (s * s) - a * a), dTh = 2.0LF * c * (b * b / (s * s * s) - a * a * s);
    double N = -D * pr * pr - pt * pt + R / D + Th;
    double dNr = -dD * pr * pr + (dR * D - R * dD) / (D * D), drho2t = -2.0LF * a * a * c * s;
    dy[0] = D * pr / rho2;
    dy[1] = pt / rho2;
    dy[2] = (a * P / D + (b - a) + b * c * c / (s * s)) / rho2;
    dy[3] = dNr / (2.0LF * rho2) - N * 2.0LF * r / (2.0LF * rho2 * rho2);
    dy[4] = dTh / (2.0LF * rho2) - N * drho2t / (2.0LF * rho2 * rho2);
}
// one Dormand-Prince 5(4) trial step (geodesic.cpp dpStep)
double dpStep(double b, double q, double y[5], double k1[5], double h, out double yn[5], out double k7[5]) {
    double k2[5] = Z5, k3[5] = Z5, k4[5] = Z5, k5[5] = Z5, k6[5] = Z5, t[5] = Z5;
    for (int i = 0; i < 5; i++) t[i] = y[i] + h * (1.0LF / 5.0LF) * k1[i];
    rhs(b, q, t, k2);
    for (int i = 0; i < 5; i++) t[i] = y[i] + h * ((3.0LF / 40.0LF) * k1[i] + (9.0LF / 40.0LF) * k2[i]);
    rhs(b, q, t, k3);
    for (int i = 0; i < 5; i++) t[i] = y[i] + h * ((44.0LF / 45.0LF) * k1[i] + (-56.0LF / 15.0LF) * k2[i] + (32.0LF / 9.0LF) * k3[i]);
    rhs(b, q, t, k4);
    for (int i = 0; i < 5; i++) t[i] = y[i] + h * ((19372.0LF / 6561.0LF) * k1[i] + (-25360.0LF / 2187.0LF) * k2[i] + (64448.0LF / 6561.0LF) * k3[i] + (-212.0LF / 729.0LF) * k4[i]);
    rhs(b, q, t, k5);
    for (int i = 0; i < 5; i++) t[i] = y[i] + h * ((9017.0LF / 3168.0LF) * k1[i] + (-355.0LF / 33.0LF) * k2[i] + (46732.0LF / 5247.0LF) * k3[i] + (49.0LF / 176.0LF) * k4[i]
                                                   + (-5103.0LF / 18656.0LF) * k5[i]);
    rhs(b, q, t, k6);
    for (int i = 0; i < 5; i++) yn[i] = y[i] + h * ((35.0LF / 384.0LF) * k1[i] + (500.0LF / 1113.0LF) * k3[i] + (125.0LF / 192.0LF) * k4[i] + (-2187.0LF / 6784.0LF) * k5[i]
                                                    + (11.0LF / 84.0LF) * k6[i]);
    rhs(b, q, yn, k7);
    double err = 0.0LF;
    for (int i = 0; i < 5; i++) {
        double e = h * ((71.0LF / 57600.0LF) * k1[i] + (-71.0LF / 16695.0LF) * k3[i] + (71.0LF / 1920.0LF) * k4[i] + (-17253.0LF / 339200.0LF) * k5[i]
                        + (22.0LF / 525.0LF) * k6[i] + (-1.0LF / 40.0LF) * k7[i]);
        double sc = ATOL + RTOL * max(abs(y[i]), abs(yn[i]));
        err = dmax(err, abs(e) / sc);
    }
    return (isnan(err) || isinf(err)) ? 1e30LF : err;
}
double planeOf(double th) { double s = 0.0LF, c = 0.0LF; dsincos(th, s, c); return c; }
// the ray at (psi, chi) followed into its past (geodesic.cpp trace with the equator's crossings) and stored as spinRay stores it: Ou[o] = dir (3), b,
// u (4), the end (1 sky, 2 horizon, 0 lost), steps; with ext the end state y (5) and q follow
void ray(double psi, double chi, int o, bool ext) {
    double y[5] = Z5, k1[5] = Z5, yn[5] = Z5, k7[5] = Z5, b = 0.0LF, q = 0.0LF, u[4] = double[4](0.0LF, 0.0LF, 0.0LF, 0.0LF);
    launch(psi, chi, y, b, q);
    rhs(b, q, y, k1);
    double h = -0.01LF * RC, dir[3] = double[3](0.0LF, 0.0LF, 0.0LF);
    int found = 0, kind = 0, steps = 0;
    for (int st = 0; st < MAXSTEPS && kind == 0; st++) {
        double err = dpStep(b, q, y, k1, h, yn, k7);
        if (err > 1.0LF) { h = h * dmax(0.2LF, 0.9LF * dpowm02(err)); continue; }
        double p0 = planeOf(y[1]), p1 = planeOf(yn[1]);
        if (found < 4 && (p0 > 0.0LF) != (p1 > 0.0LF)) {
            // (0.55.7: Newton's steps on the crossing and the last trial step at the found hc share one call of dpStep, the same steps in the same order as
            // before: a driver that inlines every call now copies the step's seven ray equations twice instead of three times)
            double hc = h * p0 / (p0 - p1), yc[5] = Z5, kc[5] = Z5;
            bool last = false;
            for (int it = 0; it < 9; it++) {
                dpStep(b, q, y, k1, hc, yc, kc);
                if (last || it == 8) break;
                double s = 0.0LF, c = 0.0LF;
                dsincos(yc[1], s, c);
                double rate = -s * kc[1];
                if (!(rate != 0.0LF)) { last = true; continue; }
                double dh = -c / rate;
                hc = hc + dh;
                if (abs(dh) <= 1e-14LF * abs(h)) last = true;
            }
            u[found] = 1.0LF / yc[0];
            found++;
        }
        for (int i = 0; i < 5; i++) { y[i] = yn[i]; k1[i] = k7[i]; }
        steps++;
        double drdz = -k1[0];
        if (y[0] >= RFAR && drdz > 0.0LF) kind = 1;
        else if (y[0] < HZ && drdz < 0.0LF) kind = 2;
        else h = h * (err <= 0.0LF ? 5.0LF : clamp(0.9LF * dpowm02(err), 0.2LF, 5.0LF));
    }
    double re = y[0], th = y[1];
    if (kind == 1) {   // KerrBL::farDirection
        double st = 0.0LF, ct = 0.0LF, sp = 0.0LF, cp = 0.0LF;
        dsincos(th, st, ct); dsincos(y[2], sp, cp);
        double vx = st * cp * k1[0] + ct * cp * (re * k1[1]) + -sp * (re * st * k1[2]);
        double vy = st * sp * k1[0] + ct * sp * (re * k1[1]) + cp * (re * st * k1[2]);
        double vz = ct * k1[0] + -st * (re * k1[1]) + 0.0LF * (re * st * k1[2]);
        double f = -1.0LF / sqrt(vx * vx + vy * vy + vz * vz);
        dir[0] = vx * f; dir[1] = vy * f; dir[2] = vz * f;
    }
    if (found < 4) {   // the first crossing not made, continued smoothly (spinRay)
        double uv = 0.0LF, rest = 0.0LF;
        if (kind == 1) {
            double st = 0.0LF, ct = 0.0LF, sp = 0.0LF, cp = 0.0LF;
            dsincos(th, st, ct); dsincos(y[2], sp, cp);
            double Px = re * st * cp, Py = re * st * sp, Pz = re * ct;
            double muEnd = Px * dir[0] + Py * dir[1] + Pz * dir[2];
            double pz = Pz - dir[2] * muEnd;
            double mu = dir[2] != 0.0LF ? -pz / dir[2] : -1e300LF;
            uv = mu > muEnd ? 1.0LF / mu : -1.0LF / sqrt(mu * mu + 100.0LF);
        } else if (kind == 2) {
            double dthdr = k1[1] / k1[0], delta = dthdr != 0.0LF ? (PI / 2.0LF - th) / dthdr : -1e300LF;
            double z = dmax(delta, -1e3LF) / re, t = dexp(2.0LF * min(z, 0.0LF)), tz = (t - 1.0LF) / (t + 1.0LF);
            uv = delta < 0.0LF ? (1.0LF - tz) / re : 2.0LF / re;
            rest = 2.0LF / re;
        }
        u[found] = uv;
        for (int k = found + 1; k < 4; k++) u[k] = rest;
    }
    Ou[o] = dir[0]; Ou[o + 1] = dir[1]; Ou[o + 2] = dir[2]; Ou[o + 3] = b;
    for (int k = 0; k < 4; k++) Ou[o + 4 + k] = u[k];
    Ou[o + 8] = double(kind); Ou[o + 9] = double(steps);
    if (ext) { for (int i = 0; i < 5; i++) Ou[o + 10 + i] = y[i]; Ou[o + 15] = q; }
}
// the closed shadow test (spin.cpp fromHorizonPos, J15 (v)) and the edge by bisection (shadowEdge)
double radialR(double b, double q, double r) { double a = SPA, D = r * r - 2.0LF * r + a * a, P = r * r + a * a - a * b; return P * P - D * ((b - a) * (b - a) + q); }
bool fromHorizon(double psi, double chi) {
    double y[5] = Z5, b = 0.0LF, q = 0.0LF, a = SPA;
    launch(psi, chi, y, b, q);
    if (B1 < b && b < B2) {
        double lo = TLO, hi = THI, flo = lo * lo * (lo - 3.0LF) + a * a * (lo + 1.0LF) + a * b * (lo - 1.0LF);
        for (int it = 0; it < 200 && hi - lo > 1e-15LF * hi; it++) {
            double mid = 0.5LF * (lo + hi), fm = mid * mid * (mid - 3.0LF) + a * a * (mid + 1.0LF) + a * b * (mid - 1.0LF);
            if ((fm > 0.0LF) == (flo > 0.0LF)) lo = mid; else hi = mid;
        }
        double r = 0.5LF * (lo + hi), d = r - 3.0LF;
        if (q < -r * r * r * (r * d * d - 4.0LF * a * a) / (a * a * (r - 1.0LF) * (r - 1.0LF))) return y[3] > 0.0LF;
    }
    double p = (a * a - b * b - q) / 2.0LF, c = ((b - a) * (b - a) + q) / 2.0LF, t[3] = double[3](0.0LF, 0.0LF, 0.0LF);
    int n = 3;
    if (p == 0.0LF) { t[0] = dcbrt(-c); n = 1; }
    else {
        double disc = c * c / 4.0LF + p * p * p / 27.0LF;
        if (disc > 0.0LF) { double s = sqrt(disc); t[0] = dcbrt(-c / 2.0LF + s) + dcbrt(-c / 2.0LF - s); n = 1; }
        else {
            double m = 2.0LF * sqrt(-p / 3.0LF), th = dacos(clamp(3.0LF * c / (p * m), -1.0LF, 1.0LF)) / 3.0LF;
            for (int k = 0; k < 3; k++) { double s = 0.0LF, cs = 0.0LF; dsincos(th - 2.0LF * PI * double(k) / 3.0LF, s, cs); t[k] = m * cs; }
        }
    }
    for (int i = 0; i < n; i++) if (t[i] > RC && radialR(b, q, t[i]) < 0.0LF) return true;
    return false;
}
double shadowEdge(double chi) {
    double lo = 0.0LF, hi = PI;
    for (int it = 0; it < 80 && hi - lo > 1e-14LF; it++) { double mid = 0.5LF * (lo + hi); if (fromHorizon(mid, chi)) lo = mid; else hi = mid; }
    return 0.5LF * (lo + hi);
}
double sOf(double ax) { return dlog(ax) + ax / XC; }
double absXOf(double s) {   // SpinTable::absXOf
    double x = min(dexp(s), XC * max(s, 1.0LF));
    for (int it = 0; it < 100; it++) {
        double f = dlog(x) + x / XC - s, d = 1.0LF / x + 1.0LF / XC, nx = x - f / d;
        x = nx > 0.0LF ? nx : x / 2.0LF;
        if (abs(f) < 1e-15LF * max(1.0LF, abs(s))) break;
    }
    return x;
}
void main() {
    int k = int(gl_GlobalInvocationID.x);
    if (uMode == 3) {
        int q = uFirst + k;
        if (k >= uN) return;
        int side = q / (uNChi * uNS), i = (q / uNS) % uNChi, j = q % uNS;
        double pe = Ed[i], s = SLO + (sOf(side == 0 ? PI - pe : pe) - SLO) * double(j) / double(uNS - 1), ax = absXOf(s);
        ray(side == 0 ? pe + ax : pe - ax, In[i], 10 * q, false);
        return;
    }
    if (k >= uN) return;
    if (uMode == 0) Ed[k] = shadowEdge(In[k]);
    else if (uMode == 1) ray(In[2 * k], In[2 * k + 1], 16 * k, true);
    else if (uMode == 2) { double pe = Ed[k]; ray(pe + 1e-4LF * pe, In[k], 10 * k, false); }
    else {   // 4: the functions above at x (tests)
        double x = In[k], s = 0.0LF, c = 0.0LF, ax = abs(x) + 1e-300LF;
        dsincos(x, s, c);
        Ou[8 * k] = s; Ou[8 * k + 1] = c; Ou[8 * k + 2] = dexp(x); Ou[8 * k + 3] = dlog(ax);
        Ou[8 * k + 4] = datan2(x, 1.0LF - x); Ou[8 * k + 5] = dacos(x / (1.0LF + abs(x))); Ou[8 * k + 6] = dcbrt(x); Ou[8 * k + 7] = dpowm02(ax);
    }
}
)";

// ---------------------------------------------------------------------------------------------------------------------------------------------------------
// 0.56.0: the spinning hole's sky and disc drawn per pixel by the graphics card from the closed forms (blackhole/kerr_closed.h, elliptic.h) in single
// precision: for each pixel its ray's constants (J15 (A.8)-(A.12), KerrBL::launch), GLa's roots and case (the closed shadow test: J15 (v) decided by the
// roots, as traceClosed decides where a ray goes), the far direction or the horizon, the first four crossings of the disc's plane, then kKerrFS's colour
// rules. No table. Three shaders are made from the pieces below (render/kerr_sky.h; tests/check_shaders.py assembles them the same way, its ASSEMBLED list):
//   kKerrSkyTinyFS (a few lines: the first link), head + kEllipticPart + kKerrSkyMathPart (the elliptic functions alone: the second link and the tests' hook),
//   head + kEllipticPart + kKerrSkyTracePart + kKerrSkyMainPart (the whole).
// SIZE (decision of 0.56.0: the 0.55.6 compute shader was hundreds of kilobytes once a driver had inlined it): every function is called from ONE place, so
// that a driver that inlines everything still holds one copy of each: carlsonRCf in carlsonFJ (R_F and R_J by one duplication: their x, y, z steps are the
// same), carlsonFJ in a loop of two in ellFP (the amplitude, and the complete integral for a reflection), ellFP in the ray's one loop over its eight
// elliptic integrals (kKerrSkyTracePart: the jobs), amAGM in that loop's one stage of five amplitudes. Every loop has a constant bound; no double.
static const char* const kKerrSkyTinyFS = R"(#version 330 core
out vec4 fragColor;
uniform vec4 uValue;
void main() { fragColor = uValue + vec4(gl_FragCoord.xy, 0.0, 0.0); }
)";

static const char* const kKerrSkyHeadPart = R"(#version 330 core
)";

// elliptic.h in single precision. R_C in closed form (elliptic.cpp carlsonRC; its series below |e| < 0.01 here (0.56.2, smaller), to the e^3 term: the next is below 3e-10).
// R_F and R_J by Carlson's duplication together (elliptic.cpp carlsonRF, carlsonRJ; each ends when its arguments are within 4e-3 of their mean: the
// fifth-order series then leaves about 4e-15). ellFP: (F, Pi) or (F, E) for any amplitude in (-pi, pi] given by its sine and cosine (ellFang, ellPiang with
// n > 1 past the pole by G, kerr_closed.cpp's nearR2 for E); nc = 1 - n given (no rounding near n = 1); dv (0.56.2): for n < 1 (Pi - F) / n in place of Pi
// (its R_J term alone: no cancellation for small n). amAGM: the Jacobi amplitude by the AGM (elliptic.cpp
// amAGM; ends when c_N < 3e-8 a_N; 0.56.2: am(u + kf K(m)), K from the AGM's own a_N); amAny: also for m < 0 by the transformation of elliptic.h
// (am(K(mu) - u sqrt(kc2) | mu), K(mu) from the same AGM).
static const char* const kEllipticPart = R"(
const float PI = 3.14159265358979;
// atan, asin and acos to single precision (the built-in ones have no precision the GLSL specification promises; Mesa's asin is off by 1e-4): atan by
// atan(z) = pi/6 + atan((z sqrt3 - 1) / (z + sqrt3)) down to |z| <= tan(pi/12) and its series to z^11 (the next term below 3e-9); asin from it
float atan1(float t) {
    float z = abs(t);
    bool inv = z > 1.0;
    if (inv) z = 1.0 / z;
    bool red = z > 0.26794919;
    float w = red ? (z * 1.7320508 - 1.0) / (z + 1.7320508) : z, w2 = w * w;
    float r = w * (1.0 - w2 * (1.0 / 3.0 - w2 * (0.2 - w2 * (1.0 / 7.0 - w2 * (1.0 / 9.0 - w2 / 11.0)))));
    if (red) r += 0.52359878;
    if (inv) r = 1.5707963 - r;
    return t < 0.0 ? -r : r;
}
float atanS(float y, float x) {
    if (x > 0.0) return atan1(y / x);
    if (x < 0.0) return y >= 0.0 ? atan1(y / x) + PI : atan1(y / x) - PI;
    return y > 0.0 ? 0.5 * PI : (y < 0.0 ? -0.5 * PI : 0.0);
}
float atanS(float t) { return atan1(t); }
float asinS(float x) { float c = sqrt(max((1.0 - x) * (1.0 + x), 0.0)); return 2.0 * atan1(x / (1.0 + c)); }
float cbrtS(float x) { return x < 0.0 ? -pow(-x, 1.0 / 3.0) : pow(x, 1.0 / 3.0); }
float carlsonRCf(float x, float y) {
    float e = (y - x) / y;
    if (abs(e) < 0.01) return (1.0 + e * (1.0 / 6.0 + e * (3.0 / 40.0 + e * (5.0 / 112.0)))) / sqrt(y);
    if (x < y) return x > 0.0 ? atanS(sqrt((y - x) / x)) / sqrt(y - x) : 1.5707963 / sqrt(y);
    float z = sqrt((x - y) / x);
    return 0.5 * log((1.0 + z) * (1.0 + z) * x / y) / sqrt(x - y);
}
vec2 carlsonFJ(float x0, float y0, float z0, float p0, bool noJ) {
    float x = x0, y = y0, z = z0, p = p0, sum = 0.0, fac = 1.0, rf = 0.0, rj = 0.0;
    bool hasF = false, hasJ = noJ;
    for (int it = 0; it < 32; it++) {
        if (!hasF) {
            float A = (x + y + z) / 3.0, dx = (A - x) / A, dy = (A - y) / A, dz = (A - z) / A;
            if (max(abs(dx), max(abs(dy), abs(dz))) < 4e-3) {
                float e2 = dx * dy - dz * dz, e3 = dx * dy * dz;
                rf = (1.0 + (e2 / 24.0 - 0.1 - 3.0 / 44.0 * e3) * e2 + e3 / 14.0) / sqrt(A);
                hasF = true;
            }
        }
        if (!hasJ) {
            float A = (x + y + z + p + p) / 5.0, dx = (A - x) / A, dy = (A - y) / A, dz = (A - z) / A, dp = (A - p) / A;
            if (max(max(abs(dx), abs(dy)), max(abs(dz), abs(dp))) < 4e-3) {
                float C1 = 3.0 / 14.0, C2 = 1.0 / 3.0, C3 = 3.0 / 22.0, C4 = 3.0 / 26.0, C5 = 0.75 * C3, C6 = 1.5 * C4, C7 = 0.5 * C2, C8 = C3 + C3;
                float ea = dx * (dy + dz) + dy * dz, eb = dx * dy * dz, ec = dp * dp, ed = ea - 3.0 * ec, ee = eb + 2.0 * dp * (ea - ec);
                rj = 3.0 * sum + fac * (1.0 + ed * (-C1 + C5 * ed - C6 * ee) + eb * (C7 + dp * (-C8 + dp * C4)) + dp * ea * (C2 - dp * C3) - C2 * dp * ec) / (A * sqrt(A));
                hasJ = true;
            }
        }
        if (hasF && hasJ) break;
        float sx = sqrt(x), sy = sqrt(y), sz = sqrt(z), l = sx * sy + sy * sz + sz * sx;
        if (!hasJ) {
            float al = p * (sx + sy + sz) + sx * sy * sz, be = p * (p + l) * (p + l);
            sum += fac * carlsonRCf(al * al, be);
            fac *= 0.25;
            p = (p + l) * 0.25;
        }
        x = (x + l) * 0.25; y = (y + l) * 0.25; z = (z + l) * 0.25;
    }
    return vec2(rf, rj);
}
vec2 ellFP(float n, float nc, float s, float c, float kc2, bool eMode, bool dv, bool noJ) {
    float as = abs(s), ac = abs(c), m = 1.0 - kc2;
    bool beyond = !eMode && n > 1.0 && (c < 0.0 || nc + n * ac * ac <= 0.0);
    float nn = beyond ? m / n : n, nnc = beyond ? (n - m) / n : nc;
    bool dn = !eMode && dv && n < 1.0;
    float cj = eMode ? -m / 3.0 : (dn ? 1.0 / 3.0 : nn / 3.0);
    vec2 v = vec2(0.0);
    for (int k = 0; k < 2; k++) {
        if (k == 1 && c >= 0.0) break;
        float ss = k == 0 ? as : 1.0, cc = k == 0 ? ac : 0.0;
        vec2 r = carlsonFJ(cc * cc, cc * cc + kc2 * ss * ss, 1.0, eMode ? 1.0 : nnc + nn * cc * cc, noJ);
        v += (k == 1 ? 2.0 : (c >= 0.0 ? 1.0 : -1.0)) * vec2(ss * r.x, (dn ? 0.0 : ss * r.x) + cj * ss * ss * ss * r.y);   // (past pi/2: 2 K - F(pi - phi))
    }
    if (beyond) {
        float P = sqrt((n - 1.0) * nnc), d = sqrt(c * c + kc2 * s * s);
        float pole = (nc + n * ac * ac) * (nnc + nn * ac * ac);
        float plus = d * c + P * as, minus = d * c - P * as;
        float L = abs(plus) >= abs(minus) ? log(abs(plus * plus / pole)) : log(abs(pole / (minus * minus)));
        v.y = v.x - v.y + L / (2.0 * P);
    }
    if (dv && !dn) v.y = (v.y - v.x) / n;
    return s < 0.0 ? -v : v;
}
float amAGM(float u, float kc2, float kf) {
    if (kc2 >= 1.0) return u + kf * 0.5 * PI;
    float a[16];
    float c[16];
    float b = sqrt(kc2);
    a[0] = 1.0; c[0] = sqrt(1.0 - kc2);
    int N = 0;
    for (int i = 0; i < 15; i++) {
        if (abs(c[i]) <= 3e-8 * a[i]) break;
        a[i + 1] = (a[i] + b) * 0.5; c[i + 1] = (a[i] - b) * 0.5; b = sqrt(a[i] * b); N = i + 1;
    }
    float phi = exp2(float(N)) * (a[N] * u + kf * 0.5 * PI);   // (am(u + kf K): a_N K = pi/2)
    for (int j = 15; j >= 1; j--) if (j <= N) phi = (phi + asinS(clamp(c[j] * sin(phi) / a[j], -1.0, 1.0))) * 0.5;
    return phi;
}
float amAny(float u, float kc2) {
    bool neg = kc2 > 1.0;
    float ph = amAGM(neg ? -u * sqrt(kc2) : u, neg ? 1.0 / kc2 : kc2, neg ? 1.0 : 0.0);
    return neg ? 1.5707963267949 - ph : ph;
}
)";

// The tests' hook (and the second link): per argument set i (texel 2i: x, y, z, p; texel 2i + 1: n, nc, s, c; texel 2i + 2... see kerr_sky.cpp math) the
// row y = 0: (R_F, R_J, R_C(x, y), am(u | m)), y = 1: (F, Pi(n; phi | m), E(phi | m), K(m))
static const char* const kKerrSkyMathPart = R"(
uniform sampler2D uArgs;
out vec4 fragColor;
void main() {
    int i = int(gl_FragCoord.x);
    vec4 A = texelFetch(uArgs, ivec2(3 * i, 0), 0), B = texelFetch(uArgs, ivec2(3 * i + 1, 0), 0), C = texelFetch(uArgs, ivec2(3 * i + 2, 0), 0);
    float kc2 = C.x, u = C.y;
    if (int(gl_FragCoord.y) == 0) {
        vec2 fj = carlsonFJ(A.x, A.y, A.z, A.w, false);
        fragColor = vec4(fj, carlsonRCf(A.x, A.y), amAny(u, kc2));
    } else {
        vec2 fp = ellFP(B.x, B.y, B.z, B.w, kc2, false, false, false);
        vec2 fe = ellFP(0.0, 1.0, B.z, B.w, kc2, true, false, false);
        fragColor = vec4(fp, fe.y, ellFP(0.0, 1.0, 1.0, 0.0, kc2, false, false, false).x);
    }
}
)";

// One ray (kerr_closed.cpp traceClosed in single precision; the same names). The camera: uCam (a, r_c, cos theta_c, sin theta_c), uMet (alpha, omega,
// varpi, rho of J15 (A.1), (A.2) at the camera), uMet2 (Delta^(1/2), r+, r-, the end r+ + 1e-3), all made in double on the processor. Its eight elliptic
// integrals are evaluated by the one call of ellFP in the loop at the end of the set-up (jobs 0..6 need only the ray's constants: the polar start, the polar
// complete integrals, the radial K, the radial antiderivatives at the camera and at the end for r+ and r- (0.56.2: K(mu) of the polar amplitude for m < 0
// comes from that amplitude's own AGM); job 7,
// the polar Pi at the end, needs the amplitudes, made by the loop's one stage of amAny: the polar end and the radii of the four crossings).
// kind: 1 sky, 2 horizon, 0 lost. f: a quantity that changes sign at the shadow's edge and is linear across it (the radial discriminant -A/2 - z^2 - B/(4z):
// r3 = r4 = z on the edge's spherical orbits; GLa (95)), signed + on the sky's side, used only where its sign agrees with the ray's end (else +-1e30).
static const char* const kKerrSkyTracePart = R"(
struct SkyRay { int kind; vec3 dir; float b; float q; vec4 u; float f; int sub; };
uniform vec4 uCam;
uniform vec4 uMet;
uniform vec4 uMet2;
SkyRay traceRay(vec3 look) {
    SkyRay o;
    o.kind = 0; o.dir = vec3(0.0); o.b = 0.0; o.q = 0.0; o.u = vec4(0.0); o.f = 1e30; o.sub = 0;
    float a = uCam.x, rc = uCam.y, cth = uCam.z, sth = uCam.w;
    float rp = uMet2.y, rm = uMet2.z, rEnd = uMet2.w;
    // the launch (KerrBL::launch: n_F = -N for the hovering camera; J15 (A.11), (A.12))
    float nr = -look.x, nth = -look.y, nph = -look.z;
    float EF = 1.0 / (uMet.x + uMet.y * uMet.z * nph);
    float pr = EF * uMet.w / uMet2.x * nr, pth = EF * uMet.w * nth;
    float lam = EF * uMet.z * nph, eta = pth * pth + cth * cth * (lam * lam / (sth * sth) - a * a);
    o.b = lam; o.q = eta;
    float nuR = pr > 0.0 ? -1.0 : 1.0, nu = pth > 0.0 ? -1.0 : 1.0;
    float A = a * a - eta - lam * lam, root = sqrt(A * A + 4.0 * a * a * eta);
#if defined(SKY_STOP) && SKY_STOP == 1
    o.kind = 1; o.dir = normalize(vec3(A, root, pr + 1e-3)); return o;
#endif
    // ---- the polar motion (Polar::init)
    bool eq0 = false, vort = false, noG = false;
    float h = 1.0, up = 0.0, um = 0.0, sq = 1.0, pk = 1.0, pn = 0.0, pnc = 1.0, gscale = 1.0, ps0 = 0.0, pc0 = 1.0;
    if (eta > 0.0) {
        float ma2 = A < 0.0 ? (root - A) * 0.5 : 2.0 * a * a * eta / (A + root);
        up = eta / ma2; sq = sqrt(ma2); pk = 1.0 + up * a * a / ma2; pn = up; pnc = lam * lam / (a * a + ma2);
        float s0 = clamp(cth / sqrt(up), -1.0, 1.0);
        float c02 = sth * sth * pth * pth / ((a * a * cth * cth + ma2) * up);
        ps0 = -nu * s0; pc0 = sqrt(clamp(c02, 0.0, 1.0));
        noG = !(pnc > 0.0);
        gscale = 1.0 / sq;
    } else if (eta < 0.0) {
        vort = true;
        h = cth >= 0.0 ? 1.0 : -1.0;
        float u2 = -2.0 * a * a * eta / (A + root);
        um = u2 / (a * a); up = -eta / u2; sq = sqrt(u2); pk = up / um; pn = (up - um) / (1.0 - um); pnc = (1.0 - up) / (1.0 - um);
        float s2 = clamp((cth * cth - um) / (up - um), 0.0, 1.0);
        ps0 = -h * nu * sqrt(s2); pc0 = sqrt(1.0 - s2);
        noG = !(pnc > 0.0);
        gscale = 1.0 / ((1.0 - um) * sq);
    } else eq0 = true;
    float ph0 = asin(clamp(ps0, -1.0, 1.0));
    // ---- the radial roots (rootsOf) and the case (Radial::init)
    float B = 2.0 * (eta + (lam - a) * (lam - a)), C = -a * a * eta;
    float P = -A * A / 12.0 - C, Q = -A / 3.0 * ((A / 6.0) * (A / 6.0) - C) - B * B / 8.0;
    float D = (P / 3.0) * (P / 3.0) * (P / 3.0) + (Q / 2.0) * (Q / 2.0);
    float t = 0.0;
    if (D >= 0.0) { float sd = sqrt(D); t = cbrtS(-Q / 2.0 + sd) + cbrtS(-Q / 2.0 - sd); }
    else t = 2.0 * sqrt(-P / 3.0) * cos(acos(clamp(3.0 * Q / (2.0 * P) * sqrt(-3.0 / P), -1.0, 1.0)) / 3.0);
    float xi = t - A / 3.0;
    for (int it = 0; it < 3; it++) {
        float fx = ((xi + A) * xi + (A * A / 4.0 - C)) * xi - B * B / 8.0, df = (3.0 * xi + 2.0 * A) * xi + (A * A / 4.0 - C);
        if (df != 0.0) xi -= fx / df;
    }
    float z = sqrt(max(xi, 0.0) * 0.5);
    float d12 = -A / 2.0 - z * z + B / (4.0 * z), d34 = -A / 2.0 - z * z - B / (4.0 * z);
    int sub = 0;
    float r1 = 0.0, r2 = 0.0, r3 = 0.0, r4 = 0.0, r21 = 0.0, r31 = 0.0, r41 = 0.0, r32 = 0.0, r42 = 0.0, r43 = 0.0;
    float rk = 1.0, pref = 1.0, AA = 0.0, BB = 0.0, b1 = 0.0, a2 = 0.0, g0 = 0.0;
    if (d12 < 0.0) {
        sub = 4;
        b1 = z; a2 = sqrt(-d12);
        float a1 = d34 < 0.0 ? sqrt(-d34) : 0.0, bb = 2.0 * z;
        float CC = sqrt((a1 - a2) * (a1 - a2) + bb * bb), DD = sqrt((a1 + a2) * (a1 + a2) + bb * bb);
        pref = 2.0 / (CC + DD);
        rk = (DD - CC) * (DD - CC) / ((CC + DD) * (CC + DD));
        g0 = sqrt((4.0 * a2 * a2 - (CC - DD) * (CC - DD)) / ((CC + DD) * (CC + DD) - 4.0 * a2 * a2));
    } else {
        float s12 = sqrt(d12);
        r1 = -z - s12; r2 = -z + s12; r21 = 2.0 * s12;
        if (d34 < 0.0) {
            sub = 3;
            float a1 = sqrt(-d34);
            b1 = z;
            AA = sqrt((b1 - r2) * (b1 - r2) + a1 * a1); BB = sqrt((b1 - r1) * (b1 - r1) + a1 * a1);
            pref = 1.0 / sqrt(AA * BB);
            float dAB = abs(AA - BB);
            rk = (r21 - dAB) * (r21 + dAB) / (4.0 * AA * BB);
        } else {
            float s34 = sqrt(d34);
            r3 = z - s34; r4 = z + s34; r43 = 2.0 * s34;
            r31 = 2.0 * z - s34 + s12; r42 = 2.0 * z + s34 - s12; r41 = 2.0 * z + s34 + s12; r32 = 2.0 * z - s34 - s12;
            pref = 2.0 / sqrt(r31 * r42);
            rk = r21 * r43 / (r31 * r42);
            sub = r4 > rp ? (rc <= r3 ? 1 : 2) : 5;
            if (sub == 2 && rc < r4 * (1.0 - 1e-4)) return o;            // (a camera at its own turning point: r4 within float rounding of r_c)
        }
    }
    o.sub = sub;
#if defined(SKY_STOP) && SKY_STOP == 2
    o.kind = 1; o.dir = normalize(vec3(rk, pref + ph0, pk + gscale + pnc)); return o;
#endif
    if (!(rk > 0.0) || isinf(pref) || isnan(pref)) return o;
    bool two = sub == 2 || sub == 5;
    // where the ray goes (traceClosed)
    bool sky = sub != 1 && (nuR > 0.0 || sub == 2);
    float sg = nuR > 0.0 ? -1.0 : 1.0;
    // the shadow's edge: f where its sign agrees with the end (see above)
    {
        float fe = rc > z ? d34 : -d34;
        bool relevant = z > rp && ((nuR < 0.0) == (rc > z));
        if (relevant && ((fe > 0.0) == sky)) o.f = fe;
        else o.f = sky ? 1e30 : -1e30;
    }
    // the amplitudes (Radial::amp) at the camera (k = 0) and the end (k = 1)
    float as_[2]; float ac_[2];
    for (int k = 0; k < 2; k++) {
        bool inf = k == 1 && sky;
        float r = k == 0 ? rc : rEnd, s = 0.0, c = 1.0;
        if (sub == 1) {
            s = sqrt(clamp((r - r2) * r31 / ((r - r1) * r32), 0.0, 1.0)); c = sqrt(clamp(r21 * (r3 - r) / ((r - r1) * r32), 0.0, 1.0));
        } else if (two) {
            s = sqrt(max(inf ? r31 / r41 : (r - r4) * r31 / ((r - r3) * r41), 0.0)); c = sqrt(max(inf ? r43 / r41 : r43 * (r - r1) / ((r - r3) * r41), 0.0));
        } else if (sub == 3) {
            if (inf) { c = (AA - BB) / (AA + BB); s = 2.0 * sqrt(AA * BB) / (AA + BB); }
            else { float pp = AA * (r - r1), qq = BB * (r - r2), den = pp + qq; c = (pp - qq) / den; s = 2.0 * sqrt(pp * qq) / den; }
        } else {
            float n0 = sqrt(1.0 + g0 * g0), s2 = g0 / n0, c2 = 1.0 / n0;
            if (inf) { s = c2; c = -s2; }
            else { float x = (r + b1) / a2, n1 = sqrt(1.0 + x * x), s1 = x / n1, c1 = 1.0 / n1; s = s1 * c2 + c1 * s2; c = c1 * c2 - s1 * s2; }
        }
        as_[k] = s; ac_[k] = c;
    }
    // the jobs (see above): n, 1 - n, sin, cos, kc2, E instead of Pi
    float jn[8]; float jnc[8]; float js[8]; float jc[8]; float jk[8]; bool je[8]; vec2 jr[8];
    for (int k = 0; k < 8; k++) { jn[k] = 0.0; jnc[k] = 1.0; js[k] = 1.0; jc[k] = 0.0; jk[k] = 1.0; je[k] = false; jr[k] = vec2(0.0); }
    jn[0] = pn; jnc[0] = pnc; js[0] = ps0; jc[0] = pc0; jk[0] = pk;
    jn[1] = pn; jnc[1] = pnc; jk[1] = pk;
    jk[2] = rk;
    float al[4];
    for (int k = 0; k < 4; k++) {
        int pt = k / 2;
        float rr = (k % 2) == 0 ? rp : rm, n = 0.0, nc = 1.0;
        bool e = false;
        al[k] = 0.0;
        if (two) {
            float r3_ = rr - r3, r4_ = rr - r4;
            n = r3_ * r41 / (r4_ * r31); nc = -(rr - r1) * r43 / (r4_ * r31);
        } else if (sub == 3) {
            float q1 = rr - r1, q2 = rr - r2;
            al[k] = (BB * q2 + AA * q1) / (BB * q2 - AA * q1);
            e = abs(al[k] * al[k] - 1.0) < 1e-3;
            n = al[k] * al[k] / (al[k] * al[k] - 1.0); nc = -1.0 / (al[k] * al[k] - 1.0);
        } else if (sub == 4) {
            float x = (rr + b1) / a2;
            al[k] = (g0 * x - 1.0) / (g0 + x);
            n = 1.0 + al[k] * al[k]; nc = -al[k] * al[k];
        }
        jn[3 + k] = n; jnc[3 + k] = nc; js[3 + k] = as_[pt]; jc[3 + k] = ac_[pt]; jk[3 + k] = rk; je[3 + k] = e;
    }
    float tau = 0.0, Kp = 1.0, PiFull = 0.0, F0 = 0.0, G0 = 0.0, Kr = 1.0, ps = 0.0;
    float taus[4];
    float rx[4];
    int nx = 0;
    bool bad = false;
    for (int k = 0; k < 8; k++) {
        if (k == 7) {
            // the set-up's results; the Mino time to the end (traceClosed) and the amplitudes at it
            F0 = jr[0].x; G0 = noG ? 0.0 : jr[0].y; Kp = jr[1].x; PiFull = jr[1].y; Kr = jr[2].x;
            float Ic = pref * jr[3].x, Ie = pref * jr[5].x;
            if (sub == 1) tau = nuR < 0.0 ? Ic - Ie : 2.0 * pref * Kr - Ic - Ie;
            else if (sky) tau = Ie + (nuR > 0.0 ? -1.0 : 1.0) * Ic;
            else tau = Ic - Ie;
            if (!(tau > 0.0) || isinf(tau)) { bad = true; break; }
            if (!eq0 && !vort) {
                for (int j = 0; j < 8; j++) {
                    float tj = (2.0 * float(j) * Kp - F0) / sq;
                    if (tj >= tau || nx >= 4) break;
                    if (tj > 0.0) { taus[nx] = tj; nx++; }
                }
            }
            float Fc = jr[3].x;
            for (int i = 0; i < 5; i++) {
                if (i > 0 && i > nx) break;
                bool pol = i == 0;
                float uu = pol ? sq * tau + F0 : (sub == 4 ? nuR * taus[i - 1] / pref + Fc : taus[i - 1] / pref + nuR * Fc);
                float am = amAny(uu, pol ? pk : rk);
                if (pol) { ps = am; continue; }
                float rr = 0.0;
                if (sub == 4) rr = a2 * tan(am - atanS(g0)) - b1;
                else if (sub == 3) { float cn = cos(am); rr = ((BB * r2 - AA * r1) + (BB * r2 + AA * r1) * cn) / ((BB - AA) + (BB + AA) * cn); }
                else { float s2 = sin(am) * sin(am); rr = sub == 1 ? (r2 * r31 - r1 * r32 * s2) / (r31 - r32 * s2) : (r4 * r31 - r3 * r41 * s2) / (r31 - r41 * s2); }
                rx[i - 1] = rr;
            }
            if (eq0) ps = 0.0;
            float jj = floor(ps / PI + 0.5), pp = ps - jj * PI;
            jn[7] = pn; jnc[7] = pnc; js[7] = sin(pp); jc[7] = cos(pp); jk[7] = pk;
            if (eq0 || noG || !sky) break;
        }
        // (0.56.2: not made where not used: K(mu) (job 2; the polar amplitude's AGM has it), the radial K but for case (1), and for a ray that ends at
        // the horizon the far direction's integrals: the I_+- terms (jobs 4, 6, the third kind of 3, 5) and the polar third kind (0, 1, 7))
        if (k == 2 && sub != 1) continue;
        if ((k == 0 || k == 1) && eq0) continue;
        if (k >= 3 && k < 7 && (sub == 1 || !sky) && (k % 2) == 0) continue;
        jr[k] = ellFP(jn[k], jnc[k], js[k], jc[k], jk[k], je[k], two && k >= 3 && k < 7, k == 2 || !sky);
    }
    if (bad) return o;
#if defined(SKY_STOP) && SKY_STOP == 3
    o.kind = 1; o.dir = normalize(vec3(jr[0].x + jr[1].y + jr[2].x, jr[3].y + jr[4].y + jr[5].y + jr[6].y + jr[7].y, ps + rx[0] + rx[1] + rx[2] + rx[3] + 1.0)); return o;
#endif
    // the crossings' u (and the first one not made, continued as spinRayClosed does), the end
    float dmu = 0.0, mu = cth, G = 0.0;
    if (eq0) { G = tau / (sth * sth); }
    else {
        float s = sin(ps), c = cos(ps), dn = sqrt(max(1.0 - (1.0 - pk) * s * s, 0.0));
        if (!vort) { mu = -nu * sqrt(up) * s; dmu = -nu * sqrt(up) * c * sq * dn; }
        else { float w = sqrt(um + (up - um) * s * s); mu = h * w; dmu = h * (up - um) * s * c * sq * dn / w; }
        float jj = floor(ps / PI + 0.5);
        G = noG ? 0.0 : (jr[7].y + 2.0 * jj * PiFull - G0) * gscale;
    }
    mu = clamp(mu, -1.0, 1.0);
    vec4 u = vec4(0.0);
    int nCross = 0;
    for (int i = 0; i < 4; i++) if (i < nx && rx[i] > 0.0 && nCross == i) { u[i] = 1.0 / rx[i]; nCross++; }
    if (sky) {
        float ip[2]; float im[2];
        if (sub != 1) for (int k = 0; k < 4; k++) {
            int pt = k / 2;
            float rr = (k % 2) == 0 ? rp : rm, F = pref * jr[3 + 2 * pt].x, Pi = jr[3 + k].y, s = as_[pt], c = ac_[pt], v = 0.0;
            if (two) {
                float r4_ = rr - r4;
                v = -pref / r4_ * (jr[3 + 2 * pt].x + r43 * r41 / (r4_ * r31) * Pi);   // (Pi here (Pi - F) / n: kerr_closed.cpp Ipm)
            } else if (sub == 3) {
                float q1 = rr - r1, q2 = rr - r2, l = al[k], l2 = l * l, jm = 1.0 - rk;
                if (je[3 + k]) {
                    float d = sqrt(c * c + rk * s * s), Ft = F / pref, R1m = -d * (1.0 + c) / s + Ft - Pi;
                    v = pref / (r21 * AA) * (-(BB + AA) * Ft + 2.0 * BB * R1m);
                } else {
                    float w = jm + (1.0 - jm) * l2, p1 = sqrt((l2 - 1.0) / w), d = sqrt(c * c + rk * s * s), pl = p1 * d + s;
                    float f1 = p1 * 0.5 * log(abs(pl * pl * w / (l2 * c * c - 1.0)));
                    float R1 = (Pi - l * f1) / (1.0 - l2);
                    v = -1.0 / (BB * q2 + AA * q1) * ((BB + AA) * F + 2.0 * r21 * sqrt(AA * BB) / (BB * q2 - AA * q1) * R1);
                }
            } else {
                float x = (rr + b1) / a2, l = al[k], l2 = l * l, d = sqrt(c * c + rk * s * s);
                float p2 = sqrt((1.0 + l2) / (rk + l2));
                float f2 = p2 * 0.5 * log(abs((1.0 + p2 * d) * (1.0 + p2 * d) / ((1.0 + p2) * (1.0 + p2) * ((1.0 + l2) * s * s - 1.0))));
                float S1 = (F / pref + l2 * Pi - l * f2) / (1.0 + l2);
                v = g0 / (a2 * (1.0 - g0 * x)) * F - pref * (1.0 + g0 * g0) / (a2 * (1.0 - g0 * x) * (g0 + x)) * S1;
            }
            if ((k % 2) == 0) ip[pt] = v; else im[pt] = v;
        }
        float Ip = ip[1] + sg * ip[0], Im = im[1] + sg * im[0];
        float Iphi = 2.0 * a / (rp - rm) * ((rp - a * lam * 0.5) * Ip - (rm - a * lam * 0.5) * Im);
        float npol = 0.0;
        if (noG && !eq0) { float off = vort ? 0.0 : PI * 0.5; npol = floor((ps - off) / PI) - floor((ph0 - off) / PI); }
        float phi = -Iphi - lam * G + PI * npol;
        float sn = sqrt(1.0 - mu * mu);
        o.dir = vec3(sn * cos(phi), sn * sin(phi), mu);
        o.kind = 1;
        if (nCross < 4) { float ms = o.dir.z != 0.0 ? dmu / o.dir.z : -1e18; u[nCross] = -1.0 / sqrt(ms * ms + 100.0); }
    } else {
        float Pq = rEnd * rEnd + a * a - a * lam, Dq = rEnd * rEnd - 2.0 * rEnd + a * a;
        float drds = -sqrt(max(Pq * Pq - Dq * (eta + (lam - a) * (lam - a)), 0.0)), sn = sqrt(max(1.0 - mu * mu, 1e-30));
        float dthdr = drds != 0.0 ? (-dmu / sn) / drds : 0.0, thEnd = acos(mu);
        o.kind = 2;
        if (nCross < 4) {
            float delta = dthdr != 0.0 ? (PI * 0.5 - thEnd) / dthdr : -1e30;
            u[nCross] = delta < 0.0 ? (1.0 - tanh(max(delta, -1e3) / rEnd)) / rEnd : 2.0 / rEnd;
            for (int i = 0; i < 4; i++) if (i > nCross) u[i] = 2.0 / rEnd;
        }
    }
    o.u = u;
    if (isnan(o.dir.x + o.dir.y + o.dir.z) || isinf(o.dir.x + o.dir.y + o.dir.z)) o.kind = 0;
    return o;
}
)";

// The pixel (kKerrFS's rules with the table replaced by traceRay): the ray at the pixel's angle th = |o| / uKpx from the middle and azimuth chi
// (uKO: the screen directions of e_theta and e_phi), the shadow by its end, anti-aliased by f's distance in pixels (f / |grad f|) on the sky's side only
// (a pixel just inside the shadow has no sky ray: its covered part is left black, at most half a pixel), the sky through uKs and the Gaia map, the disc's
// four crossings with kKerrFS's coverage, light and colour. The screen's white: uWhiteMode 0 from uDT.y (1 / white, as kKerrFS), 1 from the card's own
// reduction (uWhite, its .r the brightest channel; 0.56.0) or a set exposure (uWhiteSet). uDFix (Film): the film's colour before the white (rgb), on (w).
// The fourth output (0.56.0): the share of the pixel where the far sky shows (the stars' dots are laid over the picture through it: a star image the
// processor found for an older view never shows through the disc or the shadow). uMode: 0 the picture; 1 (dir, xpx); 2 (order, r, g, xpx); 3 (kind, b, q,
// sub); 4 (u0..u3); 10 the exposure's sample at (uSamp.x + i uSamp.y, uSamp.z - j uSamp.w): (the first disc image's brightest channel, its Y, 0, 1) in
// the disc's own light before the white (lab.h discLook's rule, its sample points).
static const char* const kKerrSkyMainPart = R"(
uniform sampler2D uTex;
uniform sampler2D uDC;
uniform sampler2D uDF;
uniform sampler2D uWhite;
uniform mat3 uKs;
uniform vec2 uCentre;
uniform float uKpx;
uniform vec4 uKO;
uniform int uMode;
uniform float uBright;
uniform int uSky;
uniform int uDisc;
uniform vec4 uDR;
uniform vec4 uDT;
uniform ivec3 uDN;
uniform vec4 uDFix;
uniform int uWhiteMode;
uniform float uWhiteSet;
uniform vec4 uSamp;
uniform int uRaw;
layout(location = 0) out vec4 fragColor;
layout(location = 1) out vec4 discOut;
layout(location = 2) out vec4 skyOut;
layout(location = 3) out vec4 maskOut;
vec2 mapUV(vec3 v) {
    vec3 g = uKs * v;
    float l = atan(g.y, g.x), b = asin(clamp(g.z, -1.0, 1.0));
    return vec2(0.5 - l / (2.0 * PI), 0.5 - b / PI);
}
vec3 encS(vec3 l) { l = clamp(l, 0.0, 1.0); return mix(12.92 * l, 1.055 * pow(l, vec3(1.0 / 2.4)) - 0.055, step(vec3(0.0031308), l)); }
vec3 decS(vec3 e) { return mix(e / 12.92, pow((e + 0.055) / 1.055, vec3(2.4)), step(vec3(0.04045), e)); }
float shiftKerr(float r, float b) {
    float a = uCam.x, D = r * r - 2.0 * r + a * a, S2 = (r * r + a * a) * (r * r + a * a) - a * a * D;
    float al = sqrt(r * r * D / S2), om = 2.0 * a * r / S2, vp = sqrt(S2) / r;
    float Om = 1.0 / (r * sqrt(r) + a), V = (Om - om) * vp / al;
    return al * sqrt(max(1.0 - V * V, 0.0)) / (1.0 - Om * b);
}
uniform float uAll;                                   // 0.58.0: 0 the eye's light; > 0 All light, 1 / T_sun (blackhole/disc.h allLightLinear)
vec3 allLight(vec3 c, float s, float T) {             // the eye's colour c (times s) of a black body at T scaled so its luminance is (T / T_sun)^4 (times s);
    float y = dot(c, vec3(0.2126, 0.7152, 0.0722)), q = T * uAll;   // called only when uAll > 0 (the eye's picture keeps 0.57's operations)
    q *= q;
    return y > 0.0 ? c * (q * q * s / y) : vec3(0.0);
}
vec3 lightOfT(float T) {                              // 0.58.0: the light of a black body seen at T (the eye's or All light), before the white
    float c = (log(T) * 0.43429448 - uDT.z) / uDT.w;
    if (c < 0.0) return vec3(0.0);
    c = min(c, float(uDN.y - 1) - 0.001);
    int k = int(c);
    vec3 col = mix(texelFetch(uDC, ivec2(k, 0), 0).rgb, texelFetch(uDC, ivec2(k + 1, 0), 0).rgb, c - float(k));
    if (uAll > 0.0) col = allLight(col, 1.0, T);
    return col;
}
uniform vec4 uLP;                                     // 0.58.0, uMode 11 (the start-up check's light): the pass's corner (x, y), its width, its pixel count
vec3 discLightRaw(float r, float g) {
    if (uDFix.w > 0.0) return uDFix.rgb;
    float q = (log(r) - uDR.z) / uDR.w;
    if (q < 0.0) return vec3(0.0);
    q = min(q, float(uDN.x - 1) - 0.001);
    int i = int(q);
    float f4 = mix(texelFetch(uDF, ivec2(i, 0), 0).r, texelFetch(uDF, ivec2(i + 1, 0), 0).r, q - float(i));
    float T = uDT.x * f4 * g;
    if (!(T > 0.0)) return vec3(0.0);
    return lightOfT(T);
}
vec3 scaledColour(vec3 c) { float m = max(c.r, max(c.g, c.b)); return m > 1.0 ? c / m : c; }
void main() {
    discOut = vec4(0.0); skyOut = vec4(0.0); maskOut = vec4(0.0);
    if (uMode == 11) {                                // 0.58.0: the light of black bodies at 10^3 to 10^7 K, even in log T over the pass's pixels: (light, T)
        float kk = (floor(gl_FragCoord.y) - uLP.y) * uLP.z + floor(gl_FragCoord.x) - uLP.x;
        float T = pow(10.0, 3.0 + 4.0 * (kk + 0.5) / uLP.w);
        fragColor = vec4(lightOfT(T), T);
        return;
    }
    vec2 pos = gl_FragCoord.xy;
    if (uMode == 10) pos = vec2(uSamp.x + floor(gl_FragCoord.x) * uSamp.y, uSamp.z - floor(gl_FragCoord.y) * uSamp.w);
    vec2 o = pos - uCentre;
    float rr = length(o), th = rr / uKpx;
    vec2 dn = rr > 0.0 ? o / rr : vec2(0.0, 1.0);
    float chi = atanS(dot(dn, uKO.zw), dot(dn, uKO.xy));
    if (chi < 0.0) chi += 2.0 * PI;
    float thc = min(th, PI);
    SkyRay R = traceRay(vec3(-cos(thc), sin(thc) * cos(chi), sin(thc) * sin(chi)));
    bool skyEnd = R.kind == 1;
    // the edge's distance in pixels (derivatives taken by every pixel: uniform control flow)
    float gx = dFdx(R.f), gy = dFdy(R.f), gl = length(vec2(gx, gy));
    float xpx = (abs(R.f) < 1e29 && gl > 0.0 && gl < 1e20) ? R.f / gl : (skyEnd ? 1e3 : -1e3);
    if (!skyEnd && xpx > 0.0) xpx = -xpx;
    if (skyEnd && xpx < 0.0) xpx = -xpx;
    vec4 rk = vec4(1e9);
    for (int k = 0; k < 4; k++) rk[k] = (R.u[k] > 0.0 && th <= PI && R.kind != 0) ? 1.0 / R.u[k] : 1e9;
    vec4 fw = fwidth(rk);
    if (uMode == 1) { fragColor = skyEnd ? vec4(R.dir, xpx) : vec4(0.0, 0.0, 0.0, xpx); return; }
    if (uMode == 3) { fragColor = vec4(float(R.kind), R.b, R.q, float(R.sub)); return; }
    if (uMode == 4) { fragColor = R.u; return; }
    int first = -1;
    for (int k = 0; k < 4; k++) if (first < 0 && rk[k] >= uDR.x && rk[k] <= uDR.y) first = k;
    bool probe = uMode == 2 || uMode == 10;
    if (probe && first < 0) { fragColor = uMode == 2 ? vec4(-1.0, 0.0, 0.0, xpx) : vec4(0.0); return; }
    vec2 uv = mapUV(skyEnd ? R.dir : vec3(1.0, 0.0, 0.0));
    vec2 ux = dFdx(uv), uy = dFdy(uv);
    ux.x -= floor(ux.x + 0.5); uy.x -= floor(uy.x + 0.5);
    ux = clamp(ux, vec2(-0.25), vec2(0.25)); uy = clamp(uy, vec2(-0.25), vec2(0.25));
    vec3 c = textureGrad(uTex, uv, ux, uy).rgb;
    float cover = skyEnd && th <= PI ? clamp(xpx + 0.5, 0.0, 1.0) : 0.0;
    vec3 back = uSky == 0 ? vec3(0.0) : c * uBright * cover;
    if (uDisc == 0 && !probe) { fragColor = vec4(back, 1.0); skyOut = vec4(decS(back), 1.0); maskOut = vec4(cover); return; }
    float white = uWhiteMode == 1 ? texelFetch(uWhite, ivec2(0, 0), 0).r : uWhiteSet;
    float inv = uWhiteMode == 0 ? uDT.y : (white > 0.0 ? 1.0 / white : 1.0);
    vec3 acc = vec3(0.0), accRaw = vec3(0.0);
    float tr = 1.0;
    for (int k = 0; k < 4; k++) {
        float w = fw[k];
        float cov = w > 1e-5 * rk[k] ? clamp((min(rk[k] + 0.5 * w, uDR.y) - max(rk[k] - 0.5 * w, uDR.x)) / w, 0.0, 1.0) : ((rk[k] >= uDR.x && rk[k] <= uDR.y) ? 1.0 : 0.0);
        if (probe ? k != first : cov <= 0.0) continue;
        float rc = clamp(rk[k], uDR.x, uDR.y), g = shiftKerr(rc, R.b);
        vec3 raw = discLightRaw(rc, g);
        if (uMode == 2) { fragColor = vec4(float(k), rk[k], g, xpx); return; }
        if (uMode == 10) { fragColor = vec4(max(raw.r, max(raw.g, raw.b)), dot(raw, vec3(0.2126, 0.7152, 0.0722)), 0.0, 1.0); return; }
        vec3 cr = raw * inv;
        if (uDFix.w > 0.0) cr = scaledColour(cr);
        acc += tr * cov * scaledColour(cr);
        accRaw += tr * cov * cr;
        tr *= 1.0 - cov;
    }
    fragColor = vec4(encS(acc + tr * decS(back)), 1.0);
    discOut = vec4(uRaw == 1 ? accRaw : acc, 1.0);
    skyOut = vec4(tr * decS(back), 1.0);
    maskOut = vec4(tr * cover);
}
)";


// 0.56.0: the card's exposure: each texel the largest of a 4 x 4 block of the texture below (texels past uN left out); repeated down to one texel
static const char* const kMaxDownFS = R"(#version 330 core
uniform sampler2D uTex;
uniform ivec2 uN;
out vec4 fragColor;
void main() {
    ivec2 b = ivec2(gl_FragCoord.xy) * 4;
    vec4 m = vec4(0.0);
    for (int j = 0; j < 4; j++)
        for (int i = 0; i < 4; i++) {
            ivec2 q = b + ivec2(i, j);
            if (q.x < uN.x && q.y < uN.y) m = max(m, texelFetch(uTex, q, 0));
        }
    fragColor = m;
}
)";

// ---- 0.57.0: the Atom Lab's two heavy loops on the graphics card (compute, GLSL 430; single precision only: no double anywhere) ----
// The Slice view's new dots (atom/cloud_sample.cpp PlaneSampler::tryPoint is the reference, in double): one invocation per ATTEMPT t of a batch. The lab's
// generator (orbit::Mulberry32) is a counter: its n-th number after the state S is a hash of S + (n + 1) 0x6D2B79F5, so attempt t, which takes numbers
// 3t, 3t + 1, 3t + 2, is made here without the attempts before it. The radius x = 2 Z s / n inverts the tail G(x) = e^(-x) H(x) above the plane
// (G(x) = u G(x0)), in logarithms (no underflow far out): ln H(x) - x = ln u + ln G(x0), by Newton's method (d/dx = -f / H) kept in a bracket.
// The direction's sine and cosine come from a polynomial of their own (no driver's sin / cos, whose precision GLSL does not fix).
// Output per attempt: x, y, z (Bohr radii) and w = 1 kept, 0 rejected, -1 the radius came out 0 (the reference then takes 2 numbers, not 3).
static const char* const kSliceDotsCS = R"(#version 430 core
layout(local_size_x = 64) in;
layout(std430, binding = 0) writeonly buffer Out { vec4 res[]; };
uniform uint uSeed;
uniform int uTries;
uniform int uAxis;
uniform float uOff;
uniform float uX0;
uniform float uLnG0;
uniform float uScale;
uniform float uBound;
uniform int uAng;
uniform int uAngIm;
uniform float uCIm;
uniform int uNh;
uniform float uH[16];
uniform float uC[16];
uniform int uStride;
uniform int uFirst;

uint mulberry(uint n) {
    uint s = uSeed + (n + 1u) * 0x6D2B79F5u;
    uint t = (s ^ (s >> 15u)) * (1u | s);
    t = (t + ((t ^ (t >> 7u)) * (61u | t))) ^ t;
    return t ^ (t >> 14u);
}
float poly(int which, float x) {
    float v = 0.0;
    for (int i = 15; i >= 0; i--) {
        if (i >= uNh) continue;
        v = v * x + (which == 0 ? uH[i] : uC[i]);
    }
    return v;
}
float angular(int a, vec3 p) {
    float x = p.x, y = p.y, z = p.z, r2 = x * x + y * y + z * z;
    if (a == 0) return 1.0;
    if (a == 1) return x;
    if (a == 2) return y;
    if (a == 3) return z;
    if (a == 4) return 3.0 * z * z - r2;
    if (a == 5) return x * z;
    if (a == 6) return y * z;
    if (a == 7) return x * y;
    if (a == 8) return x * x - y * y;
    if (a == 9) return z * (5.0 * z * z - 3.0 * r2);
    if (a == 10) return x * (5.0 * z * z - r2);
    if (a == 11) return y * (5.0 * z * z - r2);
    if (a == 12) return x * y * z;
    if (a == 13) return z * (x * x - y * y);
    if (a == 14) return x * (x * x - 3.0 * y * y);
    return y * (3.0 * x * x - y * y);
}
// cos and sin of 2 pi t, t in [0, 1): a quarter turn taken out, then Taylor series on [-pi/4, pi/4] (error below 4e-9 before rounding)
vec2 cosSinTurn(float t) {
    float q = floor(4.0 * t + 0.5);
    float a = 6.28318530717958648 * (t - 0.25 * q);
    float a2 = a * a;
    float c = 1.0 + a2 * (-0.5 + a2 * (1.0 / 24.0 + a2 * (-1.0 / 720.0 + a2 * (1.0 / 40320.0))));
    float s = a * (1.0 + a2 * (-1.0 / 6.0 + a2 * (1.0 / 120.0 + a2 * (-1.0 / 5040.0 + a2 * (1.0 / 362880.0)))));
    int k = int(q) & 3;
    if (k == 0) return vec2(c, s);
    if (k == 1) return vec2(-s, c);
    if (k == 2) return vec2(-c, -s);
    return vec2(s, -c);
}
float lnTail(float x) { return log(max(poly(0, x), 1e-37)) - x; }

void main() {
    int t = int(gl_GlobalInvocationID.x);
    if (t >= uTries) return;
    uint n0 = uint(uStride) * uint(t) + uint(uFirst);
    uint k1 = mulberry(n0), k2 = mulberry(n0 + 1u), k3 = mulberry(n0 + 2u);
    // 1 - k1 / 2^32, in (0, 1]
    float u = (float(~k1) + 1.0) * 2.3283064365386963e-10;
    float target = log(u) + uLnG0;
    float lo = uX0, hi = uX0 + 4.0;
    for (int i = 0; i < 40; i++) {
        if (!(lnTail(hi) > target) || hi >= 1e4) break;
        lo = hi; hi = uX0 + 2.0 * (hi - uX0);
    }
    float x = 0.5 * (lo + hi);
    for (int i = 0; i < 60; i++) {
        float h = poly(0, x);
        float g = log(max(h, 1e-37)) - x - target;
        if (g > 0.0) lo = x; else hi = x;
        float f = poly(1, x);
        float nx = f > 0.0 ? x + g * h / f : 0.5 * (lo + hi);
        if (!(nx > lo && nx < hi)) nx = 0.5 * (lo + hi);
        float tol = 9.5367431640625e-7 * (1.0 + x);
        if (abs(nx - x) <= tol || hi - lo <= tol) { x = nx; break; }
        x = nx;
    }
    float s = x * uScale;
    float rho = sqrt(max(0.0, s * s - uOff * uOff));
    vec2 cs = cosSinTurn(float(k2) * 2.3283064365386963e-10);
    float a = rho * cs.x, b = rho * cs.y;
    vec3 q = uAxis == 2 ? vec3(a, b, uOff) : vec3(a, uOff, b);
    if (!(s > 0.0)) { res[t] = vec4(q, -1.0); return; }
    vec3 un = q / s;
    float re = angular(uAng, un);
    float a2 = re * re;
    if (uAngIm >= 0) { float im = angular(uAngIm, un); a2 = 0.5 * (re * re + uCIm * uCIm * im * im); }
    bool kept = !(float(k3) * 2.3283064365386963e-10 * uBound > a2);
    res[t] = vec4(q, kept ? 1.0 : 0.0);
}
)";

// 0.64.0: Saturn's rings per pixel on the graphics card (Vulkan), the processor's triangles (orbit/ring_draw.h ringTriangles) as the reference: the same
// 128 angular steps with straight chords (a pixel's radius is measured along its step's middle direction and divided by cos(half a step), so the chords'
// edges are the processor's), the same half (the step's middle towards you or away), the same drawn bands (inner and outer edge in planet radii, opacity,
// light k: uBands, sorted) and the planet's shadow (F < 0, as ringShadowF, at the pixel's own point of the ring plane). The colour: the tint times k in
// linear light, encoded (render/shading.h shadeEncoded); black in the shadow. kRingsFS draws it (kVertexShader's vUV holds the point); kRingsProbeCS gives
// the same function at listed points (the start-up check and the tests: orbit/ring_check.h).
static const char* const kRingsHeadFSPart = R"(#version 330 core
in vec2 vUV;
in vec4 vColor;
in vec4 vParam;
out vec4 fragColor;
)";
static const char* const kRingsHeadCSPart = R"(#version 430 core
layout(local_size_x = 64) in;
layout(std430, binding = 0) readonly buffer Pts { vec2 pts[]; };
layout(std430, binding = 1) writeonly buffer Res { vec4 res[]; };
uniform int uCount;
)";
static const char* const kRingsPart = R"(
uniform vec2 uCen;          // the planet's middle (points)
uniform float uR;           // its equatorial radius (points)
uniform vec3 uE1;           // the ring plane's axes (screen frame: right, up, towards you)
uniform vec3 uE2;
uniform vec3 uK0;           // the screen frame to the planet's frame (rows), the pole axis then divided by uSz
uniform vec3 uK1;
uniform vec3 uK2;
uniform float uSz;
uniform vec3 uLb;           // the Sun in that frame
uniform int uBack;          // 1: the half away from you
uniform int uNb;
uniform vec3 uTint;         // the drawn tint (sRGB)
uniform vec4 uBands[768];   // a0, a1 (planet radii), opacity, k
vec3 ringDecode(vec3 c) { return mix(c / 12.92, pow((c + 0.055) / 1.055, vec3(2.4)), step(vec3(0.04045), c)); }
vec3 ringEncode(vec3 l) { l = clamp(l, 0.0, 1.0); return mix(12.92 * l, 1.055 * pow(l, vec3(1.0 / 2.4)) - 0.055, step(vec3(0.0031308), l)); }
float ringShadow(vec3 q) {
    vec3 qb = vec3(dot(uK0, q), dot(uK1, q), dot(uK2, q) / uSz);
    float LL = dot(uLb, uLb);
    float t = -dot(qb, uLb) / LL;
    vec3 d = qb + t * uLb;
    float f = dot(d, d) - 1.0;
    return t <= 0.0 ? max(f, 1e-3) : f;
}
vec4 ringAt(vec2 p) {
    vec2 q = vec2((p.x - uCen.x) / uR, -(p.y - uCen.y) / uR);
    float det = uE1.x * uE2.y - uE2.x * uE1.y;
    if (abs(det) < 1e-6 || uNb <= 0) return vec4(0.0);
    float A = (q.x * uE2.y - uE2.x * q.y) / det;
    float B = (uE1.x * q.y - q.x * uE1.y) / det;
    float t = atan(B, A);
    if (t < 0.0) t += 6.28318530717958648;
    float seg = 6.28318530717958648 / 128.0;
    int i = clamp(int(floor(t / seg)), 0, 127);
    float tm = (float(i) + 0.5) * seg;
    float cm = cos(tm), sm = sin(tm);
    float zm = cm * uE1.z + sm * uE2.z;
    if ((zm < 0.0) != (uBack != 0)) return vec4(0.0);
    float re = (A * cm + B * sm) / cos(0.5 * seg);
    if (re < uBands[0].x) return vec4(0.0);
    int lo = 0;
    int hi = uNb - 1;
    for (int k = 0; k < 12; k++) {
        if (lo >= hi) break;
        int m = (lo + hi + 1) / 2;
        if (uBands[m].x <= re) lo = m; else hi = m - 1;
    }
    vec4 b = uBands[lo];
    if (re >= b.y) return vec4(0.0);
    float F = ringShadow(A * uE1 + B * uE2);
    vec3 col = F >= 0.0 ? ringEncode(ringDecode(uTint) * b.w) : vec3(0.0);
    return vec4(col, b.z);
}
)";
static const char* const kRingsMainFSPart = R"(
uniform int uAA;            // 1: the picture with 4 samples: the function at the 4 sample places, averaged as their blend would be (the triangles' multisampling)
uniform float uPx;          // points per pixel
void main() {
    vec4 c = ringAt(vUV);
    if (uAA != 0) {
        vec4 s0 = c * 0.0;
        vec4 s1 = ringAt(vUV + vec2(-0.125, -0.375) * uPx), s2 = ringAt(vUV + vec2(0.375, -0.125) * uPx);
        vec4 s3 = ringAt(vUV + vec2(-0.375, 0.125) * uPx), s4 = ringAt(vUV + vec2(0.125, 0.375) * uPx);
        float a = 0.25 * (s1.a + s2.a + s3.a + s4.a);
        if (a <= 0.0) discard;
        s0.rgb = (s1.rgb * s1.a + s2.rgb * s2.a + s3.rgb * s3.a + s4.rgb * s4.a) / (4.0 * a);
        s0.a = a;
        fragColor = s0;
        return;
    }
    if (c.a <= 0.0) discard;
    fragColor = c;
}
)";
static const char* const kRingsMainCSPart = R"(
void main() {
    int n = int(gl_GlobalInvocationID.x);
    if (n >= uCount) return;
    res[n] = ringAt(pts[n]);
}
)";

// 0.64.0: the Classic Cloud 3D dots from the card's cloud buffer (atom/cloud_dots.cpp projectCloud + buildClassic3D are the reference): each dot put on the
// picture (the same mild perspective), its depth band (far, middle, near: > 0.35, -0.35 .. 0.35, < -0.35 of the cloud's radius) and its colour group; then
// the dots' squares (Renderer2D::Blob, kind 5: the shader picks the group's colour) written band by band, inside a band group by group: the processor's
// order of the groups (inside one band and group every square has the same colour, opacity and size, so their order does not change the blend). Three
// steps: 0 each dot's key (band x groups + group, or none) counted; 1 the keys' starts; 2 each dot's square at its key's next place (the places after
// the last square: none). uCounts: the counts, then the starts (used up as cursors), then the total.
// 0.65.0: the side view with every electron too (uSide = 1; atom/cloud_dots.cpp sideDots + sideBlobs, or the Glow's glowBlobs: the reference): x across and
// z up, no perspective, one band; the dots group by group; Classic squares (kind 5) or Glow's soft dots (kind 6), whose dots are also counted per 8 x 8-point
// cell for the exposure (read back, then steps 1 and 2 with the dots' light in uWeight).
// The Slice (uSide = 2; atom_view.cpp's Slice loop: the reference): each dot's square or soft dot written at its own index (the processor draws them in dot
// order, unsorted), a dot not shown (off the plane's slab, its group 255, or off the picture) as an empty one (size and opacity 0); step 0 counts the
// Glow's cells, step 2 writes the dots.
static const char* const kClassicCS = R"(#version 430 core
layout(local_size_x = 64) in;
layout(std430, binding = 0) readonly buffer Pos { float pos[]; };
layout(std430, binding = 1) readonly buffer Grp { uint grp[]; };
layout(std430, binding = 2) buffer Keys { uint keys[]; };
layout(std430, binding = 3) buffer Counts { uint counts[]; };
layout(std430, binding = 4) writeonly buffer Out { vec4 outb[]; };
layout(std430, binding = 5) buffer Cells { uint cells[]; };   // 0.65.0: the side view's Glow: its dots counted per 8 x 8-point cell (the exposure's grid)
uniform int uStep;
uniform int uCnt;
uniform int uNg;
uniform vec4 uCam0;                         // cx, cy (points), pxm (points per metre), ext (the cloud's radius, m)
uniform vec4 uCam1;                         // cos yaw, sin yaw, cos pitch, sin pitch
uniform vec2 uView;                         // the picture (points)
uniform float uWeight;                      // cloudDotWeight
uniform vec4 uBand[3];                      // a band's opacity and size (cloud_dots.h kDepthBand)
uniform vec4 uGroupA[24];                   // .x: a group colour's own opacity
uniform int uSide;                          // 0.65.0: 1 = the side view (cloud_dots.cpp sideDots: x and z, no perspective, one band; uBand[0] its opacity and size)
uniform float uKind;                        // the blob's kind: 5 a square (Classic), 6 a soft dot (Glow)
uniform ivec2 uCells;                       // the side view's Glow: the grid's width and height in cells (0 0: no cells counted)
uniform vec4 uSlice;                        // the Slice (uSide 2): the cut's offset and the slab's half thickness (m), 1 for a horizontal cut, the dot's offset (points)

void main() {
    int i = int(gl_GlobalInvocationID.x);
    int nk = 3 * uNg;
    if (uStep == 1) {
        if (i != 0) return;
        uint s = 0u;
        for (int k = 0; k < nk; k++) { counts[nk + k] = s; s += counts[k]; }
        counts[2 * nk] = s;
        return;
    }
    if (i >= uCnt) return;
    if (uSide == 2) {                           // the Slice: as atom_view.cpp, x across and z (a horizontal cut: y) up
        bool hz = uSlice.z > 0.5;
        float px = pos[3 * i], pu = hz ? pos[3 * i + 1] : pos[3 * i + 2], pd = hz ? pos[3 * i + 2] : pos[3 * i + 1];
        float X = uCam0.x + px * uCam0.z, Y = uCam0.y - pu * uCam0.z;
        uint g = (grp[i >> 2] >> (8u * uint(i & 3))) & 255u;
        bool shown = !(abs(pd - uSlice.x) > uSlice.y) && g != 255u && int(g) < uNg && !(X < -2.0 || X > uView.x || Y < -2.0 || Y > uView.y);
        if (uStep == 0) {
            if (shown && uCells.x > 0) {            // (glowBlobs: the cell of int(x / 8), int(y / 8), clamped into the grid)
                int cx = clamp(int(X / 8.0), 0, uCells.x - 1), cy = clamp(int(Y / 8.0), 0, uCells.y - 1);
                atomicAdd(cells[cy * uCells.x + cx], 1u);
            }
            return;
        }
        float a = shown ? clamp(uBand[0].x * uWeight, 0.0, 1.0) * uGroupA[int(min(g, 23u))].x : 0.0;
        outb[5 * i] = vec4(X + uSlice.w, Y + uSlice.w, shown ? 0.5 * uBand[0].y : 0.0, uKind);
        outb[5 * i + 1] = vec4(shown ? float(g) : 0.0, 0.0, 0.0, 0.0);
        outb[5 * i + 2] = vec4(0.0, 0.0, 0.0, a);
        outb[5 * i + 3] = vec4(0.0);
        outb[5 * i + 4] = vec4(0.0);
        return;
    }
    if (uStep == 0 && uSide == 1) {             // the side view: as sideDots, x across and z up, in view while X - 1 and Y - 1 are within (-2, w) and (-2, h)
        float sx = uCam0.x + pos[3 * i] * uCam0.z - 1.0, sy = uCam0.y - pos[3 * i + 2] * uCam0.z - 1.0;
        uint sg = (grp[i >> 2] >> (8u * uint(i & 3))) & 255u;
        uint skey = 0xFFFFFFFFu;
        if (sx > -2.0 && sx < uView.x && sy > -2.0 && sy < uView.y && int(sg) < uNg) {
            skey = sg;
            atomicAdd(counts[skey], 1u);
            if (uCells.x > 0) {                     // (glowBlobs: the cell of int(x / 8), int(y / 8), clamped into the grid)
                int cx = clamp(int((sx + 1.0) / 8.0), 0, uCells.x - 1), cy = clamp(int((sy + 1.0) / 8.0), 0, uCells.y - 1);
                atomicAdd(cells[cy * uCells.x + cx], 1u);
            }
        }
        keys[3 * i] = skey; keys[3 * i + 1] = floatBitsToUint(sx + 1.0); keys[3 * i + 2] = floatBitsToUint(sy + 1.0);
        return;
    }
    if (uStep == 0) {
        float x = pos[3 * i], y = pos[3 * i + 1], z = pos[3 * i + 2];
        float x1 = x * uCam1.x - y * uCam1.y, y1 = x * uCam1.y + y * uCam1.x;
        float dep = y1 * uCam1.z - z * uCam1.w, up = y1 * uCam1.w + z * uCam1.z;
        float f = 1.0 / (1.0 + dep / (4.0 * uCam0.w));
        float X = uCam0.x + x1 * uCam0.z * f, Y = uCam0.y - up * uCam0.z * f, D = dep / uCam0.w;
        uint g = (grp[i >> 2] >> (8u * uint(i & 3))) & 255u;
        uint key = 0xFFFFFFFFu;
        if (!(X < 0.0 || X > uView.x || Y < 0.0 || Y > uView.y) && int(g) < uNg) {
            int band = D > 0.35 ? 0 : D >= -0.35 ? 1 : 2;
            key = uint(band * uNg) + g;
            atomicAdd(counts[key], 1u);
        }
        keys[3 * i] = key; keys[3 * i + 1] = floatBitsToUint(X); keys[3 * i + 2] = floatBitsToUint(Y);
        return;
    }
    uint total = counts[2 * nk];
    if (uint(i) >= total) { for (int k = 0; k < 5; k++) outb[5 * i + k] = vec4(0.0, 0.0, 0.0, k == 0 ? 5.0 : 0.0); }
    uint key = keys[3 * i];
    if (key == 0xFFFFFFFFu) return;
    uint p = atomicAdd(counts[nk + int(key)], 1u);
    int band = int(key) / uNg, g = int(key) - band * uNg;
    float a = clamp(uBand[band].x * uWeight, 0.0, 1.0) * uGroupA[g].x;
    outb[5 * p] = vec4(uintBitsToFloat(keys[3 * i + 1]), uintBitsToFloat(keys[3 * i + 2]), 0.5 * uBand[band].y, uKind);
    outb[5 * p + 1] = vec4(float(g), 0.0, 0.0, 0.0);
    outb[5 * p + 2] = vec4(0.0, 0.0, 0.0, a);
    outb[5 * p + 3] = vec4(0.0);
    outb[5 * p + 4] = vec4(0.0);
}
)";

// 0.57.0: a jump with every electron drawn from the card's cloud buffer: each dot's group for this frame (atom_sim.cpp AtomLab::jumpGroups is the reference):
// its fixed group (its shell's colour, uploaded once per jump, 4 a word), shown when its fixed random number, the lab's splitmix64 of i + (pop << 40) as
// z >> 11, is below the share times 2^53, an integer compare (uT0 for the old dots, uT1 for the new, each the 53-bit integer ceil(share 2^53) as (low, high)
// words): the same decision as the processor's double, bit for bit. Written 4 groups a word, read by the cloud's programs as their group attribute.
static const char* const kJumpShowCS = R"(#version 430 core
layout(local_size_x = 64) in;
layout(std430, binding = 0) readonly buffer Fixed { uint fixedG[]; };
layout(std430, binding = 1) writeonly buffer Shown { uint shownG[]; };
uniform int uJn;
uniform uvec2 uT0;
uniform uvec2 uT1;

uvec2 mul64(uvec2 a, uvec2 b) { uint hi, lo; umulExtended(a.x, b.x, hi, lo); return uvec2(lo, hi + a.x * b.y + a.y * b.x); }
uvec2 add64(uvec2 a, uvec2 b) { uint c; uint lo = uaddCarry(a.x, b.x, c); return uvec2(lo, a.y + b.y + c); }
uvec2 xorShift(uvec2 z, int n) {
    uvec2 s = n >= 32 ? uvec2(z.y >> uint(n - 32), 0u) : uvec2((z.x >> uint(n)) | (z.y << uint(32 - n)), z.y >> uint(n));
    return z ^ s;
}
bool shown(uint i, uint pop) {
    uvec2 z = add64(mul64(uvec2(i, pop << 8), uvec2(0x7F4A7C15u, 0x9E3779B9u)), uvec2(0xD9B4E019u, 0x632BE59Bu));
    z = mul64(xorShift(z, 30), uvec2(0x1CE4E5B9u, 0xBF58476Du));
    z = mul64(xorShift(z, 27), uvec2(0x133111EBu, 0x94D049BBu));
    z = xorShift(z, 31);
    uvec2 m = uvec2((z.x >> 11u) | (z.y << 21u), z.y >> 11u);
    uvec2 T = pop == 1u ? uT1 : uT0;
    return m.y < T.y || (m.y == T.y && m.x < T.x);
}
void main() {
    uint w = gl_GlobalInvocationID.x;
    uint total = 2u * uint(uJn);
    if (w >= (total + 3u) / 4u) return;
    uint fixedWord = fixedG[w], word = 0u;
    for (uint j = 0u; j < 4u; j++) {
        uint k = 4u * w + j, v = 255u;
        if (k < total) {
            uint pop = k >= uint(uJn) ? 1u : 0u;
            uint g = (fixedWord >> (8u * j)) & 255u;
            if (g != 255u && shown(k - pop * uint(uJn), pop)) v = g;
        }
        word |= v << (8u * j);
    }
    shownG[w] = word;
}
)";

// ---- 0.57.1: the Molecule Lab's Density cloud on the graphics card (single precision only: no double anywhere) ----
// Each dot's place on the picture, size and depth weight, per frame (molecule/mol_cloud.h molGlowDot is the reference, in double, the formulas of the
// processor's path in the same order): the view R p + pan (R the camera's rotation, rows uR0..uR2), X = cx + vx s, Y = cy - vy s; off the picture: r = 0
// (not drawn); else t = clamp(-vz / ext, -1, 1), near = 0.5 - 0.5 t, r = 4.6 (0.8 + 0.4 near), wt = 0.3 + 0.9 near. One invocation a dot.
static const char* const kMolDotsCS = R"(#version 430 core
layout(local_size_x = 64) in;
layout(std430, binding = 0) readonly buffer Pts { float pts[]; };
layout(std430, binding = 1) writeonly buffer Dots { vec4 dots[]; };
uniform vec4 uR0;                           // R[0] R[1] R[2] panX
uniform vec4 uR1;                           // R[3] R[4] R[5] panY
uniform vec4 uR2;                           // R[6] R[7] R[8] ext (the molecule's radius, angstrom)
uniform vec4 uScr;                          // cx, cy (points), s (points per angstrom), unused
uniform vec2 uView;                         // the picture's width and height (points)
uniform int uN;
void main() {
    uint i = gl_GlobalInvocationID.x;
    if (i >= uint(uN)) return;
    float x = pts[3u * i], y = pts[3u * i + 1u], z = pts[3u * i + 2u];
    float vx = uR0.x * x + uR0.y * y + uR0.z * z + uR0.w;
    float vy = uR1.x * x + uR1.y * y + uR1.z * z + uR1.w;
    float vz = uR2.x * x + uR2.y * y + uR2.z * z;
    float X = uScr.x + vx * uScr.z, Y = uScr.y - vy * uScr.z;
    if (X < 0.0 || X > uView.x || Y < 0.0 || Y > uView.y) { dots[i] = vec4(X, Y, 0.0, 0.0); return; }
    float t = clamp(-vz / uR2.w, -1.0, 1.0);
    float near = 0.5 - 0.5 * t;
    dots[i] = vec4(X, Y, 4.6 * (0.8 + 0.4 * near), 0.3 + 0.9 * near);
}
)";
// 0.57.1: the cloud's soft dots from the dots the compute shader above wrote (one instance a dot, its vec4 as an attribute): what softDot and the blob
// shader (kBlobVS, kind 6 with one colour group) hand the blob fragment shader (kBlobFS), a kind-4 soft dot of light min(1, uLight wt); r = 0: not drawn.
static const char* const kMolDotVS = R"(#version 330 core
layout(location = 0) in vec2 aCorner;
layout(location = 1) in vec4 aDot;          // X, Y (points), r (points; 0 = not drawn), wt (the depth weight)
uniform vec2 uSize;
uniform float uLight;                       // the exposure x 12,000 / dots: the light of a dot of depth weight 1 (cloud_light.h cloudDotLight before its depth weight)
uniform vec4 uColour;
out vec2 vLocal;
flat out vec4 vPosKind;
flat out vec4 vFocal;
flat out vec4 vC0;
flat out vec4 vC1;
flat out vec4 vC2;
void main() {
    vFocal = vec4(0.0); vC1 = vec4(0.0); vC2 = vec4(0.0);
    if (!(aDot.z > 0.0)) { vLocal = vec2(0.0); vPosKind = vec4(0.0); vC0 = vec4(0.0); gl_Position = vec4(2.0, 2.0, 2.0, 1.0); return; }
    float r = aDot.z;
    float a = clamp(min(1.0, uLight * aDot.w), 0.0, 1.0);
    float ext = r + 1.0;
    vec2 p = aDot.xy + aCorner * 2.0 * ext;
    vLocal = aCorner * 2.0 * ext / max(r, 1e-6);
    vPosKind = vec4(aDot.xy, r, 4.0); vC0 = vec4(uColour.rgb, a * uColour.a);
    gl_Position = vec4(p.x / uSize.x * 2.0 - 1.0, 1.0 - p.y / uSize.y * 2.0, 0.0, 1.0);
}
)";
// 0.57.1: the exposure's grid from the same dots: each drawn dot one point in its 8 x 8-point cell adding its footprint wt r^2 0.785 (glowBlobs' grid, summed
// in another order), drawn into the small float picture with kCloudGridFS
static const char* const kMolGridVS = R"(#version 330 core
layout(location = 1) in vec4 aDot;
uniform vec2 uGrid;                         // the grid's cells across and down (int(w / 8) + 2, int(h / 8) + 2)
out float vVal;
void main() {
    vVal = 0.0;
    gl_PointSize = 1.0;
    if (!(aDot.z > 0.0)) { gl_Position = vec4(2.0, 2.0, 2.0, 1.0); return; }
    vVal = aDot.w * aDot.z * aDot.z * 0.785;
    int cx = max(0, min(int(uGrid.x) - 1, int(aDot.x / 8.0))), cy = max(0, min(int(uGrid.y) - 1, int(aDot.y / 8.0)));
    gl_Position = vec4((float(cx) + 0.5) / uGrid.x * 2.0 - 1.0, (float(cy) + 0.5) / uGrid.y * 2.0 - 1.0, 0.0, 1.0);
}
)";

// 0.67.0: the nucleus' Density look on the card (Vulkan; render/nucdens_card.h, atom/nucdens_check.h): atom_core.cpp projectChargeDensity in single precision,
// then the cells' squares for the blob program (kind 2, as atom_view.cpp makes them on the processor). uStep 0: the deformed shapes' rays on the quarter grid
// (trapezoids of one surface thickness through the table over (across^2, |along|)); 1: each cell's column density (the rays read by interpolation, or the
// round shapes' radial tables); 2: the brightest cell (one group); 3: each cell's square (r = 0, nothing drawn, under 1 % of the brightest), in the
// processor's colour and opacity 0.95 v / peak.
static const char* const kNucDensCS = R"(#version 430 core
layout(local_size_x = 64) in;
layout(std430, binding = 0) readonly buffer Tab { float tab[]; };   // deformed: the table T (nT x nT, row |z|); round: I0, K0, K2, I2 (nR each)
layout(std430, binding = 1) buffer Rays { float rays[]; };          // the quarter grid's ray sums, qn x qn (row: across the axis)
layout(std430, binding = 2) buffer Grid { float grid[]; };          // the cells' values, G x G (row 0 at the top), then the brightest at G * G
layout(std430, binding = 3) writeonly buffer Out { vec4 outb[]; };  // the squares, 5 vec4 a cell (Renderer2D::Blob)
uniform int uStep;
uniform int uRadial;                        // 1: round (or the deuteron): the radial tables
uniform ivec4 uN;                           // nT (round: nR), qn, G, the deuteron's P2 part (kk != 0)
uniform vec4 uRay;                          // rE2 (fm^2), 1 / hT, 1 / hU, dl (fm)
uniform vec4 uAxis;                         // n1, n2, n3 (the axis in the picture's frame), nperp
uniform vec4 uCell;                         // half (fm), cell (fm), hq (fm), hR (fm)
uniform vec2 uDir;                          // sx, sy: the projected axis
uniform vec4 uScreen;                       // the grid's top left (points), a cell (points)
uniform vec4 uColour;
shared float sMax[64];

float rho(float rp2, float z) {
    int nT = uN.x;
    float ip = rp2 * uRay.z, iz = abs(z) * uRay.y;
    if (ip >= float(nT - 1) || iz >= float(nT - 1)) return 0.0;
    int a = int(ip), b = int(iz);
    float fa = ip - float(a), fb = iz - float(b);
    int r0 = b * nT + a;
    return (1.0 - fb) * ((1.0 - fa) * tab[r0] + fa * tab[r0 + 1]) + fb * ((1.0 - fa) * tab[r0 + nT] + fa * tab[r0 + nT + 1]);
}
float lerpT(int which, float i) {
    int nR = uN.x, k = int(i);
    if (k >= nR - 1) return 0.0;
    int o = which * nR + k;
    return tab[o] + (tab[o + 1] - tab[o]) * (i - float(k));
}

void main() {
    int i = int(gl_GlobalInvocationID.x);
    int qn = uN.y, G = uN.z;
    if (uStep == 0) {
        if (i >= qn * qn) return;
        int it = i / qn, is = i - it * qn;
        float s = float(is) * uCell.z, t = float(it) * uCell.z, q2 = s * s + t * t;
        float sum = 0.0, h = 0.0;
        if (q2 < uRay.x) {
            float L = sqrt(uRay.x - q2), z0 = s * uAxis.w;
            int K = max(2, int(ceil(2.0 * L / uRay.w)));
            h = 2.0 * L / float(K);
            for (int k = 1; k < K; k++) {
                float l = -L + float(k) * h, z = z0 + l * uAxis.z, r2 = q2 + l * l;
                sum += rho(max(0.0, r2 - z * z), z);
            }
        }
        rays[i] = sum * h;
        return;
    }
    if (uStep == 1) {
        if (i >= G * G) return;
        int row = i / G, col = i - row * G;
        float x = -uCell.x + (float(col) + 0.5) * uCell.y, y = uCell.x - (float(row) + 0.5) * uCell.y;
        float v = 0.0;
        if (uRadial == 1) {
            float R = sqrt(x * x + y * y), ri = R / uCell.w;
            v = lerpT(0, ri);
            if (uN.w != 0) { float nx = x * uAxis.x + y * uAxis.y; v += 1.5 * nx * nx * lerpT(1, ri) + 1.5 * uAxis.z * uAxis.z * lerpT(2, ri) - 0.5 * lerpT(3, ri); }
            v = max(0.0, v);
        } else {
            float fs = abs(x * uDir.x + y * uDir.y) / uCell.z, ft = abs(-x * uDir.y + y * uDir.x) / uCell.z;
            if (fs < float(qn - 1) && ft < float(qn - 1)) {
                int a = int(fs), b = int(ft);
                float fa = fs - float(a), fb = ft - float(b);
                int r0 = b * qn + a;
                v = (1.0 - fb) * ((1.0 - fa) * rays[r0] + fa * rays[r0 + 1]) + fb * ((1.0 - fa) * rays[r0 + qn] + fa * rays[r0 + qn + 1]);
            }
        }
        grid[i] = v;
        return;
    }
    if (uStep == 2) {
        int l = int(gl_LocalInvocationID.x);
        float m = 0.0;
        for (int k = l; k < G * G; k += 64) m = max(m, grid[k]);
        sMax[l] = m;
        memoryBarrierShared();
        barrier();
        if (l == 0) {
            float top = 0.0;
            for (int k = 0; k < 64; k++) top = max(top, sMax[k]);
            grid[G * G] = top;
        }
        return;
    }
    if (i >= G * G) return;
    int row = i / G, col = i - row * G;
    float peak = grid[G * G];
    float v = peak > 0.0 ? grid[i] / peak : 0.0;
    bool shown = peak > 0.0 && !(v < 0.01);
    outb[5 * i] = vec4(uScreen.x + (float(col) + 0.5) * uScreen.z, uScreen.y + (float(row) + 0.5) * uScreen.z, shown ? uScreen.z / 2.0 : 0.0, 2.0);
    outb[5 * i + 1] = vec4(0.0);
    outb[5 * i + 2] = vec4(uColour.rgb, shown ? 0.95 * v : 0.0);
    outb[5 * i + 3] = vec4(0.0);
    outb[5 * i + 4] = vec4(0.0);
}
)";

// 0.68.0: the Orbit Lab's trails, orbits and distance rings on the card (Vulkan; render/lines_card.h, orbit/orbit_lines.h). The processor's LinePen
// (orbit/orbit_lines.cpp) is the reference: each polyline's points (a trail's points, an orbit's 257 corners, a ring's 97) are kept on the card as two
// floats each (a high and a low part of the double, so a point minus the view's reference point C is exact to single precision of that difference); a
// piece from point i - 1 to point i is projected as orbit_view's proj() (the same turn, mild perspective and offsets, worked out about C), and a piece
// that is long on the picture and near it is drawn as the processor does: a trail as a Catmull-Rom curve through its neighbours, an orbit from its
// ellipse, each in min(4096, ceil(L / 4)) steps. kLinesCS: step 0 each piece's ends on the picture and its number of steps; step 1 the offsets (one
// workgroup's prefix sum) and the draw's vertex count; step 2 (the start-up check only) every step's point. kLinesVS: six corners a step, the step
// clipped to the picture with a 20-point margin and widened as Renderer2D::line, drawn with kFragmentShader (mode 0).
static const char* const kLinesHeadCSPart = R"(#version 430 core
layout(local_size_x = 256) in;
struct Run { uvec4 a; ivec4 b; ivec4 c; vec4 colour; vec4 style; vec4 tailHi; vec4 tailLo; vec4 pa; vec4 qb; };
layout(std430, binding = 0) readonly buffer Pts { vec4 pts[]; };
layout(std430, binding = 1) readonly buffer Runs { Run runs[]; };
layout(std430, binding = 2) buffer Off { uint off[]; };
layout(std430, binding = 3) buffer Scr { vec4 scr[]; };
layout(std430, binding = 4) buffer Ind { uint ind[]; };
layout(std430, binding = 5) buffer Chk { vec2 chk[]; };
uniform int uStep;
)";
// run.a: the first slot of its points, the ring's size, the ring's start, the points (n); run.b: its first and last piece, its kind (0 straight, 1 a
// trail: point n is the body itself, tail; 2 an orbit), its first piece among all; run.c.x: the slot of the unit circle's 257 corners (orbits);
// style.x: the width; pa, qb: an orbit's P a and Q b (m) and (qb.w) the step of E between corners
static const char* const kLinesPart = R"(
uniform vec3 uCHi;
uniform vec3 uCLo;
uniform vec4 uRot;
uniform vec4 uProj;
uniform vec4 uScr;
uniform vec4 uView;
uniform ivec4 uN;
int runOf(int g) {
    int lo = 0, hi = uN.x - 1;
    while (lo < hi) { int mid = (lo + hi + 1) / 2; if (runs[mid].b.w <= g) lo = mid; else hi = mid - 1; }
    return lo;
}
void pointHL(int r, int i, out vec3 hi, out vec3 lo) {
    uvec4 a = runs[r].a;
    int n = int(a.w);
    if (runs[r].b.z == 1 && i >= n) { hi = runs[r].tailHi.xyz; lo = runs[r].tailLo.xyz; return; }
    int k = clamp(i, 0, n - 1);
    uint s = a.x + (a.z + uint(k)) % a.y;
    hi = pts[2u * s].xyz; lo = pts[2u * s + 1u].xyz;
}
vec3 pointOf(int r, int i) {
    vec3 h = vec3(0.0), l = vec3(0.0);
    pointHL(r, i, h, l);
    precise vec3 d = (h - uCHi) + (l - uCLo);
    return d;
}
vec2 project(vec3 v) {
    float x1 = v.x * uRot.x - v.y * uRot.y;
    float y1 = v.x * uRot.y + v.y * uRot.x;
    float y2 = y1 * uRot.z - v.z * uRot.w;
    float dep = y1 * uRot.w + v.z * uRot.z;
    float fm1 = 0.0;
    if (uProj.w > 0.5) { float x = uProj.z + dep; float den = uProj.y - x; fm1 = den > 0.05 * uProj.y ? x / den : 19.0; }
    float f = 1.0 + fm1;
    return vec2(uScr.x + uScr.z * fm1 + uProj.x * f * x1, uScr.y - uScr.w * fm1 - uProj.x * f * y2);
}
int steps(int r, int i, vec2 qa, vec2 qc) {
    float L = length(qc - qa);
    float mg = 20.0 + L / 4.0;
    bool near = max(qa.x, qc.x) > -mg && min(qa.x, qc.x) < uView.x + mg && max(qa.y, qc.y) > -mg && min(qa.y, qc.y) < uView.y + mg;
    if (!near || !(L > 6.0)) return 1;
    int k = int(min(4096.0, ceil(L / 4.0)));
    int kind = runs[r].b.z;
    if (kind == 1 && i >= 2 && i <= int(runs[r].a.w)) return k;
    if (kind == 2 && k >= 3) return k;
    return 1;
}
vec2 stepPoint(int r, int i, vec2 qa, vec2 qc, int k, int j) {
    if (j <= 0) return qa;
    if (j >= k) return qc;
    float t = float(j) / float(k);
    if (runs[r].b.z == 1) {                      // the processor's Catmull-Rom curve, written from point a with the neighbours as steps from it (exact differences)
        vec3 ah = vec3(0.0), al = vec3(0.0), ch = vec3(0.0), cl = vec3(0.0), h0 = vec3(0.0), l0 = vec3(0.0), h3 = vec3(0.0), l3 = vec3(0.0);
        pointHL(r, i - 1, ah, al);
        pointHL(r, i, ch, cl);
        pointHL(r, i - 2, h0, l0);
        precise vec3 A = (ah - uCHi) + (al - uCLo);
        precise vec3 d1 = (ch - ah) + (cl - al);
        precise vec3 d0 = (h0 - ah) + (l0 - al);
        vec3 d3 = d1 * 2.0;                         // (p3 = c * 2 - a past the trail's end)
        if (i + 1 <= int(runs[r].a.w)) { pointHL(r, i + 1, h3, l3); precise vec3 e3 = (h3 - ah) + (l3 - al); d3 = e3; }
        float t2 = t * t;
        float t3 = t2 * t;
        vec3 v = A + ((d1 - d0) * t + (d0 * 2.0 + d1 * 4.0 - d3) * t2 + (d3 - d0 - d1 * 3.0) * t3) * 0.5;
        return project(v);
    }
    uint cs = uint(runs[r].c.x) + uint(i - 1);
    float cE = pts[2u * cs].x + pts[2u * cs + 1u].x;
    float sE = pts[2u * cs].y + pts[2u * cs + 1u].y;
    float h = runs[r].qb.w * float(j) / (2.0 * float(k));
    float s = sin(h);
    float ch = cos(h);
    float sinM = sE * ch + cE * s;
    float cosM = cE * ch - sE * s;
    vec3 v = pointOf(r, i - 1) + runs[r].pa.xyz * (-2.0 * sinM * s) + runs[r].qb.xyz * (2.0 * cosM * s);
    return project(v);
}
)";
static const char* const kLinesMainCSPart = R"(
shared uint part[256];
void main() {
    int N = uN.y;
    if (uStep == 1) {
        uint t = gl_LocalInvocationID.x;
        uint n = uint(N);
        uint per = (n + 255u) / 256u;
        uint b0 = min(n, t * per);
        uint b1 = min(n, b0 + per);
        uint sum = 0u;
        for (uint i = b0; i < b1; i++) sum += off[i];
        part[t] = sum;
        memoryBarrierShared();
        barrier();
        for (uint d = 1u; d < 256u; d <<= 1u) {
            uint v = t >= d ? part[t - d] : 0u;
            memoryBarrierShared();
            barrier();
            part[t] += v;
            memoryBarrierShared();
            barrier();
        }
        uint run = part[t] - sum;
        for (uint i = b0; i < b1; i++) { uint c = off[i]; off[i] = run; run += c; }
        if (t == 255u) { off[n] = part[255]; ind[0] = 6u * part[255]; ind[1] = 1u; ind[2] = 0u; ind[3] = 0u; }
        return;
    }
    int g = int(gl_GlobalInvocationID.x);
    if (g >= N) return;
    int r = runOf(g);
    int i = runs[r].b.x + (g - runs[r].b.w);
    if (uStep == 0) {
        vec2 qa = project(pointOf(r, i - 1));
        vec2 qc = project(pointOf(r, i));
        off[g] = uint(steps(r, i, qa, qc));
        scr[g] = vec4(qa, qc);
        return;
    }
    int k = int(off[g + 1] - off[g]);
    for (int j = 1; j <= k; j++) { uint at = off[g] + uint(j - 1); if (at < uint(uN.z)) chk[at] = stepPoint(r, i, scr[g].xy, scr[g].zw, k, j); }
}
)";
static const char* const kLinesHeadVSPart = R"(#version 450
struct Run { uvec4 a; ivec4 b; ivec4 c; vec4 colour; vec4 style; vec4 tailHi; vec4 tailLo; vec4 pa; vec4 qb; };
layout(std430, set = 1, binding = 0) readonly buffer Pts { vec4 pts[]; };
layout(std430, set = 1, binding = 1) readonly buffer Runs { Run runs[]; };
layout(std430, set = 1, binding = 2) readonly buffer Off { uint off[]; };
layout(std430, set = 1, binding = 3) readonly buffer Scr { vec4 scr[]; };
uniform vec2 uSize;
out vec2 vUV;
out vec4 vColor;
out vec4 vParam;
)";
static const char* const kLinesMainVSPart = R"(
bool clipSeg(vec2 a, vec2 b, out vec2 o0, out vec2 o1) {
    float xa = -uView.z, ya = -uView.z, xb = uView.x + uView.z, yb = uView.y + uView.z;
    vec2 d = b - a;
    o0 = a; o1 = b;
    if (a.x >= xa && a.x <= xb && b.x >= xa && b.x <= xb && a.y >= ya && a.y <= yb && b.y >= ya && b.y <= yb) return true;
    float t0 = 0.0, t1 = 1.0;
    for (int k = 0; k < 4; k++) {
        float p = k == 0 ? -d.x : k == 1 ? d.x : k == 2 ? -d.y : d.y;
        float q = k == 0 ? a.x - xa : k == 1 ? xb - a.x : k == 2 ? a.y - ya : yb - a.y;
        if (p == 0.0) { if (q < 0.0) return false; }
        else {
            float r = q / p;
            if (p < 0.0) { if (r > t1) return false; if (r > t0) t0 = r; }
            else { if (r < t0) return false; if (r < t1) t1 = r; }
        }
    }
    o0 = a + t0 * d; o1 = a + t1 * d;
    return true;
}
void main() {
    int v = gl_VertexIndex;
    int s = v / 6;
    int corner = v - s * 6;
    int lo = 0, hi = uN.y;
    while (hi - lo > 1) { int mid = (lo + hi) / 2; if (int(off[mid]) <= s) lo = mid; else hi = mid; }
    int g = lo;
    int k = int(off[g + 1] - off[g]);
    int j = s - int(off[g]);
    int r = runOf(g);
    int i = runs[r].b.x + (g - runs[r].b.w);
    vec2 qa = scr[g].xy, qc = scr[g].zw;
    vec2 A = stepPoint(r, i, qa, qc, k, j);
    vec2 B = stepPoint(r, i, qa, qc, k, j + 1);
    vec2 P0 = A, P1 = B;
    bool keep = clipSeg(A, B, P0, P1);
    vec4 col = runs[r].colour;
    float w = runs[r].style.x;
    vec2 d = P1 - P0;
    float len = length(d);
    vec2 pos = vec2(-1.0e4, -1.0e4);
    if (keep && len >= 1e-9) {
        if (w < 1.0) { col.a *= w; w = 1.0; }
        vec2 nrm = vec2(-d.y, d.x) / len * (w / 2.0);
        bool end1 = corner == 1 || corner == 2 || corner == 4;
        bool plus = corner == 0 || corner == 1 || corner == 3;
        pos = (end1 ? P1 : P0) + (plus ? nrm : -nrm);
    }
    vUV = vec2(0.0); vColor = col; vParam = vec4(0.0);
    gl_Position = vec4(pos.x / uSize.x * 2.0 - 1.0, 1.0 - pos.y / uSize.y * 2.0, 0.0, 1.0);
}
)";

// 0.68.0: the Molecule Lab's Density surface on the card (Vulkan; render/surface_card.h, molecule/surface_draw.h). The surface's vertices (place, normal,
// potential) and triangles stay on the card while the density stays (a vibration's new phase: sent once); kSurfaceCS turns and colours the vertices as
// molecule_view.cpp did on the processor (step 0), keys each triangle by its depth as molecule/depth_order.h does (step 1: the float's bits made monotonic,
// with the triangle's number) and sorts the keys far to near (steps 2 to 4: a bitonic sort by key, then number, so equal depths keep the processor's order
// by number); kSurfaceVS draws the triangles in that order, each lit front or back by its winding on the picture, with kFragmentShader (mode 0).
static const char* const kSurfaceCS = R"(#version 430 core
layout(local_size_x = 256) in;
layout(std430, binding = 0) readonly buffer Mesh { vec4 mesh[]; };
layout(std430, binding = 1) readonly buffer Tri { uint tri[]; };
layout(std430, binding = 2) buffer Vert { vec4 vert[]; };
layout(std430, binding = 3) buffer Keys { uvec2 keys[]; };
uniform int uStep;
uniform ivec4 uN;
uniform ivec2 uKJ;
uniform vec4 uR0;
uniform vec4 uR1;
uniform vec4 uR2;
uniform vec4 uCam;
uniform vec4 uS;
uniform vec4 uPlain;
shared uvec2 sk[512];
bool after(uvec2 a, uvec2 b) { return a.x > b.x || (a.x == b.x && a.y > b.y); }
vec3 potColour(float t) {
    float c = clamp(t, -1.0, 1.0);
    vec3 e = c < 0.0 ? vec3(0.86, 0.16, 0.13) : vec3(0.15, 0.36, 0.90);
    vec3 w = vec3(0.97, 0.97, 0.97);
    return w + (e - w) * abs(c);
}
void localSort(int kFrom, int kTo, int jTop) {
    uint t = gl_LocalInvocationID.x;
    uint base = gl_WorkGroupID.x * 512u;
    sk[t] = keys[base + t];
    sk[t + 256u] = keys[base + t + 256u];
    memoryBarrierShared();
    barrier();
    for (int k = kFrom; k <= kTo; k *= 2) {
        for (int j = (jTop > 0 ? jTop : k / 2); j > 0; j /= 2) {
            uint uj = uint(j);
            uint i = (t / uj) * 2u * uj + (t % uj);
            uint l = i + uj;
            bool up = ((base + i) & uint(k)) == 0u;
            uvec2 a = sk[i], b = sk[l];
            if (up ? after(a, b) : after(b, a)) { sk[i] = b; sk[l] = a; }
            memoryBarrierShared();
            barrier();
        }
    }
    keys[base + t] = sk[t];
    keys[base + t + 256u] = sk[t + 256u];
}
void main() {
    if (uStep == 2) { localSort(2, min(uN.z, 512), 0); return; }
    if (uStep == 4) { localSort(uKJ.x, uKJ.x, 256); return; }
    uint g = gl_GlobalInvocationID.x;
    if (uStep == 0) {
        if (g >= uint(uN.x)) return;
        vec4 p = mesh[2u * g], n = mesh[2u * g + 1u];
        float sx = uCam.z + (uR0.x * p.x + uR0.y * p.y + uR0.z * p.z + uCam.x) * uS.x;
        float sy = uCam.w - (uR1.x * p.x + uR1.y * p.y + uR1.z * p.z + uCam.y) * uS.x;
        float sz = uR2.x * p.x + uR2.y * p.y + uR2.z * p.z;
        float vz = uR2.x * n.x + uR2.y * n.y + uR2.z * n.z;
        float d = abs(vz);
        float hl = pow(max(0.0, d), 24.0) * 0.15;
        vec3 base = uS.z > 0.5 ? potColour(p.w / uS.y) : uPlain.rgb;
        float k = 0.62 + 0.38 * d;
        vert[2u * g] = vec4(sx, sy, sz, 0.0);
        vert[2u * g + 1u] = vec4(min(vec3(1.0), base * k + vec3(hl)), 1.0);
        return;
    }
    if (uStep == 1) {
        if (g >= uint(uN.z)) return;
        if (g < uint(uN.y)) {
            float tz = vert[2u * tri[3u * g]].z + vert[2u * tri[3u * g + 1u]].z;
            tz = tz + vert[2u * tri[3u * g + 2u]].z;
            uint u = floatBitsToUint(tz);
            keys[g] = uvec2((u & 0x80000000u) != 0u ? ~u : (u | 0x80000000u), g);
        } else keys[g] = uvec2(0xFFFFFFFFu, g);
        return;
    }
    if (g >= uint(uN.z)) return;
    uint l = g ^ uint(uKJ.y);
    if (l <= g) return;
    bool up = (g & uint(uKJ.x)) == 0u;
    uvec2 a = keys[g], b = keys[l];
    if (up ? after(a, b) : after(b, a)) { keys[g] = b; keys[l] = a; }
}
)";
static const char* const kSurfaceVSHeadPart = R"(#version 450
layout(std430, set = 1, binding = 0) readonly buffer Vert { vec4 vert[]; };
layout(std430, set = 1, binding = 1) readonly buffer Tri { uint tri[]; };
layout(std430, set = 1, binding = 2) readonly buffer Keys { uvec2 keys[]; };
uniform vec2 uSize;
uniform vec2 uAlpha;
out vec2 vUV;
out vec4 vColor;
out vec4 vParam;
)";
static const char* const kSurfaceVSMainPart = R"(
void main() {
    int v = gl_VertexIndex;
    int k = v / 3;
    int corner = v - 3 * k;
    uint t = keys[k].y;
    uint ia = tri[3u * t], ib = tri[3u * t + 1u], ic = tri[3u * t + 2u];
    vec2 A = vert[2u * ia].xy, B = vert[2u * ib].xy, C = vert[2u * ic].xy;
    float ex = (B.x - A.x) * (C.y - A.y) - (B.y - A.y) * (C.x - A.x);
    uint me = corner == 0 ? ia : corner == 1 ? ib : ic;
    vec4 col = vert[2u * me + 1u];
    col.a = ex < 0.0 ? uAlpha.x : uAlpha.y;
    vec2 pos = vert[2u * me].xy;
    vUV = vec2(0.0); vColor = col; vParam = vec4(0.0);
    gl_Position = vec4(pos.x / uSize.x * 2.0 - 1.0, 1.0 - pos.y / uSize.y * 2.0, 0.0, 1.0);
}
)";

}  // namespace render
