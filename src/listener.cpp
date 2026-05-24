#include "listener.h"
#include "menu.h"
#include "form_tracker.h"
#include "packets.h"
#include "utils.h"
#include "item_registry.h"

#include <endstone/endstone.hpp>

// ==================== EventListener ====================

void EventListener::onPacketReceive(
    const endstone::PacketReceiveEvent &event) const {
  const int packet_id = event.getPacketId();
    auto *player = event.getPlayer();
    if (!player) return;

    const auto &log = plugin_.getLogger();

    if (packet_id == static_cast<int>(MinecraftPacketIds::ContainerClose)) {
        auto &forms = getActiveForms();
      const auto it = forms.find(player->getName());
        if (it == forms.end()) return;

        ContainerClosePacket pk;
        pk.deserialize({event.getPayload().data(), event.getPayload().data() + event.getPayload().size()});

        log.debug("ContainerClose: cid={} player={} state=FORM",
                 pk.container_id, player->getName());

        if (pk.container_id != static_cast<uint8_t>(Menu::CONTAINER_ID)) return;

        auto *form = it->second.get();
        auto *menu = form->menu;

        // Fire close listener
        if (menu && menu->getCloseListener()) {
            menu->getCloseListener()(*player);
        }

        // Restore original blocks
        restoreBlock(*player, form->pos);
        if (form->is_pair) {
            restoreBlock(*player, BlockPos(form->pos.x + 1, form->pos.y, form->pos.z));
        }

        // Clean up
        forms.erase(it);
    }
    else if (packet_id == static_cast<int>(MinecraftPacketIds::PacketViolationWarning)) {
        // Client-side container rejection — ignore in simplified architecture
    }
    else if (packet_id == static_cast<int>(MinecraftPacketIds::ItemStackRequest)) {
        auto &forms = getActiveForms();
        auto it = forms.find(player->getName());
        if (it == forms.end()) return;

        auto *form = it->second.get();
        auto *menu = form->menu;
        if (!menu) return;

        ItemStackRequestPacket pk;
        pk.deserialize({event.getPayload().data(), event.getPayload().data() + event.getPayload().size()});

        for (const auto &req_data : pk.request.request_data) {
            for (const auto &action : req_data.request_actions) {
                if (action.action_type == ItemStackRequestActionType::Take ||
                    action.action_type == ItemStackRequestActionType::Place) {
                    int slot = action.action_data->source.slot;
                    int source_id = action.action_data->source.container.container_enum;
                    if (source_id != 7) return;  // LEVEL_ENTITY

                    if (menu->getListener()) {
                        auto item = menu->getInventoryRef().getItem(slot);
                        menu->getListener()(*player, slot, item, menu->getInventoryRef());
                        return;
                    }
                }
            }
        }
    }
}

void EventListener::onPacketSend(const endstone::PacketSendEvent &event) const {
    if (event.getPacketId() == static_cast<int>(MinecraftPacketIds::ItemRegistryPacket)) {
    const auto &log = plugin_.getLogger();
        log.debug("ItemRegistryPacket: player={} cached={}",
                 event.getPlayer() ? event.getPlayer()->getName() : "NONE",
                 allItemData().size());
        if (allItemData().empty()) {
            try {
                ItemRegistryPacket pk;
                auto payload = event.getPayload();
                pk.deserialize({reinterpret_cast<const uint8_t *>(payload.data()),
                                reinterpret_cast<const uint8_t *>(payload.data()) + payload.size()});
                log.debug("ItemRegistryPacket: deserialized {} items", pk.item_registry.size());
                for (const auto &item : pk.item_registry) {
                    addItemData(item.item_name, item);
                }
                log.debug("ItemRegistryPacket: cached {} items", allItemData().size());
            } catch (const std::exception &e) {
                log.warning("ItemRegistryPacket deserialize failed: {}", e.what());
            }
        }
    }
}

void EventListener::onPlayerQuit(const endstone::PlayerQuitEvent &event)
{
    closeForm(event.getPlayer().getName());
}
