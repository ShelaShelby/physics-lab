#include "app/shot.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace app {

namespace {

uint32_t crc32(const uint8_t* p, std::size_t n, uint32_t crc = 0) {
    static uint32_t table[256];
    static bool ready = false;
    if (!ready) {
        for (uint32_t i = 0; i < 256; i++) {
            uint32_t c = i;
            for (int k = 0; k < 8; k++) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            table[i] = c;
        }
        ready = true;
    }
    crc = ~crc;
    for (std::size_t i = 0; i < n; i++) crc = table[(crc ^ p[i]) & 255] ^ (crc >> 8);
    return ~crc;
}

void put32(std::vector<uint8_t>& o, uint32_t v) {
    o.push_back(uint8_t(v >> 24)); o.push_back(uint8_t(v >> 16)); o.push_back(uint8_t(v >> 8)); o.push_back(uint8_t(v));
}

void chunk(std::vector<uint8_t>& o, const char type[5], const std::vector<uint8_t>& data) {
    put32(o, uint32_t(data.size()));
    std::vector<uint8_t> td(type, type + 4);
    td.insert(td.end(), data.begin(), data.end());
    o.insert(o.end(), td.begin(), td.end());
    put32(o, crc32(td.data(), td.size()));
}

// ---- deflate with the fixed Huffman code (RFC 1951, block type 1) ----
struct BitWriter {
    std::vector<uint8_t> out;
    uint32_t acc = 0;
    int n = 0;
    void bits(uint32_t v, int count) {                   // least significant bit first
        acc |= v << n; n += count;
        while (n >= 8) { out.push_back(uint8_t(acc & 255)); acc >>= 8; n -= 8; }
    }
    void code(uint32_t c, int len) {                     // Huffman codes go most significant bit first
        uint32_t r = 0;
        for (int i = 0; i < len; i++) r |= ((c >> i) & 1u) << (len - 1 - i);
        bits(r, len);
    }
    void literal(int v) {
        if (v < 144) code(0x30 + v, 8);
        else if (v < 256) code(0x190 + (v - 144), 9);
        else if (v < 280) code(v - 256, 7);
        else code(0xC0 + (v - 280), 8);
    }
    void flush() { if (n > 0) { out.push_back(uint8_t(acc & 255)); acc = 0; n = 0; } }
};

const int kLenBase[29] = {3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
const int kLenExtra[29] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
const int kDistBase[30] = {1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};
const int kDistExtra[30] = {0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};

std::vector<uint8_t> deflateFixed(const std::vector<uint8_t>& in) {
    BitWriter w;
    w.bits(1, 1);                                        // the last (only) block
    w.bits(1, 2);                                        // fixed Huffman codes
    const int kHashSize = 1 << 15, kWindow = 32768, kMaxChain = 24;
    std::vector<int> head(kHashSize, -1), prev(in.size(), -1);
    auto hash = [&](std::size_t i) { return ((in[i] << 10) ^ (in[i + 1] << 5) ^ in[i + 2]) & (kHashSize - 1); };
    auto insert = [&](std::size_t i) { if (i + 2 < in.size()) { const int h = hash(i); prev[i] = head[h]; head[h] = int(i); } };
    std::size_t i = 0;
    while (i < in.size()) {
        int bestLen = 0, bestDist = 0;
        if (i + 2 < in.size()) {
            int cand = head[hash(i)], chain = 0;
            const std::size_t maxLen = std::min<std::size_t>(258, in.size() - i);
            while (cand >= 0 && int(i) - cand <= kWindow && chain++ < kMaxChain) {
                std::size_t l = 0;
                while (l < maxLen && in[cand + l] == in[i + l]) l++;
                if (int(l) > bestLen) { bestLen = int(l); bestDist = int(i) - cand; if (l == maxLen) break; }
                cand = prev[cand];
            }
        }
        if (bestLen >= 3) {
            int lc = 28;
            while (kLenBase[lc] > bestLen) lc--;
            const int sym = 257 + lc;
            if (sym < 280) w.code(sym - 256, 7); else w.code(0xC0 + (sym - 280), 8);
            if (kLenExtra[lc]) w.bits(uint32_t(bestLen - kLenBase[lc]), kLenExtra[lc]);
            int dc = 29;
            while (kDistBase[dc] > bestDist) dc--;
            w.code(uint32_t(dc), 5);
            if (kDistExtra[dc]) w.bits(uint32_t(bestDist - kDistBase[dc]), kDistExtra[dc]);
            for (int k = 0; k < bestLen; k++) insert(i + k);
            i += bestLen;
        } else {
            w.literal(in[i]);
            insert(i);
            i++;
        }
    }
    w.code(0, 7);                                        // end of block (symbol 256)
    w.flush();
    return w.out;
}

uint32_t adler32(const std::vector<uint8_t>& d) {
    uint32_t a = 1, b = 0;
    for (std::size_t i = 0; i < d.size(); i++) { a = (a + d[i]) % 65521; b = (b + a) % 65521; }
    return (b << 16) | a;
}

}  // namespace

std::vector<uint8_t> encodePng(int w, int h, const uint8_t* rgb) {
    std::vector<uint8_t> png = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    std::vector<uint8_t> ihdr;
    put32(ihdr, uint32_t(w)); put32(ihdr, uint32_t(h));
    ihdr.push_back(8); ihdr.push_back(2); ihdr.push_back(0); ihdr.push_back(0); ihdr.push_back(0);   // 8 bit, RGB, deflate, filters, no interlace
    chunk(png, "IHDR", ihdr);
    // the scanlines, each with a filter byte: none, Sub or Up, whichever leaves the smaller numbers
    const std::size_t stride = std::size_t(w) * 3;
    std::vector<uint8_t> raw;
    raw.reserve((stride + 1) * h);
    std::vector<uint8_t> sub(stride), up(stride);
    for (int y = 0; y < h; y++) {
        const uint8_t* row = rgb + stride * y;
        const uint8_t* above = y ? row - stride : nullptr;
        long sNone = 0, sSub = 0, sUp = 0;
        for (std::size_t x = 0; x < stride; x++) {
            sub[x] = uint8_t(row[x] - (x >= 3 ? row[x - 3] : 0));
            up[x] = uint8_t(row[x] - (above ? above[x] : 0));
            sNone += std::abs(int(int8_t(row[x]))); sSub += std::abs(int(int8_t(sub[x]))); sUp += std::abs(int(int8_t(up[x])));
        }
        if (sUp <= sSub && sUp <= sNone) { raw.push_back(2); raw.insert(raw.end(), up.begin(), up.end()); }
        else if (sSub <= sNone) { raw.push_back(1); raw.insert(raw.end(), sub.begin(), sub.end()); }
        else { raw.push_back(0); raw.insert(raw.end(), row, row + stride); }
    }
    std::vector<uint8_t> z = {0x78, 0x9C};               // zlib header
    const std::vector<uint8_t> body = deflateFixed(raw);
    z.insert(z.end(), body.begin(), body.end());
    put32(z, adler32(raw));
    chunk(png, "IDAT", z);
    chunk(png, "IEND", {});
    return png;
}

std::string shotFileName(const std::string& lab, int Z, int N, const ShotTime& t) {
    std::string slug;                                    // the web: simSel.value.toLowerCase().replace(/\W+/g, '-')
    bool dash = false;
    for (unsigned char c : lab) {
        if (std::isalnum(c) || c == '_') { slug += char(std::tolower(c)); dash = false; }
        else if (!dash) { slug += '-'; dash = true; }
    }
    if (lab == "Atom Lab") slug += "-Z" + std::to_string(Z) + "-N" + std::to_string(N);   // the web: atom-lab-Z6-N6
    char stamp[40];
    std::snprintf(stamp, sizeof stamp, "-%04d-%02d-%02d-%02d%02d%02d", t.year, t.month, t.day, t.hour, t.minute, t.second);
    return slug + stamp + ".png";
}

std::string joinPath(const std::string& folder, const std::string& name) {
    if (folder.empty()) return name;
    const char last = folder.back();
    if (last == '/' || last == '\\') return folder + name;
    return folder + (folder.find('\\') != std::string::npos ? "\\" : "/") + name;
}

}  // namespace app
