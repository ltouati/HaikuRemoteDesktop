/*
 * PingPacketHandler.h
 */
#ifndef PING_PACKET_HANDLER_H
#define PING_PACKET_HANDLER_H

#include "PacketHandler.h"

class PingPacketHandler : public PacketHandler {
public:
	virtual void Handle(NetworkServer* server, NetworkServer::ClientState* client, const haiku::remote::InputEvent& event);
};

#endif // PING_PACKET_HANDLER_H
