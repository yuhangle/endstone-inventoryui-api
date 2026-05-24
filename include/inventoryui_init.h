#pragma once

#include <endstone/plugin/plugin.h>

#include <memory>
#include <string>

namespace inventoryui {

class Menu;
enum class MenuTypeId;

/// 纯内嵌初始化 — 注册事件监听器，不走 ServiceManager，不受外置插件影响。
/// 在宿主 onEnable() 中调用。
void initialize_embedded(endstone::Plugin &host);

/// 直接创建菜单，不经过 ServiceManager。仅能在 initialize_embedded() 后调用。
std::shared_ptr<Menu> create_menu(MenuTypeId type, const std::string &name = "");

/// 关闭 inventoryui 核心（清理监听器与活跃表单）。
void shutdown();

} // namespace inventoryui
