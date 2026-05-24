#pragma once

#include "menu.h"

#include <endstone_inventoryui/inventoryui.h>

// ==================== InventoryUIService — factory ====================

class InventoryUIService : public inventoryui::InventoryUI {
public:
    explicit InventoryUIService(endstone::Plugin &plugin) : plugin_(plugin) {}

    std::shared_ptr<inventoryui::Menu> create_menu(
        inventoryui::MenuTypeId type, const std::string &name = "") override
    {
        auto internal_type = static_cast<MenuTypeId>(static_cast<int>(type));
        return std::make_shared<Menu>(plugin_, internal_type, name);
    }

private:
    endstone::Plugin &plugin_;
};
