#pragma once

#include "packets.h"

#include <endstone/endstone.hpp>

#include <memory>
#include <string>
#include <unordered_map>

class Menu;

// ==================== Per-player form tracking ====================

struct FormData {
    Menu *menu{};
    endstone::Player *player{};
    BlockPos pos;
    bool is_pair{};
};

/// Global registry of active forms (one per player).
inline std::unordered_map<std::string, std::unique_ptr<FormData>> &getActiveForms()
{
    static std::unordered_map<std::string, std::unique_ptr<FormData>> forms;
    return forms;
}

/// Remove the active form for a player (safe to call even if none exists).
inline void closeForm(const std::string &player_name)
{
    getActiveForms().erase(player_name);
}

/// Remove all active forms (called during plugin disable).
inline void clearActiveForms()
{
    getActiveForms().clear();
}
