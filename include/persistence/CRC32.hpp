#pragma once

#include <cstdint>
#include <cstddef>
#include <string_view>

namespace sse::persistence {

/**
 * @brief Standard IEEE 802.3 32-bit Cyclic Redundancy Check (CRC-32).
 *
 * Implements standard Ethernet/ZIP/PNG CRC-32 using polynomial 0xEDB88320.
 * Supports single-shot and streaming/incremental evaluation.
 *
 * Known-Answer Test Vector:
 * Input:  "123456789" (9 bytes)
 * Result: 0xCBF43926
 */
class CRC32 {
public:
    CRC32() noexcept : m_crc(0xFFFFFFFF) {}

    /**
     * @brief Resets accumulator for a new checksum computation.
     */
    void reset() noexcept {
        m_crc = 0xFFFFFFFF;
    }

    /**
     * @brief Incrementally consumes a buffer of bytes.
     */
    void update(const void* data, std::size_t length) noexcept;

    /**
     * @brief Incrementally consumes a string view.
     */
    void update(std::string_view sv) noexcept {
        update(sv.data(), sv.size());
    }

    /**
     * @brief Finalizes and returns the 32-bit CRC-32 checksum.
     */
    uint32_t value() const noexcept {
        return m_crc ^ 0xFFFFFFFF;
    }

    /**
     * @brief Convenience single-shot calculation over a buffer.
     */
    static uint32_t calculate(const void* data, std::size_t length) noexcept {
        CRC32 crc;
        crc.update(data, length);
        return crc.value();
    }

    /**
     * @brief Convenience single-shot calculation over a string view.
     */
    static uint32_t calculate(std::string_view sv) noexcept {
        return calculate(sv.data(), sv.size());
    }

private:
    uint32_t m_crc{0xFFFFFFFF};
    static const uint32_t s_table[256];
};

} // namespace sse::persistence
