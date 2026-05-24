#pragma once

#include "packets.h"

#include <endstone/endstone.hpp>

#include <cmath>
#include <numbers>

// ==================== Packet sending helpers ====================

/// Helper: serialize a Packet and send it via player.sendPacket.
inline void sendPacket(const endstone::Player &player, const Packet &pk)
{
  const auto payload = pk.serialize();
    player.sendPacket(static_cast<int>(pk.getId()),
                      {reinterpret_cast<const char *>(payload.data()), payload.size()});
}

/// Place a block at the given position (network-only).
inline void sendBlock(const endstone::Player &player, const std::string &name, const BlockPos &pos)
{
  const auto block_data = player.getServer().createBlockData(name);
  const UpdateBlockPacket pk(BlockPos(pos.x, pos.y, pos.z), block_data->getRuntimeId(), 0b0011, 0);
    sendPacket(player, pk);
}

/// Restore the original block at the given position.
inline void restoreBlock(const endstone::Player &player, const BlockPos &pos)
{
  const auto &dimension = player.getDimension();
  const auto block = dimension.getBlockAt(pos.x, pos.y, pos.z);
  if (const auto block_data = player.getServer().createBlockData(block->getType())) {
    const UpdateBlockPacket pk(pos, block_data->getRuntimeId(), 0b0011, 0);
        sendPacket(player, pk);
    }
}

// ==================== Position helpers ====================

[[nodiscard]]
inline BlockPos getBlockBehind(const endstone::Player &player, int distance = 1)
{
    auto loc = player.getLocation();
    float yaw = loc.getYaw();

    float behind_yaw = yaw + 180.0f;
    float yaw_rad = behind_yaw * std::numbers::pi_v<float> / 180.0f;
    float x = -std::sin(yaw_rad) * static_cast<float>(distance);
    float z = std::cos(yaw_rad) * static_cast<float>(distance);

    return BlockPos(
        static_cast<int>(std::floor(loc.getX() + x)),
        static_cast<int>(std::floor(loc.getY()) + 1),
        static_cast<int>(std::floor(loc.getZ() + z))
    );
}

/// Get a block position above the player's head (y+4), with height limit check.
[[nodiscard]]
inline BlockPos abovePlayer(const endstone::Player &player)
{
    auto loc = player.getLocation();
    int x = static_cast<int>(std::floor(loc.getX()));
    int y = static_cast<int>(std::floor(loc.getY())) + 4;
    int z = static_cast<int>(std::floor(loc.getZ()));

    auto dim_type = loc.getDimension().getType();
    int max_y = 320;
    if (dim_type == endstone::Dimension::Type::Nether) max_y = 128;
    else if (dim_type == endstone::Dimension::Type::TheEnd) max_y = 256;

    if (y >= max_y) {
        y = static_cast<int>(std::floor(loc.getY())) - 3;
    }

    return BlockPos(x, y, z);
}

[[nodiscard]]
inline BlockPos west(const BlockPos &pos) { return BlockPos(pos.x - 1, pos.y, pos.z); }
[[nodiscard]]
inline BlockPos east(const BlockPos &pos) { return BlockPos(pos.x + 1, pos.y, pos.z); }
