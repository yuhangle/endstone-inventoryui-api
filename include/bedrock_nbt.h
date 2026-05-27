#pragma once

#include "bedrock_ffi.h"

#include <cstdint>
#include <string>
#include <stdexcept>
#include <vector>

// ==================== CompoundTag (RAII wrapper) ====================

class CompoundTag {
public:
    CompoundTag() : ptr_(bedrock_nbt_create())
    {
        if (!ptr_) throw std::runtime_error("bedrock_nbt_create failed");
    }

    explicit CompoundTag(void *ptr) : ptr_(ptr) {}

    ~CompoundTag()
    {
        if (ptr_) {
            bedrock_nbt_destroy(ptr_);
            ptr_ = nullptr;
        }
    }

    CompoundTag(const CompoundTag &) = delete;
    CompoundTag &operator=(const CompoundTag &) = delete;

    CompoundTag(CompoundTag &&other) noexcept : ptr_(other.ptr_) { other.ptr_ = nullptr; }
    CompoundTag &operator=(CompoundTag &&other) noexcept
    {
        if (this != &other) {
            if (ptr_) bedrock_nbt_destroy(ptr_);
            ptr_ = other.ptr_;
            other.ptr_ = nullptr;
        }
        return *this;
    }

    void *release()
    {
        auto p = ptr_;
        ptr_ = nullptr;
        return p;
    }

    [[nodiscard]] void *get() const { return ptr_; }

    // -- set methods --

    void set(const std::string &key, const std::string &value) const {
        check(bedrock_nbt_set_string(ptr_, key.c_str(), value.c_str()));
    }

    void set(const std::string &key, const int32_t value) const {
        check(bedrock_nbt_set_int(ptr_, key.c_str(), value));
    }

    void set(const std::string &key, const int16_t value) const {
        check(bedrock_nbt_set_short(ptr_, key.c_str(), value));
    }

    void set(const std::string &key, const int8_t value) const {
        check(bedrock_nbt_set_byte(ptr_, key.c_str(), value));
    }

    void set(const std::string &key, CompoundTag &&child) const {
        check(bedrock_nbt_set_tag(ptr_, key.c_str(), child.ptr_));
        child.ptr_ = nullptr;  // ownership transferred
    }

    // -- list helpers --

    void listAppendString(const std::string &key, const std::string &value) const {
        check(bedrock_nbt_list_append_string(ptr_, key.c_str(), value.c_str()));
    }

    void listAppendTag(const std::string &key, CompoundTag &&child) const {
        check(bedrock_nbt_list_append_tag(ptr_, key.c_str(), child.ptr_));
        child.ptr_ = nullptr;
    }

    // -- query --

    [[nodiscard]] bool empty() const { return bedrock_nbt_empty(ptr_); }
    [[nodiscard]] bool contains(const std::string &key) const { return bedrock_nbt_contains(ptr_, key.c_str()); }
    [[nodiscard]] int32_t getInt(const std::string &key) const
    {
        int32_t val = 0;
        check(bedrock_nbt_get_int(ptr_, key.c_str(), &val));
        return val;
    }

    // -- serialization --

    [[nodiscard]] std::vector<uint8_t> toNetworkNbt() const {
        uint8_t *data = nullptr;
        size_t len = 0;
        check(bedrock_nbt_to_network(ptr_, &data, &len));
        std::vector result(data, data + len);
        bedrock_free(data);
        return result;
    }

    [[nodiscard]] std::vector<uint8_t> toBinaryNbt() const {
        uint8_t *data = nullptr;
        size_t len = 0;
        check(bedrock_nbt_to_binary(ptr_, &data, &len));
        std::vector result(data, data + len);
        bedrock_free(data);
        return result;
    }

    [[nodiscard]] std::string toSnbt() const {
        void *ptr = bedrock_nbt_to_snbt(ptr_);
        if (!ptr) return {};
        const auto cstr = static_cast<const char *>(ptr);
        std::string result(cstr);
        bedrock_free(ptr);
        return result;
    }

    // -- deserialization --

    /// Parse a binary NBT CompoundTag (with TAG_Compound header) from raw bytes.
    /// `little_endian`: true for LE, false for BE.
    /// Returns a new CompoundTag, or throws on failure.
    static CompoundTag fromBinaryNbt(const uint8_t *data, size_t len, bool little_endian = true) {
        CompoundTag tag;
        size_t consumed = 0;
        check(bedrock_nbt_from_binary_into(tag.ptr_, data, len, little_endian, &consumed));
        return tag;
    }

    // -- entry enumeration --

    [[nodiscard]] size_t entryCount() const {
        return bedrock_nbt_entry_count(ptr_);
    }

    [[nodiscard]] const char *entryKeyAt(size_t index) const {
        return bedrock_nbt_entry_key_at(ptr_, index);
    }

