// 0.13 stage 4, part C: the Cloud 2D view's cost (included by test_atom.cpp after test_outline_norm.inc). WRITTEN BEFORE the code (the functions and accessors used here did not exist yet).
// MEASURED CAUSE (REPORT.md): the side view collected its 48,000 blobs (80 bytes each) in a vector that was made new every frame and grew by doubling (about 17 reallocations, each copying and
// touching fresh memory), where the Cloud 3D path reserved once; and both views then copied the 3.8 MB again into the renderer. The fix must change NO blob: the checks compare with the old
// loop written out here, and prove that a reused vector does not allocate. (That the renderer takes the blobs without copying is checked in tests/gl_compare.cpp: it needs the renderer.)
using BlobT = render::Renderer2D::Blob;

static bool c4SameBlob(const BlobT& a, const BlobT& b) {
    return a.x == b.x && a.y == b.y && a.r == b.r && a.kind == b.kind && a.fx == b.fx && a.fy == b.fy && a.f0 == b.f0 && a.s1 == b.s1 &&
           gcSameColour(a.c0, b.c0) && gcSameColour(a.c1, b.c1) && gcSameColour(a.c2, b.c2);
}

static void testCloudBuffers() {
    AtomLab L;
    gcFill(L, "4f", "z3");
    L.setParam("view", std::string("Cloud"));                              // the side view (scene 10)
    for (int i = 0; i < 40000 && L.s.k < L.cloudCap(); i++) L.step(1.0 / 240);
    const double w = 1040, h = 713, m = std::min(w, h), pxm = L.pxPerM(m, L.s.szCur) * std::pow(10, L.p.zoom);
    const long cnt = std::min<long>(L.s.k, L.cloudCap());
    checkTrue("side view blobs: the scene has 48,000 dots", cnt == 48000);
    const render::Color plus = render::hexColor("#6c9cff"), minus = render::hexColor("#f08a3c");
    for (bool gpuColours : {true, false}) {
        const std::string tag = std::string("side view blobs, GPU colours ") + (gpuColours ? "On" : "Off");
        const DotColours dc = makeDotColours(gpuColours, true, false, plus, minus);
        std::vector<unsigned char> grp(static_cast<std::size_t>(cnt));
        plainGroups(true, L.s.ph.data(), L.s.sg.data(), cnt, grp.data());
        std::vector<SideDot> side;
        sideDots(L.s.pts.data(), cnt, grp.data(), dc.n, w / 2, h / 2, pxm, w, h, gpuColours, side);
        const double weight = 0.3 * cloudDotWeight(cnt);
        // the old loop of atom_view.cpp, as it was: a vector made empty, one push_back per dot
        std::vector<BlobT> old;
        for (const SideDot& d : side) old.push_back(squareDot(dc, d.x, d.y, 1, d.g, weight));
        std::vector<BlobT> now;
        sideBlobs(side, dc, weight, now);
        bool same = now.size() == old.size();
        for (std::size_t i = 0; same && i < old.size(); i++) same = c4SameBlob(now[i], old[i]);
        checkTrue(tag + ": sideBlobs makes the blobs of the old loop, one per dot, in the same order (" + std::to_string(old.size()) + ")", same && old.size() > 40000);
        // a vector kept from frame to frame: the second and third frame must not allocate (the data pointer and the capacity stay) and must give the same blobs
        const BlobT* data0 = now.data(); const std::size_t cap0 = now.capacity();
        sideBlobs(side, dc, weight, now);
        sideBlobs(side, dc, weight, now);
        bool again = now.size() == old.size();
        for (std::size_t i = 0; again && i < old.size(); i++) again = c4SameBlob(now[i], old[i]);
        checkTrue(tag + ": a kept vector is not reallocated by the next frames (same buffer, same capacity) and gives the same blobs", again && now.data() == data0 && now.capacity() == cap0);
        // the first call reserves the whole list at once (the old way grew by doubling: its capacity went through about 17 sizes)
        std::vector<BlobT> fresh;
        sideBlobs(side, dc, weight, fresh);
        checkTrue(tag + ": the first call reserves the whole list in one step (capacity is exactly the number of dots)", fresh.capacity() == side.size());
    }
}
