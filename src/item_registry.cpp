#include "item_registry.h"
#include "bedrock_nbt.h"

#include <endstone/endstone.hpp>
#include <endstone/nbt/tag.h>

// ==================== Item Registry ====================

std::unordered_map<std::string, ItemData> &allItemData()
{
    static std::unordered_map<std::string, ItemData> cache;
    return cache;
}

void addItemData(const std::string &item_name, const ItemData &data)
{
    allItemData()[item_name] = data;
}

void clearItemData()
{
    allItemData().clear();
}

ItemData *getItemData(const std::string &item_name)
{
    auto &cache = allItemData();
    auto it = cache.find(item_name);
    return (it != cache.end()) ? &it->second : nullptr;
}

int16_t getItemRuntimeId(const std::string &item_name)
{
    auto *data = getItemData(item_name);
    return data ? data->item_id : 0;
}

// ==================== Endstone NBT → Rust NBT converter ====================

/// Recursively copy an Endstone nbt::Tag to our Rust-backed CompoundTag.
static void copyTag(const endstone::nbt::Tag &src, const std::string &key,
                    const CompoundTag &dst)
{
    switch (src.type()) {
    case endstone::nbt::Type::Byte: {
      const auto v = static_cast<int8_t>(src.get<endstone::ByteTag>().value());
        dst.set(key, v);
        break;
    }
    case endstone::nbt::Type::Short:
        dst.set(key, src.get<endstone::ShortTag>().value());
        break;
    case endstone::nbt::Type::Int:
        dst.set(key, src.get<endstone::IntTag>().value());
        break;
    case endstone::nbt::Type::Long:
        dst.set(key, static_cast<int32_t>(src.get<endstone::LongTag>().value()));
        break;
    case endstone::nbt::Type::String:
        dst.set(key, src.get<endstone::StringTag>().value());
        break;
    case endstone::nbt::Type::Compound: {
        CompoundTag child;
        for (const auto &[k, v] : src.get<endstone::CompoundTag>()) {
            copyTag(v, k, child);
        }
        dst.set(key, std::move(child));
        break;
    }
    case endstone::nbt::Type::List: {
      for (auto &list = src.get<endstone::ListTag>(); const auto &elem : list) {
            // Determine element type and append accordingly
            switch (elem.type()) {
            case endstone::nbt::Type::String:
                dst.listAppendString(key, elem.get<endstone::StringTag>().value());
                break;
            case endstone::nbt::Type::Compound: {
                CompoundTag child;
                for (const auto &[ck, cv] : elem.get<endstone::CompoundTag>()) {
                    copyTag(cv, ck, child);
                }
                dst.listAppendTag(key, std::move(child));
                break;
            }
            case endstone::nbt::Type::Byte:
                // Bedrock client expects Byte in list context as IntTag
                // Use set approach for single-byte list detection
                dst.listAppendString(key, std::to_string(elem.get<endstone::ByteTag>().value()));
                break;
            default:
                break;
            }
        }
        break;
    }
    default:
        break;
    }
}

// ==================== Item NBT builder ====================

std::vector<uint8_t> buildItemNbt(const endstone::ItemStack &item_stack)
{
    auto nbt = item_stack.getNbt();
    if (nbt.empty()) {
        return {};
    }

    const CompoundTag tag;

    for (const auto &[key, value] : nbt) {
        // Skip tags that are serialized separately in the item stack
        if (key == "Count" || key == "id" || key == "WasPickedUp") {
            continue;
        }
        copyTag(value, key, tag);
    }

    if (tag.empty()) {
        return {};
    }

    return tag.toBinaryNbt();
}

// ==================== Rust CompoundTag → Endstone CompoundTag ====================

