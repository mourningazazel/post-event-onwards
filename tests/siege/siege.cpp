// PEO-088 (N022): peo_siege, the siege suite's runner. The horde round each of three
// buildings, in three opening setups at two densities, for 48 game hours while the player
// waits; a report of where the Dead are and what they do at six checkpoints, and maps.
// On demand only: never part of verify, ctest or the regular CI (Ryan, 2026-10-02).
//
//   peo_siege [--building A,B,C] [--setup enclosed,one-opening,two-openings]
//             [--density sparse,heavy] [--seeds N] [--threads N] [--out DIR]
//             [--check | --write]
//
// Each building's report and maps go to DIR/<B>/report.md and DIR/<B>/maps.txt (PEO-103),
// so a building's files are the same whether it ran alone or with the others. --check
// compares each building run with its committed baseline (tests/siege/baseline/<B>/) and
// prints the first differing lines; --write replaces those. Both take --building but no
// other filter, so a check is always a building's full share. Exit codes: 0 fine, 1 a
// broken invariant or a --check difference, 2 bad arguments.

#include "peo/core/world.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <limits>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "siege_layouts.hpp"
#include "thread_pool.hpp"

#ifndef PEO_SIEGE_BASELINE_DIR
#define PEO_SIEGE_BASELINE_DIR "tests/siege/baseline"
#endif

using namespace peo::core;
using namespace peo::siege;

