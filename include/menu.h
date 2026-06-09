#pragma once

#include "inventory.h"
#include "menu_type.h"

#include <endstone/endstone.hpp>
#include <endstone_inventoryui/inventoryui.h>

#include <functional>
#include <memory>
#include <string>
#include <vector>

// ==================== Menu ====================

class Menu : public inventoryui::Menu, public std::enable_shared_from_this<Menu> {
public:
    static constexpr int CONTAINER_ID = 200;

    Menu(endstone::Plugin &plugin, MenuTypeId type, std::string name = "");
    ~Menu() override;

    Menu(const Menu &) = delete;
    Menu &operator=(const Menu &) = delete;

    // --- inventoryui::Menu interface (snake_case) ---

    std::string get_name() const override { return name_; }
    void set_name(const std::string &name) override { name_ = name; }

    inventoryui::MenuTypeId get_type() const override
    {
        return static_cast<inventoryui::MenuTypeId>(static_cast<int>(type_));
    }

    void set_listener(SlotCallback callback) override { listener_ = std::move(callback); }
    void set_open_listener(PlayerCallback callback) override { open_listener_ = std::move(callback); }
    void set_close_listener(PlayerCallback callback) override { close_listener_ = std::move(callback); }

    void send_to(endstone::Player &player) override;
    bool close(endstone::Player &player) override;
    void close_all() override;

    void refresh_contents(endstone::Player &player) override { sendContents(player); }

    std::vector<std::shared_ptr<endstone::Player>> get_viewers() const override
    {
        // Not implemented — callers should use close_player / ActiveForms directly.
        return {};
    }

    std::shared_ptr<inventoryui::UIInventory> get_inventory() const override
    {
        return inventory_;
    }

    // --- Internal API (camelCase) ---

    const SlotCallback &getListener() const { return listener_; }
    const PlayerCallback &getCloseListener() const { return close_listener_; }

    // --- Internal helpers ---

    void sendContents(const endstone::Player &player) const;

    // --- Actions (camelCase aliases) ---

    void sendTo(endstone::Player &player, const std::shared_ptr<Menu>& self) const;
    void closeAll() const;

    UIInventory &getInventoryRef() const { return *inventory_; }

private:
    endstone::Plugin &plugin_;
    std::string name_;
    MenuTypeId type_;
    std::shared_ptr<UIInventory> inventory_;
    SlotCallback listener_;
    PlayerCallback open_listener_;
    PlayerCallback close_listener_;
};
