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

        // Copy data needed after erase
        const auto pos = it->second->pos;
        const auto is_pair = it->second->is_pair;
        auto close_listener = it->second->menu ? it->second->menu->getCloseListener() : Menu::PlayerCallback{};

        // Erase form BEFORE firing close_listener to prevent iterator invalidation
        // (close_listener may call send_to which also erases from getActiveForms)
        forms.erase(it);

        // Restore original blocks
        restoreBlock(*player, pos);
        if (is_pair) {
            restoreBlock(*player, BlockPos(pos.x + 1, pos.y, pos.z));
        }

        // Defer close_listener to avoid re-entrant map modification
        if (close_listener) {
            const auto player_name = player->getName();
            plugin_.getServer().getScheduler().runTaskLater(
                plugin_,
                [&plugin = plugin_, fn = std::move(close_listener), player_name]() {
                  if (auto *p = plugin.getServer().getPlayer(player_name)) fn(*p);
                },
                1);
        }
    }
    else if (packet_id == static_cast<int>(MinecraftPacketIds::PacketViolationWarning)) {
        // Client-side container rejection — ignore in simplified architecture
    }
    else if (packet_id == static_cast<int>(MinecraftPacketIds::ItemStackRequest)) {
        auto &forms = getActiveForms();
      const auto it = forms.find(player->getName());
        if (it == forms.end()) return;

        const auto *form = it->second.get();
        const auto &menu = form->menu;
        if (!menu) return;

        ItemStackRequestPacket pk;
        pk.deserialize({event.getPayload().data(), event.getPayload().data() + event.getPayload().size()});

        for (const auto &req_data : pk.request.request_data) {
            for (const auto &[action_type_, action_data_] : req_data.request_actions) {
            const int action_type = action_type_;
                if (!action_data_) continue;

                if (action_type == ItemStackRequestActionType::Take ||
                    action_type == ItemStackRequestActionType::Place ||
                    action_type == ItemStackRequestActionType::Swap) {
                  const int slot = action_data_->source.slot;
                  const int source_id = action_data_->source.container.container_enum;

                  // 容器ID定义
                  constexpr int UI_CONTAINER = 7;           // LevelEntityContainer (虚拟箱子)
                  constexpr int HOTBAR_CONTAINER = 28;      // HotbarContainer
                  constexpr int INVENTORY_CONTAINER = 29;   // InventoryContainer
                  constexpr int COMBINED_CONTAINER = 12;    // CombinedHotbarAndInventoryContainer
                  constexpr int CURSOR_CONTAINER = 59;      // CursorContainer

                  // source_id=0 在某些客户端中表示游标/全局物品栏
                  const bool is_ui_container = (source_id == UI_CONTAINER);
                  const bool is_player_inventory = (source_id == HOTBAR_CONTAINER ||
                                                     source_id == INVENTORY_CONTAINER ||
                                                     source_id == COMBINED_CONTAINER ||
                                                     source_id == CURSOR_CONTAINER ||
                                                     source_id == 0);  // Cursor/global inventory

                  if (!is_ui_container && !is_player_inventory) continue;

                  // UI物品栏回调
                  if (is_ui_container && menu->getListener()) {
                    const auto item = menu->getInventoryRef().getItem(slot);
                      if (auto deferred = menu->getListener()(
                              *player, slot, item, menu->getInventoryRef())) {
                            // 关闭物品栏UI，但不触发close_listener
                            const auto player_name = player->getName();
                            auto& forms1 = getActiveForms();
                            if (const auto form_it = forms1.find(player_name);
                                form_it != forms1.end()) {
                                const auto *form2 = form_it->second.get();
                                const auto pos = form2->pos;
                                const auto is_pair = form2->is_pair;

                                // 从ActiveForms移除，后续客户端回复ContainerClose时不会触发close_listener
                                forms1.erase(form_it);

                                // 发送ContainerClose关闭客户端物品栏
                                ContainerClosePacket close_pk;
                                close_pk.container_id = Menu::CONTAINER_ID;
                                close_pk.is_server_side = true;
                                sendPacket(*player, close_pk);

                                // 恢复假方块
                                restoreBlock(*player, pos);
                                if (is_pair) {
                                    restoreBlock(*player, BlockPos(pos.x + 1, pos.y, pos.z));
                                }
                            }

                            // 延迟执行回调中的表单发送逻辑
                            auto& scheduler = plugin_.getServer().getScheduler();
                            scheduler.runTaskLater(
                                plugin_,
                                [fn = std::move(deferred)]() { fn(); },
                                10);
                        } else {
                            // 回调返回空函数，不关闭UI，刷新物品栏显示
                            menu->sendContents(*player);
                        }
                        return;
                  }

                  // 玩家物品栏回调
                  if (is_player_inventory && menu->getPlayerInventoryListener()) {
                    // 映射协议槽位到 Endstone 玩家物品栏槽位
                    // HotbarContainer(28): protocol 0-8 → Endstone 0-8
                    // InventoryContainer(29): protocol 0-35 → Endstone 0-35 (绝对编号)
                    // CombinedHotbarAndInventoryContainer(12): protocol 0-35 → Endstone 0-35
                    int mapped_slot = slot;

                    // 从玩家自身物品栏获取物品
                    auto &player_inv = player->getInventory();
                    if (const auto item = player_inv.getItem(mapped_slot)) {
                      if (auto deferred = menu->getPlayerInventoryListener()(
                              *player, mapped_slot, *item, source_id)) {
                        // 关闭物品栏UI，但不触发close_listener
                        const auto player_name = player->getName();
                        auto& forms1 = getActiveForms();
                        if (const auto form_it = forms1.find(player_name);
                            form_it != forms1.end()) {
                            const auto *form2 = form_it->second.get();
                            const auto pos = form2->pos;
                            const auto is_pair = form2->is_pair;

                            // 从ActiveForms移除
                            forms1.erase(form_it);

                            // 发送ContainerClose关闭客户端物品栏
                            ContainerClosePacket close_pk;
                            close_pk.container_id = Menu::CONTAINER_ID;
                            close_pk.is_server_side = true;
                            sendPacket(*player, close_pk);

                            // 恢复假方块
                            restoreBlock(*player, pos);
                            if (is_pair) {
                                restoreBlock(*player, BlockPos(pos.x + 1, pos.y, pos.z));
                            }
                        }

                        // 延迟执行回调中的表单发送逻辑
                        auto& scheduler = plugin_.getServer().getScheduler();
                        scheduler.runTaskLater(
                            plugin_,
                            [deferred_fn = std::move(deferred)]() { deferred_fn(); },
                            10);
                      }
                    }
                    // 玩家物品栏回调不刷新UI显示（因为UI内容未变化）
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
              const auto payload = event.getPayload();
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
