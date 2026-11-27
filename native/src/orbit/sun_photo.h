// 0.94.0: NASA SDO/HMI's photo of the Sun's surface for the shown day (the Orbit Lab's Sun close up, dates from 6 Dec 2010).
// Data: NASA SDO/HMI continuum intensity (617.3 nm, the header's WAVELNTH 6173 A), as served by the Helioviewer Project's API v2
// (getClosestImage, getJP2Header, downloadImage; native/data/sun/hmi/helioviewer_api_v2_2026-11-27.html in the sources repo).
// Terms: NASA SDO's data rules (sdo_data_rules_2026-11-27.html): "SDO images and movies are not copyrighted unless explicitly noted"; non-commercial and education
// use "requires no expressed authorization"; credit "Courtesy of NASA/SDO and the AIA, EVE, and HMI science teams." The card and About say it.
// The picture is Helioviewer's PNG at scale 2 (2048 x 2048, 8-bit grey, about 1.3 MB). Its grey level is proportional to the intensity: on 10 May 2024 its
// edge profile matches Neckel & Labs 1994 at 617.3 nm within about 1 grey level out to 0.9 of the radius (REPORT 0.94.0; tests/test_sun094.inc).
// Only its relative brightness is used: each pixel over the picture's own limb fit (a quintic in mu, fitted here to the medians of rings of the disc).
// Geometry from the header: the disc's centre CRPIX1, CRPIX2 (FITS pixels, 1-based, y up), the pixel size CDELT1 and the Sun's radius RSUN_OBS (arcsec),
// CROTA2 (0 for Helioviewer's HMI pictures: solar north up; another value is refused), the observer's Carrington longitude and latitude CRLN_OBS, CRLT_OBS.
// A point of the Sun's IAU frame (orbit/spin.cpp; its longitudes are Carrington's: orbit/sunspots.h) is on the picture at west = b . W, north = b . N,
// with W = (-sin L0, cos L0, 0), N = (-sin B0 cos L0, -sin B0 sin L0, cos B0), seen where mu = b . O > 0, O = (cos B0 cos L0, cos B0 sin L0, sin B0).
// The photo is used where its own mu is above 0.3, faded out down to 0.2 (its edge is too foreshortened, and the limb fit reaches mu 0.14); beyond, the drawn surface.
// The download: only the date goes out (and the picture's number from Helioviewer's reply). It runs on a worker thread, never on the frame. Windows: WinHTTP.
// Other builds: no download (this build has no HTTP library): the drawn surface, or a picture already in the cache.
// Cache: <the app's folder>/sun_photos/<YYYY-MM-DD>.json, .xml, .png (PL_SUNPHOTO_CACHE replaces the folder; PL_SUNPHOTO_OFFLINE=1 turns the download off).
#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace orbit {

constexpr double kSunPhotoMinPx = 150;              // the Sun "close up": its radius on screen at least this many pixels (the photo is asked for then)
constexpr int kPhotoScale = 2;                       // Helioviewer downloadImage scale: 4096 / 2 = 2048 pixels
constexpr double kPhotoMuFull = 0.3, kPhotoMuNone = 0.2;   // inside the limb fit's range (mu >= 0.14), where the picture's pixels are stretched at most 5 times
constexpr double kPhotoMaxAwayDays = 1.0 / 24;       // the reply's time must be within an hour of the asked 00:00 UT, else no photo for that day
// Helioviewer's first HMI continuum picture: 2010-12-06 06:53:41 UT (its getClosestImage reply for 2010-01-01, 27 Nov 2026). Earlier days are not asked.
constexpr double kPhotoFirstJD = 2455536.5;          // 2010-12-06 00:00 UT

struct SunPhotoMeta {
    long long id = 0;
    std::string date;                                // the reply's "date" (UT), e.g. "2024-05-09 23:59:50"
    double jd = 0;                                   // the same as a Julian day (UT)
    int naxis1 = 0, naxis2 = 0;
    double crpix1 = 0, crpix2 = 0, cdelt1 = 0, rsunObs = 0, crlnObs = 0, crltObs = 0, crota2 = 0, wavelnth = 0;
};
struct SunPhoto {
    bool ok = false;
    std::string why;                                 // when not ok
    SunPhotoMeta meta;
    int w = 0, h = 0, scale = kPhotoScale;
    std::vector<std::uint8_t> grey;                  // row 0 at the top (solar north up), west to the right
    double cx = 0, cy = 0, rpx = 0;                  // the disc's centre (pixel centres at whole numbers, x right, y down) and radius, in the picture's pixels
    double fit[6] = {};                              // the picture's own limb darkening: grey(mu) = Sum fit[n] mu^n
    double O[3] = {}, W[3] = {}, N[3] = {};          // the observer's direction and the picture's west and north, in the Sun's IAU frame
};

double jdFromUtText(const std::string& s);           // "YYYY-MM-DD hh:mm:ss" or "YYYY-MM-DDThh:mm:ss[.s]" (UT); NaN when not readable
std::string photoDayKey(double jdUT);                // the day whose 00:00 UT is nearest: "YYYY-MM-DD"
double photoDayJD(const std::string& key);           // that 00:00 UT
bool parseClosestReply(const std::string& json, SunPhotoMeta& m, std::string& why);
bool parseHeaderXml(const std::string& xml, SunPhotoMeta& m, std::string& why);
// the picture from the three files' contents (the PNG may be shrunk by 1, 2, 4 or 8 from the header's size)
SunPhoto loadSunPhoto(const std::vector<std::uint8_t>& png, const std::string& xml, const std::string& json);
// the relative brightness at b (unit vector, Sun's IAU frame): the picture over its limb fit, bilinear; weight 1 where the photo is used, 0 where not
void photoAt(const SunPhoto& p, const double b[3], double& rel, double& weight);
double photoFitAt(const SunPhoto& p, double mu);

// ---- the day's photo: the cache and the download, on a worker ----
struct SunPhotoLoad { bool done = false, fromNet = false, ok = false; double ms = 0; std::string day; };
class SunPhotoSource {
public:
    static SunPhotoSource& get();
    void setCacheDir(const std::string& dir);
    // each frame while the Sun is drawn close up (closeUp) at jdUT; starts the day's load when needed
    void want(double jdUT, bool closeUp);
    const SunPhoto* shown() const;                   // the photo of the day asked, or nullptr
    int version() const { return version_; }         // grows by one each time a new photo becomes ready (the renderer uploads it)
    std::vector<std::string> cardLines(double jdUT) const;
    const SunPhotoLoad& lastLoad() const { return last_; }   // the last load's time (the benchmark's response line)
    void dropMemory();                               // forget the photo in memory (the benchmark: the next load comes from the cache)
    void dropCacheDay(const std::string& day);       // remove a day from the cache (the benchmark: the next load comes from the network, once)
    bool canDownload() const;
    void hold(bool on) { held_ = on; }               // the benchmark's orbit.sun_surface: no photo asked or shown (the drawn surface)
    bool busy() const { return job_ != nullptr; }    // a photo being downloaded or read (the benchmark waits for it)
    std::string cacheDir() const { return dir_; }
private:
    SunPhotoSource() = default;
    struct Job;
    void poll();
    std::string dir_;
    std::string wantDay_, shownDay_, noPhotoDay_, failDay_, failWhy_;
    std::unique_ptr<SunPhoto> photo_;
    std::shared_ptr<Job> job_;
    int version_ = 0;
    bool held_ = false;
    SunPhotoLoad last_;
};

}  // namespace orbit
