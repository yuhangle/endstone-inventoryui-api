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

        const auto *form = it->second.get();

        // Fire close listener
        if (const auto *menu = form->menu; menu && menu->getCloseListener()) {
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
      const auto it = forms.find(player->getName());
        if (it == forms.end()) return;

        const auto *form = it->second.get();
        const auto *menu = form->menu;
        if (!menu) return;

        ItemStackRequestPacket pk;
        pk.deserialize({event.getPayload().data(), event.getPayload().data() + event.getPayload().size()});

        for (const auto &req_data : pk.request.request_data) {
            for (const auto &[action_type_, action_data_] : req_data.request_actions) {
            const int action_type = action_type_;
                if (!action_data_) continue;

                if (action_type == 0 ||
                    action_type == ItemStackRequestActionType::Take ||
                    action_type == ItemStackRequestActionType::Place) {
                  const int slot = action_data_->source.slot;
                  if (const int source_id =
                          action_data_->source.container.container_enum;
                      source_id != 7) continue;

                    if (menu->getListener()) {
                    const auto item = menu->getInventoryRef().getItem(slot);
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