/// Recursively copy a single Rust-backed Tag to an Endstone CompoundTag at the given key.
static void copyRustTagToEndstone(const CompoundTag &rust_tag, const std::string &key,
                                    endstone::CompoundTag &endstone_tag)
{
    if (!rust_tag.contains(key)) return;
    // Try each type in specific order — Bedrock NBT distinguishes Byte/Short/Int,
    // so we must try the narrower types FIRST to avoid type corruption.
    // (e.g. get_int also matches Short, turning ShortTag into IntTag silently)

    // Try Byte (narrowest)
    int8_t byte_val;
    if (bedrock_nbt_get_byte(rust_tag.get(), key.c_str(), &byte_val) == 0) {
        endstone_tag[key] = endstone::ByteTag(static_cast<uint8_t>(byte_val));
        return;
    }
    // Try Short
    int16_t short_val;
    if (bedrock_nbt_get_short(rust_tag.get(), key.c_str(), &short_val) == 0) {
        endstone_tag[key] = endstone::ShortTag(short_val);
        return;
    }
    // Try Int
    int32_t int_val;
    if (bedrock_nbt_get_int(rust_tag.get(), key.c_str(), &int_val) == 0) {
        endstone_tag[key] = endstone::IntTag(int_val);
        return;
    }
    // Try Long
    int64_t long_val;
    if (bedrock_nbt_get_long(rust_tag.get(), key.c_str(), &long_val) == 0) {
        endstone_tag[key] = endstone::LongTag(long_val);
        return;
    }
    // Try Float
    float float_val;
    if (bedrock_nbt_get_float(rust_tag.get(), key.c_str(), &float_val) == 0) {
        endstone_tag[key] = endstone::FloatTag(float_val);
        return;
    }
    // Try Double
    double double_val;
    if (bedrock_nbt_get_double(rust_tag.get(), key.c_str(), &double_val) == 0) {
        endstone_tag[key] = endstone::DoubleTag(double_val);
        return;
    }
    // Try String
    size_t str_len = 256;
    std::string str_buf(str_len, '\0');
    int ret_str = bedrock_nbt_get_string(rust_tag.get(), key.c_str(), str_buf.data(), &str_len);
    if (ret_str == BEDROCK_ERR_INVALID_ARG && str_len > 256) {
        str_buf.resize(str_len);
        ret_str = bedrock_nbt_get_string(rust_tag.get(), key.c_str(), str_buf.data(), &str_len);
    }
    if (ret_str == 0) {
        if (str_len > 0) str_buf.resize(str_len - 1);
        else str_buf.clear();
        endstone_tag[key] = endstone::StringTag(str_buf);
        return;
    }
    // Try Compound
    if (auto *child_ptr = bedrock_nbt_get_tag(rust_tag.get(), key.c_str())) {
        CompoundTag child(child_ptr);
        endstone::CompoundTag child_tag;
        rustNbtToEndstone(child, child_tag);
        endstone_tag[key] = std::move(child_tag);
        return;
    }
    // Try List
    int list_sz = bedrock_nbt_list_size(rust_tag.get(), key.c_str());
    if (list_sz >= 0) {
        endstone::ListTag list_tag;
        int elem_type = bedrock_nbt_list_get_element_type(rust_tag.get(), key.c_str());
        for (int i = 0; i < list_sz; i++) {
            if (elem_type == 8) {  // TAG_String
                size_t elem_len = 256;
                std::string elem_buf(elem_len, '\0');
                int r = bedrock_nbt_list_get_string_at(rust_tag.get(), key.c_str(), i, elem_buf.data(), &elem_len);
                if (r == BEDROCK_ERR_INVALID_ARG && elem_len > 256) {
                    elem_buf.resize(elem_len);
                    r = bedrock_nbt_list_get_string_at(rust_tag.get(), key.c_str(), i, elem_buf.data(), &elem_len);
                }
                if (r == 0) {
                    if (elem_len > 0) elem_buf.resize(elem_len - 1);
                    else elem_buf.clear();
                    list_tag.emplace_back(endstone::StringTag(elem_buf));
                }
            } else if (elem_type == 10) {  // TAG_Compound
                if (auto *elem_child = bedrock_nbt_list_get_tag_at(rust_tag.get(), key.c_str(), i)) {
                    CompoundTag elem_rust(elem_child);
                    endstone::CompoundTag elem_endstone;
                    rustNbtToEndstone(elem_rust, elem_endstone);
                    list_tag.emplace_back(std::move(elem_endstone));
                }
            }
        }
        endstone_tag[key] = std::move(list_tag);
        return;
    }
    // Try ByteArray
    uint8_t *ba_data = nullptr;
    size_t ba_len = 0;
    if (bedrock_nbt_get_byte_array(rust_tag.get(), key.c_str(), &ba_data, &ba_len) == 0) {
        std::vector<uint8_t> arr(ba_data, ba_data + ba_len);
        bedrock_free(ba_data);
        endstone_tag[key] = endstone::ByteArrayTag(std::move(arr));
        return;
    }
    // Try IntArray
    int32_t *ia_data = nullptr;
    size_t ia_len = 0;
    if (bedrock_nbt_get_int_array(rust_tag.get(), key.c_str(), &ia_data, &ia_len) == 0) {
        std::vector<int32_t> arr(ia_data, ia_data + ia_len);
        bedrock_free(ia_data);
        endstone_tag[key] = endstone::IntArrayTag(std::move(arr));
        return;
    }
}

void rustNbtToEndstone(const CompoundTag &rust_tag, endstone::CompoundTag &endstone_tag)
{
    size_t count = rust_tag.entryCount();
    // Collect all keys first using safe copy (avoids Rust pointer lifetime issues)
    std::vector<std::string> keys;
    keys.reserve(count);
    for (size_t i = 0; i < count; i++) {
        keys.push_back(rust_tag.entryKeyCopy(i));
    }
    for (const auto &key : keys) {
        copyRustTagToEndstone(rust_tag, key, endstone_tag);
    }
}
