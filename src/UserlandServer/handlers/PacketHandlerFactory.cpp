/*
 * PacketHandlerFactory.cpp
 */
#include "PacketHandlerFactory.h"
#include "MousePacketHandler.h"
#include "KeyPacketHandler.h"
#include "PingPacketHandler.h"
#include "ResolutionPacketHandler.h"
#include "CodecPacketHandler.h"
#include "ClipboardPacketHandler.h"
#include "FpsPacketHandler.h"

PacketHandler *
PacketHandlerFactory::GetHandler(haiku::remote::InputEvent::EventType type) {
    static MousePacketHandler mouseHandler;
    static KeyPacketHandler keyHandler;
    static PingPacketHandler pingHandler;
    static ResolutionPacketHandler resHandler;
    static CodecPacketHandler codecHandler;
    static ClipboardPacketHandler clipboardHandler;

    switch (type) {
        case haiku::remote::InputEvent::MOUSE:
            return &mouseHandler;
        case haiku::remote::InputEvent::KEY:
            return &keyHandler;
        case haiku::remote::InputEvent::PING:
            return &pingHandler;
        case haiku::remote::InputEvent::RESOLUTION:
            return &resHandler;
        case haiku::remote::InputEvent::CODEC:
            return &codecHandler;
        case haiku::remote::InputEvent::CLIPBOARD:
            return &clipboardHandler;
        case haiku::remote::InputEvent::FPS:
        {
            static FpsPacketHandler fpsHandler;
            return &fpsHandler;
        }
        default:
            return nullptr;
    }
}