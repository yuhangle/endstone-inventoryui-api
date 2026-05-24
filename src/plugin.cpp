#include "plugin.h"
#include "version.h"
#include "listener.h"
#include "service.h"
#include "form_tracker.h"
#include "item_registry.h"

#include <endstone/endstone.hpp>

#ifndef INVENTORYUI_AS_STATIC
ENDSTONE_PLUGIN("inventoryui_api", ENDSTONE_INVENTORYUI_VERSION, InventoryUIPlugin)
{
    prefix = "InventoryUIAPI";
    description = "Inventory UI plugin for Endstone servers";
    depend = {};
}
#endif

void InventoryUIPlugin::onEnable()
{
    getLogger().info("=== InventoryUI v{} enabled ===", ENDSTONE_INVENTORYUI_VERSION);

    listener_ = std::make_unique<EventListener>(*this);
    auto *listener = listener_.get();
    registerEvent<endstone::PacketReceiveEvent>([listener](auto &e) { listener->onPacketReceive(e); });
    registerEvent<endstone::PacketSendEvent>([listener](auto &e) { listener->onPacketSend(e); });
    registerEvent<endstone::PlayerQuitEvent>([listener](auto &e) { listener->onPlayerQuit(e); });

    service_ = std::make_shared<InventoryUIService>(*this);
    getServer().getServiceManager().registerService(
        "InventoryUIAPI", service_, *this, endstone::ServicePriority::Highest);

    getLogger().info("InventoryUI service registered");
}

void InventoryUIPlugin::onDisable()
{
    getLogger().info("InventoryUI shutting down");
    clearActiveForms();
    clearItemData();
    listener_.reset();
    service_.reset();
    getLogger().info("InventoryUI disabled");
}