    [[nodiscard]] std::string entryKeyCopy(size_t index) const {
        size_t len = 256;
        std::string buf(len, '\0');
        int ret = bedrock_nbt_entry_key_copy(ptr_, index, buf.data(), &len);
        if (ret == BEDROCK_ERR_INVALID_ARG && len > 256) {
            buf.resize(len);
            check(bedrock_nbt_entry_key_copy(ptr_, index, buf.data(), &len));
        } else if (ret != 0) {
            check(ret);
        }
        if (len > 0) buf.resize(len - 1);
        else buf.clear();
        return buf;
    }

    [[nodiscard]] int entryTypeAt(size_t index) const {
        return bedrock_nbt_entry_type_at(ptr_, index);
    }

    // -- typed getters (beyond getInt) --

    [[nodiscard]] int8_t getByte(const std::string &key) const {
        int8_t val = 0;
        check(bedrock_nbt_get_byte(ptr_, key.c_str(), &val));
        return val;
    }

    [[nodiscard]] int16_t getShort(const std::string &key) const {
        int16_t val = 0;
        check(bedrock_nbt_get_short(ptr_, key.c_str(), &val));
        return val;
    }

    [[nodiscard]] int64_t getLong(const std::string &key) const {
        int64_t val = 0;
        check(bedrock_nbt_get_long(ptr_, key.c_str(), &val));
        return val;
    }

    [[nodiscard]] float getFloat(const std::string &key) const {
        float val = 0;
        check(bedrock_nbt_get_float(ptr_, key.c_str(), &val));
        return val;
    }

    [[nodiscard]] double getDouble(const std::string &key) const {
        double val = 0;
        check(bedrock_nbt_get_double(ptr_, key.c_str(), &val));
        return val;
    }

    [[nodiscard]] std::string getString(const std::string &key) const {
        size_t len = 256;
        std::string buf(len, '\0');
        int ret = bedrock_nbt_get_string(ptr_, key.c_str(), buf.data(), &len);
        if (ret == BEDROCK_ERR_INVALID_ARG && len > 256) {
            buf.resize(len);
            check(bedrock_nbt_get_string(ptr_, key.c_str(), buf.data(), &len));
        } else if (ret != 0) {
            check(ret);
        }
        if (len > 0) buf.resize(len - 1);
        else buf.clear();
        return buf;
    }

    /// Get a nested CompoundTag by key. Returns a new CompoundTag handle.
    /// Throws if the key doesn't exist or isn't a CompoundTag.
    [[nodiscard]] CompoundTag getTag(const std::string &key) const {
        void *child = bedrock_nbt_get_tag(ptr_, key.c_str());
        if (!child) throw std::runtime_error("bedrock_nbt_get_tag: key not found or not a compound");
        return CompoundTag(child);
    }

    [[nodiscard]] std::vector<uint8_t> getByteArray(const std::string &key) const {
        uint8_t *data = nullptr;
        size_t len = 0;
        check(bedrock_nbt_get_byte_array(ptr_, key.c_str(), &data, &len));
        std::vector result(data, data + len);
        bedrock_free(data);
        return result;
    }

    [[nodiscard]] std::vector<int32_t> getIntArray(const std::string &key) const {
        int32_t *data = nullptr;
        size_t len = 0;
        check(bedrock_nbt_get_int_array(ptr_, key.c_str(), &data, &len));
        std::vector result(data, data + len);
        bedrock_free(data);
        return result;
    }

    // -- list query --

    [[nodiscard]] int listSize(const std::string &key) const {
        return bedrock_nbt_list_size(ptr_, key.c_str());
    }

    [[nodiscard]] int listGetElementType(const std::string &key) const {
        return bedrock_nbt_list_get_element_type(ptr_, key.c_str());
    }

    [[nodiscard]] CompoundTag listGetTagAt(const std::string &key, int32_t index) const {
        void *child = bedrock_nbt_list_get_tag_at(ptr_, key.c_str(), index);
        if (!child) throw std::runtime_error("bedrock_nbt_list_get_tag_at: not found or not a compound");
        return CompoundTag(child);
    }

    [[nodiscard]] std::string listGetStringAt(const std::string &key, int32_t index) const {
        size_t len = 256;
        std::string buf(len, '\0');
        int ret = bedrock_nbt_list_get_string_at(ptr_, key.c_str(), index, buf.data(), &len);
        if (ret == BEDROCK_ERR_INVALID_ARG && len > 256) {
            buf.resize(len);
            check(bedrock_nbt_list_get_string_at(ptr_, key.c_str(), index, buf.data(), &len));
        } else if (ret != 0) {
            check(ret);
        }
        if (len > 0) buf.resize(len - 1);
        else buf.clear();
        return buf;
    }

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
