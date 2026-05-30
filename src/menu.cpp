#include "menu.h"
#include "form_tracker.h"
#include "utils.h"
#include "item_registry.h"
#include "bedrock_nbt.h"

#include <endstone/endstone.hpp>

// ==================== Menu ====================

Menu::Menu(endstone::Plugin &plugin, MenuTypeId type, std::string name)
    : plugin_(plugin),
      name_(std::move(name)),
      type_(type),
      inventory_(std::make_shared<UIInventory>(getMenuTypeData(type).container_size))
{
}

Menu::~Menu()
{ Menu::close_all();
}

void Menu::sendTo(endstone::Player &player, const std::shared_ptr<Menu>& self) const {
    auto &forms = getActiveForms();
    auto player_name = player.getName();

    // Close any existing form for this player
    forms.erase(player_name);

    auto data = getMenuTypeData(type_);
    auto pos = abovePlayer(player);

    // Place the fake block
    sendBlock(player, data.block_id, pos);

    // For double chests, place the adjacent block
    BlockPos pair_pos(pos.x + 1, pos.y, pos.z);
    if (data.is_pair) {
        sendBlock(player, data.block_id, pair_pos);
    }

    // Send block actor data (sets the custom name on the chest)
    {
        CompoundTag tag;
        tag.set("id", data.block_actor_id);
        tag.set("CustomName", name_);
        auto nbt = tag.toNetworkNbt();
        BlockActorDataPacket actor_pk(pos, std::move(nbt));
        sendPacket(player, actor_pk);

        if (data.is_pair) {
            CompoundTag pair_tag;
            pair_tag.set("id", data.block_actor_id);
            pair_tag.set("CustomName", name_);
            auto pair_nbt = pair_tag.toNetworkNbt();
            BlockActorDataPacket pair_actor_pk(pair_pos, std::move(pair_nbt));
            sendPacket(player, pair_actor_pk);
        }
    }

    // Register the active form immediately
    auto form = std::make_unique<FormData>();
    form->menu = self;
    form->player = &player;
    form->pos = pos;
    form->is_pair = data.is_pair;
    forms[player_name] = std::move(form);

    // Defer ContainerOpen so the client has time to render the chest block
    auto pos_copy = pos;
    const auto& name_copy = player_name;
    const auto server = &plugin_.getServer();
    constexpr auto cid = CONTAINER_ID;
    const auto ctype = data.container_type;
    plugin_.getServer().getScheduler().runTaskLater(
        plugin_,
        [self, server, ctype, pos_copy, name_copy]() {
            auto *player1 = server->getPlayer(name_copy);
            if (!player1) return;

            // Check the form is still active (player didn't close in the meantime)
            auto &forms1 = getActiveForms();
            const auto it = forms1.find(name_copy);
            if (it == forms1.end()) return;
            if (it->second->menu.get() != self.get()) return;

            const ContainerOpenPacket open_pk(cid, ctype, pos_copy);
            sendPacket(*player1, open_pk);

            // Send inventory contents
            self->sendContents(*player1);

            // Fire open listener now that the UI is actually opening
            if (self->open_listener_) {
                self->open_listener_(*player1);
            }
        },
        10);
}

void Menu::sendContents(const endstone::Player &player) const {
    InventoryContentPacket pk(CONTAINER_ID);

    for (int i = 0; i < inventory_->getSize(); ++i) {
        // Check pre-encoded items first (from offline inventory reader)
        if (const auto* pe = inventory_->getPreEncodedItem(i)) {
            const bool is_air = (pe->type_id == "minecraft:air");
          if (const auto *item_data = getItemData(pe->type_id);
              !item_data && !is_air) {
                plugin_.getLogger().warning("sendContents: slot[{}]: {} not in registry -> air", i, pe->type_id);
            }

            pk.items.emplace_back(
                is_air ? 0 : i + 1,
                pe->type_id,
                0,
                pe->count,
                pe->damage
            );
            pk.items.back().extra_nbt = pe->nbt_bytes;
            continue;
        }

        // Normal ItemStack path (for online player inventories)
        auto item = inventory_->getItem(i);
        auto type_id = static_cast<std::string>(item.getType().getId());
        const bool is_air = (type_id == "minecraft:air");
        auto nbt = buildItemNbt(item);

        if (const auto *item_data = getItemData(type_id);
            !item_data && !is_air) {
            plugin_.getLogger().warning("sendContents: slot[{}]: {} not in registry -> air", i, type_id);
        }

        pk.items.emplace_back(
            is_air ? 0 : i + 1,
            type_id,
            0,
            item.getAmount(),
            item.getData()
        );
        pk.items.back().extra_nbt = std::move(nbt);
    }

    auto payload = pk.serialize();
    player.sendPacket(static_cast<int>(pk.getId()),
                      {reinterpret_cast<const char *>(payload.data()), payload.size()});
}

void Menu::send_to(endstone::Player &player) { sendTo(player, shared_from_this()); }

bool Menu::close(endstone::Player &player)
{
    auto &forms = getActiveForms();
  const auto it = forms.find(player.getName());
    if (it == forms.end() || it->second->menu.get() != this) return false;

    // Copy data needed after erase
    const auto pos = it->second->pos;
    const auto is_pair = it->second->is_pair;
    const auto close_listener = it->second->menu ? it->second->menu->close_listener_ : PlayerCallback{};

    // Send ContainerClose (server-initiated) to the client
    ContainerClosePacket close_pk;
    close_pk.container_id = CONTAINER_ID;
    close_pk.is_server_side = true;
    sendPacket(player, close_pk);

    // Erase form BEFORE firing close_listener to prevent iterator invalidation
    forms.erase(it);

    // Restore original blocks
    restoreBlock(player, pos);
    if (is_pair) {
        restoreBlock(player, BlockPos(pos.x + 1, pos.y, pos.z));
    }

    // Fire close listener
    if (close_listener) {
        close_listener(player);
    }

    return true;
}

void Menu::closeAll() const {
    // Collect player names for this menu, then close each
    std::vector<std::string> to_close;
    for (const auto &[name, form] : getActiveForms()) {
        if (form->menu.get() == this) {
            to_close.push_back(name);
        }
    }
    for (const auto &name : to_close) {
        auto &forms = getActiveForms();
      if (auto it = forms.find(name); it != forms.end()) {
          if (auto *player = it->second->player) {
          // Copy data needed after erase
          const auto pos = it->second->pos;
          const auto is_pair = it->second->is_pair;
          auto close_listener = it->second->menu ? it->second->menu->close_listener_ : PlayerCallback{};

                ContainerClosePacket close_pk;
                close_pk.container_id = CONTAINER_ID;
                close_pk.is_server_side = true;
                sendPacket(*player, close_pk);

                // Erase form BEFORE firing close_listener
                forms.erase(it);

                restoreBlock(*player, pos);
                if (is_pair) {
                    restoreBlock(*player, BlockPos(pos.x + 1, pos.y, pos.z));
                }
                if (close_listener) {
                    close_listener(*player);
                }
          } else {
              forms.erase(it);
          }
        }
    }
}

void Menu::close_all() {
    // Called from destructor — cannot use shared_from_this().
    // Just erase all forms for this menu from ActiveForms.
    // The shared_ptr in FormData will be released, potentially destroying this menu.
    std::vector<std::string> to_erase;
    for (const auto &[name, form] : getActiveForms()) {
        if (form->menu.get() == this) {
            to_erase.push_back(name);
        }
    }
    for (const auto &name : to_erase) {
        getActiveForms().erase(name);
    }
}

