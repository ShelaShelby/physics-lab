// 0.67.0: the helper process's side (app/gpu_helper.h): PhysicsLab --gpu-helper <folder> <device> program|variant <name>. No window: its own Vulkan device on the
// card the app chose, the program's pipelines made with the app's own SPIR-V (into the helpers' cache file), each step's line written and committed to the
// result file before it begins, so a driver that kills the process leaves the step it died in. Exit codes: 0 done (passed or the driver said no), 2 the
// device could not be made (the app then makes the program itself, guarded as before); anything else: it died.
#include "render/vk_helper.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <functional>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

#include "app/gpu_guard.h"
#include "app/gpu_helper.h"
#include "render/vk_card.h"
#include "render/vk_blackhole.h"
#include "render/vk_draw2d.h"
#include "render/lines_card.h"
#include "render/surface_card.h"
#include "render/classic_card.h"
#include "render/nucdens_card.h"
#include "render/ring_card.h"
#include "render/atom_card.h"
#include "render/porkchop_card.h"
#include "render/molecule_card.h"
#include "atom/classic_check.h"
#include "atom/nucdens_check.h"
#include "orbit/lines_check.h"
#include "molecule/surface_check.h"
#include <thread>

#include "r2d_kerrtable_comp.h"
#include "r2d_classic_comp.h"
#include "r2d_nucdens_comp.h"
#include "r2d_lines_comp.h"
#include "r2d_surface_comp.h"
#include "r2d_porkchop_comp.h"
#include "r2d_kerrvar_math.h"
#include "r2d_kerrvar_step.h"
#include "r2d_kerrvar_edge.h"
#include "r2d_kerrvar_ray.h"
#include "r2d_kerrvar_short.h"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace render {

namespace {
const char kMagic[8] = {'P', 'L', 'V', 'K', 'P', 'C', '1', 0};
std::uint64_t fnv(const char* p, std::size_t n) { std::uint64_t h = 1469598103934665603ULL; for (std::size_t i = 0; i < n; i++) { h ^= static_cast<unsigned char>(p[i]); h *= 1099511628211ULL; } return h; }
}  // namespace

