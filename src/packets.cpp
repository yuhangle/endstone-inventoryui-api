#include "packets.h"
#include "item_registry.h"
#include "bedrock_nbt.h"
#include <endstone/endstone.hpp>

// Debug logging macro — uses stderr with flush
// Debug log — define to enable stderr debug output
// #define PKT_LOG(fmt, ...) do { fprintf(stderr, "[InventoryUI] " fmt "\n", ##__VA_ARGS__); fflush(stderr); } while(0)
#define PKT_LOG(fmt, ...) do {} while(0)

// ==================== Packet base ====================

void Packet::read(ReadOnlyBinaryStream &) {}
void Packet::write(BinaryStream &) const {}

std::vector<uint8_t> Packet::serialize() const
{
    BinaryStream stream;
    const_cast<Packet *>(this)->write(stream);
    return stream.copyBuffer();
}

void Packet::deserialize(const std::vector<uint8_t> &data)
{
    ReadOnlyBinaryStream stream(data);
    read(stream);
}

// ==================== BlockPos ====================

void BlockPos::write(const BinaryStream &s) const
{
    s.writeVarint(x);
    s.writeVarint(y);
    s.writeVarint(z);
}

void BlockPos::read(const ReadOnlyBinaryStream &s)
{
    x = s.getVarint();
    y = s.getVarint();
    z = s.getVarint();
}

// ==================== FullContainerName ====================

void FullContainerName::write(const BinaryStream &s) const
{
    s.writeByte(container_enum);
    s.writeBool(has_dynamic_slot);
    if (has_dynamic_slot) {
        s.writeUnsignedInt(dynamic_slot);
    }
}

void FullContainerName::read(const ReadOnlyBinaryStream &s)
{
    container_enum = s.getByte();
    has_dynamic_slot = s.getBool();
    if (has_dynamic_slot) {
        dynamic_slot = s.getUnsignedInt();
    }
}

// ==================== ItemData ====================

void ItemData::write(const BinaryStream &s) const
{
    s.writeString(item_name);
    s.writeShort(item_id);
    s.writeBool(is_component_based);
    s.writeVarint(item_version);
    s.writeRawBytes(component_data);
}

void ItemData::read(const ReadOnlyBinaryStream &s)
{
    item_name = s.getString();
    item_id = s.getSignedShort();
    is_component_based = s.getBool();
    item_version = s.getVarint();
    // Read component NBT (CompoundTag-prefixed)
    const auto pos = s.getPosition();
    const auto remaining = s.readRemaining();
    if (!remaining.empty() && remaining[0] == 0x0A) {
        // Attempt to parse via Rust NBT
      const CompoundTag nbt;
        size_t consumed = 0;
        int ret = bedrock_nbt_from_network_into(nbt.get(), remaining.data(), remaining.size(), &consumed);
        if (ret == 0 && consumed > 0) {
            component_data = nbt.toNetworkNbt();
            s.setPosition(pos + consumed);
            return;
        }
    }
    s.setPosition(pos);
    component_data.clear();
}

// ==================== ItemStackRequestSlotInfo ====================

void ItemStackRequestSlotInfo::write(const BinaryStream &s) const
{
    container.write(s);
    s.writeByte(slot);
    s.writeVarint(net_id);
}

void ItemStackRequestSlotInfo::read(const ReadOnlyBinaryStream &s)
{
    container.read(s);
    slot = s.getByte();
    net_id = s.getVarint();
}

// ==================== ItemStackRequestActionTransferBase ====================

void ItemStackRequestActionTransferBase::write(const BinaryStream &s) const
{
    s.writeByte(amount);
    source.write(s);
    destination.write(s);
}

void ItemStackRequestActionTransferBase::read(const ReadOnlyBinaryStream &s)
{
    amount = s.getByte();
    source.read(s);
    destination.read(s);
}

// ==================== ItemStackRequestAction ====================

void ItemStackRequestAction::write(const BinaryStream &s) const
{
    s.writeByte(action_type);
    if (action_data) {
        action_data->write(s);
    }
}

void ItemStackRequestAction::read(const ReadOnlyBinaryStream &s)
{
    action_type = s.getByte();
    if (action_type == 0 ||
        action_type == ItemStackRequestActionType::Take ||
        action_type == ItemStackRequestActionType::Place)
    {
        auto data = std::make_unique<ItemStackRequestActionTransferBase>();
        data->read(s);
        action_data = std::move(data);
    }
}

// ==================== ItemStackRequestData ====================

void ItemStackRequestData::write(const BinaryStream &s) const
{
    if (is_parsable_action) {
        s.writeVarint(client_request_id);
        s.writeUnsignedVarint(static_cast<uint32_t>(request_actions.size()));
        for (const auto &action : request_actions) {
            action.write(s);
        }
        s.writeUnsignedVarint(static_cast<uint32_t>(strings_to_filter.size()));
        for (const auto &stf : strings_to_filter) {
            s.writeBytes(stf);
        }
        s.writeSignedInt(strings_to_filter_origin);
    } else {
        s.writeRawBytes(request_buffer);
    }
}

