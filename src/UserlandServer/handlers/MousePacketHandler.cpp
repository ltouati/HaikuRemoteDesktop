/*
 * MousePacketHandler.cpp
 */
#include "MousePacketHandler.h"
#include <OS.h>
#include <Screen.h>
#include <string.h>

#include <Messenger.h>
#include <Message.h>
#include <InterfaceDefs.h>
#include <WindowScreen.h> // for set_mouse_position

const char* kInputServerSignature = "application/x-vnd.Be-input_server"; // Unused now

void 

MousePacketHandler::Handle(NetworkServer* server, NetworkServer::ClientState* client, const haiku::remote::InputEvent& event) 
{
	const haiku::remote::MouseEvent& mouse = event.mouse();
	
	input_packet driverEvent;
	memset(&driverEvent, 0, sizeof(driverEvent));
	
	driverEvent.type = PACKET_MOUSE;
	float x = mouse.x();
	float y = mouse.y();
	
	// Clamp to [0.0, 1.0]
	if (x < 0.0f) x = 0.0f;
	if (x > 1.0f) x = 1.0f;
	if (y < 0.0f) y = 0.0f;
	if (y > 1.0f) y = 1.0f;

	driverEvent.data.mouse.x = x;
	driverEvent.data.mouse.y = y;
	driverEvent.data.mouse.buttons = mouse.buttons();
	driverEvent.data.mouse.wheel_x = mouse.wheel_x();
	driverEvent.data.mouse.wheel_y = mouse.wheel_y();
	
	port_id inputPort = server->GetInputPort();
	if (inputPort >= 0) {
		write_port(inputPort, 0, &driverEvent, sizeof(driverEvent));
	}
}
