#pragma once

#include "peo/core/types.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>
#include <vector>

namespace peo::core {

/// Little-endian bytes for save payloads (PEO-091), written value by value so the
/// layout never depends on a struct's padding or the host's byte order.
class ByteWriter {
public:
    void u8(std::uint8_t v) { out_.push_back(std::byte{v}); }
    void u16(std::uint16_t v) { put(v, sizeof v); }
    void u32(std::uint32_t v) { put(v, sizeof v); }
    void u64(std::uint64_t v) { put(v, sizeof v); }
    void i32(std::int32_t v) { u32(static_cast<std::uint32_t>(v)); }
    void vec2i(Vec2i v) {
        i32(v.x);
        i32(v.y);
    }
    void bytes(std::span<const std::byte> b) { out_.insert(out_.end(), b.begin(), b.end()); }
    /// A count, then each element of a byte-sized vector.
    template <typename T> void byte_vector(const std::vector<T>& v) {
        static_assert(sizeof(T) == 1);
        u64(v.size());
        for (const T x : v) {
            u8(static_cast<std::uint8_t>(x));
        }
    }
    void i32_vector(const std::vector<std::int32_t>& v) {
        u64(v.size());
        for (const std::int32_t x : v) {
            i32(x);
        }
    }

    [[nodiscard]] const std::vector<std::byte>& data() const noexcept { return out_; }
    [[nodiscard]] std::vector<std::byte> take() noexcept { return std::move(out_); }

private:
    void put(std::uint64_t v, std::size_t n) {
        for (std::size_t i = 0; i < n; ++i) {
            out_.push_back(static_cast<std::byte>((v >> (8 * i)) & 0xFFU));
        }
    }
    std::vector<std::byte> out_;
};

/// Reads what ByteWriter wrote. Never throws: reading past the end sets failed() and
/// returns zeros from then on, so a parser checks once at the end. A length is checked
/// against the bytes left before anything is allocated for it.
class ByteReader {
public:
    explicit ByteReader(std::span<const std::byte> in) noexcept : in_(in) {}

    std::uint8_t u8() noexcept { return static_cast<std::uint8_t>(get(1)); }
    std::uint16_t u16() noexcept { return static_cast<std::uint16_t>(get(2)); }
    std::uint32_t u32() noexcept { return static_cast<std::uint32_t>(get(4)); }
    std::uint64_t u64() noexcept { return get(8); }
    std::int32_t i32() noexcept { return static_cast<std::int32_t>(u32()); }
    Vec2i vec2i() noexcept {
        const std::int32_t x = i32();
        return {x, i32()};
    }
    /// The next `n` bytes, or an empty span (and failed()) when fewer are left.
    std::span<const std::byte> bytes(std::size_t n) noexcept {
        if (!has(n)) {
            return {};
        }
        const auto out = in_.subspan(pos_, n);
        pos_ += n;
        return out;
    }
    template <typename T> std::vector<T> byte_vector() {
        static_assert(sizeof(T) == 1);
        const std::uint64_t n = u64();
        std::vector<T> v;
        if (!has(n)) {
            return v;
        }
        v.resize(static_cast<std::size_t>(n));
        for (T& x : v) {
            x = static_cast<T>(u8());
        }
        return v;
    }
    std::vector<std::int32_t> i32_vector() {
        const std::uint64_t n = u64();
        std::vector<std::int32_t> v;
        if (n > remaining() / sizeof(std::int32_t)) {
            failed_ = true;
            return v;
        }
        v.resize(static_cast<std::size_t>(n));
        for (std::int32_t& x : v) {
            x = i32();
        }
        return v;
    }

    /// Whether `n` more bytes are there; sets failed() when not.
    bool has(std::uint64_t n) noexcept {
        if (failed_ || n > remaining()) {
            failed_ = true;
            return false;
        }
        return true;
    }
    [[nodiscard]] std::size_t remaining() const noexcept { return failed_ ? 0 : in_.size() - pos_; }
    [[nodiscard]] bool failed() const noexcept { return failed_; }
    /// Read to the end, nothing left over and nothing missing.
    [[nodiscard]] bool done() const noexcept { return !failed_ && pos_ == in_.size(); }

private:
    std::uint64_t get(std::size_t n) noexcept {
        if (!has(n)) {
            return 0;
        }
        std::uint64_t v = 0;
        for (std::size_t i = 0; i < n; ++i) {
            v |= static_cast<std::uint64_t>(std::to_integer<std::uint8_t>(in_[pos_ + i])) << (8 * i);
        }
        pos_ += n;
        return v;
    }
    std::span<const std::byte> in_;
    std::size_t pos_ = 0;
    bool failed_ = false;
};

} // namespace peo::core
