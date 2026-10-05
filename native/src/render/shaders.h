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

}  // namespace render
