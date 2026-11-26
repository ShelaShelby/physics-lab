// Black Hole Lab (0.55.0): the picture. The physics is the module's (schwarzschild.h, double precision); this file only hands it to the renderer:
//   - the bent sky: Renderer2D::lensSky with the module's LensTable for the camera's distance (filled again only when the distance changes), drawn by the
//     graphics card per pixel and kept until the view changes (plan section 6, the level "Basic": a table made on the processor, the closed shadow test per pixel);
//   - the catalogue stars as points, every image of each (starImagesFast) at its magnified brightness, by the night sky's one rule (orbit/sky.h starDraw);
//     the images are worked out again only when the view changes. An image brighter than the brightest star of the catalogue seen without the hole (Sirius)
//     is drawn as bright as that star: near the ring of light the magnification grows without limit for a true point, and a drawn dot cannot show it.
//   - the map at the Orbit Lab's full level (its night sky zoomed out, level 1: the map holds the bright stars faintly, the catalogue's dots carry their measured light).
//   - 0.55.1, the gas disc (disc.h, lab.h): its table (DiscTable, filled again only when the distance changes) and a colour table, handed to the same shader
//     pass, which draws the disc in front of the sky; a star image the disc covers (lab.h discPixel at the image's point) is not drawn. The exposure: lab.h discLook.
#pragma once

#include <array>
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "blackhole/lab.h"
#include "blackhole/light.h"
#include "blackhole/spin_card.h"
#include "blackhole/spin_sky.h"
#include "blackhole/sky_card.h"
#include "render/renderer2d.h"

