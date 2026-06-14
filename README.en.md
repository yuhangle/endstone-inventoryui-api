# InventoryUI

[中文说明](README.md)

A virtual container UI plugin for Endstone. Supports two usage modes:

- **Embedded mode** — Compiled as a static library linked into the host plugin, no separate `.so` required
- **Standalone plugin mode** — Compiled as an independent plugin, called by other plugins via Endstone ServiceManager

## Project Structure

```
include/
├── bedrock_ffi.h          ← Rust C FFI declarations (35 NBT functions)
├── bedrock_stream.h       ← BinaryStream RAII wrapper
├── bedrock_nbt.h          ← CompoundTag RAII wrapper (fromBinaryNbt, enum, type readers)
├── packets.h              ← Packet type definitions
├── item_registry.h        ← Item registry + Rust→Endstone NBT converter
├── menu_type.h            ← Menu type enum + properties
├── inventory.h            ← UIInventory implementation
├── menu.h                 ← Menu interface
├── form_tracker.h         ← Per-player form tracking
├── listener.h             ← Event listener
├── service.h              ← Service factory
├── utils.h                ← Utility functions
├── inventoryui_init.h     ← Embedded mode init API (initialize_embedded/create_menu/shutdown)
└── endstone_inventoryui/
    └── inventoryui.h      ← Public API (Menu, UIInventory, MenuTypeId)
src/
├── inventoryui_init.cpp   ← Embedded mode init implementation
├── plugin.cpp             ← Plugin entry (standalone mode)
├── packets.cpp            ← Packet serialization/deserialization
├── menu.cpp               ← Menu implementation
├── inventory.cpp          ← UIInventory implementation (pre-encoded NBT → ItemStack)
├── listener.cpp           ← Event handling (ItemStackRequest packet interception)
└── item_registry.cpp      ← Item registry + Rust↔Endstone NBT conversion
examples/cpp/              ← Usage examples
```

## Building

### Dependencies

- Clang 18+ / MSVC 2022+
- CMake 3.15+ / Ninja
- bedrock-protocol-rs

### Standalone Plugin Mode

```bash
cd /path/to/endstone-inventoryui
CC=clang CXX=clang++ cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Output: `build/endstone_inventoryui.so`

Deploy: `cp build/endstone_inventoryui.so /path/to/bedrock_server/plugins/`

### Embedded Mode (Static Library)

```bash
cd /path/to/endstone-inventoryui
mkdir build-static && cd build-static
CC=clang CXX=clang++ cmake .. -DCMAKE_BUILD_TYPE=Release -DINVENTORYUI_BUILD_AS_STATIC=ON
cmake --build .
```

Output: `build-static/libendstone_inventoryui.a`

## Usage

### Mode 1: Pure Embedded

Compile inventoryui as a static library linked into the host plugin.

#### CMake Integration

```cmake
set(INVENTORYUI_BUILD_AS_STATIC ON)

# Via local path
add_subdirectory(
    "${CMAKE_CURRENT_SOURCE_DIR}/libs/endstone-inventoryui"
    "${CMAKE_BINARY_DIR}/endstone_inventoryui"
)
target_link_libraries(your_plugin PRIVATE endstone_inventoryui)
```

Or with FetchContent:

```cmake
set(INVENTORYUI_BUILD_AS_STATIC ON)
FetchContent_Declare(
    endstone_inventoryui
    SOURCE_DIR /path/to/endstone-inventoryui
)
FetchContent_MakeAvailable(endstone_inventoryui)
target_link_libraries(your_plugin PRIVATE endstone_inventoryui)
```

#### Plugin Code

```cpp
#include <inventoryui_init.h>

void YourPlugin::onEnable()
{
    inventoryui::initialize_embedded(*this);

    auto menu = inventoryui::create_menu(inventoryui::MenuTypeId::CHEST, "Embedded");
    if (menu) {
        auto inv = menu->get_inventory();
        inv->set_item(0, endstone::ItemStack("minecraft:diamond"));
        menu->send_to(*player);
    }
}

void YourPlugin::onDisable()
{
    inventoryui::shutdown();
}
```

### Mode 2: Standalone Plugin

Deploy `endstone_inventoryui.so` to the server; other plugins call it via ServiceManager.

#### Dependency Declaration

```cpp
ENDSTONE_PLUGIN("shop", "1.0.0", ShopPlugin)
{
    depend = {"inventoryui_api"};
}
```

#### CMake Integration

```cmake
FetchContent_Declare(
    endstone_inventoryui
    GIT_REPOSITORY https://github.com/yuhangle/endstone-inventoryui-api.git
    GIT_TAG main
)
FetchContent_MakeAvailable(endstone_inventoryui)
target_link_libraries(shop PRIVATE endstone_inventoryui)
```

#### Plugin Code

```cpp
#include <endstone_inventoryui/inventoryui.h>

