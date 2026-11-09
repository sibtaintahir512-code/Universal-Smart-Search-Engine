#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <cstring>
#include <string_view>
#include "dsa/CustomDynamicArray.hpp"

namespace sse::persistence {

/**
 * @brief Endian-safe, bounds-checked binary writer.
 *
 * Enforces explicit little-endian bit-level serializations for all integral
 * and floating-point types, guaranteeing compiler-independent binary output.
 */
class BinaryWriter {
public:
    BinaryWriter() = default;
    explicit BinaryWriter(std::size_t initialCapacity) {
        m_buffer.reserve(initialCapacity);
    }

    void writeUint8(uint8_t val) {
        m_buffer.push_back(val);
    }

    void writeUint16(uint16_t val) {
        m_buffer.push_back(static_cast<uint8_t>(val & 0xFF));
        m_buffer.push_back(static_cast<uint8_t>((val >> 8) & 0xFF));
    }

    void writeUint32(uint32_t val) {
        m_buffer.push_back(static_cast<uint8_t>(val & 0xFF));
        m_buffer.push_back(static_cast<uint8_t>((val >> 8) & 0xFF));
        m_buffer.push_back(static_cast<uint8_t>((val >> 16) & 0xFF));
        m_buffer.push_back(static_cast<uint8_t>((val >> 24) & 0xFF));
    }

    void writeUint64(uint64_t val) {
        for (int i = 0; i < 8; ++i) {
            m_buffer.push_back(static_cast<uint8_t>((val >> (i * 8)) & 0xFF));
        }
    }

    void writeInt32(int32_t val) {
        writeUint32(static_cast<uint32_t>(val));
    }

    void writeDouble(double val) {
        uint64_t bits = 0;
        std::memcpy(&bits, &val, sizeof(double));
        writeUint64(bits);
    }

    void writeBytes(const void* data, std::size_t len) {
        if (!data || len == 0) return;
        const auto* ptr = static_cast<const uint8_t*>(data);
        for (std::size_t i = 0; i < len; ++i) {
            m_buffer.push_back(ptr[i]);
        }
    }

    /**
     * @brief Writes string with uint16_t length prefix (max 65,535 bytes).
     */
    void writeString16(std::string_view sv) {
        uint16_t len = static_cast<uint16_t>(sv.size() > 0xFFFF ? 0xFFFF : sv.size());
        writeUint16(len);
        writeBytes(sv.data(), len);
    }

    /**
     * @brief Writes string with uint32_t length prefix.
     */
    void writeString32(std::string_view sv) {
        uint32_t len = static_cast<uint32_t>(sv.size());
        writeUint32(len);
        writeBytes(sv.data(), len);
    }

    const uint8_t* data() const noexcept {
        return m_buffer.data();
    }

    std::size_t size() const noexcept {
        return m_buffer.size();
    }

    void clear() noexcept {
        m_buffer.clear();
    }

    const dsa::CustomDynamicArray<uint8_t>& buffer() const noexcept {
        return m_buffer;
    }

private:
    dsa::CustomDynamicArray<uint8_t> m_buffer;
};

/**
 * @brief Endian-safe, bounds-checked binary reader with strict arithmetic validation.
 */
class BinaryReader {
public:
    BinaryReader(const uint8_t* data, std::size_t size) noexcept
        : m_data(data), m_size(size), m_offset(0), m_error(false) {}

    std::size_t size() const noexcept { return m_size; }
    std::size_t offset() const noexcept { return m_offset; }
    std::size_t remaining() const noexcept {
        return (m_offset <= m_size) ? (m_size - m_offset) : 0;
    }

    bool hasError() const noexcept { return m_error; }
    const std::string& errorMessage() const noexcept { return m_errorMsg; }

    bool seek(std::size_t pos) noexcept {
        if (pos > m_size) {
            fail("Seek past end of binary buffer");
            return false;
        }
        m_offset = pos;
        return true;
    }

    bool readUint8(uint8_t& out) noexcept {
        if (remaining() < 1) {
            fail("Unexpected EOF while reading uint8");
            return false;
        }
        out = m_data[m_offset++];
        return true;
    }

    bool readUint16(uint16_t& out) noexcept {
        if (remaining() < 2) {
            fail("Unexpected EOF while reading uint16");
            return false;
        }
        out = static_cast<uint16_t>(static_cast<uint16_t>(m_data[m_offset]) |
                                    static_cast<uint16_t>(static_cast<uint16_t>(m_data[m_offset + 1]) << 8));
        m_offset += 2;
        return true;
    }

    bool readUint32(uint32_t& out) noexcept {
        if (remaining() < 4) {
            fail("Unexpected EOF while reading uint32");
            return false;
        }
        out = static_cast<uint32_t>(m_data[m_offset]) |
              (static_cast<uint32_t>(m_data[m_offset + 1]) << 8) |
              (static_cast<uint32_t>(m_data[m_offset + 2]) << 16) |
              (static_cast<uint32_t>(m_data[m_offset + 3]) << 24);
        m_offset += 4;
        return true;
    }

    bool readUint64(uint64_t& out) noexcept {
        if (remaining() < 8) {
            fail("Unexpected EOF while reading uint64");
            return false;
        }
        out = 0;
        for (int i = 0; i < 8; ++i) {
            out |= (static_cast<uint64_t>(m_data[m_offset + static_cast<std::size_t>(i)]) << (i * 8));
        }
        m_offset += 8;
        return true;
    }

    bool readInt32(int32_t& out) noexcept {
        uint32_t uval = 0;
        if (!readUint32(uval)) return false;
        out = static_cast<int32_t>(uval);
        return true;
    }

    bool readDouble(double& out) noexcept {
        uint64_t bits = 0;
        if (!readUint64(bits)) return false;
        std::memcpy(&out, &bits, sizeof(double));
        return true;
    }

    bool readBytes(void* dest, std::size_t len) noexcept {
        if (len == 0) return true;
        if (!dest) {
            fail("Null destination buffer in readBytes");
            return false;
        }
        if (remaining() < len) {
            fail("Unexpected EOF while reading raw bytes");
            return false;
        }
        std::memcpy(dest, m_data + m_offset, len);
        m_offset += len;
        return true;
    }

    bool readString16(std::string& out, std::size_t maxLength = 65536) {
        uint16_t len = 0;
        if (!readUint16(len)) return false;
        if (len > maxLength) {
            fail("String length exceeds maximum allowed limit");
            return false;
        }
        if (remaining() < len) {
            fail("Unexpected EOF while reading string contents");
            return false;
        }
        out.assign(reinterpret_cast<const char*>(m_data + m_offset), len);
        m_offset += len;
        return true;
    }

    bool readString32(std::string& out, std::size_t maxLength = 1048576) {
        uint32_t len = 0;
        if (!readUint32(len)) return false;
        if (len > maxLength) {
            fail("String length exceeds maximum allowed limit");
            return false;
        }
        if (remaining() < len) {
            fail("Unexpected EOF while reading string contents");
            return false;
        }
        out.assign(reinterpret_cast<const char*>(m_data + m_offset), len);
        m_offset += len;
        return true;
    }

    const uint8_t* currentPtr() const noexcept {
        return m_data + m_offset;
    }

private:
    const uint8_t* m_data{nullptr};
    std::size_t    m_size{0};
    std::size_t    m_offset{0};
    bool           m_error{false};
    std::string    m_errorMsg;

    void fail(std::string msg) noexcept {
        m_error = true;
        m_errorMsg = std::move(msg);
    }
};

} // namespace sse::persistence
