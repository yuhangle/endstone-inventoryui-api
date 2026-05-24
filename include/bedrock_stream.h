#pragma once

#include "bedrock_ffi.h"

#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

// ==================== BinaryStream (write-only) ====================

class BinaryStream {
public:
    BinaryStream() : ptr_(bedrock_stream_create(false))
    {
        if (!ptr_) throw std::runtime_error("bedrock_stream_create failed");
    }

    ~BinaryStream()
    {
        if (ptr_) {
            bedrock_stream_destroy(ptr_);
            ptr_ = nullptr;
        }
    }

    BinaryStream(const BinaryStream &) = delete;
    BinaryStream &operator=(const BinaryStream &) = delete;

    BinaryStream(BinaryStream &&other) noexcept : ptr_(other.ptr_) { other.ptr_ = nullptr; }
    BinaryStream &operator=(BinaryStream &&other) noexcept
    {
        if (this != &other) {
            if (ptr_) bedrock_stream_destroy(ptr_);
            ptr_ = other.ptr_;
            other.ptr_ = nullptr;
        }
        return *this;
    }

    void writeBool(bool v) const { check(bedrock_stream_write_bool(ptr_, v)); }
    void writeByte(int v) const { check(bedrock_stream_write_u8(ptr_, static_cast<uint8_t>(v))); }
    void writeSignedByte(int v) const { check(bedrock_stream_write_u8(ptr_, static_cast<uint8_t>(static_cast<int8_t>(v)))); }
    void writeShort(int v) const { check(bedrock_stream_write_i16(ptr_, static_cast<int16_t>(v))); }
    void writeSignedShort(int v) const { check(bedrock_stream_write_i16(ptr_, static_cast<int16_t>(v))); }
    void writeUnsignedShort(int v) const { check(bedrock_stream_write_u16(ptr_, static_cast<uint16_t>(v))); }
    void writeInt(int v) const { check(bedrock_stream_write_i32(ptr_, static_cast<int32_t>(v))); }
    void writeSignedInt(int v) const { check(bedrock_stream_write_i32(ptr_, static_cast<int32_t>(v))); }
    void writeUnsignedInt(unsigned v) const { check(bedrock_stream_write_u32(ptr_, static_cast<uint32_t>(v))); }
    void writeLong(int64_t v) const { check(bedrock_stream_write_i64(ptr_, v)); }
    void writeUnsignedLong(uint64_t v) const { check(bedrock_stream_write_u64(ptr_, v)); }
    void writeSignedLong(int64_t v) const { check(bedrock_stream_write_i64(ptr_, v)); }
    void writeUnsignedInt64(uint64_t v) const { check(bedrock_stream_write_u64(ptr_, v)); }
    void writeFloat(float v) const { check(bedrock_stream_write_f32(ptr_, v)); }
    void writeDouble(double v) const { check(bedrock_stream_write_f64(ptr_, v)); }
    void writeVarint(int32_t v) const { check(bedrock_stream_write_varint(ptr_, v)); }
    void writeUnsignedVarint(uint32_t v) const { check(bedrock_stream_write_unsigned_varint(ptr_, v)); }
    void writeVarint64(int64_t v) const { check(bedrock_stream_write_varint64(ptr_, v)); }
    void writeUnsignedVarint64(uint64_t v) const { check(bedrock_stream_write_unsigned_varint64(ptr_, v)); }

    void writeString(const std::string &v) const {
        check(bedrock_stream_write_string(ptr_, v.c_str()));
    }

    void writeRawBytes(const uint8_t *data, size_t len) const {
        check(bedrock_stream_write_raw_bytes(ptr_, data, len));
    }

    void writeRawBytes(const std::vector<uint8_t> &data) const
    {
        writeRawBytes(data.data(), data.size());
    }

    void writeBytes(const std::vector<uint8_t> &data) const
    {
        writeUnsignedVarint(static_cast<uint32_t>(data.size()));
        writeRawBytes(data.data(), data.size());
    }

    /// Get a copy of the internal buffer.
    [[nodiscard]] std::vector<uint8_t> copyBuffer() const
    {
        const uint8_t *data = nullptr;
        size_t len = 0;
        check(bedrock_stream_data(ptr_, &data, &len));
        return {data, data + len};
    }

    void *release() { auto p = ptr_; ptr_ = nullptr; return p; }

private:
    void *ptr_;

    static void check(int ret)
    {
        if (ret != 0) {
            const char *msg = bedrock_last_error();
            throw std::runtime_error(msg ? msg : "bedrock-ffi error");
        }
    }
};

// ==================== ReadOnlyBinaryStream ====================

class ReadOnlyBinaryStream {
public:
    explicit ReadOnlyBinaryStream(const uint8_t *data, size_t len)
        : ptr_(bedrock_stream_from_bytes(data, len, false))
    {
        if (!ptr_) throw std::runtime_error("bedrock_stream_from_bytes failed");
    }

    explicit ReadOnlyBinaryStream(const std::vector<uint8_t> &data)
        : ReadOnlyBinaryStream(data.data(), data.size())
    {
    }

    ~ReadOnlyBinaryStream()
    {
        if (ptr_) {
            bedrock_stream_destroy(ptr_);
            ptr_ = nullptr;
        }
    }

