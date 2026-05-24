#include "inventory.h"

#include <endstone/endstone.hpp>

// ==================== UIInventory ====================

UIInventory::UIInventory(const int size, const int max_stack_size, SlotCallback slot_updated)
    : size_(size), max_stack_size_(max_stack_size),
      slots_(size, endstone::ItemStack("minecraft:air")),
      pre_encoded_(size, std::nullopt),
      slot_updated_(std::move(slot_updated))
{
}

void UIInventory::notifySlot(const int index) const {
    if (slot_updated_ && isValidIndex(index)) {
        slot_updated_(index);
    }
}

endstone::ItemStack UIInventory::getItem(const int index) const
{
    if (!isValidIndex(index)) {
        throw std::out_of_range("Slot index " + std::to_string(index) + " out of range");
    }
    return slots_[index];
}

void UIInventory::setItem(const int index, const endstone::ItemStack &item)
{
    if (!isValidIndex(index)) {
        throw std::out_of_range("Slot index " + std::to_string(index) + " out of range");
    }
    slots_[index] = item;
    notifySlot(index);
}

int UIInventory::firstEmpty() const
{
    for (int i = 0; i < size_; ++i) {
        if (slots_[i].getType().getId() == "minecraft:air") {
            return i;
        }
    }
    return -1;
}

bool UIInventory::isEmpty() const
{
    for (int i = 0; i < size_; ++i) {
        if (slots_[i].getType().getId() != "minecraft:air") {
            return false;
        }
    }
    return true;
}

std::vector<endstone::ItemStack> UIInventory::getContents() const
{
    return slots_;
}

void UIInventory::clear(const int index)
{
    if (index < 0) {
        for (int i = 0; i < size_; ++i) {
            if (slots_[i].getType().getId() != "minecraft:air") {
                slots_[i] = endstone::ItemStack("minecraft:air");
                notifySlot(i);
            }
        }
    } else if (isValidIndex(index)) {
        if (slots_[index].getType().getId() != "minecraft:air") {
            slots_[index] = endstone::ItemStack("minecraft:air");
            notifySlot(index);
        }
    }
}

std::unordered_map<int, endstone::ItemStack> UIInventory::addItem(const std::vector<endstone::ItemStack> &items)
{
    std::unordered_map<int, endstone::ItemStack> leftover;
    for (size_t i = 0; i < items.size(); ++i) {
        auto remaining = items[i];
        if (remaining.getType().getId() == "minecraft:air" || remaining.getAmount() <= 0) {
            continue;
        }

        // Try to stack with existing items
        for (int slot_idx = 0; slot_idx < size_ && remaining.getAmount() > 0; ++slot_idx) {
            auto &slot = slots_[slot_idx];
            if (slot.getType().getId() != "minecraft:air" && slot.isSimilar(remaining)) {
                int max_add = std::min(remaining.getAmount(),
                    std::min(max_stack_size_, slot.getMaxStackSize()) - slot.getAmount());
                if (max_add > 0) {
                    slot.setAmount(slot.getAmount() + max_add);
                    notifySlot(slot_idx);
                    remaining.setAmount(remaining.getAmount() - max_add);
                }
            }
        }

        // Fill empty slots
        for (int slot_idx = 0; slot_idx < size_ && remaining.getAmount() > 0; ++slot_idx) {
          if (auto &slot = slots_[slot_idx];
              slot.getType().getId() == "minecraft:air") {
            const int max_add = std::min(remaining.getAmount(), std::min(max_stack_size_, remaining.getMaxStackSize()));
            const auto new_item = endstone::ItemStack(remaining.getType().getId(), max_add, remaining.getData());
                slot = new_item;
                notifySlot(slot_idx);
                remaining.setAmount(remaining.getAmount() - max_add);
            }
        }

        if (remaining.getAmount() > 0) {
            leftover.emplace(static_cast<int>(i), remaining);
        }
    }
    return leftover;
}

