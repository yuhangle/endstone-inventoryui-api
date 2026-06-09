# InventoryUI

[English](README.en.md)

Endstone 虚拟容器界面插件。支持两种使用方式：

- **内嵌模式** — 编译为静态库嵌入宿主插件，不依赖单独的 `.so`
- **外置插件模式** — 编译为独立插件，其他插件通过 Endstone ServiceManager 调用

## 目录结构

```
include/
├── bedrock_ffi.h          ← Rust C FFI 声明（35 个 NBT 函数）
├── bedrock_stream.h       ← BinaryStream RAII 包装
├── bedrock_nbt.h          ← CompoundTag RAII 包装（含 fromBinaryNbt、枚举、类型读取）
├── packets.h              ← 数据包类型定义
├── item_registry.h        ← 物品注册表 + Rust→Endstone NBT 转换器
├── menu_type.h            ← 菜单类型枚举 + 属性
├── inventory.h            ← UIInventory 实现
├── menu.h                 ← Menu 接口
├── form_tracker.h         ← 玩家表单追踪
├── listener.h             ← 事件监听器
├── service.h              ← 服务工厂
├── utils.h                ← 工具函数
├── inventoryui_init.h     ← 内嵌模式初始化 API (initialize_embedded/create_menu/shutdown)
└── endstone_inventoryui/
    └── inventoryui.h      ← 公共 API 接口（Menu、UIInventory、MenuTypeId）
src/
├── inventoryui_init.cpp   ← 内嵌模式初始化实现
├── plugin.cpp             ← 插件入口（外置插件模式）
├── packets.cpp            ← 数据包序列化/反序列化
├── menu.cpp               ← Menu 实现
├── inventory.cpp          ← UIInventory 实现（含预编码 NBT → ItemStack）
├── listener.cpp           ← 事件处理（ItemStackRequest 包拦截）
└── item_registry.cpp      ← 物品注册表 + Rust↔Endstone NBT 双向转换
examples/cpp/              ← 使用示例
```

## 构建

### 依赖

- Clang 18+ / MSVC 2022+
- CMake 3.15+ / Ninja
- bedrock-protocol-rs

### 外置插件模式

```bash
cd /path/to/endstone-inventoryui
CC=clang CXX=clang++ cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

产物：`build/endstone_inventoryui.so`

部署：`cp build/endstone_inventoryui.so /path/to/bedrock_server/plugins/`

### 内嵌模式（静态库）

```bash
cd /path/to/endstone-inventoryui
mkdir build-static && cd build-static
CC=clang CXX=clang++ cmake .. -DCMAKE_BUILD_TYPE=Release -DINVENTORYUI_BUILD_AS_STATIC=ON
cmake --build .
```

产物：`build-static/libendstone_inventoryui.a`

## 使用方式

### 方式一：纯内嵌

将 inventoryui 编译为静态库嵌入宿主插件。

#### CMake 集成

```cmake
set(INVENTORYUI_BUILD_AS_STATIC ON)

# 通过本地路径引入
add_subdirectory(
    "${CMAKE_CURRENT_SOURCE_DIR}/libs/endstone-inventoryui"
    "${CMAKE_BINARY_DIR}/endstone_inventoryui"
)
target_link_libraries(your_plugin PRIVATE endstone_inventoryui)
```

或用 FetchContent：

```cmake
set(INVENTORYUI_BUILD_AS_STATIC ON)
FetchContent_Declare(
    endstone_inventoryui
    SOURCE_DIR /path/to/endstone-inventoryui
)
FetchContent_MakeAvailable(endstone_inventoryui)
target_link_libraries(your_plugin PRIVATE endstone_inventoryui)
```

#### 插件代码

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

### 方式二：外置插件

将 `endstone_inventoryui.so` 部署到服务端，其他插件通过 ServiceManager 调用。

#### 依赖声明

```cpp
ENDSTONE_PLUGIN("shop", "1.0.0", ShopPlugin)
{
    depend = {"inventoryui_api"};
}
```

#### CMake 集成

```cmake
FetchContent_Declare(
    endstone_inventoryui
    GIT_REPOSITORY https://github.com/yuhangle/endstone-inventoryui-api.git
    GIT_TAG main
)
FetchContent_MakeAvailable(endstone_inventoryui)
target_link_libraries(shop PRIVATE endstone_inventoryui)
```

#### 插件代码

```cpp
#include <endstone_inventoryui/inventoryui.h>

