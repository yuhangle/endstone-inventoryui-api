#pragma once

#include "bedrock_stream.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

// Forward declarations
namespace endstone {
class ItemStack;
}  // namespace endstone

// ==================== MinecraftPacketIds ====================

enum class MinecraftPacketIds : int {
    KeepAlive = 0,
    UpdateBlock = 21,
    ContainerOpen = 46,
    ContainerClose = 47,
    InventoryContent = 49,
    InventorySlot = 50,
    BlockActorData = 56,
    Ping = 115,
    ItemStackRequest = 147,
    ItemRegistryPacket = 162,
    PacketViolationWarning = 156,
};

// ==================== BlockPos ====================

struct BlockPos {
    int x = 0, y = 0, z = 0;

    BlockPos() = default;
    BlockPos(int x, int y, int z) : x(x), y(y), z(z) {}

    void write(const BinaryStream &s) const;
    void read(const ReadOnlyBinaryStream &s);
};

// ==================== FullContainerName ====================

struct FullContainerName {
    uint8_t container_enum = 0;
    bool has_dynamic_slot = false;
    uint32_t dynamic_slot = 0;

    void write(const BinaryStream &s) const;
    void read(const ReadOnlyBinaryStream &s);
};

// ==================== ItemData ====================

struct ItemData {
    std::string item_name;
    int16_t item_id = 0;
    bool is_component_based = false;
    int32_t item_version = 0;
    std::vector<uint8_t> component_data;

    void write(const BinaryStream &s) const;
    void read(const ReadOnlyBinaryStream &s);
};

// ==================== ItemStackRequest types ====================

struct ItemStackRequestActionType {
    // Bedrock protocol action type values (v685+)
    static constexpr int Take = 0;
    static constexpr int Place = 1;
    static constexpr int Swap = 2;
    static constexpr int Drop = 3;
    static constexpr int Destroy = 4;
    static constexpr int Create = 5;
    static constexpr int PlaceInItem = 6;
    static constexpr int TakeFromItem = 7;
    static constexpr int LabTableCombine = 8;
    static constexpr int BeaconPayment = 9;
    static constexpr int MineBlock = 10;
    static constexpr int CraftRecipe = 11;
    static constexpr int CraftRecipeAuto = 12;
    static constexpr int CraftCreative = 13;
    static constexpr int CraftRecipeOptional = 14;
    static constexpr int CraftNonImplemented_Deprecated = 15;
    static constexpr int CraftResults_Deprecated = 16;
};

struct ItemStackRequestSlotInfo {
    FullContainerName container;
    uint8_t slot = 0;
    int32_t net_id = 0;

    void write(const BinaryStream &s) const;
    void read(const ReadOnlyBinaryStream &s);
};

struct ItemStackRequestActionTransferBase {
    uint8_t amount = 0;
    ItemStackRequestSlotInfo source;
    ItemStackRequestSlotInfo destination;

    void write(const BinaryStream &s) const;
    void read(const ReadOnlyBinaryStream &s);
};

struct ItemStackRequestAction {
    int action_type = -1;  // No default action type
    std::unique_ptr<ItemStackRequestActionTransferBase> action_data;

    void write(const BinaryStream &s) const;
    void read(const ReadOnlyBinaryStream &s);
};

struct ItemStackRequestData {
    int32_t client_request_id = 0;
    std::vector<std::vector<uint8_t>> strings_to_filter;
    int32_t strings_to_filter_origin = 0;
    std::vector<ItemStackRequestAction> request_actions;
    bool is_parsable_action = false;
    std::vector<uint8_t> request_buffer;

    void write(const BinaryStream &s) const;
    void read(const ReadOnlyBinaryStream &s);
};

struct ItemStackRequest {
    std::vector<ItemStackRequestData> request_data;

    void write(const BinaryStream &s) const;
    void read(const ReadOnlyBinaryStream &s);
};

// ==================== ItemStackWrapper ====================

class ItemStackWrapper {
public:
    int32_t stack_id = 0;
    std::string item_id;               // e.g. "minecraft:stone"
    int16_t runtime_item_id = 0;       // from ItemData lookup
    int32_t amount = 1;
    uint16_t data = 0;
    std::vector<uint8_t> extra_nbt;    // pre-serialized NBT (empty if none)

    ItemStackWrapper() = default;
    ItemStackWrapper(int32_t stack_id, std::string item_id, int16_t runtime_id,
                     int32_t amount, uint16_t data)
        : stack_id(stack_id), item_id(std::move(item_id)), runtime_item_id(runtime_id),
          amount(amount), data(data) {}

    [[nodiscard]] bool isAir() const { return item_id == "minecraft:air"; }

    bool writeHeader(const BinaryStream &s) const;
    void writeFooter(const BinaryStream &s) const;
    void write(const BinaryStream &s) const;
    static void read(const ReadOnlyBinaryStream &s);
};

// ==================== Packet base class ====================

class Packet {
public:
    virtual ~Packet() = default;
    virtual MinecraftPacketIds getId() const = 0;
    virtual std::string getName() const = 0;
    virtual void read(ReadOnlyBinaryStream &s);
    virtual void write(BinaryStream &s) const;

    [[nodiscard]] std::vector<uint8_t> serialize() const;
    void deserialize(const std::vector<uint8_t> &data);
};

// ==================== NullPacket (unimplemented/fallback) ====================

class NullPacket : public Packet {
public:
    MinecraftPacketIds getId() const override { return static_cast<MinecraftPacketIds>(0); }
    std::string getName() const override { return "NullPacket"; }
};

