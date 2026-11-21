// Black Hole Lab (0.70.0): the Real look's camera-like picture. Plain functions in double, no graphics: the processor's exact reference for the card's
// shaders (render/shaders.h kLensFS, kKerrFS, kKerrSkyFS, kGlareAddFS, kExpSumFS) and the readout's numbers.
//
// THE TONE CURVE: E. Reinhard, M. Stark, P. Shirley, J. Ferwerda, "Photographic Tone Reproduction for Digital Images", ACM Transactions on Graphics 21(3),
// 267 (2002), SIGGRAPH 2002, read from its technical report UUCS-02-01 (University of Utah, 14 January 2002; the private sources repo
// data/blackhole/tone/reinhard2002_uucs-02-01.*; the lines used are copied exactly in native/data/blackhole/tone/reinhard2002_uucs-02-01.extract.txt, which
// tests/test_blackhole.cpp testTone reads). Its global operator (section 3.1), in the paper's own words and numbers:
//   (1) the log-average luminance  Lbar_w = exp( (1/N) sum_{x,y} log(delta + L_w(x, y)) ), "delta is a small value to avoid the singularity that occurs if
//       black pixels are present in the image";
//   (2) the key scaling  L = (a / Lbar_w) L_w  with a = 0.18 ("middle-grey of the displayed image, or 0.18 on a scale from zero to one");
//   (4) the curve with a burn-out white  L_d = L (1 + L / L_white^2) / (1 + L), "L_white is the smallest luminance that will be mapped to pure white";
//       "By default we set L_white to the maximum luminance in the scene".
// HOW THE APP USES IT (our choices, stated in the app): the world luminance L_w is the disc's visible luminance Y (CIE 1931 ybar, in units of the Sun's
// surface; lab.h discPixel); the log-average is taken over the same sample points the automatic exposure always used (lab.h discLook: every 4 points,
// the first disc image), so over the disc's light only (the sky and the shadow are not in it); delta = kToneDelta x the brightest sample (the paper gives
// no number: we take 2^-14, so that the disc's parts fainter than 14 stops below its brightest count as that floor in the average, not as minus infinity);
// L_white = the brightest sample's L (the paper's default). The curve acts on luminance only: each pixel keeps its chromaticity (the physics' colour ratios),
// L_d / L scales its three linear channels alike, and a channel that then passes the screen's white scales the three down together (its hue kept, as before).
// "Brighter or darker than automatic" (the manual exposure) multiplies a by 2^stops (photographic stops). The Film look does not use the curve.
#pragma once

#include <array>
#include <vector>

namespace blackhole {

constexpr double kToneKey = 0.18;                     // a, Reinhard et al. 2002 eq. (2): the key value, "middle-grey"
constexpr double kToneDelta = 1.0 / 16384;           // delta of eq. (1) over the brightest sample: 2^-14 (our choice; the paper gives no number)
constexpr double kEvMin = -20, kEvMax = 50;          // the manual exposure's range in stops from automatic (+: brighter; our choice)

// eq. (1) over samples Y (delta absolute, in Y's units)
double logAverage(const std::vector<double>& Y, double delta);
// eq. (4); Lwhite <= 0: eq. (3) (Lwhite infinite)
double toneCurve(double L, double Lwhite);

// The exposure of one picture: L = k Y, the burn-out white Lw (in L), the log-average and the brightest Y it was made from
struct ToneExposure { double k = 0, lw = 0, logAvg = 0, yMax = 0; bool ok = false; };
// from eq. (1)'s log-average and the brightest sample: k = a 2^stops / logAvg, Lw = k yMax (ok false when either is not positive)
ToneExposure toneExposure(double logAvg, double yMax, double stops);
// the same from the samples themselves (delta = kToneDelta x their largest)
ToneExposure toneFromSamples(const std::vector<double>& Y, double stops);
// one pixel: linear sRGB light in the disc's units (the Sun's surface) -> linear screen light 0..1 (before the sRGB encoding): its luminance through eq. (4),
// its chromaticity kept, scaled down together when a channel passes 1
std::array<double, 3> toneRGB(const std::array<double, 3>& rgb, const ToneExposure& e);
// the same on light already scaled by k (in L units), as the card has it
std::array<double, 3> toneScaled(const std::array<double, 3>& c, double lw);
// the luminance of linear sRGB (IEC 61966-2-1's Y row, as every shader of the lab)
double lumaSrgb(const std::array<double, 3>& c);

}  // namespace blackhole