void YourPlugin::onEnable()
{
    auto inventory_ui = getServer().getServiceManager().load<inventoryui::InventoryUI>("InventoryUIAPI");
    if (!inventory_ui) {
        getLogger().warning("InventoryUI not available!");
        return;
    }

    auto menu = inventory_ui->create_menu(inventoryui::MenuTypeId::CHEST, "Shop");
    auto inv = menu->get_inventory();
    inv->set_item(0, endstone::ItemStack("minecraft:diamond_sword"));
    menu->send_to(*player);
}
```

## C++ API

### Creating a Menu

```cpp
auto menu = inventory_ui->create_menu(inventoryui::MenuTypeId::CHEST, "Shop");
// MenuTypeId::CHEST        — 27 slots
// MenuTypeId::DOUBLE_CHEST — 54 slots
// MenuTypeId::DISPENSER    — 9 slots
// MenuTypeId::HOPPER       — 5 slots
```

Equivalent in embedded mode:

```cpp
auto menu = inventoryui::create_menu(inventoryui::MenuTypeId::CHEST, "Shop");
```

### Setting Items

```cpp
auto inv = menu->get_inventory();
inv->set_item(0, endstone::ItemStack("minecraft:diamond_sword"));
inv->set_item(1, endstone::ItemStack("minecraft:diamond", 64));
```

### Pre-encoded Items (Bypassing ItemStack)

For offline player inventory display and similar scenarios, directly use pre-serialized binary NBT data from external sources, bypassing `endstone::ItemStack` construction and NBT conversion, preserving all NBT attributes.

```cpp
auto inv = menu->get_inventory();
inv->set_pre_encoded_item(slot, "minecraft:diamond_sword", 1, 0, nbt_bytes);
```

> Slots using `set_pre_encoded_item()` will prioritize pre-encoded data during `sendContents()`, ignoring the `ItemStack` in the same slot.

#### Click Callback with Full NBT

When a pre-encoded slot is clicked, `getItem()` automatically deserializes the pre-encoded data into a complete `endstone::ItemStack` (with NBT metadata). Conversion flow:

```
Pre-encoded NBT bytes → Rust CompoundTag(fromBinaryNbt)
                      → Endstone CompoundTag(rustNbtToEndstone)
                      → endstone::ItemStack.setNbt()
```

The `item` parameter in the `set_listener()` callback is fully readable and writable, just like a regular ItemStack.

### Registering Callbacks

```cpp
// Click callback
// Returns std::function<void()>: non-empty closes the inventory UI and
//   executes the returned function after a 10-tick delay
// Returns empty function (default): keeps the inventory open and auto-refreshes display
//   (useful for dynamic inventory modification scenarios)
menu->set_listener(
    [](endstone::Player &player, int slot,
       const endstone::ItemStack &item,
       inventoryui::UIInventory &inventory) -> std::function<void()> {
        player.sendMessage("slot {}: {}", slot, item.getType().getId());
        return {};  // Keep inventory open, auto-refresh display
    });

// Example: close inventory and open a new form
menu->set_listener(
    [&plugin](endstone::Player &player, int slot,
              const endstone::ItemStack &item,
              inventoryui::UIInventory &inventory) -> std::function<void()> {
        auto item_data = ItemSerializer::fromItemStack(item);
        // Return deferred task: executed 10 ticks after inventory closes
        return [&plugin, player_name = player.getName(), item_data]() {
            auto *p = plugin.getServer().getPlayer(player_name);
            if (!p) return;
            // Send ModalForm or other forms here
            endstone::ModalForm form;
            form.setTitle("Confirm");
            p->sendForm(form);
        };
    });

// Open callback
menu->set_open_listener([](endstone::Player &player) {
    player.sendMessage("Menu opened");
});

// Close callback (triggered when the player manually closes the inventory)
menu->set_close_listener([](endstone::Player &player) {
    player.sendMessage("Menu closed");
});

// Player inventory callback (triggered when player clicks their own inventory while UI is open)
// container_id: 28=HotbarContainer, 29=InventoryContainer, 12=Combined, 59=Cursor
menu->set_player_inventory_listener(
    [](endstone::Player &player, int slot,
       const endstone::ItemStack &item,
       int container_id) -> std::function<void()> {
        player.sendMessage("slot {}: {} [container={}]", slot, item.getType().getId(), container_id);
        return {};  // Keep inventory open
    });
```

### Dynamic Inventory Refresh

When the callback returns an empty function, the inventory automatically refreshes its display. This is useful for scenarios requiring dynamic inventory modification, such as taking items, purchasing goods, etc.:

```cpp
menu->set_listener(
    [&some_inventory](endstone::Player &player, int slot,
                      const endstone::ItemStack &item,
                      inventoryui::UIInventory &inventory) -> std::function<void()> {
        // Move item from some inventory to player's inventory
        auto& playerInv = player.getInventory();
        auto remaining = playerInv.addItem(item);
        
        if (remaining.empty()) {
            // Success: remove item from UI
            some_inventory.removeItem(std::vector{item});
            
            // Update UI display (set_item triggers auto-refresh)
            if (auto newItem = some_inventory.getItem(slot); newItem.has_value()) {
                inventory.set_item(slot, newItem.value());
            } else {
                inventory.set_item(slot, endstone::ItemStack("minecraft:air"));
            }
            
            player.sendMessage("Item taken successfully!");
        } else {
            player.sendMessage("Inventory full!");
        }
        
        return {};  // Keep inventory open, auto-refresh display
    });
```

### Sending and Closing

```cpp
menu->send_to(player);       // Send to player
menu->close(player);         // Close for a single player
menu->close_all();           // Close for all players
```

### Querying State

```cpp
auto viewers = menu->get_viewers();  // Current viewers
auto inv = menu->get_inventory();    // Inventory reference
```

## Complete Example

See [`examples/cpp/plugin.cpp`](examples/cpp/plugin.cpp).

## License

Apache License 2.0
