#pragma once

#include "packets.h"

#include <endstone/endstone.hpp>

#include <string>
#include <unordered_map>

// ==================== Item Registry (cached from ItemRegistryPacket) ====================

/// Get all cached item data.
[[nodiscard]] std::unordered_map<std::string, ItemData> &allItemData();

/// Add an item to the registry.
void addItemData(const std::string &item_name, const ItemData &data);

/// Clear all cached item data (called on plugin disable).
void clearItemData();

/// Look up an item by name. Returns nullptr if not found.
[[nodiscard]] ItemData *getItemData(const std::string &item_name);

/// Get the runtime item ID for an item name.
/// Returns 0 if not found in registry.
[[nodiscard]] int16_t getItemRuntimeId(const std::string &item_name);

// ==================== Item NBT builder ====================

/// Build NBT data from an Endstone ItemStack's ItemMeta.
/// Returns empty vector if no metadata is present.
[[nodiscard]] std::vector<uint8_t> buildItemNbt(const endstone::ItemStack &item_stack);
