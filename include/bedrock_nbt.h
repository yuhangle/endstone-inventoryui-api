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
