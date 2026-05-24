#pragma once

#include <endstone/endstone.hpp>

class EventListener {
public:
    explicit EventListener(endstone::Plugin &plugin) : plugin_(plugin) {}

    void onPacketReceive(const endstone::PacketReceiveEvent &event) const;
    void onPacketSend(const endstone::PacketSendEvent &event) const;
    static void onPlayerQuit(const endstone::PlayerQuitEvent &event);

private:
    endstone::Plugin &plugin_;
};
