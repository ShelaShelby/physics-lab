#include "svg_icon.h"

#include <algorithm>
#include <cctype>
#include <cfloat>
#include <cmath>
#include <cstdlib>
#include <map>
#include <vector>

#include "fonts.h"

namespace ui {

namespace {

std::string svgAttr(const std::string& tag, const char* name) {
    const std::string key = std::string(" ") + name + "=\"";
    const std::size_t a = tag.find(key);
    if (a == std::string::npos) return "";
    const std::size_t b = tag.find('"', a + key.size());
    return tag.substr(a + key.size(), b - a - key.size());
}

ImVec4 svgColor(const std::string& c) {
    if (c.size() == 7 && c[0] == '#') {
        const unsigned long v = std::strtoul(c.c_str() + 1, nullptr, 16);
        return ImVec4(((v >> 16) & 255) / 255.f, ((v >> 8) & 255) / 255.f, (v & 255) / 255.f, 1);
    }
    if (c == "#fff") return ImVec4(1, 1, 1, 1);
    return ImVec4(0.5f, 0.5f, 0.5f, 1);
}

// "rotate(-18 28 28)" -> angle in radians and the centre; false if there is no rotation
bool parseRotate(const std::string& t, double& a, double& cx, double& cy) {
    const std::size_t p = t.find("rotate(");
    if (p == std::string::npos) return false;
    const char* s = t.c_str() + p + 7;
    char* end = nullptr;
    a = std::strtod(s, &end) * 3.14159265358979 / 180;
    cx = std::strtod(end, &end);
    cy = std::strtod(end, &end);
    return true;
}

// Strokes a line of points; with a dash pattern ("2 3": dash, gap, in SVG units) only the dashes.
void strokeLine(ImDrawList* dl, const std::vector<ImVec2>& pts, bool closed, ImU32 col, float width, const std::vector<float>& dash, float scale) {
    if (pts.size() < 2) return;
    if (dash.empty()) { dl->AddPolyline(pts.data(), int(pts.size()), col, closed ? ImDrawFlags_Closed : 0, width); return; }
    std::vector<ImVec2> p = pts;
    if (closed) p.push_back(pts.front());
    std::size_t di = 0;                                  // which part of the pattern we are in; even = dash
    double left = dash[0] * scale;
    for (std::size_t i = 0; i + 1 < p.size(); i++) {
        ImVec2 a = p[i];
        const ImVec2 b = p[i + 1];
        double seg = std::hypot(b.x - a.x, b.y - a.y);
        const double ux = seg > 0 ? (b.x - a.x) / seg : 0, uy = seg > 0 ? (b.y - a.y) / seg : 0;
        while (seg > 1e-6) {
            const double step = std::min(seg, left);
            const ImVec2 e(float(a.x + ux * step), float(a.y + uy * step));
            if (di % 2 == 0) dl->AddLine(a, e, col, width);
            a = e; seg -= step; left -= step;
            if (left <= 1e-6) { di++; left = std::max(0.05f, dash[di % dash.size()]) * scale; }
        }
    }
}

std::vector<float> parseDash(const std::string& s) {
    std::vector<float> out;
    const char* p = s.c_str();
    char* end = nullptr;
    while (*p) {
        while (*p == ' ' || *p == ',') p++;
        if (!*p) break;
        out.push_back(float(std::strtod(p, &end)));
        if (end == p) break;
        p = end;
    }
    if (out.size() % 2) out.insert(out.end(), out.begin(), out.end());   // SVG: an odd list is repeated
    return out;
}

}  // namespace

void svgIcon(const std::string& svg, ImVec2 o, float scale) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    std::map<std::string, std::vector<std::pair<float, ImVec4>>> grads;   // id -> stops (offset, colour with opacity)
    std::string curGrad;
    for (std::size_t i = 0; i < svg.size();) {
        const std::size_t a = svg.find('<', i);
        if (a == std::string::npos) break;
        const std::size_t b = svg.find('>', a);
        const std::string tag = svg.substr(a, b - a + 1);
        i = b + 1;
        const std::string nm = tag.substr(1, tag.find_first_of(" />", 1) - 1);
        auto num = [&](const char* k, double d = 0) { const std::string v = svgAttr(tag, k); return v.empty() ? d : std::atof(v.c_str()); };
        double ra = 0, rcx = 0, rcy = 0;
        const bool rotated = parseRotate(svgAttr(tag, "transform"), ra, rcx, rcy);
        auto P = [&](double x, double y) {                // a point of this element, turned by its transform, in screen points
            if (rotated) {
                const double dx = x - rcx, dy = y - rcy;
                x = rcx + dx * std::cos(ra) - dy * std::sin(ra);
                y = rcy + dx * std::sin(ra) + dy * std::cos(ra);
            }
            return ImVec2(o.x + float(x) * scale, o.y + float(y) * scale);
        };
        if (nm == "radialGradient") { curGrad = svgAttr(tag, "id"); grads[curGrad].clear(); continue; }
        if (nm == "stop") {
            ImVec4 c = svgColor(svgAttr(tag, "stop-color"));
            c.w = float(num("stop-opacity", 1));
            grads[curGrad].push_back({float(num("offset")), c});
            continue;
        }
        const float op = float(num("opacity", 1));
        const std::string fill = svgAttr(tag, "fill"), stroke = svgAttr(tag, "stroke");
        const float sw = float(num("stroke-width", 1)) * scale;
        const std::vector<float> dash = parseDash(svgAttr(tag, "stroke-dasharray"));
        auto col = [&](const std::string& c) { ImVec4 v = svgColor(c); v.w *= op; return ImGui::ColorConvertFloat4ToU32(v); };
        const bool hasFill = !fill.empty() && fill != "none";
        if (nm == "circle" || nm == "ellipse") {
            const double cx = num("cx"), cy = num("cy"), rx = nm == "circle" ? num("r") : num("rx"), ry = nm == "circle" ? num("r") : num("ry");
            const ImVec2 c = P(cx, cy);
            const float r = float(rx) * scale;
            if (fill.rfind("url(#", 0) == 0) {                            // a radial gradient: rings from the outside in
                const auto& st = grads[fill.substr(5, fill.size() - 6)];
                if (st.size() >= 2)
                    for (int k = 24; k >= 1; k--) {
                        const float t = k / 24.f;
                        const auto& s0 = st.front(); const auto& s1 = st.back();
                        const float f = std::max(0.f, std::min(1.f, (t - s0.first) / std::max(1e-3f, s1.first - s0.first)));
                        ImVec4 v(s0.second.x + (s1.second.x - s0.second.x) * f, s0.second.y + (s1.second.y - s0.second.y) * f, s0.second.z + (s1.second.z - s0.second.z) * f, s0.second.w + (s1.second.w - s0.second.w) * f);
                        dl->AddCircleFilled(c, r * t, ImGui::ColorConvertFloat4ToU32(v));
                    }
            } else if (nm == "circle" && !rotated && dash.empty()) {
                if (hasFill) dl->AddCircleFilled(c, r, col(fill));
                else if (fill.empty() && stroke.empty()) dl->AddCircleFilled(c, r, col("#000000"));
                if (!stroke.empty()) dl->AddCircle(c, r, col(stroke), 0, sw);
            } else {                                                      // an ellipse (turned), or a dashed circle: as a many-sided shape
                const int n = 72;
                std::vector<ImVec2> pts;
                for (int k = 0; k < n; k++) pts.push_back(P(cx + rx * std::cos(k * 6.2831853 / n), cy + ry * std::sin(k * 6.2831853 / n)));
                if (hasFill) dl->AddConvexPolyFilled(pts.data(), n, col(fill));
                else if (fill.empty() && stroke.empty()) dl->AddConvexPolyFilled(pts.data(), n, col("#000000"));
                if (!stroke.empty()) strokeLine(dl, pts, true, col(stroke), sw, dash, scale);
            }
        } else if (nm == "rect") {
            const ImVec2 p0 = P(num("x"), num("y")), p1 = P(num("x") + num("width"), num("y") + num("height"));
            if (hasFill) dl->AddRectFilled(p0, p1, col(fill));
            if (!stroke.empty()) dl->AddRect(p0, p1, col(stroke), 0, 0, sw);
        } else if (nm == "path") {
            const std::string d = svgAttr(tag, "d");
            std::vector<std::vector<ImVec2>> subs;           // each M starts a new line
            std::vector<bool> closed;
            std::vector<double> nums;
            char cmd = 0;
            double cx = 0, cy = 0, qx = 0, qy = 0;
            std::size_t k = 0;
            auto readNums = [&](int n) {
                nums.clear();
                while ((int)nums.size() < n && k < d.size()) {
                    while (k < d.size() && (d[k] == ' ' || d[k] == ',')) k++;
                    char* end = nullptr;
                    nums.push_back(std::strtod(d.c_str() + k, &end));
                    if (end == d.c_str() + k) { k = d.size(); break; }
                    k = end - d.c_str();
                }
                while ((int)nums.size() < n) nums.push_back(0);
            };
            auto quad = [&](double x1, double y1, double x2, double y2) {   // a quadratic curve from the current point, in 12 steps
                for (int s = 1; s <= 12; s++) {
                    const double t = s / 12.0, u = 1 - t;
                    subs.back().push_back(P(u * u * cx + 2 * u * t * x1 + t * t * x2, u * u * cy + 2 * u * t * y1 + t * t * y2));
                }
                qx = x1; qy = y1; cx = x2; cy = y2;
            };
            while (k < d.size()) {
                while (k < d.size() && (d[k] == ' ' || d[k] == ',')) k++;
                if (k >= d.size()) break;
                if (std::isalpha(static_cast<unsigned char>(d[k]))) cmd = d[k++];
                if (cmd == 'M') { readNums(2); cx = nums[0]; cy = nums[1]; subs.push_back({P(cx, cy)}); closed.push_back(false); cmd = 'L'; }
                else if (cmd == 'L' && !subs.empty()) { readNums(2); cx = nums[0]; cy = nums[1]; subs.back().push_back(P(cx, cy)); }
                else if (cmd == 'Q' && !subs.empty()) { readNums(4); quad(nums[0], nums[1], nums[2], nums[3]); }
                else if (cmd == 'T' && !subs.empty()) { readNums(2); quad(2 * cx - qx, 2 * cy - qy, nums[0], nums[1]); }
                else if (cmd == 'Z' || cmd == 'z') { if (!closed.empty()) closed.back() = true; cmd = 0; }
                else break;
            }
            for (std::size_t s = 0; s < subs.size(); s++) {
                if (hasFill && subs[s].size() >= 3) dl->AddConcavePolyFilled(subs[s].data(), int(subs[s].size()), col(fill));
                const bool plain = fill.empty() && stroke.empty();      // neither given: the lines in the muted blue-grey
                if (!stroke.empty() || plain) strokeLine(dl, subs[s], closed[s], col(stroke.empty() ? "#7a86a0" : stroke), sw, dash, scale);
            }
        } else if (nm == "text") {
            const std::size_t e = svg.find("</text>", i);
            const std::string t = svg.substr(i, e - i);
            i = e + 7;
            const float fs = float(num("font-size", 12)) * scale;
            ImFont* f = svgAttr(tag, "font-weight") == "700" ? fonts().bold : fonts().regular;
            const ImVec2 ts = f->CalcTextSizeA(fs, FLT_MAX, 0, t.c_str());
            const ImVec2 at = P(num("x"), num("y"));
            dl->AddText(f, fs, ImVec2(at.x - (svgAttr(tag, "text-anchor") == "middle" ? ts.x / 2 : 0), at.y - f->GetFontBaked(fs)->Ascent), col(fill.empty() ? "#000000" : fill), t.c_str());
        }
    }
}

}  // namespace ui
