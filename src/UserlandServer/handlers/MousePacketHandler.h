/*
 * MousePacketHandler.h
 */
#ifndef MOUSE_PACKET_HANDLER_H
#define MOUSE_PACKET_HANDLER_H

#include "PacketHandler.h"

class MousePacketHandler final : public PacketHandler {
public:
    void Handle(NetworkServer *server, NetworkServer::ClientState *client,
                const haiku::remote::InputEvent &event) override;
};

#endif // MOUSE_PACKET_HANDLER_H