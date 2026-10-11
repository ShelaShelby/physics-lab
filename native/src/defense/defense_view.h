// Defense Lab (0.17 stage 1): the picture. 2D top-down with fake depth (soft shadows, a lit top and a darker front face), NOT isometric and NOT 3D.
// Speed guard: the terrain of each chunk is a mesh made once and kept on the GPU (one draw call a chunk, only the camera's numbers change per frame); belts and
// buildings go into the renderer's one triangle batch, belt items into one square batch per item kind. Nothing is allocated per frame once the buffers have grown.
#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>

#include "app/prefs.h"
#include "defense/defense_lab.h"
#include "render/mesh.h"

struct ImDrawList;
namespace render { class Renderer2D; }

namespace defense {

struct ViewStats { int chunks = 0, meshesBuilt = 0, meshesAlive = 0, entitiesDrawn = 0, items = 0; double cpuMs = 0; };

class DefenseView {
public:
    void draw(render::Renderer2D& r, ImDrawList* text, double uiScale, DefenseLab& lab, double w, double h, const app::ThemeColors& c);
    const ViewStats& stats() const { return stats_; }

private:
    struct ChunkMesh { int id = -1, count = 0; uint32_t version = 0; uint32_t seed = 0; long lastFrame = 0; };
    void buildChunk(World& w, int cx, int cy);
    std::unordered_map<int64_t, ChunkMesh> meshes_;
    std::vector<render::MeshVertex> build_;
    std::vector<float> itemXY_[kItemCount];
    std::vector<int> poles_;
    long frame_ = 0;
    ViewStats stats_;
};

}  // namespace defense
