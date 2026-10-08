// peo_geo (PEO-095): draw geography squares as an image for review. Headless; file I/O
// lives here, never in core.
//
//   peo_geo --seed N --square X,Y [--span K] [--scale S] [--grid] --out file.ppm
//
// Generates the K x K squares from X,Y and writes one binary PPM (P6) at one pixel per
// S x S cells. Sea is darker with depth, lakes a distinct blue, forest green, bare land an
// elevation ramp with hill shading; --grid draws square edges so seams can be checked.
// Prints per square: generation ms (wall clock), bytes(), and the shares of sea, lake,
// forest and land above 1000 m.

#include "peo/core/geography.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

using namespace peo::core;

namespace {
struct Rgb {
    std::uint8_t r = 0;
    std::uint8_t g = 0;
    std::uint8_t b = 0;
};
constexpr int kHighM = 1000;
constexpr int kPercent = 100;

std::uint8_t channel(int v) {
    return static_cast<std::uint8_t>(std::clamp(v, 0, 255));
}

Rgb colour(std::int16_t e, std::uint8_t cover, int shade) {
    if ((cover & kCoverLake) != 0) {
        return {40, 110, 220};
    }
    if ((cover & kCoverSea) != 0) {
        const int depth = std::min(-e, 600);
        return {channel(20 - depth / 40), channel(50 - depth / 20), channel(120 - depth / 10)};
    }
    if ((cover & kCoverForest) != 0) {
        return {channel(30 + shade), channel(95 + e / 40 + shade), channel(35 + shade)};
    }
    // Bare land: low green-brown through rock to snow.
    const int t = std::clamp(static_cast<int>(e), 0, 3000);
    Rgb c;
    if (t < 600) {
        c = {channel(150 + t / 12), channel(160 + t / 20), channel(100)};
    } else if (t < 1800) {
        c = {channel(200 - (t - 600) / 20), channel(190 - (t - 600) / 15), channel(110 + (t - 600) / 30)};
    } else {
        c = {channel(150 + (t - 1800) / 12), channel(150 + (t - 1800) / 12), channel(150 + (t - 1800) / 12)};
    }
    return {channel(c.r + shade), channel(c.g + shade), channel(c.b + shade)};
}

bool parse_pair(std::string_view s, int& a, int& b) {
    const auto comma = s.find(',');
    if (comma == std::string_view::npos) {
        return false;
    }
    a = std::atoi(std::string(s.substr(0, comma)).c_str());
    b = std::atoi(std::string(s.substr(comma + 1)).c_str());
    return true;
}
} // namespace

int main(int argc, char** argv) {
    Seed seed = 1;
    int sx = 0;
    int sy = 0;
    int span = 1;
    int scale = 1;
    bool grid = false;
    std::string out;
    for (int i = 1; i < argc; ++i) {
        const std::string_view a = argv[i];
        const bool has_value = i + 1 < argc;
        if (a == "--seed" && has_value) {
            seed = std::strtoull(argv[++i], nullptr, 10);
        } else if (a == "--square" && has_value && parse_pair(argv[i + 1], sx, sy)) {
            ++i;
        } else if (a == "--span" && has_value) {
            span = std::max(1, std::atoi(argv[++i]));
        } else if (a == "--scale" && has_value) {
            scale = std::max(1, std::atoi(argv[++i]));
        } else if (a == "--grid") {
            grid = true;
        } else if (a == "--out" && has_value) {
            out = argv[++i];
        } else {
            std::fprintf(
                stderr,
                "usage: peo_geo --seed N --square X,Y [--span K] [--scale S] [--grid] --out file.ppm\n");
            return 2;
        }
    }
    if (out.empty()) {
        std::fprintf(stderr, "peo_geo: --out is required\n");
        return 2;
    }
    const GeographyParams params;
    const int side = span * kSquareCells / scale;
    std::vector<Rgb> image(static_cast<std::size_t>(side) * static_cast<std::size_t>(side));
    for (int qy = 0; qy < span; ++qy) {
        for (int qx = 0; qx < span; ++qx) {
            const SquareCoord sq{sx + qx, sy + qy};
            const auto t0 = std::chrono::steady_clock::now();
            const GeographyRegion g = generate_square(seed, params, sq);
            const auto t1 = std::chrono::steady_clock::now();
            std::array<std::int64_t, 4> count{}; // sea, lake, forest, high
            std::int64_t land = 0;
            for (int y = 0; y < kSquareCells; ++y) {
                for (int x = 0; x < kSquareCells; ++x) {
                    const std::uint8_t c = g.cover.at(x, y);
                    const std::int16_t e = g.elevation_m.at(x, y);
                    count[0] += (c & kCoverSea) != 0 ? 1 : 0;
                    count[1] += (c & kCoverLake) != 0 ? 1 : 0;
                    count[2] += (c & kCoverForest) != 0 ? 1 : 0;
                    const bool is_land = (c & (kCoverSea | kCoverLake)) == 0;
                    land += is_land ? 1 : 0;
                    count[3] += is_land && e > kHighM ? 1 : 0;
                }
            }
            const double cells = double{kSquareCells} * kSquareCells;
            std::printf(
                "square %d,%d: %.1f ms, %zu bytes, %zu bodies; sea %.1f%%, lake %.2f%%, forest %.1f%% of "
                "land, above %d m %.1f%% of land\n",
                sq.x, sq.y, std::chrono::duration<double, std::milli>(t1 - t0).count(), g.bytes(),
                g.bodies.size(), kPercent * static_cast<double>(count[0]) / cells,
                kPercent * static_cast<double>(count[1]) / cells,
                land > 0 ? kPercent * static_cast<double>(count[2]) / static_cast<double>(land) : 0.0, kHighM,
                land > 0 ? kPercent * static_cast<double>(count[3]) / static_cast<double>(land) : 0.0);
            for (int py = 0; py < kSquareCells / scale; ++py) {
                for (int px = 0; px < kSquareCells / scale; ++px) {
                    const int x = px * scale;
                    const int y = py * scale;
                    const std::int16_t e = g.elevation_m.at(x, y);
                    // Light from the north-west: brighter where the ground rises that way.
                    const int ex = x > 0 ? g.elevation_m.at(x - 1, y) : e;
                    const int ey = y > 0 ? g.elevation_m.at(x, y - 1) : e;
                    const int shade = std::clamp((ex - e) + (ey - e), -60, 60) / 2;
                    Rgb c = colour(e, g.cover.at(x, y), shade);
                    if (grid && (x == 0 || y == 0)) {
                        c = {255, 40, 200};
                    }
                    const auto ix = static_cast<std::size_t>(qx * kSquareCells / scale + px);
                    const auto iy = static_cast<std::size_t>(qy * kSquareCells / scale + py);
                    image[iy * static_cast<std::size_t>(side) + ix] = c;
                }
            }
        }
    }
    std::ofstream f(out, std::ios::binary);
    f << "P6\n" << side << ' ' << side << "\n255\n";
    for (const Rgb c : image) {
        f.put(static_cast<char>(c.r)).put(static_cast<char>(c.g)).put(static_cast<char>(c.b));
    }
    std::printf("peo_geo: wrote %s (%d x %d)\n", out.c_str(), side, side);
    return f ? 0 : 1;
}