bool vkReadCacheFile(const std::string& path, VkPhysicalDevice phys, std::vector<char>& data, std::string& note) {
    data.clear();
    std::ifstream in(path, std::ios::binary);
    std::vector<char> all((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (all.empty()) { note = "none yet"; return false; }
    std::uint64_t size = 0, hash = 0;
    if (all.size() >= 24) { std::memcpy(&size, all.data() + 8, 8); std::memcpy(&hash, all.data() + 16, 8); }
    VkPhysicalDeviceProperties p{};
    vkGetPhysicalDeviceProperties(phys, &p);
    VkPipelineCacheHeaderVersionOne hd{};
    if (all.size() >= 24 + sizeof hd) std::memcpy(&hd, all.data() + 24, sizeof hd);
    if (all.size() < 24 + sizeof hd || std::memcmp(all.data(), kMagic, 8) != 0 || size != all.size() - 24 || fnv(all.data() + 24, all.size() - 24) != hash) { note = "ignored: the file is damaged"; return false; }
    if (hd.headerVersion != VK_PIPELINE_CACHE_HEADER_VERSION_ONE || hd.vendorID != p.vendorID || hd.deviceID != p.deviceID || std::memcmp(hd.pipelineCacheUUID, p.pipelineCacheUUID, VK_UUID_SIZE) != 0) {
        note = "ignored: made by another card or driver"; return false;
    }
    data.assign(all.begin() + 24, all.end());
    note = "read, " + std::to_string(data.size()) + " bytes";
    return true;
}

void vkWriteCacheFile(const std::string& path, VkDevice dev, VkPipelineCache cache) {
    std::size_t n = 0;
    if (!cache || vkGetPipelineCacheData(dev, cache, &n, nullptr) != VK_SUCCESS || n == 0) return;
    std::vector<char> data(n);
    if (vkGetPipelineCacheData(dev, cache, &n, data.data()) < 0) return;
    data.resize(n);
    const std::uint64_t size = data.size(), h = fnv(data.data(), data.size());
    const std::string tmp = path + ".new";
    {   std::ofstream o(tmp, std::ios::binary | std::ios::trunc);
        if (!o) return;
        o.write(kMagic, 8);
        o.write(reinterpret_cast<const char*>(&size), 8);
        o.write(reinterpret_cast<const char*>(&h), 8);
        o.write(data.data(), static_cast<std::streamsize>(data.size())); }
    std::remove(path.c_str());
    std::rename(tmp.c_str(), path.c_str());
}

}  // namespace render

namespace app {

namespace {
std::string g_result, g_text;
void put(const std::string& line) { g_text += line + "\n"; writeCommitted(g_result, g_text); }   // (committed: a crash right after still leaves it)
double nowMs() { return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now().time_since_epoch()).count(); }

// 0.68.0: every guarded program's first steps, as the app makes them: its pipelines and its first small run. A step written "begin <step>" before it
// begins and "end <step>|ok|<ms>" or "end <step>|FAILED|<why>" after it; the programs that make themselves with their own guarded steps (the groups, the
// outline, the rocks, the Glow dots, the lens) have those steps written by the guards' observer (app/gpu_guard.h gpuGuardObserve)
struct Ctx { render::VkCards& cards; VkPipelineCache cache; bool f64; render::VkDraw2D* d2; };
struct Step { std::string name; std::function<bool(Ctx&, std::string&)> run; bool f64 = false; bool observed = false; };
const char* g_die = nullptr;
double g_stepAt = 0;
void dieCheck(const std::string& step) {                  // (tests: as if the driver had crashed inside that step; 0.68.0: several steps' starts, split by '|')
    if (!g_die || !*g_die) return;
    std::string all = g_die;
    for (std::size_t a = 0; a <= all.size();) {
        const std::size_t b = std::min(all.find('|', a), all.size());
        const std::string one = all.substr(a, b - a);
        if (!one.empty() && step.compare(0, one.size(), one) == 0) std::_Exit(3);
        a = b + 1;
    }
}
void observe(int what, const std::string& step, const std::string& result) {
    if (what == 0) { put("begin " + step); dieCheck(step); g_stepAt = nowMs(); return; }
    char ms[32];
    std::snprintf(ms, sizeof ms, "%.0f", nowMs() - g_stepAt);
    if (what == 1) put("end " + step + (result.rfind("FAILED", 0) == 0 ? "|FAILED|" + result : "|ok|" + std::string(ms)));
    else put("end " + step + "|FAILED|" + (what == 3 ? std::string("it did not finish in time") : result));
}
struct Steps : render::KerrSteps {                         // the steps a program's build tells (the Atom Lab's, the Density cloud's, the light paths')
    void begin(const std::string& step) override { observe(0, step, std::string()); }
    void end(const std::string& step, const std::string& result) override { observe(1, step, result); }
};
// a job polled until done (the first small runs), within 20 s
template <class F> bool waitJob(F poll) {
    const double t0 = nowMs();
    for (;;) {
        const render::JobState st = poll();
        if (st == render::JobState::Done) return true;
        if (st == render::JobState::Failed || nowMs() - t0 > 20000) return false;
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
}
Step kernel(const char* name, const std::uint32_t* w, std::size_t bytes, bool f64) {
    return {name, [w, bytes](Ctx& c, std::string& why) { render::VkCards::Kernel k; const bool ok = c.cards.makeKernel(w, bytes, k, why, c.cache); if (ok) c.cards.dropKernel(k); return ok; }, f64};
}
Step program(const char* name, render::VkDraw2D::Prog p) {
    return {name, [p](Ctx& c, std::string& why) { if (!c.d2) { why = "the helper's drawing programs did not start"; return false; } return c.d2->addProgram(p, why); }};
}

std::vector<Step> programsOf(const std::string& kind, const std::string& name) {
    if (kind == "variant") {
        if (name == "math") return {kernel("kKerrTableCS variant math (the double-precision functions alone)", k_r2d_kerrvar_math, sizeof k_r2d_kerrvar_math, true)};
        if (name == "step") return {kernel("kKerrTableCS variant step (a ray's start and one step, no loop)", k_r2d_kerrvar_step, sizeof k_r2d_kerrvar_step, true)};
        if (name == "edge") return {kernel("kKerrTableCS variant edge (the shadow's edge alone)", k_r2d_kerrvar_edge, sizeof k_r2d_kerrvar_edge, true)};
        if (name == "ray") return {kernel("kKerrTableCS variant ray (one whole ray with its end handling)", k_r2d_kerrvar_ray, sizeof k_r2d_kerrvar_ray, true)};
        if (name == "short") return {kernel("kKerrTableCS variant short (the whole shader, its step loops bounded at 64)", k_r2d_kerrvar_short, sizeof k_r2d_kerrvar_short, true)};
        return {};
    }
    using P = render::VkDraw2D;
    // the programs that make themselves with their own guarded steps (pipelines and first small run): written by the observer
    const auto self = [](std::function<bool(Ctx&, std::string&)> f) { Step s{"", std::move(f)}; s.observed = true; return s; };
    if (name == "groups") return {self([](Ctx& c, std::string& why) { render::IGroupCompute* g = render::makeVkGroupCompute(c.cards, why); delete g; return g != nullptr; })};
    if (name == "outline") return {self([](Ctx& c, std::string& why) { render::IOutlineCompute* o = render::makeVkOutlineCompute(c.cards, why); delete o; return o != nullptr; })};
    if (name == "rocks") return {self([](Ctx& c, std::string& why) { render::IRockCompute* r = render::makeVkRockCompute(c.cards, why); delete r; return r != nullptr; })};
    if (name == "cloud") return {self([](Ctx& c, std::string& why) { render::VkGlow* g = render::makeVkGlow(c.cards, why); render::dropVkGlow(g); return g != nullptr; })};
    if (name == "lens") return {self([](Ctx& c, std::string& why) { render::VkBlackHole bh; const bool ok = bh.init(c.cards, std::string(), why); bh.destroy(); return ok; })};
    if (name == "slice" || name == "jump") {
        const int which = name == "slice" ? render::AtomCard::kSlice : render::AtomCard::kJump;
        std::vector<Step> v = {self([which](Ctx& c, std::string& why) { render::AtomCard* a = render::makeVkAtomCard(c.cards); Steps st; const bool ok = a->build(which, &st, why); delete a; return ok; })};
        if (which == render::AtomCard::kSlice)
            v.push_back({"the Slice's first small run", [](Ctx& c, std::string& why) {   // 64 attempts on a plane through an s state (a constant angular part)
                render::AtomCard* a = render::makeVkAtomCard(c.cards);
                Steps st;
                bool ok = a->build(render::AtomCard::kSlice, &st, why);
                a->setSliceReady(true);
                render::SliceDotsJob jb;
                jb.seed = 1; jb.tries = 64; jb.axis = 1; jb.scale = 1; jb.bound = 1; jb.ang = 0; jb.nh = 1; jb.h[0] = 1; jb.c[0] = 1;
                std::vector<float> res;
                const int j = ok ? a->beginSlice(jb) : 0;
                ok = j > 0 && waitJob([&] { return a->pollSlice(j, res); });
                if (!ok && why.empty()) why = "the run did not finish";
                delete a;
                return ok; }});
        if (which == render::AtomCard::kJump)
            v.push_back({"the jump's first small run", [](Ctx& c, std::string& why) {     // two dots, each with its old and new group: the groups of the moment
                render::AtomCard* a = render::makeVkAtomCard(c.cards);
                Steps st;
                bool ok = a->build(render::AtomCard::kJump, &st, why);
                const unsigned char fixed[4] = {1, 2, 3, 4};
                std::vector<unsigned char> out;
                const int j = ok ? a->beginJumpRead(fixed, 2, 0, 1) : 0;
                ok = j > 0 && waitJob([&] { return a->pollJumpRead(j, out); });
                if (!ok && why.empty()) why = "the run did not finish";
                delete a;
                return ok; }});
        return v;
    }
    if (name == "molcloud") return {self([](Ctx& c, std::string& why) { render::MoleculeCard* m = render::makeVkMoleculeCard(c.cards); Steps st; const bool ok = m->build(&st, why); delete m; return ok; }),
                                    {"the Density cloud's first small run", [](Ctx& c, std::string& why) {   // three dots projected
                                        render::MoleculeCard* m = render::makeVkMoleculeCard(c.cards);
                                        Steps st;
                                        bool ok = m->build(&st, why);
                                        const float pts[9] = {0, 0, 0, 1, 0, 0, 0, 1, 0};
                                        render::MolCloudCam cam;
                                        std::vector<float> out;
                                        const int j = ok ? m->beginProject(pts, 3, cam) : 0;
                                        ok = j > 0 && waitJob([&] { return m->pollProject(j, out); });
                                        if (!ok && why.empty()) why = "the run did not finish";
                                        delete m;
                                        return ok; }}};
    if (name == "lightpaths") return {kernel("the light paths' pipeline", k_r2d_kerrtable_comp, sizeof k_r2d_kerrtable_comp, true)};
    if (name == "sky") return {self([](Ctx& c, std::string& why) {   // the three stages' pipelines in turn (their steps told by the black hole's worker)
        render::VkBlackHole bh;
        if (!bh.init(c.cards, std::string(), why)) return false;
        Steps st;
        int r = 1;
        const double t0 = nowMs();
        for (int s = 0; s < 3 && r == 1; s++)
            while ((r = bh.buildSky(s, &st, why)) == 0 && nowMs() - t0 < 120000) std::this_thread::sleep_for(std::chrono::milliseconds(5));
        bh.destroy();
        if (r == 0) why = "it did not finish in time";
        return r == 1; })};
    if (name == "rings") return {{"the rings' pipelines", [](Ctx& c, std::string& why) { render::RingCard* r = render::makeVkRingCard(c.cards); const bool ok = r->build(why); delete r; return ok; }},
                                 {"the rings' first small run", [](Ctx& c, std::string& why) {   // the probe at three points of a plain ring
                                     render::RingCard* r = render::makeVkRingCard(c.cards);
                                     bool ok = r->build(why);
                                     render::RingsDraw d;
                                     std::vector<float> rgba;
                                     const int j = ok ? r->beginProbe(d, {10, 10, 20, 20, 30, 30}) : 0;
                                     ok = j > 0 && waitJob([&] { return r->pollProbe(j, rgba); });
                                     if (!ok && why.empty()) why = "the run did not finish";
                                     delete r;
                                     return ok; }}};
    if (name == "classic") return {kernel("the Classic dots' pipeline", k_r2d_classic_comp, sizeof k_r2d_classic_comp, false),
                                   {"the Classic dots' first small run", [](Ctx& c, std::string& why) {   // the start-up check's first case
                                       render::ClassicCard* k = render::makeVkClassicCard(c.cards);
                                       bool ok = k->build(why);
                                       const atom::ClassicCase cc = atom::classicCheckCase();
                                       std::vector<std::uint32_t> keys;
                                       std::vector<float> sq;
                                       long total = 0;
                                       const int j = ok ? k->beginCheck(atom::classicCaseDraw(cc)) : 0;
                                       ok = j > 0 && waitJob([&] { return k->pollCheck(j, keys, sq, total); });
                                       if (!ok && why.empty()) why = "the run did not finish";
                                       delete k;
                                       return ok; }}};
    if (name == "porkchop") return {kernel("the porkchop grid's pipeline", k_r2d_porkchop_comp, sizeof k_r2d_porkchop_comp, false),   // 0.92.0
                                    {"the porkchop grid's first small run", [](Ctx& c, std::string& why) {   // two departures and two arrivals of Earth and Mars
                                        render::PorkchopCard* k = render::makeVkPorkchopCard(c.cards);
                                        bool ok = k->build(nullptr, why);
                                        render::PorkchopJob j;
                                        j.nDep = j.nArr = 2; j.off = 200; j.depStep = 10; j.arrStep = 10; j.tu = 58.13244f; j.vk = 29.78469f;
                                        j.dep = {1.0f, 0.0f, 0.0f, 0, 0.0f, 1.0f, 0.0f, 0, 0.98f, 0.17f, 0.0f, 0, -0.17f, 0.98f, 0.0f, 0};
                                        j.arr = {-1.2f, 1.0f, 0.0f, 0, -0.65f, -0.8f, 0.0f, 0, -1.4f, 0.8f, 0.0f, 0, -0.5f, -0.9f, 0.0f, 0};
                                        std::vector<float> res;
                                        const int id = ok ? k->begin(j) : 0;
                                        ok = id > 0 && waitJob([&] { return k->poll(id, res); });
                                        if (!ok && why.empty()) why = "the run did not finish";
                                        delete k;
                                        return ok; }}};
    if (name == "nucdensity") return {kernel("the nucleus density's pipeline", k_r2d_nucdens_comp, sizeof k_r2d_nucdens_comp, false),
                                      {"the nucleus density's first small run", [](Ctx& c, std::string& why) {   // the start-up check's first case
                                          render::NucDensCard* k = render::makeVkNucDensCard(c.cards);
                                          bool ok = k->build(why);
                                          const std::vector<atom::NucDensCase> cs = atom::nucDensCheckCases();
                                          std::vector<float> table, grid;
                                          const float col[4] = {1, 1, 1, 1};
                                          const int j = ok && !cs.empty() ? k->beginCheck(atom::nucDensDraw(cs[0].d, cs[0].e1, cs[0].e2, cs[0].e3, cs[0].G, cs[0].cellFm, table, 1, 0, 0, 1, col)) : 0;
                                          ok = j > 0 && waitJob([&] { return k->pollCheck(j, grid); });
                                          if (!ok && why.empty()) why = "the run did not finish";
                                          delete k;
                                          return ok; }}};
    if (name == "lines") return {kernel("the lines' pipelines", k_r2d_lines_comp, sizeof k_r2d_lines_comp, false), program("the lines' drawing program", P::kLine),
                                 {"the lines' first small run", [](Ctx& c, std::string& why) {   // the start-up check's first case
                                     render::LinesCard* k = render::makeVkLinesCard(c.cards);
                                     bool ok = k->build(why);
                                     const std::vector<orbit::LinesCase> cs = orbit::linesCheckCases();
                                     render::LinesDraw d = orbit::linesDrawOf(cs[0]);
                                     d.checkRoom = 4 * d.pieces + 65536;
                                     std::vector<float> ends, pts;
                                     std::vector<std::uint32_t> offs;
                                     const int j = ok ? k->beginCheck(d) : 0;
                                     ok = j > 0 && waitJob([&] { return k->pollCheck(j, ends, offs, pts); });
                                     if (!ok && why.empty()) why = "the run did not finish";
                                     delete k;
                                     return ok; }}};
    if (name == "surface") return {kernel("the surface's pipelines", k_r2d_surface_comp, sizeof k_r2d_surface_comp, false), program("the surface's drawing program", P::kSurf),
                                   {"the surface's first small run", [](Ctx& c, std::string& why) {   // the start-up check's first case
                                       render::SurfaceCard* k = render::makeVkSurfaceCard(c.cards);
                                       bool ok = k->build(why);
                                       const std::vector<molecule::SurfaceCase> cs = molecule::surfaceCheckCases();
                                       std::vector<float> verts;
                                       std::vector<std::uint32_t> order;
                                       const int j = ok && !cs.empty() ? k->beginCheck(molecule::surfaceDrawOf(cs[0])) : 0;
                                       ok = j > 0 && waitJob([&] { return k->pollCheck(j, verts, order); });
                                       if (!ok && why.empty()) why = "the run did not finish";
                                       delete k;
                                       return ok; }}};
    return {};
}
}  // namespace

bool gpuHelperKnows(const std::string& name) { return !programsOf("program", name).empty(); }

int gpuHelperMain(int argc, char** argv) {
    if (argc < 6) return 2;
#ifdef _WIN32
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);          // a driver's crash ends the helper quietly (no Windows error box to wait for)
#endif
    const std::string folder = argv[2], device = argv[3], kind = argv[4], name = argv[5];
    g_result = folder + "gpu-helper-result.txt";
    const std::vector<Step> progs = programsOf(kind, name);
    g_die = std::getenv("PL_GPU_DIE");
    if (const char* h = std::getenv("PL_GPU_HANG"))       // 0.68.1 (test switch): this helper (a variant's or a program's name, '|' between several) never answers: the app's time limit ends it
        for (std::stringstream hs(h); hs.good();) { std::string one; std::getline(hs, one, '|'); if (one == name) for (;;) std::this_thread::sleep_for(std::chrono::seconds(1)); }
    if (const char* q = std::getenv("PL_GPU_NORESULT"))   // 0.68.1 (test switch): this helper ends at once with exit code 7 and writes nothing
        for (std::stringstream hs(q); hs.good();) { std::string one; std::getline(hs, one, '|'); if (one == name) return 7; }
    gpuGuardObserve(observe);
    if (progs.empty()) { put("no such program: " + name); return 2; }
    if (volkInitialize() != VK_SUCCESS) { put("no Vulkan loader"); return 2; }
    VkApplicationInfo ai{};
    ai.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    ai.pApplicationName = "Physics Lab (helper)";
    ai.apiVersion = VK_API_VERSION_1_3;
    VkInstanceCreateInfo ii{};
    ii.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    ii.pApplicationInfo = &ai;
    VkInstance inst = VK_NULL_HANDLE;
    if (vkCreateInstance(&ii, nullptr, &inst) != VK_SUCCESS) { put("vkCreateInstance failed"); return 2; }
    volkLoadInstance(inst);
    std::uint32_t nd = 0;
    vkEnumeratePhysicalDevices(inst, &nd, nullptr);
    std::vector<VkPhysicalDevice> devs(nd);
    vkEnumeratePhysicalDevices(inst, &nd, devs.data());
    VkPhysicalDevice phys = VK_NULL_HANDLE;
    std::uint32_t fam = 0;
    for (int pass = 0; pass < 2 && !phys; pass++)                           // the app's card by its name first, else the first that computes
        for (VkPhysicalDevice d : devs) {
            VkPhysicalDeviceProperties p{};
            vkGetPhysicalDeviceProperties(d, &p);
            if (pass == 0 && device != p.deviceName) continue;
            std::uint32_t nq = 0;
            vkGetPhysicalDeviceQueueFamilyProperties(d, &nq, nullptr);
            std::vector<VkQueueFamilyProperties> qf(nq);
            vkGetPhysicalDeviceQueueFamilyProperties(d, &nq, qf.data());
            for (std::uint32_t q = 0; q < nq && !phys; q++)
                if ((qf[q].queueFlags & VK_QUEUE_GRAPHICS_BIT) && (qf[q].queueFlags & VK_QUEUE_COMPUTE_BIT)) { phys = d; fam = q; }
            if (phys) break;
        }
    if (!phys) { put("no Vulkan device with a graphics and compute queue"); vkDestroyInstance(inst, nullptr); return 2; }
    VkPhysicalDeviceFeatures have{}, want{};
    vkGetPhysicalDeviceFeatures(phys, &have);
    const bool f64 = have.shaderFloat64 == VK_TRUE;
    want.shaderFloat64 = have.shaderFloat64;
    const float prio = 1.0f;
    VkDeviceQueueCreateInfo qi{};
    qi.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    qi.queueFamilyIndex = fam; qi.queueCount = 1; qi.pQueuePriorities = &prio;
    VkPhysicalDeviceVulkan13Features f13{};
    f13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    f13.dynamicRendering = VK_TRUE; f13.synchronization2 = VK_TRUE;
    VkDeviceCreateInfo di{};
    di.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    di.pNext = &f13; di.queueCreateInfoCount = 1; di.pQueueCreateInfos = &qi; di.pEnabledFeatures = &want;
    VkDevice dev = VK_NULL_HANDLE;
    if (vkCreateDevice(phys, &di, nullptr, &dev) != VK_SUCCESS) { put("vkCreateDevice failed"); vkDestroyInstance(inst, nullptr); return 2; }
    volkLoadDevice(dev);
    VkPhysicalDeviceProperties props{};
    vkGetPhysicalDeviceProperties(phys, &props);
    put(std::string("device ") + props.deviceName);
    render::VkDevCtx c;
    c.phys = phys; c.device = dev; c.limits = props.limits; c.family = fam; c.compute = true;
    vkGetDeviceQueue(dev, fam, 0, &c.queue);
    vkGetPhysicalDeviceMemoryProperties(phys, &c.mem);
    c.check = [](VkResult r, const char* what) { if (r >= 0) return true; put(std::string("a Vulkan call failed: ") + what); return false; };
    // 0.68.0: one-time commands (the drawing programs' start uploads its white texture): a command buffer submitted and waited for
    VkCommandPool pool = VK_NULL_HANDLE;
    {   VkCommandPoolCreateInfo pi{};
        pi.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        pi.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        pi.queueFamilyIndex = fam;
        if (vkCreateCommandPool(dev, &pi, nullptr, &pool) != VK_SUCCESS) pool = VK_NULL_HANDLE; }
    c.once = [dev, pool, &c](const std::function<void(VkCommandBuffer)>& rec) {
        if (!pool) return false;
        VkCommandBufferAllocateInfo ai{};
        ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        ai.commandPool = pool; ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; ai.commandBufferCount = 1;
        VkCommandBuffer cb = VK_NULL_HANDLE;
        if (vkAllocateCommandBuffers(dev, &ai, &cb) != VK_SUCCESS) return false;
        VkCommandBufferBeginInfo bi{};
        bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        bool ok = vkBeginCommandBuffer(cb, &bi) == VK_SUCCESS;
        if (ok) { rec(cb); ok = vkEndCommandBuffer(cb) == VK_SUCCESS; }
        VkSubmitInfo si{};
        si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        si.commandBufferCount = 1; si.pCommandBuffers = &cb;
        if (ok) ok = vkQueueSubmit(c.queue, 1, &si, VK_NULL_HANDLE) == VK_SUCCESS && vkQueueWaitIdle(c.queue) == VK_SUCCESS;
        vkFreeCommandBuffers(dev, pool, 1, &cb);
        return ok;
    };
    int code = 0;
    {
        render::VkCards cards;
        std::string why;
        render::VkDraw2D d2;                                                  // 0.68.0: the drawing programs (the Glow dots', the lines', the surface's ...)
        const bool drawing = kind != "variant" && d2.init(c, VK_FORMAT_B8G8R8A8_UNORM, 4, why);
        if (kind != "variant" && !drawing) put("the drawing programs did not start in the helper: " + why);
        if (!cards.init(c, drawing ? &d2 : nullptr, f64, why)) { put("the card programs' engine did not start: " + why); code = 2; }
        // the helpers' own cache file (the app merges it into its own: render/vk_blackhole.cpp mergeCache)
        const std::string cachePath = gpuHelperCacheFile(folder);
        std::vector<char> data;
        std::string note;
        render::vkReadCacheFile(cachePath, phys, data, note);
        VkPipelineCacheCreateInfo ci{};
        ci.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
        ci.initialDataSize = data.size(); ci.pInitialData = data.empty() ? nullptr : data.data();
        VkPipelineCache cache = VK_NULL_HANDLE;
        if (vkCreatePipelineCache(dev, &ci, nullptr, &cache) != VK_SUCCESS) { ci.initialDataSize = 0; ci.pInitialData = nullptr; if (vkCreatePipelineCache(dev, &ci, nullptr, &cache) != VK_SUCCESS) cache = VK_NULL_HANDLE; }
        Ctx ctx{cards, cache, f64, drawing ? &d2 : nullptr};
        for (const Step& p : progs) {
            if (code) break;
            if (p.f64 && !f64) { put("begin " + p.name); put("end " + p.name + "|FAILED|this card has no double precision in its shaders"); continue; }
            if (p.observed) { why.clear(); p.run(ctx, why); continue; }   // (its own guarded steps were written as they began and ended)
            put("begin " + p.name);
            dieCheck(p.name);
            const double t0 = nowMs();
            why.clear();
            const bool ok = p.run(ctx, why);
            char ms[32];
            std::snprintf(ms, sizeof ms, "%.0f", nowMs() - t0);
            put("end " + p.name + (ok ? "|ok|" + std::string(ms) : "|FAILED|" + why));
        }
        if (cache) { if (!code) render::vkWriteCacheFile(cachePath, dev, cache); vkDestroyPipelineCache(dev, cache, nullptr); }
        cards.destroy();
        d2.destroy();
    }
    if (pool) vkDestroyCommandPool(dev, pool, nullptr);
    vkDestroyDevice(dev, nullptr);
    vkDestroyInstance(inst, nullptr);
    if (!code) put("done");
    return code;
}

}  // namespace app