namespace blackhole {

struct ViewStats {
    int stars = 0, images = 0, extraImages = 0, capped = 0;   // stars with an image on the picture; images drawn; of them second or further; drawn at the cap
    double tableMs = 0, starsMs = 0;                           // the last refill of the table and of the stars' images (processor time)
    long tableFills = 0, starFills = 0;
    double shadowPx = 0;                                       // the shadow's radius on the picture, points
    double cx = 0, cy = 0;                                     // the hole's place on the picture, points
    // 0.55.1, the disc: its table's last refill (processor time), the picture's brightest part (lab.h discLook) and the screen's white in use, the stars it hides
    double discMs = 0;
    long discFills = 0;
    DiscLook look;
    int hidden = 0;
    // 0.55.2, spin: the shown table (fine or coarse), its last coarse and fine fills (processor time, ms), how many, and the star images' inversion
    bool spinFine = false;
    double spinCoarseMs = 0, spinFineMs = 0, spinStarsMs = 0;
    long spinCoarseFills = 0, spinFineFills = 0;
    // 0.55.4, the true exposure (light.h): the brightest catalogue star, the solid angle of a square point at the middle, the readout's numbers; of the last
    // stars' fill, the images drawn and those below the screen's faintest step (not drawn)
    double brightMag = 0, ptSr = 0;
    TrueSkyNumbers trueSky;
    int trueShown = 0, trueHidden = 0;
    // 0.55.5: the exposure's last scan (processor time, ms) and how many; the star bins' (SpinStars) builds; behind: the picture shown is not yet of the
    // settings asked for (it is being worked out), for behindMs; frameMs: this frame's time in draw (the frame's own thread); workMs: the shown picture's
    // time on the worker; picture: the shown picture's number; made: the view (yaw, pitch, distance, spin) its exposure and stars were worked out for
    double lookMs = 0;
    long lookFills = 0, spinBins = 0;
    bool behind = false;
    double behindMs = 0, frameMs = 0, workMs = 0;
    // 0.83.0: lagMs: how old the shown picture is: 0 while it is of the settings asked for, else the time since its settings were last the ones asked for.
    // (behindMs counts from the first frame behind: while the view moves every frame the settings change every frame, so it grows for the whole movement.)
    double lagMs = 0;
    std::uint64_t picture = 0;
    double made[4] = {0, 0, 0, 0};
    // 0.55.6: the shown spin table was filled by the graphics card; the card's last fill (ms, from its start to the frame that found it done) and how many
    bool spinCard = false;
    double cardMs = 0;
    long cardFills = 0;
    // 0.56.0: the spinning hole's sky and disc drawn per pixel by the graphics card (skyCard; sky_card.h): the pass's time on the card for the whole picture
    // and the resolution's scale of the last one, the new pictures of sky and disc the card has drawn (skyPictures) and the stars' pictures installed
    // (starPictures), the exposure found on the card (lookCard)
    bool skyCard = false, lookCard = false;
    double skyGpuMs = -1, skyScale = 1;
    long skyPictures = 0, starPictures = 0;
};
float catalogueBrightestMag();   // 0.55.4: the brightest catalogue star seen without the hole (the cap of a drawn image), V magnitude

class BlackHoleView {
    friend void compareHook(render::Renderer2D& R, const char* spec);
    friend void toneCompare(render::Renderer2D& R);
// 0.69.0: the tests' 8-bit pictures (compareHook, PL_BH_COMPARE): what the renderer recorded, drawn off screen and read back, RGBA, rows from the bottom;
// set by main.cpp (render/vk_backend.h renderToImage); false: not drawn
using TestShot = std::function<bool(render::Renderer2D&, int, int, std::vector<unsigned char>&)>;
TestShot& testShot();
public:
    BlackHoleView();                                           // (0.55.5: in the .cpp, where Request and Picture are complete)
    BlackHoleView(const BlackHoleView&) = delete;
    BlackHoleView& operator=(const BlackHoleView&) = delete;
    ~BlackHoleView();
    // The picture W x H points with the hole at (cx, cy) points.
    // 0.55.5: async (the app): the frame never waits for the processor: the tables, the stars' images and the exposure are worked out on a worker thread
    // (a request not yet taken is replaced by a newer one; every finished picture is shown), the eye's glare on a thread of the renderer's; the frame draws the last finished picture,
    // all of one view, until a newer one is done (stats().behind). Off (the default; the automatic checks and the tests' own views): all of it in draw, as before.
    bool async = false;
    // 0.55.6 (Settings, Performance, "GPU light paths"): async only: the spinning hole's table filled by the graphics card (spin_card.h) once its start-up
    // check has passed; the card's table is the picture while moving and at rest (no fine table on the processor). Off, or no Vulkan compute, or the check
    // failed: the processor's tables of 0.55.5.
    bool gpuPaths = true;
    // 0.56.0 (Settings, Performance): the spinning hole's sky and disc drawn per pixel by the graphics card from the closed forms (render/kerr_sky.h) once
    // its start-up check has passed; the processor's tables then serve the stars only (they may be a few pictures behind while the view moves). Off, or the
    // check failed, or the guard switched it off: the processor's (or 0.55.6's card's) table draws them, as in 0.55.9. refreshHz: the screen's (the
    // resolution rule's budget: half its period)
    bool gpuSky = true;
    double gasT = 0;                                           // 0.72.0: the moving gas's far time now (M; the app advances it: gasTick)
    double refreshHz = 60;
    // 0.56.2 (Settings, Performance, shown only on a slow card): "Lower resolution while moving", 0 Auto (0.56.0's rule, only where the card's whole
    // picture takes longer than kSkySharpMs), 1 Off (always every pixel). Not saved. skySlowMs: the card's time for the whole picture (> kSkySharpMs: slow)
    static constexpr double kSkySharpMs = 1000.0 / 60;
    int lowResMode = 0;
    double skySlowMs() const;
    int skyState() const { return skyState_; }                 // 0 not yet asked, 2 its steps under way, 1 the card draws, -1 the processor's (skyWhy says why)
    const std::string& skyWhy() const { return skyWhy_; }
    const SkyCheck& skyCheck() const { return skyCheck_; }
    // 0.55.8: how the fine table's rays are followed (spin_sky.h; the checks set SpinFill{} to draw with 0.55.7's rays)
    SpinFill fineFill = kSpinFineFill;
    double fineFillMs() const { return fine_ ? fine_->fillMs() : 0; }   // 0.55.8: the fine table's fill (the benchmark: the one makeFineNow made)
    int cardState() const { return cardState_; }               // 0 not yet asked (the lab has not drawn a spinning hole), 2 (0.55.7) its check under way, 1 the card's path, -1 the processor's
    const std::string& cardWhy() const { return cardWhy_; }    // -1: why
    const CardCheck& cardCheck() const { return cardCheck_; }
    bool fineThreadBusy() const { return spinWorker_.joinable(); }   // 0.55.6 (tests): the processor's fine table is being made
    void stopWork();                                           // 0.55.6: the worker stopped and joined (before the renderer, which owns the card's memory, shuts down)
    void draw(render::Renderer2D& R, const Settings& s, double W, double H, double cx, double cy);
    void makeFineNow(const Settings& s);                       // 0.55.5 (the benchmark): the fine spin table for s now, on this thread
    bool settled();                                            // 0.55.5 (automatic checks): nothing under way, the picture shown is the settings' (fine table included)
    const ViewStats& stats() const { return st_; }
    const LensTable* table() const { return table_.get(); }
    const Settings& shownSettings() const { return shownS_; }  // 0.55.5: the settings of the picture shown (async: may be behind the ones asked for)
    // the renderer's description of the bent sky for this view (also used by the tests and the benchmark)
    render::Renderer2D::LensSky lensFor(const Settings& s, const HoleView& v) const;
    void toneFields(render::Renderer2D::LensSky& L, const Settings& s) const;   // 0.70.0
    void ensureTable(double dist);
    void ensureDisc(double dist, bool moving = false);         // 0.55.1: the disc's table for this distance (0.56.0: moving: kDiscMovingRows x kDiscMovingCols)
    // 0.56.0: the disc's table while the distance changes (spin 0): 256 x 128 rows and columns per side in place of 768 x 256 (a third of the time; at
    // 1280 x 800 not one pixel changes between disc and no disc against the full table and r within 1.1e-5, 10, 3 and 0.5 degrees above the disc: REPORT 0.56.0)
    static constexpr int kDiscMovingRows = 256, kDiscMovingCols = 128;
    const DiscTable* discTable() const { return disc_.get(); }
    // tests (PL_DRAWSTAT): up to n pixel centres where the disc of s (taken as switched on) covers the picture, and n where it does not, each at least 3 points
    // from an edge of the disc's image (lab.h discPixel), on a grid; points of the picture
    void discSamples(const Settings& s, double W, double H, double cx, double cy, int n, std::vector<double>& on, std::vector<double>& off);
    int discGridOn() const { return discGridOn_; }             // of the last discSamples: how many grid points (of 63 x 39) lie on the disc
    // the colour of a black body against log10 T for the card (blackhole/disc.h blackBodyLinear), made once
    static const std::vector<float>& colourTable(int& n, double& log0, double& dlog);