void ItemStackRequestData::read(const ReadOnlyBinaryStream &s)
{
    is_parsable_action = true;
    client_request_id = s.getVarint();
    auto actions_len = s.getUnsignedVarint();
    request_actions.reserve(actions_len);
    for (uint32_t i = 0; i < actions_len; ++i) {
        ItemStackRequestAction action;
        action.read(s);
        request_actions.push_back(std::move(action));
    }
    auto stf_len = s.getUnsignedVarint();
    strings_to_filter.reserve(stf_len);
    for (uint32_t i = 0; i < stf_len; ++i) {
        strings_to_filter.push_back(s.getBytes());
    }
    strings_to_filter_origin = s.getSignedInt();
}

// ==================== ItemStackRequest ====================

void ItemStackRequest::write(const BinaryStream &s) const
{
    s.writeUnsignedVarint(static_cast<uint32_t>(request_data.size()));
    for (const auto &req : request_data) {
        req.write(s);
    }
}

void ItemStackRequest::read(const ReadOnlyBinaryStream &s)
{
    auto length = s.getUnsignedVarint();
    request_data.reserve(length);
    for (uint32_t i = 0; i < length; ++i) {
        ItemStackRequestData data;
        data.read(s);
        request_data.push_back(std::move(data));
    }
}

// ==================== ItemStackWrapper ====================

bool ItemStackWrapper::writeHeader(const BinaryStream &s) const
{
    if (isAir()) {
        s.writeVarint(0);
        return false;
    }
    // Look up item in registry
    auto *item_data = getItemData(item_id);
    int16_t rid = runtime_item_id;

    if (rid == 0 && item_data) {
        rid = item_data->item_id;  // may be negative (valid for some items)
    }

    if (!item_data && rid == 0) {
        PKT_LOG("writeHeader: item '%s' not in registry -> air", item_id.c_str());
        s.writeVarint(0);
        return false;
    }
    s.writeVarint(rid);
    s.writeUnsignedShort(static_cast<uint16_t>(amount));
    s.writeUnsignedVarint(data);
    return true;
}

void ItemStackWrapper::writeFooter(const BinaryStream &s) const
{
    if (!extra_nbt.empty()) {
        s.writeSignedShort(-1);  // NBT length indicator
        s.writeByte(1);           // NBT version tag
        s.writeRawBytes(extra_nbt);
    } else {
        s.writeSignedShort(0);   // no NBT
    }
    s.writeUnsignedInt(0);   // canPlaceOn count
    s.writeUnsignedInt(0);   // canDestroy count
}

void ItemStackWrapper::write(const BinaryStream &s) const
{
    if (writeHeader(s)) {
    const bool has_net_id = stack_id != 0;
        s.writeBool(has_net_id);
        if (has_net_id) {
            s.writeVarint(stack_id);
        }
        s.writeVarint(0);  // BlockRuntimeID
        const BinaryStream user_data;
        writeFooter(user_data);
        s.writeBytes(user_data.copyBuffer());
    }
}

void ItemStackWrapper::read(const ReadOnlyBinaryStream &s)
{
    // Placeholder — for future use
    (void)s;
}

// ==================== NetworkStackLatencyPacket ====================

void NetworkStackLatencyPacket::read(ReadOnlyBinaryStream &s)
{
    timestamp = s.getUnsignedInt64();
    from_server = s.getBool();
}

void NetworkStackLatencyPacket::write(BinaryStream &s) const
{
    s.writeUnsignedInt64(timestamp);
    s.writeBool(from_server);
}

// ==================== ContainerClosePacket ====================

void ContainerClosePacket::read(ReadOnlyBinaryStream &s)
{
    container_id = s.getByte();
    container_type = s.getByte();
    is_server_side = s.getBool();
}

void ContainerClosePacket::write(BinaryStream &s) const
{
    s.writeByte(container_id);
    s.writeByte(container_type);
    s.writeBool(is_server_side);
}

// ==================== ContainerOpenPacket ====================

void ContainerOpenPacket::read(ReadOnlyBinaryStream &s)
{
    container_id = s.getByte();
    container_type = s.getByte();
    position.read(s);
    target_actor_id = s.getVarint64();
}

void ContainerOpenPacket::write(BinaryStream &s) const
{
    s.writeByte(container_id);
    s.writeByte(container_type);
    position.write(s);
    s.writeVarint64(target_actor_id);
}

// ==================== UpdateBlockPacket ====================

void UpdateBlockPacket::read(ReadOnlyBinaryStream &s)
{
    block_position.read(s);
    block_runtime_id = s.getUnsignedVarint();
    update_flag = s.getUnsignedVarint();
    block_layer = s.getUnsignedVarint();
}

void UpdateBlockPacket::write(BinaryStream &s) const
{
    block_position.write(s);
    s.writeUnsignedVarint(block_runtime_id);
    s.writeUnsignedVarint(update_flag);
    s.writeUnsignedVarint(block_layer);
}

