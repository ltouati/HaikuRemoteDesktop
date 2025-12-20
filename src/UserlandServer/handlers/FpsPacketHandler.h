/*
 * FpsPacketHandler.h
 */
#ifndef FPS_PACKET_HANDLER_H
#define FPS_PACKET_HANDLER_H

#include "PacketHandler.h"

class FpsPacketHandler : public PacketHandler {
public:
    FpsPacketHandler();
    virtual ~FpsPacketHandler();

    virtual void Handle(NetworkServer *server, NetworkServer::ClientState *client,
                        const haiku::remote::InputEvent &event);
};

#endif // FPS_PACKET_HANDLER_H
