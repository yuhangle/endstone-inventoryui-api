#include "item_registry.h"
#include "bedrock_nbt.h"
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
