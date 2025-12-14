/*
 * ResolutionPacketHandler.cpp
 */
#include "ResolutionPacketHandler.h"

void 
ResolutionPacketHandler::Handle(NetworkServer* server, NetworkServer::ClientState* client, const haiku::remote::InputEvent& event) 
{
	if (!event.has_resolution()) return;

	const haiku::remote::ResolutionEvent& resArgs = event.resolution();
	
	BMessage msg(MSG_CHANGE_RESOLUTION);
	msg.AddInt32("width", resArgs.width());
	msg.AddInt32("height", resArgs.height());
	
	server->SendMessageToTarget(&msg);
}
