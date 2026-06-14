#include <endstone/endstone.hpp>
#include <endstone_inventoryui/inventoryui.h>

/**
 * Example: consuming InventoryUI via ServiceManager (external plugin mode).
 *
 * For embedded mode (no endstone_inventoryui.so required), see README:
 *   inventoryui::initialize_embedded(*this);
 *   auto menu = inventoryui::create_menu(inventoryui::MenuTypeId::CHEST, "Embedded");
 */
class InventoryUIExample : public endstone::Plugin {
public:
    void onEnable() override
    {
    if (const auto *plugin_mgr =
            getServer().getPluginManager().getPlugin("inventoryui_api");
        !plugin_mgr) {
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

        const auto menu = inventory_ui_->create_menu(inventoryui::MenuTypeId::CHEST, "C++ Menu");

        // Normal items
        const auto inv = menu->get_inventory();
        inv->set_item(0, endstone::ItemStack("minecraft:diamond_sword"));
        inv->set_item(1, endstone::ItemStack("minecraft:diamond_axe"));

        // Pre-encoded items (bypass ItemStack, preserve full NBT)
        // nbt_bytes: binary NBT data from external source (e.g. world-inspector)
        // inv->set_pre_encoded_item(2, "minecraft:diamond_helmet", 1, 0, nbt_bytes);

        menu->set_listener(
            [](const endstone::Player &player, const int slot,
               const endstone::ItemStack &item,
               inventoryui::UIInventory &inventory) -> std::function<void()> {
                player.sendMessage(
                    "You clicked UI slot " + std::to_string(slot)
                    + " (" + item.getType().getId() + ")");
                return {};  // Keep inventory open
            });

        // Player inventory callback (triggered when player clicks their own inventory while UI is open)
        menu->set_player_inventory_listener(
            [](const endstone::Player &player, const int slot,
               const endstone::ItemStack &item,
               const int container_id) -> std::function<void()> {
                // container_id: 28=Hotbar, 29=Inventory, 12=Combined, 59=Cursor
                player.sendMessage(
                    "You clicked your inventory slot " + std::to_string(slot)
                    + " (" + item.getType().getId() + ")"
                    + " [container=" + std::to_string(container_id) + "]");
                return {};  // Keep inventory open
            });

        menu->set_open_listener([](const endstone::Player &player) {
            player.sendMessage("Menu opened!");
        });

        menu->set_close_listener([](const endstone::Player &player) {
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