// ==================== NetworkStackLatencyPacket (Ping) ====================

class NetworkStackLatencyPacket : public Packet {
public:
    uint64_t timestamp = 0;
    bool from_server = false;

    NetworkStackLatencyPacket() = default;
    NetworkStackLatencyPacket(uint64_t ts, bool server) : timestamp(ts), from_server(server) {}

    MinecraftPacketIds getId() const override { return MinecraftPacketIds::Ping; }
    std::string getName() const override { return "NetworkStackLatencyPacket"; }
    void read(ReadOnlyBinaryStream &s) override;
    void write(BinaryStream &s) const override;
};

// ==================== ContainerClosePacket ====================

class ContainerClosePacket : public Packet {
public:
    uint8_t container_id = 0;
    uint8_t container_type = 0;
    bool is_server_side = false;

    MinecraftPacketIds getId() const override { return MinecraftPacketIds::ContainerClose; }
    std::string getName() const override { return "ContainerClosePacket"; }
    void read(ReadOnlyBinaryStream &s) override;
    void write(BinaryStream &s) const override;
};

// ==================== ContainerOpenPacket ====================

class ContainerOpenPacket : public Packet {
public:
    uint8_t container_id = 0;
    uint8_t container_type = 0;
    BlockPos position;
    int64_t target_actor_id = -1;

    ContainerOpenPacket() = default;
    ContainerOpenPacket(uint8_t cid, uint8_t ctype, BlockPos pos, int64_t actor_id = -1)
        : container_id(cid), container_type(ctype), position(pos), target_actor_id(actor_id) {}

    MinecraftPacketIds getId() const override { return MinecraftPacketIds::ContainerOpen; }
    std::string getName() const override { return "ContainerOpenPacket"; }
    void read(ReadOnlyBinaryStream &s) override;
    void write(BinaryStream &s) const override;
};

// ==================== UpdateBlockPacket ====================

class UpdateBlockPacket : public Packet {
public:
    BlockPos block_position;
    uint32_t block_runtime_id = 0;
    uint32_t update_flag = 0;
    uint32_t block_layer = 0;

    UpdateBlockPacket() = default;
    UpdateBlockPacket(BlockPos pos, uint32_t runtime_id, uint32_t flag, uint32_t layer)
        : block_position(pos), block_runtime_id(runtime_id), update_flag(flag), block_layer(layer) {}

    MinecraftPacketIds getId() const override { return MinecraftPacketIds::UpdateBlock; }
    std::string getName() const override { return "UpdateBlockPacket"; }
    void read(ReadOnlyBinaryStream &s) override;
    void write(BinaryStream &s) const override;
};

// ==================== BlockActorDataPacket ====================

class BlockActorDataPacket : public Packet {
public:
    BlockPos block_position;
    std::vector<uint8_t> actor_data_tags;  // Network NBT raw bytes

    BlockActorDataPacket() = default;
    BlockActorDataPacket(BlockPos pos, std::vector<uint8_t> nbt)
        : block_position(pos), actor_data_tags(std::move(nbt)) {}

    MinecraftPacketIds getId() const override { return MinecraftPacketIds::BlockActorData; }
    std::string getName() const override { return "BlockActorDataPacket"; }
    void read(ReadOnlyBinaryStream &s) override;
    void write(BinaryStream &s) const override;
};

// ==================== InventoryContentPacket ====================

class InventoryContentPacket : public Packet {
public:
    uint32_t container_id = 0;
    std::vector<ItemStackWrapper> items;
    FullContainerName container_name;
    ItemStackWrapper storage;

    InventoryContentPacket() = default;
    explicit InventoryContentPacket(uint32_t cid) : container_id(cid) {}

    MinecraftPacketIds getId() const override { return MinecraftPacketIds::InventoryContent; }
    std::string getName() const override { return "InventoryContentPacket"; }
    void read(ReadOnlyBinaryStream &s) override;
    void write(BinaryStream &s) const override;
};

// ==================== InventorySlotPacket ====================

class InventorySlotPacket : public Packet {
public:
    uint32_t container_id = 0;
    uint32_t slot = 0;
    FullContainerName container_name;
    ItemStackWrapper storage;
    ItemStackWrapper item;

    InventorySlotPacket() = default;
    InventorySlotPacket(uint32_t cid, uint32_t slt, ItemStackWrapper it)
        : container_id(cid), slot(slt), item(std::move(it)) {}

    MinecraftPacketIds getId() const override { return MinecraftPacketIds::InventorySlot; }
    std::string getName() const override { return "InventorySlotPacket"; }
    void read(ReadOnlyBinaryStream &s) override;
    void write(BinaryStream &s) const override;
};

// ==================== ItemStackRequestPacket ====================

class ItemStackRequestPacket : public Packet {
public:
    ItemStackRequest request;

    MinecraftPacketIds getId() const override { return MinecraftPacketIds::ItemStackRequest; }
    std::string getName() const override { return "ItemStackRequestPacket"; }
    void read(ReadOnlyBinaryStream &s) override;
    void write(BinaryStream &s) const override;
};

// ==================== ItemRegistryPacket ====================

class ItemRegistryPacket : public Packet {
public:
    std::vector<ItemData> item_registry;

    MinecraftPacketIds getId() const override { return MinecraftPacketIds::ItemRegistryPacket; }
    std::string getName() const override { return "ItemRegistryPacket"; }
    void read(ReadOnlyBinaryStream &s) override;
    void write(BinaryStream &s) const override;
};