    // 0.55.2, spin (s.spin > 0 with bending on): the table for (spin, distance, latitude). A change of any of them makes the coarse table at once (on this thread)
    // and drops a finer one being made; the fine one is made on a thread of its own once the settings have rested for kSpinRest seconds, and is shown when it is
    // done if the settings are still its own. A table of other settings is never shown. wait: tests: make the fine one now, on this thread.
    void ensureSpin(const Settings& s, bool wait = false);
    const SpinTable* spinTable() const { return spin_.get(); }
    bool spinMatches(const Settings& s) const { return spin_ && spin_->spin() == s.spin && spin_->rC() == s.dist && spin_->thC() == s.pitch; }
    // a pixel of the spinning picture through the shown table: the disc's hit and its light as discPixel (lab.h); hit order -1 for none
    DiscPixel spinPixel(const Settings& s, const HoleView& v, double X, double Y, bool light = true) const;
    static constexpr double kSpinRest = 0.3;
    static constexpr double kSkyRest = 0.15;                    // 0.56.0: the view rests (the card's sky at full resolution) this long after its last change
    // 0.55.4: the eye's glare is made (light.h, on the processor: a fifth of a second at 1280 x 800 on 4 cores) once the view has rested for kGlareRest seconds;
    // while it moves the picture is drawn without it. glareNow: tests draw it at once.
    static constexpr double kGlareRest = 0.25;
    // 0.55.5: while the view moves the exposure is scanned on a grid of at most the points of 1280 x 800 in steps of 4 (its cost does not grow with the
    // picture); once the view has rested kGlareRest seconds, in steps of 4 everywhere (lab.h discLook's rule: the picture at rest is exactly as before)
    static int lookStep(double W, double H, bool exact);
    bool glareNow = false;
    // tests (PL_BH_SPINEDGE): n points of the shown table's shadow edge on the picture (X, Y in points) and the unit direction outward (right, down), 4 numbers each
    void spinEdgePoints(const Settings& s, double W, double H, double cx, double cy, int n, std::vector<double>& out) const;
private:
    struct Request;
    struct Picture;
    std::shared_ptr<const SpinTable> spin_;
    std::unique_ptr<SpinStars> spinStars_;
    std::uint64_t spinVer_ = 0;
    std::thread spinWorker_;
    std::atomic<bool> spinCancel_{false}, spinDone_{false};
    std::mutex spinMx_;
    std::shared_ptr<const SpinTable> spinReady_;
    double spinChangedAt_ = 0;
    bool spinFineStarted_ = false;
    std::shared_ptr<const std::vector<float>> discF_;
    double discFSpin_ = -1, discFLog0_ = 0, discFDl_ = 1;
    std::uint64_t discFVer_ = 0;
    std::vector<SpinImage> spinScratch_;
    void stopSpinWorker();
    void ensureDiscF(double spin);
    bool ensureSpinTable(const Settings& s, bool wait, const std::atomic<bool>* cancel);
    // 0.55.5: prepare: the processor's part of a picture (the tables, the exposure, the stars' images; false: a table fill was stopped); emit: hands it to
    // the renderer (restKey: the view asked for, for the glare's rest)
    bool prepare(const Settings& s, double W, double H, double cx, double cy, const std::atomic<bool>* cancel);
    bool prepareSpin(const Settings& s, double W, double H, double cx, double cy, const std::atomic<bool>* cancel);
    bool lit_ = false;                                         // something is lit (Sky: Black and no disc: the picture is black)
    bool exact_ = true, keyExact_ = false;                     // the exposure in steps of 4 (asked; made for key_)
    // 0.55.5, the worker (async): engine_ is a view of its own that only the worker thread touches; it hands finished Pictures back
    bool fineExternal_ = false;                                // (the engine) the fine table comes from the owner (fineOffer_), never a thread of its own
    std::shared_ptr<const SpinTable> fineOffer_;
    std::unique_ptr<BlackHoleView> engine_;
    std::thread worker_;
    std::mutex workMx_;
    std::condition_variable workCv_;
    bool workQuit_ = false, workBusy_ = false;
    std::unique_ptr<Request> req_;
    std::unique_ptr<Picture> done_;
    std::atomic<bool> stale_{false};                           // the view is closing: a table fill under way stops
    std::uint64_t reqSeq_ = 0;
    // the frame's side: the fine table (made on its own thread once the settings rest) and what was asked and shown
    std::shared_ptr<const SpinTable> fine_;
    double fineA_ = -1, fineR_ = -1, fineTh_ = -1;
    bool hasShown_ = false, shownExact_ = false, postedExact_ = false, posted_ = false;
    const SpinTable* postedFine_ = nullptr;
    Settings shownS_;
    double shownW_ = 0, shownH_ = 0, shownCx_ = 0, shownCy_ = 0, wantSince_ = 0, behindSince_ = 0;
    std::uint64_t shownSeq_ = 0;
    double made_[4] = {0, 0, 0, 0};
    void ensureWorker();
    void workLoop();
    // 0.55.6, the card's fill (frame thread)
    int cardState_ = 0;
    std::string cardWhy_;
    CardCheck cardCheck_;
    std::unique_ptr<CardFill> cardFill_;
    std::unique_ptr<CardStartup> cardStartup_;                 // 0.55.7: the start-up check, a step per frame
    int asyncDraws_ = 0, guardGen_ = 0;                        // 0.55.7: the card's path starts only once the first frames are on screen; "Try again" seen
    std::shared_ptr<const SpinTable> cardFine_;                // the last table the card filled (installed with a picture)
    std::weak_ptr<CardTableData> cardPosted_;                  // the last fill handed to the worker, its (a, r, th), and whether the worker used it
    std::shared_ptr<std::atomic<bool>> cardPostedUsed_;
    double cardPostedKey_[3] = {-1, -1, -1};
    bool cardReady(render::Renderer2D& R);
    // 0.56.0, the sky's path (frame thread): the steps (build 0..2, the probes, the check, the first picture), one per frame
    int skyState_ = 0, skyStep_ = 0, skyGen_ = 0, skyFrames_ = 0;
    std::string skyWhy_;
    SkyCheck skyCheck_;
    std::future<std::shared_ptr<const std::array<SkyRef, 2>>> skyRefJob_;   // 0.57.2: the check's reference, made on a thread of its own (skyReady)
    std::shared_ptr<const std::array<SkyRef, 2>> skyRefs_;
    std::string skyReadText_;
    std::vector<float> skyProbe_[2];
    double skyStepAt_ = 0;
    long skyTimed0_ = 0;
    bool skyFirst_ = false, skyUse_ = false, skyMoving_ = false, skyEngine_ = false;
    DiscLook skyLook_;                     // 0.70.0: the card's exposure as last read back (its brightest channel, Y and log-average; 0: none yet)
    double skyChangedAt_ = 0;
    long skyDrawn_ = 0, installs_ = 0;
    bool lookEngine_ = false, movingEngine_ = false, discCoarse_ = false, shownDiscCoarse_ = false, postedMoving_ = false, lookUse_ = false;
    std::shared_ptr<const std::vector<float>> skyF_;
    double skyFSpin_ = -1, skyFLog0_ = 0, skyFDl_ = 1;
    std::uint64_t skyFVer_ = 0;
    bool skyReady(render::Renderer2D& R);
    void skyOff(const std::string& why);
    void ensureSkyF(double spin);
    void cardOff(const std::string& why);                      // 0.55.7: the processor's path from now on in this run (said, logged, printed)
    void pumpCard(const Settings& s, double W, double H, double cx, double cy, bool start);
    void pollFine(const Settings& s);
    void install(Picture& p);
    void drawAsync(render::Renderer2D& R, const Settings& s, double W, double H, double cx, double cy);
private:
    std::shared_ptr<const DiscTable> disc_;
    double discDist_ = -1;
    int discGridOn_ = 0;
    std::uint64_t discVer_ = 0;
    std::shared_ptr<const LensTable> table_;
    double tableDist_ = -1;
    std::uint64_t tableVer_ = 0;
    struct Dot { float x, y, r; render::Color c; };
    bool addDot(const Settings& s, const HoleView& v, double X, double Y, double mag, const std::array<int, 3>& c);   // 0.55.4: one star image by the rule in force
    void trueNumbers(const Settings& s, const HoleView& v);
    std::vector<Dot> dots_;
    std::vector<StarImage> scratch_;
    struct Key { double W = -1, H, cx, cy, dist, fov, yaw, pitch; bool bend, sky; bool disc; double outer, feed, mass, exp; bool expAuto; double spin; std::uint64_t spinVer; int look; bool trueSky = false, glare = false; bool allLight = false; bool operator==(const Key& o) const; } key_;
    static Key keyOf(const Settings& s, double W, double H, double cx, double cy);   // 0.55.5: the view asked for (spinVer 0)
    Key wantKey_, shownKey_, postedKey_, skyKey_;
    struct WantNote { Key k; double last = 0; };                   // 0.83.0: the last views asked for and when each was last asked for (lagMs)
    std::array<WantNote, 256> wants_{};
    int wantsN_ = 0, wantsAt_ = 0;
    double shownWanted_ = -1;                                       //   when the shown picture's view was last asked for (-1: not among them)
    void emit(render::Renderer2D& R, const Settings& s, double W, double H, double cx, double cy, const Key& restKey);
    ViewStats st_;
    Key glareKey_;
    double glareChangedAt_ = 0;
    bool glareReady_ = false;
    void glareRest(const Key& k);
};

// Tests (PL_BH_COMPARE): the card's picture against the double-precision reference (lensPixel) for the distances given, the plain sky against the Orbit
// Lab's sky map, and the frame times; prints PLBHCMP / PLBHTIME lines. Needs the renderer's Vulkan device.
void compareHook(render::Renderer2D& R, const char* spec);
void toneCompare(render::Renderer2D& R);   // 0.70.0 (PL_BH_COMPARE=sky)
// 0.69.0: the tests' 8-bit pictures (compareHook, PL_BH_COMPARE): what the renderer recorded, drawn off screen and read back, RGBA, rows from the bottom;
// set by main.cpp (render/vk_backend.h renderToImage); false: not drawn
using TestShot = std::function<bool(render::Renderer2D&, int, int, std::vector<unsigned char>&)>;
TestShot& testShot();

}  // namespace blackhole
