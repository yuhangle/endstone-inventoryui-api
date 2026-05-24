#include "inventoryui_init.h"
#include "listener.h"
#include "menu.h"
#include "form_tracker.h"
#include "item_registry.h"

namespace {
    struct EmbeddedCore {
        endstone::Plugin *host = nullptr;
        std::unique_ptr<EventListener> listener;
    };
    EmbeddedCore *g_embedded = nullptr;
}

void inventoryui::initialize_embedded(endstone::Plugin &host)
{
    if (g_embedded) {
        return;
    }

    auto core = std::make_unique<EmbeddedCore>();
    core->host = &host;

    core->listener = std::make_unique<EventListener>(host);
    auto *listener = core->listener.get();
    host.registerEvent<endstone::PacketReceiveEvent>([listener](auto &e) { listener->onPacketReceive(e); });
    host.registerEvent<endstone::PacketSendEvent>([listener](auto &e) { listener->onPacketSend(e); });
    host.registerEvent<endstone::PlayerQuitEvent>([listener](auto &e) { listener->onPlayerQuit(e); });

    host.getLogger().info("=== InventoryUI initialized (embedded mode) ===");
    g_embedded = core.release();
}

void inventoryui::shutdown()
{
    if (!g_embedded) return;
    clearActiveForms();
    clearItemData();
    g_embedded->listener.reset();
    delete g_embedded;
    g_embedded = nullptr;
}

std::shared_ptr<inventoryui::Menu> inventoryui::create_menu(MenuTypeId type, const std::string &name)
{
    if (!g_embedded || !g_embedded->host) {
        return nullptr;
    }
    return std::make_shared<::Menu>(*g_embedded->host, static_cast<::MenuTypeId>(type), name);
}
