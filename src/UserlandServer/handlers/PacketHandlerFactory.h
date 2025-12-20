/*
 * PacketHandlerFactory.h
 */
#ifndef PACKET_HANDLER_FACTORY_H
#define PACKET_HANDLER_FACTORY_H

#include "PacketHandler.h"

class PacketHandlerFactory {
public:
    static PacketHandler *GetHandler(haiku::remote::InputEvent::EventType type);
};

#endif // PACKET_HANDLER_FACTORY_H
