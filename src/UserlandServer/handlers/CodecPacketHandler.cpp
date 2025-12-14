/*
 * CodecPacketHandler.cpp
 */
#include "CodecPacketHandler.h"

void 
CodecPacketHandler::Handle(NetworkServer* server, NetworkServer::ClientState* client, const haiku::remote::InputEvent& event) 
{
	if (!event.has_codec()) return;

	const haiku::remote::CodecChangeEvent& args = event.codec();
	
	BMessage msg(MSG_CHANGE_CODEC);
	msg.AddString("codec", args.codec().c_str());
	
	server->SendMessageToTarget(&msg);
}
