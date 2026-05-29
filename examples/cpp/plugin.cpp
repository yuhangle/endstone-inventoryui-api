#include <endstone/endstone.hpp>
#include <endstone_inventoryui/inventoryui.h>

/**
 * Example: consuming InventoryUI via ServiceManager (外置插件模式).
 *
 * 内嵌模式（无需依赖 endstone_inventoryui.so）请参考 README 中的方式一：
 *   inventoryui::initialize_embedded(*this);
 *   auto menu = inventoryui::create_menu(inventoryui::MenuTypeId::CHEST, "Embedded");
 */
class InventoryUIExample : public endstone::Plugin {
public:
    void onEnable() override
    {
        auto *plugin_mgr = getServer().getPluginManager().getPlugin("inventoryui_api");
        if (!plugin_mgr) {
            getLogger().warning("InventoryUI plugin not found!");
            getServer().getPluginManager().disablePlugin(*this);
            return;
        }

        inventory_ui_ = getServer().getServiceManager().load<inventoryui::InventoryUI>("InventoryUIAPI");
        if (!inventory_ui_) {
            getLogger().warning("InventoryUI service not available!");
            getServer().getPluginManager().disablePlugin(*this);
            return;
        }

        auto menu = inventory_ui_->create_menu(inventoryui::MenuTypeId::CHEST, "C++ Menu");

        // 普通物品
        auto inv = menu->get_inventory();
        inv->set_item(0, endstone::ItemStack("minecraft:diamond_sword"));
        inv->set_item(1, endstone::ItemStack("minecraft:diamond_axe"));

        // 预编码物品（跳过 ItemStack，保留完整 NBT）
        // nbt_bytes 为外部来源（如 world-inspector）提供的二进制 NBT 数据
        // inv->set_pre_encoded_item(2, "minecraft:diamond_helmet", 1, 0, nbt_bytes);

        menu->set_listener(
            [](endstone::Player &player, int slot,
               const endstone::ItemStack &item,
               inventoryui::UIInventory &inventory) -> std::function<void()> {
                player.sendMessage(
                    "You clicked slot " + std::to_string(slot)
                    + " (" + item.getType().getId() + ")");
                return {};  // 不关闭物品栏
            });

        menu->set_open_listener([](endstone::Player &player) {
            player.sendMessage("Menu opened!");
        });

        menu->set_close_listener([](endstone::Player &player) {
            player.sendMessage("Menu closed!");
        });

        menu_ = menu;
        getLogger().info("InventoryUIExample enabled!");
    }

    void onDisable() override
    {
        if (menu_) {
            menu_->close_all();
        }
        getLogger().info("InventoryUIExample disabled!");
    }

    bool onCommand(endstone::CommandSender &sender, const endstone::Command &command,
                   const std::vector<std::string> &args) override
    {
        auto *player = sender.asPlayer();
        if (player && menu_) {
            menu_->send_to(*player);
        }
        return true;
    }

private:
    std::shared_ptr<inventoryui::InventoryUI> inventory_ui_;
    std::shared_ptr<inventoryui::Menu> menu_;
};

ENDSTONE_PLUGIN(/*name=*/"inventoryui_example", /*version=*/"0.1.0", /*main_class=*/InventoryUIExample)
{
    prefix = "InventoryUIExample";
    description = "Example C++ plugin consuming the InventoryUI API";
    website = "https://github.com/yuhangle/endstone-inventoryui-api";
    authors = {"yuhangle"};
    depend = {"inventoryui_api"};
}
