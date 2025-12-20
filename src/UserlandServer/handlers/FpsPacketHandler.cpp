/*
 * FpsPacketHandler.cpp
 */
#include "FpsPacketHandler.h"
#include "../NetworkServer.h" // For MSG_CHANGE_FPS
#include <stdio.h>

FpsPacketHandler::FpsPacketHandler() = default;

FpsPacketHandler::~FpsPacketHandler() = default;

void
FpsPacketHandler::Handle(NetworkServer *server, NetworkServer::ClientState *client,
                         const haiku::remote::InputEvent &event) {
    if (event.type() != haiku::remote::InputEvent::FPS) return;

    const haiku::remote::FpsChangeEvent &fpsEvent = event.fps();
    int32 fps = fpsEvent.fps();

    printf("FPS Change Request: %d\n", fps);

    BMessage *msg = new BMessage(MSG_CHANGE_FPS);
    msg->AddInt32("fps", fps);

    server->SendMessageToTarget(msg);
}
