// 📷 Save image: the PNG file (written here, with no library), and the file's name and folder.
// with exactly the picture area of the window.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace app {

// A PNG file from 8-bit RGB pixels, top row first (w × h × 3 bytes). The pixels are compressed
// (a row filter, then LZ77 with the fixed Huffman code), so a flat picture is small.
std::vector<uint8_t> encodePng(int w, int h, const uint8_t* rgb);

struct ShotTime { int year, month, day, hour, minute, second; };

// turned into "-", lower case: "orbit-lab"; the Atom Lab adds the nucleus: "atom-lab-Z6-N6"), then the local
// date and time, so no save overwrites another: "orbit-lab-2026-10-01-153045.png".
std::string shotFileName(const std::string& lab, int Z, int N, const ShotTime& t);


// Joins a folder and a file name with the right separator ("C:\Users\User\Pictures" + "a.png").
std::string joinPath(const std::string& folder, const std::string& name);

}  // namespace app
