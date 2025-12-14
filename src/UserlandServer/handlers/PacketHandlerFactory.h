/*
 * PacketHandlerFactory.h
 */
#ifndef PACKET_HANDLER_FACTORY_H
#define PACKET_HANDLER_FACTORY_H

#include "PacketHandler.h"
#include <map>

class PacketHandlerFactory {
public:
	static PacketHandler* GetHandler(haiku::remote::InputEvent::EventType type);

private:
	// Singleton-ish handlers storage?
	// For simplicity, we can just return static instances or new ones.
	// Since KeyHandler has state (the Map), we should keep it alive.
	// Let's use static instances in the implementation.
};

#endif // PACKET_HANDLER_FACTORY_H