    ReadOnlyBinaryStream(const ReadOnlyBinaryStream &) = delete;
    ReadOnlyBinaryStream &operator=(const ReadOnlyBinaryStream &) = delete;

    ReadOnlyBinaryStream(ReadOnlyBinaryStream &&other) noexcept : ptr_(other.ptr_) { other.ptr_ = nullptr; }
    ReadOnlyBinaryStream &operator=(ReadOnlyBinaryStream &&other) noexcept
    {
        if (this != &other) {
            if (ptr_) bedrock_stream_destroy(ptr_);
            ptr_ = other.ptr_;
            other.ptr_ = nullptr;
        }
        return *this;
    }

    // -- get_* methods --

    [[nodiscard]] bool getBool() const { bool v{}; check(bedrock_stream_read_bool(ptr_, &v)); return v; }
    [[nodiscard]] uint8_t getByte() const { uint8_t v{}; check(bedrock_stream_read_u8(ptr_, &v)); return v; }
    [[nodiscard]] unsigned char getUnsignedByte() const { return getByte(); }
    [[nodiscard]] int16_t getSignedShort() const { int16_t v{}; check(bedrock_stream_read_i16(ptr_, &v)); return v; }
    [[nodiscard]] uint16_t getUnsignedShort() const { uint16_t v{}; check(bedrock_stream_read_u16(ptr_, &v)); return v; }
    [[nodiscard]] int32_t getInt() const { int32_t v{}; check(bedrock_stream_read_i32(ptr_, &v)); return v; }
    [[nodiscard]] uint32_t getUnsignedInt() const { uint32_t v{}; check(bedrock_stream_read_u32(ptr_, &v)); return v; }
    [[nodiscard]] int32_t getSignedInt() const { return getInt(); }
    [[nodiscard]] int64_t getLong() const { int64_t v{}; check(bedrock_stream_read_i64(ptr_, &v)); return v; }
    [[nodiscard]] uint64_t getUnsignedLong() const { uint64_t v{}; check(bedrock_stream_read_u64(ptr_, &v)); return v; }
    [[nodiscard]] int64_t getSignedLongLong() const { return getLong(); }
    [[nodiscard]] uint64_t getUnsignedInt64() const { uint64_t v{}; check(bedrock_stream_read_u64(ptr_, &v)); return v; }
    [[nodiscard]] float getFloat() const { float v{}; check(bedrock_stream_read_f32(ptr_, &v)); return v; }
    [[nodiscard]] double getDouble() const { double v{}; check(bedrock_stream_read_f64(ptr_, &v)); return v; }
    [[nodiscard]] int32_t getVarint() const { int32_t v{}; check(bedrock_stream_read_varint(ptr_, &v)); return v; }
    [[nodiscard]] uint32_t getUnsignedVarint() const { uint32_t v{}; check(bedrock_stream_read_unsigned_varint(ptr_, &v)); return v; }
    [[nodiscard]] int64_t getVarint64() const { int64_t v{}; check(bedrock_stream_read_varint64(ptr_, &v)); return v; }
    [[nodiscard]] uint64_t getUnsignedVarint64() const { uint64_t v{}; check(bedrock_stream_read_unsigned_varint64(ptr_, &v)); return v; }

    [[nodiscard]] std::string getString() const
    {
        size_t buf_size = 256;
        std::string result;
        for (int attempt = 0; attempt < 2; ++attempt) {
            result.resize(buf_size);
            int ret = bedrock_stream_read_string(ptr_, result.data(), &buf_size);
            if (ret == 0) {
                result.resize(buf_size > 0 ? buf_size - 1 : 0);  // exclude null terminator
                return result;
            }
            if (ret == -3) continue;  // BEDROCK_ERR_INVALID_ARG: buffer too small, retry
            check(ret);
        }
        throw std::runtime_error("failed to read string after buffer resize");
    }

    [[nodiscard]] std::string readString() const { return getString(); }

    [[nodiscard]] std::vector<uint8_t> getBytes() const
    {
        auto len = getUnsignedVarint();
        return readRawBytes(len);
    }

    [[nodiscard]] std::vector<uint8_t> readRawBytes(size_t len) const
    {
        std::vector<uint8_t> buf(len);
        if (len > 0) {
            check(bedrock_stream_read_raw_bytes(ptr_, buf.data(), len));
        }
        return buf;
    }

    [[nodiscard]] std::vector<uint8_t> readRemaining() const
    {
        auto remaining = getSize() - getPosition();
        return readRawBytes(remaining);
    }

    [[nodiscard]] std::vector<uint8_t> getLeftBuffer() const { return readRemaining(); }

    // -- utility --

    [[nodiscard]] size_t getSize() const { return bedrock_stream_size(ptr_); }
    [[nodiscard]] size_t size() const { return getSize(); }
    [[nodiscard]] size_t getPosition() const { return bedrock_stream_position(ptr_); }
    void setPosition(const size_t pos) const { check(bedrock_stream_set_position(ptr_, pos)); }
    [[nodiscard]] bool hasDataLeft() const { return getPosition() < getSize(); }

private:
    void *ptr_;

    static void check(const int ret)
    {
        if (ret != 0) {
            const char *msg = bedrock_last_error();
            throw std::runtime_error(msg ? msg : "bedrock-ffi error");
        }
    }
};
