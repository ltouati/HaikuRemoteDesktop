/*
 * PingPacketHandler.cpp
 */
#include "PingPacketHandler.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>

void 
PingPacketHandler::Handle(NetworkServer* server, NetworkServer::ClientState* client, const haiku::remote::InputEvent& event) 
{
	if (!event.has_ping()) return;

	// 1. Congestion Control
	const haiku::remote::PingEvent& pingMsg = event.ping();
	int32 rtt = pingMsg.last_rtt();
	
	if (rtt >= 0) {
		int32 oldBitrate = server->GetBitrate();
		int32 newBitrate = oldBitrate;
		
		if (rtt > 150) {
			// Backoff 20%
			newBitrate = (int32)(oldBitrate * 0.8);
			if (newBitrate < 500) newBitrate = 500;
		} else if (rtt < 50) {
			// Increase 5%
			newBitrate = (int32)(oldBitrate * 1.05);
			if (newBitrate > 8000) newBitrate = 8000;
		}
		
		server->SetBitrate(newBitrate);

		// Notify if change is significant (> 50kbps) from the *previous*
		if (abs(newBitrate - oldBitrate) > 50) {
			// printf("Rate Update: %d kbps (RTT: %d ms)\n", newBitrate, rtt);
			
			BMessage msg(MSG_UPDATE_BITRATE);
			msg.AddInt32("bitrate", newBitrate);
			server->SendMessageToTarget(&msg);
		}
	}

	// 2. Pong (Echo)
	// We need to access the raw data? 
	// The interface provided the parsed event.
	// To echo exact payload, we might need to reconstruct it or pass raw buffer.
	// However, the original code echoed "data, len" which was the raw InputEvent protobuf.
	// But `Handle` interface takes parsed event.
	// Let's modify the interface or just re-encode the pong? 
	// Actually, the original requirement to echo "data" implies echoing the *InputEvent* bytes.
	// The `Handle` signature I designed takes `const haiku::remote::InputEvent& event`.
	// I don't have the raw bytes in `Handle`.
	
	// FIX: Let's pass raw data to `Handle` as well.
	// To avoid changing interface file via write_to_file again right now (inefficient),
	// I will just re-serialize the event. Protocols usually allow this or simplified pong.
	// But wait, the client expects the same ping payload back?
	// The client JS:
	// if (msg.type === 3 && msg.ping) { const sent = msg.ping.timestamp; ... }
	// So re-serializing the parsed event is fine.
	
	size_t size = event.ByteSizeLong();
	uint8* buffer = new uint8[size];
	event.SerializeToArray(buffer, size);
	
	// Create WebSocket Frame for PONG (or just Binary message containing the InputEvent)
	// The original code sent a WebSocket Binary Frame (0x82) containing the InputEvent bytes.
	
	uint8 header[10];
	size_t headerSize = 2;
	header[0] = 0x82; // Binary Frame
	
	if (size < 126) {
		header[1] = (uint8)size;
	} else if (size < 65536) {
		header[1] = 126;
		header[2] = (size >> 8) & 0xFF;
		header[3] = size & 0xFF;
		headerSize = 4;
	} else {
		delete[] buffer;
		return;
	}
	
	// Use Server's raw send helper
	server->SendToClient(client, header, headerSize);
	server->SendToClient(client, buffer, size);
	
	delete[] buffer;
}
