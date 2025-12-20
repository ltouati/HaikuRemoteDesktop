/*
 * ResolutionPacketHandler.h
 */
#ifndef RESOLUTION_PACKET_HANDLER_H
#define RESOLUTION_PACKET_HANDLER_H

#include "PacketHandler.h"

class ResolutionPacketHandler final : public PacketHandler {
public:
    void Handle(NetworkServer *server, NetworkServer::ClientState *client,
                const haiku::remote::InputEvent &event) override;
};

#endif // RESOLUTION_PACKET_HANDLER_H