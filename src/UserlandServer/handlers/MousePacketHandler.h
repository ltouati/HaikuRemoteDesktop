/*
 * MousePacketHandler.h
 */
#ifndef MOUSE_PACKET_HANDLER_H
#define MOUSE_PACKET_HANDLER_H

#include "PacketHandler.h"

class MousePacketHandler : public PacketHandler {
public:
	virtual void Handle(NetworkServer* server, NetworkServer::ClientState* client, const haiku::remote::InputEvent& event);
};

#endif // MOUSE_PACKET_HANDLER_H
