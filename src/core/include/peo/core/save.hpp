#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace peo::core {

/// Who owns a section once games are networked (ADR-0017 decision 3): the host owns
/// World, the player's machine owns Character, Local is never synced.
enum class SyncScope : std::uint8_t { World = 1, Character = 2, Local = 3 };

/// One system's part of a save (ADR-0002): its own id and version, its scope, and
/// its bytes. The container knows nothing of what the payload means.
struct SaveSection {
    std::uint32_t id = 0; ///< four ASCII characters, little-endian (section_id("WRLD"))
    std::uint16_t version = 1;
    SyncScope scope = SyncScope::World;
    std::vector<std::byte> payload;
};

/// A whole save in memory: the format it was written in, the build that wrote it
/// (ADR-0017 decision 1), and its sections in order.
struct SaveImage {
    std::uint32_t format = 0;
    std::string build;
    std::vector<SaveSection> sections;
};

/// The container format written today. A reader refuses anything newer.
inline constexpr std::uint32_t kSaveFormat = 1;

enum class SaveStatus : std::uint8_t {
    Ok,
    BadHeader,   ///< not a save: wrong magic or format 0
    Truncated,   ///< ends before what its lengths promise
    NewerFormat, ///< written by a newer build: never guessed at
    Corrupt,     ///< a World or Character section fails its checksum, or a value fails validation
};

/// Something a reader noticed: a dropped or refused section, or the field that failed.
/// Header problems name section 0.
struct SaveIssue {
    std::uint32_t section_id = 0;
    SyncScope scope = SyncScope::World;
    std::string what;
};

struct SaveRead {
    SaveStatus status = SaveStatus::BadHeader;
    SaveImage image;
    std::vector<SaveIssue> issues;
};

/// "WRLD" -> its id: the four characters, first in the lowest byte.
[[nodiscard]] constexpr std::uint32_t section_id(std::string_view four) noexcept {
    std::uint32_t id = 0;
    for (std::size_t i = 0; i < 4 && i < four.size(); ++i) {
        id |= static_cast<std::uint32_t>(static_cast<unsigned char>(four[i])) << (8 * i);
    }
    return id;
}
/// The id as its four characters, for messages.
[[nodiscard]] std::string section_name(std::uint32_t id);

/// CRC-32 (IEEE 802.3: reflected, polynomial 0xEDB88320), as zip and PNG compute it.
[[nodiscard]] std::uint32_t crc32(std::span<const std::byte> bytes) noexcept;

/// The image as bytes: magic "PEOSAVE\0", u32 format, u16-length build, u32 section
/// count, then per section u32 id, u16 version, u8 scope, u8 codec (0 = raw), u64
/// length, u32 crc32 of the stored payload, the payload. All little-endian. A build
/// string longer than 65,535 bytes is cut there.
[[nodiscard]] std::vector<std::byte> write_save(const SaveImage& image);

/// Read bytes write_save made. Never throws and never trusts a length: a bad checksum
/// or unknown codec in a World or Character section makes the image Corrupt; in a
/// Local section the section is dropped with an issue and the image stays Ok.
[[nodiscard]] SaveRead read_save(std::span<const std::byte> bytes);

} // namespace peo::core