namespace {

constexpr std::array<int, 6> kCheckpointHours{1, 2, 6, 12, 24, 48};
constexpr Seed kSeedBase = 1;
/// Two, not three: three took 766 s on the x86 CI runner, over the 10 min budget (PEO-088 rework).
constexpr int kDefaultSeeds = 2;
/// The maps: this many cells square round the building's centre.
constexpr int kMapSide = 41;
/// Cells "at" an opening: within this Chebyshev distance of it, outside the footprint.
constexpr int kNearOpening = 3;
/// Cells "by" a side of the building: within this distance of the footprint.
constexpr int kBySide = 10;
constexpr double kPercent = 100.0;
/// A column with no value in this configuration (an opening the setup lacks).
constexpr double kNone = std::numeric_limits<double>::quiet_NaN();

/// Distance bands from the footprint's box, [lo, hi].
struct Band {
    int lo;
    int hi;
    const char* name;
};
constexpr std::array<Band, 6> kBands{{{1, 1, "1"},
                                      {2, 3, "2-3"},
                                      {4, 10, "4-10"},
                                      {11, 30, "11-30"},
                                      {31, 60, "31-60"},
                                      {61, std::numeric_limits<int>::max(), "61+"}}};

/// The columns measured at each checkpoint, in table order. Table 1 is where the Dead
/// are; table 2 what they are doing.
enum Column : std::size_t {
    kWestRoom,
    kEastRoom,
    kHallway,
    kDoorways,
    kOnOpenings,
    kNearWest,
    kNearEast,
    kBand1,
    kBand6 = kBand1 + kBands.size() - 1,
    kMeanDist,
    kSdDist,
    kTable2,
    kSideWest = kTable2,
    kSideEast,
    kSideNorth,
    kSideSouth,
    kOnPlayer,
    kRoundPlayer,
    kMovedPct,
    kFromStart,
    kOnScentPct,
    kStates,
    kColumns = kStates + kDeadStateNames.size()
};

struct Config {
    Building building;
    Setup setup;
    Density density;
};

struct RunResult {
    /// [checkpoint][column]
    std::vector<std::array<double, kColumns>> rows;
    /// Updates until the first Dead stands inside, and until the first is on or beside
    /// the player; empty when never.
    std::optional<int> first_inside;
    std::optional<int> first_contact;
    std::vector<std::string> maps; // seed 0 only: one per checkpoint
    std::string failure;           // a broken invariant, or empty
};

bool inside(const SiegeLayout& l, Vec2i c) {
    const Region r = l.region.at(c);
    return r == Region::WestRoom || r == Region::EastRoom || r == Region::Hallway || r == Region::Doorway;
}

std::string map_of(const SiegeLayout& l, const World& w) {
    const int cx = (l.x0 + l.x1) / 2 - kMapSide / 2;
    const int cy = (l.y0 + l.y1) / 2 - kMapSide / 2;
    Grid<std::uint8_t> dead(kStageSide, kStageSide, 0);
    for (const Dead& d : w.horde()) {
        dead.at(d.pos) = 1;
    }
    std::string out;
    for (int y = cy; y < cy + kMapSide; ++y) {
        for (int x = cx; x < cx + kMapSide; ++x) {
            const bool player = Vec2i{x, y} == w.player();
            const bool d = dead.at(x, y) != 0;
            out += player && d ? '+' : (player ? '@' : (d ? 'd' : (l.stage.blocked.at(x, y) ? '#' : '.')));
        }
        out += '\n';
    }
    return out;
}

/// One run: a fresh World on the layout, the player waiting 48 hours.
RunResult run(const Config& c, Seed seed, bool keep_maps) {
    RunResult result;
    const SiegeLayout l = build_layout(c.building, c.setup);
    WorldParams params{.initial_dead = 0, .stage_width = kStageSide, .stage_height = kStageSide};
    params.wind_max = 0; // calm (the brief)
    World w(seed, params);
    w.load_layout(l.stage, l.player, place_horde(l, c.density, seed, params.update_period));
    const std::size_t size = w.horde().size();
    std::vector<Vec2i> start(size);
    std::vector<Vec2i> last(size);
    for (std::size_t i = 0; i < size; ++i) {
        start[i] = last[i] = w.horde()[i].pos;
    }
    const int per_hour = static_cast<int>(kSubstepsPerHour / params.update_period);
    const int total = kCheckpointHours.back() * per_hour;
    std::size_t next = 0;
    Grid<std::uint8_t> on(kStageSide, kStageSide, 0);
    for (int u = 1; u <= total; ++u) {
        w.step(Action::wait());
        for (const Dead& d : w.horde()) {
            if (!result.first_inside && inside(l, d.pos)) {
                result.first_inside = u;
            }
            if (!result.first_contact &&
                std::max(std::abs(d.pos.x - l.player.x), std::abs(d.pos.y - l.player.y)) <= 1) {
                result.first_contact = u;
            }
        }
        if (u != kCheckpointHours[next] * per_hour) {
            continue;
        }
        // Invariants: one to a tile, the horde whole, and an enclosed building sealed.
        std::fill(on.begin(), on.end(), 0);
        for (const Dead& d : w.horde()) {
            if (++on.at(d.pos) > 1) {
                result.failure = "two Dead on one tile";
            }
        }
        if (w.horde().size() != size) {
            result.failure = "the horde changed size";
        }
        if (c.setup == Setup::Enclosed) {
            for (const Dead& d : w.horde()) {
                if (l.in_footprint(d.pos)) {
                    result.failure = "a Dead inside an enclosed building";
                }
            }
            for (int y = 0; y < kStageSide; ++y) {
                for (int x = 0; x < kStageSide; ++x) {
                    if (!l.in_footprint({x, y}) && w.scent().sample({x, y}) > 0) {
                        result.failure = "scent outside an enclosed building";
                    }
                }
            }
        }
        std::array<double, kColumns> row{};
        if (l.openings.size() < 2) {
            row[kNearEast] = kNone;
        }
        if (l.openings.empty()) {
            row[kNearWest] = kNone;
        }
        double dist_sum = 0;
        double dist_sq = 0;
        int outside = 0;
        int moved = 0;
        double from_start = 0;
        int on_scent = 0;
        for (std::size_t i = 0; i < size; ++i) {
            const Dead& d = w.horde()[i];
            const Vec2i p = d.pos;
            switch (l.region.at(p)) {
            case Region::WestRoom:
                ++row[kWestRoom];
                break;
            case Region::EastRoom:
                ++row[kEastRoom];
                break;
            case Region::Hallway:
                ++row[kHallway];
                break;
            case Region::Doorway:
                ++row[kDoorways];
                break;
            case Region::Opening:
                ++row[kOnOpenings];
                break;
            default:
                break;
            }
            if (!l.in_footprint(p)) {
                for (std::size_t o = 0; o < l.openings.size(); ++o) {
                    const Vec2i q = l.openings[o];
                    if (std::max(std::abs(p.x - q.x), std::abs(p.y - q.y)) <= kNearOpening) {
                        ++row[o == 0 ? kNearWest : kNearEast];
                    }
                }
                const int dist = l.distance_out(p);
                for (std::size_t b = 0; b < kBands.size(); ++b) {
                    if (dist >= kBands[b].lo && dist <= kBands[b].hi) {
                        ++row[kBand1 + b];
                    }
                }
                dist_sum += dist;
                dist_sq += static_cast<double>(dist) * dist;
                ++outside;
                if (dist <= kBySide) {
                    // The side it is on: the furthest the cell lies past the box, W E N S on ties.
                    const std::array<int, 4> past{l.x0 - p.x, p.x - l.x1, l.y0 - p.y, p.y - l.y1};
                    const auto side =
                        static_cast<std::size_t>(std::max_element(past.begin(), past.end()) - past.begin());
                    ++row[kSideWest + side];
                }
            }
            const int to_player = std::max(std::abs(p.x - l.player.x), std::abs(p.y - l.player.y));
            row[kOnPlayer] += to_player == 0 ? 1 : 0;
            row[kRoundPlayer] += to_player == 1 ? 1 : 0;
            moved += p != last[i] ? 1 : 0;
            last[i] = p;
            from_start += std::max(std::abs(p.x - start[i].x), std::abs(p.y - start[i].y));
            on_scent += w.scent().sample(p) > 0 ? 1 : 0;
            ++row[kStates + static_cast<std::size_t>(state_of(d))];
        }
        const double mean = outside > 0 ? dist_sum / outside : 0;
        row[kMeanDist] = mean;
        row[kSdDist] = outside > 0 ? std::sqrt(std::max(dist_sq / outside - mean * mean, 0.0)) : 0;
        const auto n = static_cast<double>(std::max<std::size_t>(size, 1));
        row[kMovedPct] = kPercent * moved / n;
        row[kFromStart] = from_start / n;
        row[kOnScentPct] = kPercent * on_scent / n;
        result.rows.push_back(row);
        if (keep_maps) {
            result.maps.push_back(map_of(l, w));
        }
        ++next;
    }
    return result;
}

// ---- formatting --------------------------------------------------------------------

std::string fixed(double v, int decimals) {
    char buf[32];
    std::snprintf(buf, sizeof buf, "%.*f", decimals, v);
    return buf;
}

/// "mean", or "mean (min-max)" when the seeds differ; "-" for no value.
std::string cell(const std::vector<double>& v, int decimals) {
    if (v.empty() || std::isnan(v[0])) {
        return "-";
    }
    double sum = 0;
    double lo = v[0];
    double hi = v[0];
    for (const double x : v) {
        sum += x;
        lo = std::min(lo, x);
        hi = std::max(hi, x);
    }
    const std::string mean = fixed(sum / static_cast<double>(v.size()), decimals);
    if (lo == hi) {
        return mean;
    }
    return mean + " (" + fixed(lo, decimals) + "-" + fixed(hi, decimals) + ")";
}

/// The firsts over the seeds: "never", or the mean time in hours with the range, and how
/// many seeds never saw it.
std::string first_cell(const std::vector<std::optional<int>>& v, int per_hour) {
    std::vector<double> seen;
    for (const auto& f : v) {
        if (f) {
            seen.push_back(static_cast<double>(*f) / per_hour);
        }
    }
    if (seen.empty()) {
        return "never";
    }
    std::string s = cell(seen, 2) + " h";
    if (seen.size() < v.size()) {
        s += ", never in " + std::to_string(v.size() - seen.size()) + " of " + std::to_string(v.size()) +
             " seeds";
    }
    return s;
}

std::string config_name(const Config& c) {
    return std::string(kBuildingNames[static_cast<std::size_t>(c.building)]) + " " +
           kSetupNames[static_cast<std::size_t>(c.setup)] + " " +
           kDensityNames[static_cast<std::size_t>(c.density)];
}

// ---- arguments ---------------------------------------------------------------------

std::vector<std::string> split(std::string_view s) {
    std::vector<std::string> out;
    std::string cur;
    for (const char ch : s) {
        if (ch == ',') {
            out.push_back(cur);
            cur.clear();
        } else {
            cur += ch;
        }
    }
    out.push_back(cur);
    return out;
}

template <typename E, std::size_t N>
bool pick(std::string_view arg, const std::array<const char*, N>& names, const std::array<E, N>& all,
          std::vector<E>& out) {
    out.clear();
    for (const std::string& want : split(arg)) {
        bool found = false;
        for (std::size_t i = 0; i < N; ++i) {
            if (want == names[i]) {
                out.push_back(all[i]);
                found = true;
            }
        }
        if (!found) {
            std::fprintf(stderr, "peo_siege: unknown value '%s'\n", want.c_str());
            return false;
        }
    }
    return true;
}

std::string read_file(const std::filesystem::path& p) {
    std::ifstream in(p, std::ios::binary);
    std::ostringstream s;
    s << in.rdbuf();
    return s.str();
}

/// Prints the first lines where `fresh` differs from the file at `base`; true when equal.
bool same_as(const std::string& fresh, const std::filesystem::path& base) {
    if (!std::filesystem::exists(base)) {
        std::printf("--check: no baseline at %s\n", base.string().c_str());
        return false;
    }
    std::istringstream a(read_file(base));
    std::istringstream b(fresh);
    std::string la;
    std::string lb;
    int line = 0;
    int shown = 0;
    constexpr int kShow = 8;
    while (true) {
        const bool ha = static_cast<bool>(std::getline(a, la));
        const bool hb = static_cast<bool>(std::getline(b, lb));
        ++line;
        if (!ha && !hb) {
            break;
        }
        if (!ha || !hb || la != lb) {
            if (shown == 0) {
                std::printf("--check: %s differs:\n", base.filename().string().c_str());
            }
            std::printf("  line %d\n    baseline: %s\n    fresh:    %s\n", line, ha ? la.c_str() : "(end)",
                        hb ? lb.c_str() : "(end)");
            if (++shown == kShow) {
                break;
            }
        }
    }
    return shown == 0;
}

// ---- the files: one report and one maps file per building ---------------------------

/// Building `building`'s report: its header, its rows of the Firsts table and its section. It reads
/// only its configurations, so it is the same whether it ran alone or in the full matrix.
std::string building_report(Building building, const std::vector<Config>& configs,
                            const std::vector<RunResult>& results, int seeds) {
    const int per_hour = static_cast<int>(kSubstepsPerHour / kUpdatePeriodSubsteps);
    std::ostringstream r;
    r << "# Siege suite report (PEO-088, N022): building "
      << kBuildingTitles[static_cast<std::size_t>(building)] << "\n\n"
      << "The horde round the building for 48 game hours while the player waits; calm wind. Each value is "
         "the "
         "mean over "
      << seeds << " seed" << (seeds == 1 ? "" : "s") << ", with the range in brackets when the seeds differ. "
      << "Times are game hours; counts are Dead.\n\n";
    r << "## Firsts\n\n"
      << "How long until the first of the Dead stood inside the building (a room, the hallway or a doorway), "
         "and "
         "until the first stood on or beside the player's tile.\n\n"
      << "| building | setup | density | Dead | first inside | first on or beside the player |\n"
      << "|---|---|---|---|---|---|\n";
    for (std::size_t c = 0; c < configs.size(); ++c) {
        if (configs[c].building != building) {
            continue;
        }
        std::vector<std::optional<int>> in;
        std::vector<std::optional<int>> contact;
        for (int s = 0; s < seeds; ++s) {
            const RunResult& rr = results[c * static_cast<std::size_t>(seeds) + static_cast<std::size_t>(s)];
            in.push_back(rr.first_inside);
            contact.push_back(rr.first_contact);
        }
        const SiegeLayout l = build_layout(configs[c].building, configs[c].setup);
        r << "| " << kBuildingNames[static_cast<std::size_t>(configs[c].building)] << " | "
          << kSetupNames[static_cast<std::size_t>(configs[c].setup)] << " | "
          << kDensityNames[static_cast<std::size_t>(configs[c].density)] << " | "
          << place_horde(l, configs[c].density, kSeedBase).size() << " | " << first_cell(in, per_hour)
          << " | " << first_cell(contact, per_hour) << " |\n";
    }
    const auto column = [&](std::size_t c, std::size_t row, std::size_t col) {
        std::vector<double> v;
        for (int s = 0; s < seeds; ++s) {
            v.push_back(
                results[c * static_cast<std::size_t>(seeds) + static_cast<std::size_t>(s)].rows[row][col]);
        }
        return v;
    };
    r << "\n## Building " << kBuildingTitles[static_cast<std::size_t>(building)] << "\n";
    for (std::size_t c = 0; c < configs.size(); ++c) {
        const Config& cf = configs[c];
        if (cf.building != building) {
            continue;
        }
        r << "\n### " << kSetupNames[static_cast<std::size_t>(cf.setup)] << ", "
          << kDensityNames[static_cast<std::size_t>(cf.density)] << "\n\n";
        // Building A is one room: its floor counts as the west room, and it has no east room,
        // hallway or doorways, so it gets one "room" column in place of those four.
        const bool one_room = cf.building == Building::A;
        r << "Where the Dead are (counts; distance bands are cells from the building's outer wall):\n\n"
          << "| hours |" << (one_room ? " room |" : " west room | east room | hallway | doorways |")
          << " on openings | at west opening | at east opening |";
        for (const Band& b : kBands) {
            r << " out " << b.name << " |";
        }
        r << " mean distance | sd |\n|---|" << (one_room ? "---|" : "---|---|---|---|") << "---|---|---|";
        for (std::size_t b = 0; b < kBands.size(); ++b) {
            r << "---|";
        }
        r << "---|---|\n";
        for (std::size_t row = 0; row < kCheckpointHours.size(); ++row) {
            r << "| " << kCheckpointHours[row] << " |";
            if (one_room) {
                r << " " << cell(column(c, row, kWestRoom), 1) << " |";
            }
            for (std::size_t col = one_room ? kOnOpenings : kWestRoom; col <= kBand6; ++col) {
                r << " " << cell(column(c, row, col), 1) << " |";
            }
            r << " " << cell(column(c, row, kMeanDist), 1) << " | " << cell(column(c, row, kSdDist), 1)
              << " |\n";
        }
        r << "\nRound the building and what they do (counts within " << kBySide
          << " cells of each side; moved is the share that moved since the last checkpoint; from start is "
             "the mean "
             "distance in cells from where each started):\n\n"
          << "| hours | west side | east side | north side | south side | on player | round player | moved % "
             "| "
             "from start | on scent % |";
        for (const char* s : kDeadStateNames) {
            r << " " << s << " |";
        }
        r << "\n|---|---|---|---|---|---|---|---|---|---|";
        for (std::size_t s = 0; s < kDeadStateNames.size(); ++s) {
            r << "---|";
        }
        r << "\n";
        for (std::size_t row = 0; row < kCheckpointHours.size(); ++row) {
            r << "| " << kCheckpointHours[row] << " |";
            for (std::size_t col = kSideWest; col < kColumns; ++col) {
                r << " " << cell(column(c, row, col), 1) << " |";
            }
            r << "\n";
        }
    }
    return r.str();
}

/// Building `building`'s maps: seed kSeedBase of each of its configurations at every checkpoint.
std::string building_maps(Building building, const std::vector<Config>& configs,
                          const std::vector<RunResult>& results, int seeds) {
    std::ostringstream m;
    m << "Siege suite maps (PEO-088): seed " << kSeedBase << " of every configuration of building "
      << kBuildingNames[static_cast<std::size_t>(building)] << ", " << kMapSide << " x " << kMapSide
      << " cells round the building. # wall, . floor or ground, d one of the Dead, @ the player, + both.\n";
    for (std::size_t c = 0; c < configs.size(); ++c) {
        if (configs[c].building != building) {
            continue;
        }
        const RunResult& rr = results[c * static_cast<std::size_t>(seeds)];
        for (std::size_t row = 0; row < rr.maps.size(); ++row) {
            m << "\n" << config_name(configs[c]) << ", " << kCheckpointHours[row] << " h\n" << rr.maps[row];
        }
    }
    return m.str();
}

} // namespace

