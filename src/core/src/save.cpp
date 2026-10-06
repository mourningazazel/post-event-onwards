#include "peo/core/save.hpp"

#include "peo/core/byte_io.hpp"

#include <algorithm>
#include <array>

namespace peo::core {

namespace {
constexpr std::array<std::byte, 8> kMagic{std::byte{'P'}, std::byte{'E'}, std::byte{'O'}, std::byte{'S'},
                                          std::byte{'A'}, std::byte{'V'}, std::byte{'E'}, std::byte{0}};
constexpr std::uint8_t kCodecRaw = 0; // PEO-108 adds zstd
constexpr std::size_t kMaxBuild = 0xFFFF;
constexpr std::uint32_t kCrcPolynomial = 0xEDB88320U;
constexpr std::size_t kCrcTableSize = 256;

constexpr std::array<std::uint32_t, kCrcTableSize> crc_table() noexcept {
    std::array<std::uint32_t, kCrcTableSize> t{};
    for (std::uint32_t i = 0; i < kCrcTableSize; ++i) {
        std::uint32_t c = i;
        for (int k = 0; k < 8; ++k) {
            c = (c & 1U) != 0 ? kCrcPolynomial ^ (c >> 1) : c >> 1;
        }
        t[i] = c;
    }
    return t;
}
constexpr std::array<std::uint32_t, kCrcTableSize> kCrcTable = crc_table();

bool valid_scope(std::uint8_t s) noexcept {
    return s >= static_cast<std::uint8_t>(SyncScope::World) &&
           s <= static_cast<std::uint8_t>(SyncScope::Local);
}
} // namespace

std::string section_name(std::uint32_t id) {
    std::string s;
    for (int i = 0; i < 4; ++i) {
        const auto c = static_cast<char>((id >> (8 * i)) & 0xFFU);
        s += c >= ' ' && c <= '~' ? c : '?';
    }
    return s;
}

std::uint32_t crc32(std::span<const std::byte> bytes) noexcept {
    std::uint32_t c = 0xFFFFFFFFU;
    for (const std::byte b : bytes) {
        c = kCrcTable[(c ^ std::to_integer<std::uint32_t>(b)) & 0xFFU] ^ (c >> 8);
    }
    return c ^ 0xFFFFFFFFU;
}

std::vector<std::byte> write_save(const SaveImage& image) {
    ByteWriter w;
    w.bytes(kMagic);
    w.u32(image.format);
    const std::size_t build = std::min(image.build.size(), kMaxBuild);
    w.u16(static_cast<std::uint16_t>(build));
    for (std::size_t i = 0; i < build; ++i) {
        w.u8(static_cast<std::uint8_t>(image.build[i]));
    }
    w.u32(static_cast<std::uint32_t>(image.sections.size()));
    for (const SaveSection& s : image.sections) {
        w.u32(s.id);
        w.u16(s.version);
        w.u8(static_cast<std::uint8_t>(s.scope));
        w.u8(kCodecRaw);
        w.u64(s.payload.size());
        w.u32(crc32(s.payload));
        w.bytes(s.payload);
    }
    return w.take();
}

SaveRead read_save(std::span<const std::byte> bytes) {
    SaveRead out;
    const auto header = [&](SaveStatus status, const char* what) {
        out.status = status;
        out.issues.push_back({.section_id = 0, .scope = SyncScope::World, .what = what});
        return out;
    };
    const std::size_t magic = std::min(bytes.size(), kMagic.size());
    if (!std::equal(bytes.begin(), bytes.begin() + static_cast<std::ptrdiff_t>(magic), kMagic.begin())) {
        return header(SaveStatus::BadHeader, "not a save: wrong magic");
    }
    ByteReader r(bytes.subspan(magic));
    if (magic < kMagic.size()) {
        return header(SaveStatus::Truncated, "ends inside the magic");
    }
    out.image.format = r.u32();
    const std::uint16_t build = r.u16();
    const auto build_bytes = r.bytes(build);
    const std::uint32_t count = r.u32();
    if (r.failed()) {
        return header(SaveStatus::Truncated, "ends inside the header");
    }
    if (out.image.format == 0) {
        return header(SaveStatus::BadHeader, "format 0");
    }
    if (out.image.format > kSaveFormat) {
        return header(SaveStatus::NewerFormat, "written in a newer format");
    }
    for (const std::byte b : build_bytes) {
        out.image.build += static_cast<char>(std::to_integer<unsigned char>(b));
    }
    out.status = SaveStatus::Ok;
    for (std::uint32_t i = 0; i < count; ++i) {
        SaveSection s;
        s.id = r.u32();
        s.version = r.u16();
        const std::uint8_t scope = r.u8();
        const std::uint8_t codec = r.u8();
        const std::uint64_t length = r.u64();
        const std::uint32_t crc = r.u32();
        const auto payload = r.bytes(r.has(length) ? static_cast<std::size_t>(length) : 0);
        if (r.failed()) {
            out.status = SaveStatus::Truncated;
            out.issues.push_back(
                {.section_id = s.id, .scope = SyncScope::World, .what = "ends inside the section"});
            return out;
        }
        if (!valid_scope(scope)) {
            out.status = SaveStatus::Corrupt;
            out.issues.push_back(
                {.section_id = s.id, .scope = SyncScope::World, .what = "unknown sync scope"});
            continue;
        }
        s.scope = static_cast<SyncScope>(scope);
        const bool readable = codec == kCodecRaw && crc32(payload) == crc;
        if (!readable) {
            out.issues.push_back({.section_id = s.id,
                                  .scope = s.scope,
                                  .what = codec != kCodecRaw ? "unknown codec" : "bad checksum"});
            if (s.scope != SyncScope::Local) {
                out.status = SaveStatus::Corrupt;
            }
            continue; // a Local section is dropped; the rest of the save stands
        }
        s.payload.assign(payload.begin(), payload.end());
        out.image.sections.push_back(std::move(s));
    }
    if (r.remaining() != 0) {
        out.status = out.status == SaveStatus::Ok ? SaveStatus::Corrupt : out.status;
        out.issues.push_back(
            {.section_id = 0, .scope = SyncScope::World, .what = "bytes after the last section"});
    }
    return out;
}

} // namespace peo::core
