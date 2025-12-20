/*
 * CodecPacketHandler.h
 */
#ifndef CODEC_PACKET_HANDLER_H
#define CODEC_PACKET_HANDLER_H

#include "PacketHandler.h"

class CodecPacketHandler final : public PacketHandler {
public:
    void Handle(NetworkServer *server, NetworkServer::ClientState *client,
                const haiku::remote::InputEvent &event) override;
};

#endif // CODEC_PACKET_HANDLER_H