int main(int argc, char** argv) {
    std::vector<Building> buildings(kBuildings.begin(), kBuildings.end());
    std::vector<Setup> setups(kSetups.begin(), kSetups.end());
    std::vector<Density> densities(kDensities.begin(), kDensities.end());
    int seeds = kDefaultSeeds;
    std::size_t threads = std::max(1U, std::thread::hardware_concurrency());
    std::filesystem::path out_dir = "siege-out";
    bool check = false;
    bool write = false;
    /// --setup, --density or --seeds: a run that is less than a building's full share.
    bool filtered = false;
    for (int i = 1; i < argc; ++i) {
        const std::string_view a = argv[i];
        const bool has_value = i + 1 < argc;
        if (a == "--check") {
            check = true;
        } else if (a == "--write") {
            write = true;
        } else if (a == "--building" && has_value) {
            if (!pick(argv[++i], kBuildingNames, kBuildings, buildings)) {
                return 2;
            }
        } else if (a == "--setup" && has_value) {
            filtered = true;
            if (!pick(argv[++i], kSetupNames, kSetups, setups)) {
                return 2;
            }
        } else if (a == "--density" && has_value) {
            filtered = true;
            if (!pick(argv[++i], kDensityNames, kDensities, densities)) {
                return 2;
            }
        } else if (a == "--seeds" && has_value) {
            filtered = true;
            seeds = std::max(1, std::atoi(argv[++i]));
        } else if (a == "--threads" && has_value) {
            threads = static_cast<std::size_t>(std::max(1, std::atoi(argv[++i])));
        } else if (a == "--out" && has_value) {
            out_dir = argv[++i];
        } else {
            std::fprintf(stderr,
                         "usage: peo_siege [--building A,B,C] [--setup enclosed,one-opening,two-openings]\n"
                         "                 [--density sparse,heavy] [--seeds N] [--threads N] [--out DIR]\n"
                         "                 [--check | --write]\n");
            return 2;
        }
    }
    if ((check || write) && filtered) {
        std::fprintf(
            stderr,
            "peo_siege: --check and --write take whole buildings (--building only, no other filter)\n");
        return 2;
    }

    std::vector<Config> configs;
    for (const Building b : buildings) {
        for (const Setup s : setups) {
            for (const Density d : densities) {
                configs.push_back({b, s, d});
            }
        }
    }
    const std::size_t runs = configs.size() * static_cast<std::size_t>(seeds);
    std::vector<RunResult> results(runs);
    peo::app::ThreadPool pool(threads);
    // Each run owns its World; results land by index, so any order gives the same files.
    pool.run(runs, [&](std::size_t k) {
        const std::size_t c = k / static_cast<std::size_t>(seeds);
        const auto s = static_cast<int>(k % static_cast<std::size_t>(seeds));
        results[k] = run(configs[c], kSeedBase + static_cast<Seed>(s), s == 0);
    });

    int failures = 0;
    for (std::size_t k = 0; k < runs; ++k) {
        if (!results[k].failure.empty()) {
            std::printf("INVARIANT BROKEN: %s, seed %zu: %s\n",
                        config_name(configs[k / static_cast<std::size_t>(seeds)]).c_str(),
                        k % static_cast<std::size_t>(seeds), results[k].failure.c_str());
            ++failures;
        }
    }
    // Each building's files go to <dir>/<B>/; --check and --write touch only the buildings run.
    const std::filesystem::path base = PEO_SIEGE_BASELINE_DIR;
    bool differ = false;
    for (const Building building : buildings) {
        const char* name = kBuildingNames[static_cast<std::size_t>(building)];
        const std::string report = building_report(building, configs, results, seeds);
        const std::string maps = building_maps(building, configs, results, seeds);
        std::filesystem::create_directories(out_dir / name);
        std::ofstream(out_dir / name / "report.md", std::ios::binary) << report;
        std::ofstream(out_dir / name / "maps.txt", std::ios::binary) << maps;
        if (write) {
            std::filesystem::create_directories(base / name);
            std::ofstream(base / name / "report.md", std::ios::binary) << report;
            std::ofstream(base / name / "maps.txt", std::ios::binary) << maps;
        }
        if (check) {
            const bool a = same_as(report, base / name / "report.md");
            const bool m = same_as(maps, base / name / "maps.txt");
            differ = differ || !(a && m);
        }
    }
    std::printf("peo_siege: %zu runs, reports in %s/<building>/\n", runs, out_dir.string().c_str());
    if (write) {
        std::printf("peo_siege: baseline written to %s\n", base.string().c_str());
    }
    if (check) {
        std::printf("peo_siege --check: %s\n", differ ? "DIFFERS from the baseline" : "matches the baseline");
    }
    return failures > 0 || differ ? 1 : 0;
}
