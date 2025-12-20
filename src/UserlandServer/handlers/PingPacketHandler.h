/*
 * PingPacketHandler.h
 */
#ifndef PING_PACKET_HANDLER_H
#define PING_PACKET_HANDLER_H

#include "PacketHandler.h"

class PingPacketHandler final : public PacketHandler {
public:
    void Handle(NetworkServer *server, NetworkServer::ClientState *client,
                const haiku::remote::InputEvent &event) override;
};

#endif // PING_PACKET_HANDLER_H