std::unordered_map<int, endstone::ItemStack> UIInventory::removeItem(const std::vector<endstone::ItemStack> &items)
{
    std::unordered_map<int, endstone::ItemStack> leftover;
    for (size_t i = 0; i < items.size(); ++i) {
        const auto& item = items[i];
        if (item.getType().getId() == "minecraft:air" || item.getAmount() <= 0) {
            continue;
        }
        int to_remove = item.getAmount();
        for (int slot_idx = 0; slot_idx < size_ && to_remove > 0; ++slot_idx) {
          if (auto &slot = slots_[slot_idx];
              slot.getType().getId() != "minecraft:air" && slot.isSimilar(item)) {
            const int removed = std::min(to_remove, slot.getAmount());
                slot.setAmount(slot.getAmount() - removed);
                to_remove -= removed;
                if (slot.getAmount() <= 0) {
                    slot = endstone::ItemStack("minecraft:air");
                }
                notifySlot(slot_idx);
            }
        }
        if (to_remove > 0) {
            auto leftover_item = endstone::ItemStack(item.getType().getId(), to_remove, item.getData());
            leftover.emplace(static_cast<int>(i), leftover_item);
        }
    }
    return leftover;
}

bool UIInventory::contains(const std::string &type) const
{
    for (int i = 0; i < size_; ++i) {
        if (slots_[i].getType().getId() == type) {
            return true;
        }
    }
    return false;
}

bool UIInventory::contains(const endstone::ItemStack &item, int amount) const
{
    if (amount < 0) {
        for (int i = 0; i < size_; ++i) {
            if (slots_[i] == item) return true;
        }
        return false;
    }
    int count = 0;
    for (int i = 0; i < size_; ++i) {
        if (slots_[i] == item) ++count;
    }
    return count >= amount;
}

bool UIInventory::containsAtLeast(const std::string &type, int amount) const
{
    int total = 0;
    for (int i = 0; i < size_; ++i) {
        if (slots_[i].getType().getId() == type) {
            total += slots_[i].getAmount();
        }
    }
    return total >= amount;
}

bool UIInventory::containsAtLeast(const endstone::ItemStack &item, int amount) const
{
    int total = 0;
    for (int i = 0; i < size_; ++i) {
        if (slots_[i].isSimilar(item)) {
            total += slots_[i].getAmount();
        }
    }
    return total >= amount;
}

std::unordered_map<int, endstone::ItemStack> UIInventory::all(const std::string &type) const
{
    std::unordered_map<int, endstone::ItemStack> result;
    for (int i = 0; i < size_; ++i) {
        if (slots_[i].getType().getId() == type) {
            result.emplace(i, slots_[i]);
        }
    }
    return result;
}

std::unordered_map<int, endstone::ItemStack> UIInventory::all(const endstone::ItemStack &item) const
{
    std::unordered_map<int, endstone::ItemStack> result;
    for (int i = 0; i < size_; ++i) {
        if (slots_[i] == item) {
            result.emplace(i, slots_[i]);
        }
    }
    return result;
}

int UIInventory::first(const std::string &type) const
{
    for (int i = 0; i < size_; ++i) {
        if (slots_[i].getType().getId() == type) return i;
    }
    return -1;
}

int UIInventory::first(const endstone::ItemStack &item) const
{
    for (int i = 0; i < size_; ++i) {
        if (slots_[i] == item) return i;
    }
    return -1;
}

void UIInventory::remove(const std::string &type)
{
    for (int i = 0; i < size_; ++i) {
        if (slots_[i].getType().getId() == type) {
            slots_[i] = endstone::ItemStack("minecraft:air");
            notifySlot(i);
        }
    }
}

void UIInventory::remove(const endstone::ItemStack &item)
{
    for (int i = 0; i < size_; ++i) {
        if (slots_[i] == item) {
            slots_[i] = endstone::ItemStack("minecraft:air");
            notifySlot(i);
        }
    }
}

// ── Pre-encoded items ──

void UIInventory::setPreEncodedItem(int index, std::string type_id, int count, uint16_t damage, std::vector<uint8_t> nbt_bytes) {
    if (!isValidIndex(index)) return;
    pre_encoded_[index] = PreEncodedItem{std::move(type_id), count, damage, std::move(nbt_bytes)};
}

bool UIInventory::hasPreEncodedItem(int index) const {
    return isValidIndex(index) && pre_encoded_[index].has_value();
}

const PreEncodedItem* UIInventory::getPreEncodedItem(int index) const {
    if (!isValidIndex(index)) return nullptr;
    const auto& opt = pre_encoded_[index];
    return opt.has_value() ? &opt.value() : nullptr;
}
