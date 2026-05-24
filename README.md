# InventoryUI

Endstone 虚拟容器界面插件。支持两种使用方式：

- **内嵌模式** — 编译为静态库嵌入宿主插件，不依赖单独的 `.so`
- **外置插件模式** — 编译为独立插件，其他插件通过 Endstone ServiceManager 调用

## 目录结构

```
include/
├── bedrock_ffi.h          ← Rust C FFI 声明
├── bedrock_stream.h       ← BinaryStream RAII 包装
├── bedrock_nbt.h          ← CompoundTag RAII 包装
├── packets.h              ← 数据包类型定义
├── item_registry.h        ← 物品注册表缓存
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
├── packets.cpp            ← 数据包序列化
├── menu.cpp               ← Menu 实现
├── inventory.cpp          ← UIInventory 实现
├── listener.cpp           ← 事件处理
└── item_registry.cpp      ← 物品注册表
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

> 使用 `set_pre_encoded_item()` 的槽位在 `sendContents()` 时会优先使用预编码数据，忽略同槽位的 `ItemStack`。菜单对象的生命周期需要由调用方管理（`send_to()` 内部使用延迟发送，菜单销毁后不会显示）。

### 注册回调

```cpp
// 点击回调
menu->set_listener(
    [](endstone::Player &player, int slot,
       const endstone::ItemStack &item,
       inventoryui::UIInventory &inventory) {
        player.sendMessage("slot {}: {}", slot, item.getType().getId());
    });

// 打开回调
menu->set_open_listener([](endstone::Player &player) {
    player.sendMessage("Menu opened");
});

// 关闭回调
menu->set_close_listener([](endstone::Player &player) {
    player.sendMessage("Menu closed");
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
