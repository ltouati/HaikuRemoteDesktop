/*
 * ClipboardPacketHandler.h
 */
#ifndef CLIPBOARD_PACKET_HANDLER_H
#define CLIPBOARD_PACKET_HANDLER_H

#include "PacketHandler.h"

class ClipboardPacketHandler : public PacketHandler {
public:
	virtual void Handle(NetworkServer* server, NetworkServer::ClientState* client, const haiku::remote::InputEvent& event);
};

#endif // CLIPBOARD_PACKET_HANDLER_H