// ==================== BlockActorDataPacket ====================

// Forward declaration of helper (defined below)
namespace {

std::vector<uint8_t> readNetworkNbt(const ReadOnlyBinaryStream &s)
{
  const auto pos = s.getPosition();
    while (true) {
    const uint8_t field_type = s.getByte();
        if (field_type == 0) break;  // TAG_End

        // Skip field name (unsigned varint length + UTF-8 bytes)
        const auto name_len = s.getUnsignedVarint();
        (void)s.readRawBytes(name_len);

        // Skip field value based on type
        auto zigzag = [](uint32_t v) -> int32_t {
            return static_cast<int32_t>((v >> 1) ^ -(v & 1));
        };

        switch (field_type) {
        case 1: (void)s.getByte(); break;                                                             // TAG_Byte
        case 2:  // TAG_Short / TAG_Int — both zigzag varint in Bedrock network format
        case 3: (void)zigzag(s.getUnsignedVarint()); break;
        case 4: (void)s.getVarint64(); break;                                                         // TAG_Long
        case 5: (void)s.getFloat(); break;
        case 6: (void)s.getDouble(); break;
        case 7: {   // TAG_ByteArray (zigzag length)
            auto len = zigzag(s.getUnsignedVarint());
            (void)s.readRawBytes(len);
            break;
        }
        case 8: {   // TAG_String (unsigned varint length)
            auto len = s.getUnsignedVarint();
            (void)s.readRawBytes(len);
            break;
        }
        case 9: {   // TAG_List
            auto elem_type = s.getByte();
            auto count = zigzag(s.getUnsignedVarint());
            for (int32_t i = 0; i < count; ++i) {
                // skip each element — this recursive approach works for simple cases
                // For the current use case (lore strings), we just skip bytes
                // A full skip would need a proper NBT skipper
                (void)elem_type;
            }
            break;
        }
        case 10: {  // TAG_Compound
            while (true) {
                auto inner_type = s.getByte();
                if (inner_type == 0) break;
                auto inner_name_len = s.getUnsignedVarint();
                (void)s.readRawBytes(inner_name_len);
                // Would need full recursive skip — for now, try to continue
                // This is simplified; for complex NBT use from_network_into
                (void)inner_type;
            }
            break;
        }
        case 11: {  // TAG_IntArray
            auto len = zigzag(s.getUnsignedVarint());
            for (int32_t i = 0; i < len; ++i) (void)s.getInt();
            break;
        }
        case 12: {  // TAG_LongArray
            auto len = zigzag(s.getUnsignedVarint());
            for (int32_t i = 0; i < len; ++i) (void)s.getLong();
            break;
        }
        default:
            return {};
        }
    }
    auto end_pos = s.getPosition();
    s.setPosition(pos);
    auto result = s.readRawBytes(end_pos - pos);
    return result;
}

} // anonymous namespace

void BlockActorDataPacket::read(ReadOnlyBinaryStream &s)
{
    block_position.read(s);
    actor_data_tags = readNetworkNbt(s);
}

void BlockActorDataPacket::write(BinaryStream &s) const
{
    block_position.write(s);
    s.writeRawBytes(actor_data_tags);
}

// ==================== InventoryContentPacket ====================

void InventoryContentPacket::read(ReadOnlyBinaryStream &s)
{
    container_id = s.getUnsignedVarint();
    // Items reading not implemented in Phase 1
    (void)s;
}

void InventoryContentPacket::write(BinaryStream &s) const
{
    s.writeUnsignedVarint(container_id);
    s.writeUnsignedVarint(static_cast<uint32_t>(items.size()));
    for (const auto &item : items) {
        item.write(s);
    }
    container_name.write(s);
    storage.write(s);
}

// ==================== InventorySlotPacket ====================

void InventorySlotPacket::read(ReadOnlyBinaryStream &)
{
    // Not implemented in Phase 1
}

void InventorySlotPacket::write(BinaryStream &s) const
{
    s.writeUnsignedVarint(container_id);
    s.writeUnsignedVarint(slot);
    container_name.write(s);
    (void)storage.writeHeader(s);
    item.write(s);
}

// ==================== ItemStackRequestPacket ====================

void ItemStackRequestPacket::read(ReadOnlyBinaryStream &s)
{
    request.read(s);
}

void ItemStackRequestPacket::write(BinaryStream &s) const
{
    request.write(s);
}

// ==================== ItemRegistryPacket ====================

void ItemRegistryPacket::read(ReadOnlyBinaryStream &s)
{
    auto length = s.getUnsignedVarint();
    item_registry.reserve(length);
    for (uint32_t i = 0; i < length; ++i) {
        ItemData data;
        data.read(s);
        item_registry.push_back(std::move(data));
    }
}

void ItemRegistryPacket::write(BinaryStream &s) const
{
    s.writeUnsignedVarint(static_cast<uint32_t>(item_registry.size()));
    for (const auto &data : item_registry) {
        data.write(s);
    }
}
