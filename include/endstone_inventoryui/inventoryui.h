#pragma once

#include <endstone/endstone.hpp>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace inventoryui {

// ==================== Enums ====================

enum class MenuTypeId {
    CHEST,
    DOUBLE_CHEST,
    DISPENSER,
    HOPPER,
};

// ==================== UIInventory ====================

class UIInventory {
public:
    virtual ~UIInventory() = default;

    [[nodiscard]] virtual int get_size() const = 0;

    [[nodiscard]] virtual endstone::ItemStack get_item(int index) const = 0;

    virtual void set_item(int index, const endstone::ItemStack &item) = 0;

    /// Set a slot from pre-encoded binary NBT (bypasses ItemStack).
    /// Used for offline player inventory display via world-inspector.
    virtual void set_pre_encoded_item(int index, const std::string& type_id, int count, uint16_t damage, const std::vector<uint8_t>& nbt_bytes) = 0;

    [[nodiscard]] virtual int first_empty() const = 0;

    [[nodiscard]] virtual bool is_empty() const = 0;

    [[nodiscard]] virtual std::vector<endstone::ItemStack> get_contents() const = 0;

    virtual void clear() = 0;
};

// ==================== Menu ====================

class Menu {
public:
    virtual ~Menu() = default;

    // Callback types
    using SlotCallback = std::function<std::function<void()>(
        endstone::Player &player, int slot,
        const endstone::ItemStack &item,
        UIInventory &inventory)>;

    using PlayerCallback = std::function<void(endstone::Player &player)>;

    [[nodiscard]] virtual std::string get_name() const = 0;

    virtual void set_name(const std::string &name) = 0;

    [[nodiscard]] virtual MenuTypeId get_type() const = 0;

    virtual void set_listener(SlotCallback callback) = 0;

    virtual void set_open_listener(PlayerCallback callback) = 0;

    virtual void set_close_listener(PlayerCallback callback) = 0;

    virtual void send_to(endstone::Player &player) = 0;

    virtual bool close(endstone::Player &player) = 0;

    virtual void close_all() = 0;

    [[nodiscard]] virtual std::vector<std::shared_ptr<endstone::Player>> get_viewers() const = 0;

    [[nodiscard]] virtual std::shared_ptr<UIInventory> get_inventory() const = 0;
};

// ==================== InventoryUI (Service Factory) ====================

class InventoryUI : public endstone::Service {
public:
    ~InventoryUI() override = default;

    [[nodiscard]] virtual std::shared_ptr<Menu> create_menu(
        MenuTypeId type, const std::string &name = "") = 0;
};

} // namespace inventoryui