void YourPlugin::onEnable()
{
    auto inventory_ui = getServer().getServiceManager().load<inventoryui::InventoryUI>("InventoryUIAPI");
    if (!inventory_ui) {
        getLogger().warning("InventoryUI not available!");
        return;
    }

    auto menu = inventory_ui->create_menu(inventoryui::MenuTypeId::CHEST, "商店");
    auto inv = menu->get_inventory();
    inv->set_item(0, endstone::ItemStack("minecraft:diamond_sword"));
    menu->send_to(*player);
}
```

## C++ API

### 创建菜单

```cpp
auto menu = inventory_ui->create_menu(inventoryui::MenuTypeId::CHEST, "商店");
// MenuTypeId::CHEST        — 27 格
// MenuTypeId::DOUBLE_CHEST — 54 格
// MenuTypeId::DISPENSER    — 9 格
// MenuTypeId::HOPPER       — 5 格
```

内嵌模式下的等价写法：

```cpp
auto menu = inventoryui::create_menu(inventoryui::MenuTypeId::CHEST, "商店");
```

### 设置物品

```cpp
auto inv = menu->get_inventory();
inv->set_item(0, endstone::ItemStack("minecraft:diamond_sword"));
inv->set_item(1, endstone::ItemStack("minecraft:diamond", 64));
```

### 预编码物品（跳过 ItemStack）

用于离线玩家背包展示等场景，直接从外部来源获取预序列化的二进制 NBT 数据，绕过 `endstone::ItemStack` 构造和 NBT 转换，保留所有 NBT 属性。

```cpp
auto inv = menu->get_inventory();
inv->set_pre_encoded_item(slot, "minecraft:diamond_sword", 1, 0, nbt_bytes);
```

> 使用 `set_pre_encoded_item()` 的槽位在 `sendContents()` 时会优先使用预编码数据，忽略同槽位的 `ItemStack`。

#### 点击回调获取完整 NBT

预编码槽位被点击时，`getItem()` 会自动将预编码数据反序列化为完整的 `endstone::ItemStack`（含 NBT 元数据）。转换流程：

```
预编码 NBT 字节 → Rust CompoundTag(fromBinaryNbt)
                → Endstone CompoundTag(rustNbtToEndstone)
                → endstone::ItemStack.setNbt()
```

因此 `set_listener()` 回调中拿到的 `item` 参数与普通 ItemStack 一样可读可写。

### 注册回调

```cpp
// 点击回调
// 返回 std::function<void()>：非空时关闭物品栏 UI 并延迟 10 tick 执行返回的函数
// 返回空函数（默认）：不关闭物品栏，且自动刷新物品栏显示（适用于动态修改物品的场景）
menu->set_listener(
    [](endstone::Player &player, int slot,
       const endstone::ItemStack &item,
       inventoryui::UIInventory &inventory) -> std::function<void()> {
        player.sendMessage("slot {}: {}", slot, item.getType().getId());
        return {};  // 不关闭物品栏，自动刷新显示
    });

// 需要关闭物品栏并打开新表单的示例
menu->set_listener(
    [&plugin](endstone::Player &player, int slot,
              const endstone::ItemStack &item,
              inventoryui::UIInventory &inventory) -> std::function<void()> {
        auto item_data = ItemSerializer::fromItemStack(item);
        // 返回延迟任务：物品栏关闭后 10 tick 执行
        return [&plugin, player_name = player.getName(), item_data]() {
            auto *p = plugin.getServer().getPlayer(player_name);
            if (!p) return;
            // 在此处发送 ModalForm 等新表单
            endstone::ModalForm form;
            form.setTitle("确认");
            p->sendForm(form);
        };
    });

// 打开回调
menu->set_open_listener([](endstone::Player &player) {
    player.sendMessage("Menu opened");
});

// 关闭回调（由客户端主动关闭时触发）
menu->set_close_listener([](endstone::Player &player) {
    player.sendMessage("Menu closed");
});
```

### 动态刷新物品栏

当回调返回空函数时，物品栏会自动刷新显示。这适用于需要动态修改物品栏内容的场景，例如取出物品、购买商品等：

```cpp
menu->set_listener(
    [&some_inventory](endstone::Player &player, int slot,
                      const endstone::ItemStack &item,
                      inventoryui::UIInventory &inventory) -> std::function<void()> {
        // 将物品从某个库存移到玩家背包
        auto& playerInv = player.getInventory();
        auto remaining = playerInv.addItem(item);
        
        if (remaining.empty()) {
            // 成功：从UI中移除物品
            some_inventory.removeItem(std::vector{item});
            
            // 更新UI显示（set_item 会触发自动刷新）
            if (auto newItem = some_inventory.getItem(slot); newItem.has_value()) {
                inventory.set_item(slot, newItem.value());
            } else {
                inventory.set_item(slot, endstone::ItemStack("minecraft:air"));
            }
            
            player.sendMessage("取出成功！");
        } else {
            player.sendMessage("背包已满！");
        }
        
        return {};  // 不关闭物品栏，自动刷新显示
    });
```

### 发送与关闭

```cpp
menu->send_to(player);       // 发送给玩家
menu->close(player);         // 关闭单个玩家
menu->close_all();           // 关闭所有玩家
```

### 查询状态

```cpp
auto viewers = menu->get_viewers();  // 当前查看者
auto inv = menu->get_inventory();    // 物品栏引用
```

## 完整示例

见 [`examples/cpp/plugin.cpp`](examples/cpp/plugin.cpp)。

## 协议

Apache License 2.0
