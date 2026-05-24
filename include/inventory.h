#pragma once

#include <endstone/endstone.hpp>
#include <endstone_inventoryui/inventoryui.h>

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

// ==================== PreEncodedItem ====================

/// An inventory item with pre-serialized binary NBT, bypassing endstone::ItemStack.
struct PreEncodedItem {
    std::string type_id;
    int count = 0;
    uint16_t damage = 0;
    std::vector<uint8_t> nbt_bytes;
};

// ==================== UIInventory ====================

class UIInventory : public inventoryui::UIInventory {
public:
    using SlotCallback = std::function<void(int slot)>;

    explicit UIInventory(int size, int max_stack_size = 64, SlotCallback slot_updated = nullptr);
    ~UIInventory() override = default;

    UIInventory(const UIInventory &) = delete;
    UIInventory &operator=(const UIInventory &) = delete;
    UIInventory(UIInventory &&) = default;
    UIInventory &operator=(UIInventory &&) = default;

    // --- inventoryui::UIInventory interface (snake_case) ---

    [[nodiscard]] int get_size() const override { return getSize(); }
    [[nodiscard]] endstone::ItemStack get_item(const int index) const override { return getItem(index); }
    void set_item(const int index, const endstone::ItemStack &item) override { setItem(index, item); }
    void set_pre_encoded_item(const int index, const std::string& type_id,
                              const int count, const uint16_t damage, const std::vector<uint8_t>& nbt_bytes) override {
        setPreEncodedItem(index, type_id, count, damage, nbt_bytes);
    }
    [[nodiscard]] int first_empty() const override { return firstEmpty(); }
    [[nodiscard]] bool is_empty() const override { return isEmpty(); }
    [[nodiscard]] std::vector<endstone::ItemStack> get_contents() const override { return getContents(); }
    void clear() override { clear(-1); }

    // --- Internal API (camelCase) ---

    [[nodiscard]] int getSize() const { return size_; }
    [[nodiscard]] int getMaxStackSize() const { return max_stack_size_; }

    [[nodiscard]] endstone::ItemStack getItem(int index) const;
    void setItem(int index, const endstone::ItemStack &item);

    [[nodiscard]] int firstEmpty() const;
    [[nodiscard]] bool isEmpty() const;
    [[nodiscard]] std::vector<endstone::ItemStack> getContents() const;
    void clear(int index = -1);

    // -- higher-level inventory ops (from Python) --

    [[nodiscard]] std::unordered_map<int, endstone::ItemStack> addItem(const std::vector<endstone::ItemStack> &items);
    [[nodiscard]] std::unordered_map<int, endstone::ItemStack> removeItem(const std::vector<endstone::ItemStack> &items);

    [[nodiscard]] bool contains(const std::string &type) const;
    [[nodiscard]] bool contains(const endstone::ItemStack &item, int amount = -1) const;
    [[nodiscard]] bool containsAtLeast(const std::string &type, int amount) const;
    [[nodiscard]] bool containsAtLeast(const endstone::ItemStack &item, int amount) const;

    [[nodiscard]] std::unordered_map<int, endstone::ItemStack> all(const std::string &type) const;
    [[nodiscard]] std::unordered_map<int, endstone::ItemStack> all(const endstone::ItemStack &item) const;

    [[nodiscard]] int first(const std::string &type) const;
    [[nodiscard]] int first(const endstone::ItemStack &item) const;

    void remove(const std::string &type);
    void remove(const endstone::ItemStack &item);

    void setSlotUpdated(SlotCallback cb) { slot_updated_ = std::move(cb); }

    // -- Pre-encoded items (bypass endstone::ItemStack) --

    /// Set a slot from pre-encoded binary NBT data (from world-inspector).
    void setPreEncodedItem(int index, std::string type_id, int count, uint16_t damage, std::vector<uint8_t> nbt_bytes);

    /// Check if a slot has pre-encoded data.
    [[nodiscard]] bool hasPreEncodedItem(int index) const;

    /// Get pre-encoded item for a slot, or nullptr.
    [[nodiscard]] const PreEncodedItem* getPreEncodedItem(int index) const;

private:
    int size_;
    int max_stack_size_;
    std::vector<endstone::ItemStack> slots_;
    std::vector<std::optional<PreEncodedItem>> pre_encoded_;
    SlotCallback slot_updated_;

    void notifySlot(int index) const;
    [[nodiscard]] bool isValidIndex(int index) const { return index >= 0 && index < size_; }

    friend class ItemStackWrapper;
};
