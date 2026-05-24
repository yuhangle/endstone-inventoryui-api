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

class Menu : public inventoryui::Menu {
public:
    static constexpr int CONTAINER_ID = 200;

    Menu(endstone::Plugin &plugin, MenuTypeId type, std::string name = "");
    ~Menu() override;

    Menu(const Menu &) = delete;
    Menu &operator=(const Menu &) = delete;

    // --- inventoryui::Menu interface (snake_case) ---

    std::string get_name() const override { return getName(); }
    void set_name(const std::string &name) override { setName(name); }

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

    std::vector<std::shared_ptr<endstone::Player>> get_viewers() const override
    {
        std::vector<std::shared_ptr<endstone::Player>> result;
        for (auto *p : getViewers()) {
            // We cannot create a shared_ptr from a raw Player reference
            // without ownership semantics; return empty for safety.
            (void)p;
        }
        return result;
    }

    std::shared_ptr<inventoryui::UIInventory> get_inventory() const override
    {
        return inventory_;
    }

    // --- Internal API (camelCase) ---

    std::string getName() const { return name_; }
    void setName(const std::string &name) { name_ = name; }

    MenuTypeId getType() const { return type_; }

    std::shared_ptr<UIInventory> getInventory() const { return inventory_; }

    void setListener(SlotCallback callback) { listener_ = std::move(callback); }
    void setOpenListener(PlayerCallback callback) { open_listener_ = std::move(callback); }
    void setCloseListener(PlayerCallback callback) { close_listener_ = std::move(callback); }

    const SlotCallback &getListener() const { return listener_; }
    const PlayerCallback &getOpenListener() const { return open_listener_; }
    const PlayerCallback &getCloseListener() const { return close_listener_; }

    // --- Internal helpers ---

    void sendContents(const endstone::Player &player) const;

    // --- Actions (camelCase aliases) ---

    void sendTo(endstone::Player &player);
    void closeAll() const;

    std::vector<endstone::Player *> getViewers() const;

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
