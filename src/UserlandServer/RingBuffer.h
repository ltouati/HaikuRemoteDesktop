/*
 * RingBuffer.h
 * Circular buffer for Screen Capture Server
 */
#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#include <SupportDefs.h>

class RingBuffer {
public:
	RingBuffer(void* buffer, size_t size);
	~RingBuffer();

	// returns offset where data was written
	int32 Write(const void* data, size_t len);
	
	size_t Size() const { return fSize; }

private:
	uint8* fBuffer;
	size_t fSize;
	volatile int32 fHead;
};

#endif // RING_BUFFER_H
