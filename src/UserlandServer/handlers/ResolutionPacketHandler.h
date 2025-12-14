/*
 * ResolutionPacketHandler.h
 */
#ifndef RESOLUTION_PACKET_HANDLER_H
#define RESOLUTION_PACKET_HANDLER_H

#include "PacketHandler.h"

class ResolutionPacketHandler : public PacketHandler {
public:
	virtual void Handle(NetworkServer* server, NetworkServer::ClientState* client, const haiku::remote::InputEvent& event);
};

#endif // RESOLUTION_PACKET_HANDLER_H
