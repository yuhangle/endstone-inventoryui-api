#pragma once

#include <string>

// ==================== MenuTypeId (matches inventoryui.h) ====================

enum class MenuTypeId {
    CHEST,
    DOUBLE_CHEST,
    DISPENSER,
    HOPPER,
};

// ==================== MenuType data container ====================

struct MenuTypeData {
    bool is_pair;
    std::string block_id;
    int container_type;
    std::string block_actor_id;
    int container_size;
};

inline const MenuTypeData &getMenuTypeData(MenuTypeId type)
{
    static const MenuTypeData data[] = {
        {false, "minecraft:chest",    0x0, "Chest",    27},  // CHEST
        {true,  "minecraft:chest",    0x0, "Chest",    54},  // DOUBLE_CHEST
        {false, "minecraft:dispenser", 0x6, "Dispenser", 9},  // DISPENSER
        {false, "minecraft:hopper",   0x8, "Hopper",    5},  // HOPPER
    };
    return data[static_cast<int>(type)];
}
