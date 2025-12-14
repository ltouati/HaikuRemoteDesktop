/*
 * NetworkUtils.h
 * Helper functions for WebSocket Handshake (SHA1, Base64)
 */
#ifndef NETWORK_UTILS_H
#define NETWORK_UTILS_H

#include <SupportDefs.h>
#include <String.h>

class NetworkUtils {
public:
	static void SHA1(const uint8* data, size_t len, uint8* outHash);
	static BString Base64Encode(const uint8* data, size_t len);
	static size_t MakeWebSocketHeader(size_t payloadLen, uint8* frame, uint8 opcode);
};

#endif // NETWORK_UTILS_H
