/*
 * ClipboardPacketHandler.h
 */
#ifndef CLIPBOARD_PACKET_HANDLER_H
#define CLIPBOARD_PACKET_HANDLER_H

#include "PacketHandler.h"

class ClipboardPacketHandler final : public PacketHandler {
public:
    void Handle(NetworkServer *server, NetworkServer::ClientState *client,
                const haiku::remote::InputEvent &event) override;
};

#endif // CLIPBOARD_PACKET_HANDLER_H
