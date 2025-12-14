/*
 * NetworkUtils.cpp
 */
#include "NetworkUtils.h"
#include <openssl/sha.h>
#include <openssl/evp.h>
#include <openssl/buffer.h>
#include <arpa/inet.h>
#include <string.h>

void 
NetworkUtils::SHA1(const uint8* data, size_t len, uint8* outHash) 
{
	::SHA1(data, len, outHash);
}

BString 
NetworkUtils::Base64Encode(const uint8* data, size_t len) 
{
	BString result;
	
	BIO *bmem, *b64;
	BUF_MEM *bptr;

	b64 = BIO_new(BIO_f_base64());
	BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
	bmem = BIO_new(BIO_s_mem());
	b64 = BIO_push(b64, bmem);
	
	BIO_write(b64, data, len);
	BIO_flush(b64);
	BIO_get_mem_ptr(b64, &bptr);

	result.SetTo(bptr->data, bptr->length);
	
	BIO_free_all(b64);
	
	return result;
}

size_t 
NetworkUtils::MakeWebSocketHeader(size_t payloadLen, uint8* frame, uint8 opcode) 
{
	// FIN bit set (0x80) | opcode
	frame[0] = 0x80 | (opcode & 0x0F);
	
	if (payloadLen < 126) {
		frame[1] = (uint8)payloadLen;
		return 2;
	} else if (payloadLen < 65536) {
		frame[1] = 126;
		uint16 len = htons((uint16)payloadLen);
		memcpy(frame + 2, &len, 2);
		return 4;
	} else {
		frame[1] = 127;
		// 64-bit length needed. 
		// Manual Big-Endian pack
		uint64 len64 = (uint64)payloadLen;
		for (int i = 0; i < 8; i++) {
			frame[2 + i] = (len64 >> ((7 - i) * 8)) & 0xFF;
		}
		return 10;
	}
}
