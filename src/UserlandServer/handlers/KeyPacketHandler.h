/*
 * KeyPacketHandler.h
 */
#ifndef KEY_PACKET_HANDLER_H
#define KEY_PACKET_HANDLER_H

#include "PacketHandler.h"
#include <map>
#include <string>

class KeyPacketHandler : public PacketHandler {
public:
	KeyPacketHandler();
	virtual void Handle(NetworkServer* server, NetworkServer::ClientState* client, const haiku::remote::InputEvent& event);

private:
	struct KeyInfo {
		uint32 scancode;
		uint32 charcode;
	};
	std::map<std::string, KeyInfo> fKeyMap;
	void _InitKeyMap();
};

#endif // KEY_PACKET_HANDLER_H
