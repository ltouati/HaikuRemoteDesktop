/*
 * CodecPacketHandler.h
 */
#ifndef CODEC_PACKET_HANDLER_H
#define CODEC_PACKET_HANDLER_H

#include "PacketHandler.h"

class CodecPacketHandler : public PacketHandler {
public:
	virtual void Handle(NetworkServer* server, NetworkServer::ClientState* client, const haiku::remote::InputEvent& event);
};

#endif // CODEC_PACKET_HANDLER_H
