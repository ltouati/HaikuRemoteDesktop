/*
 * PacketHandler.h
 * Abstract base class for handling InputEvents
 */
#ifndef PACKET_HANDLER_H
#define PACKET_HANDLER_H

#include <SupportDefs.h>
#include "NetworkServer.h"
#include "messages.pb.h"

class PacketHandler {
public:
	virtual ~PacketHandler() {}
	virtual void Handle(NetworkServer* server, NetworkServer::ClientState* client, const haiku::remote::InputEvent& event) = 0;
};

#endif // PACKET_HANDLER_H
