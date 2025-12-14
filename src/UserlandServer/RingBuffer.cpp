/*
 * RingBuffer.cpp
 */
#include "RingBuffer.h"
#include <string.h>
#include <stdio.h>

RingBuffer::RingBuffer(void* buffer, size_t size) 
	: fBuffer((uint8*)buffer), fSize(size), fHead(0) 
{
}

RingBuffer::~RingBuffer() 
{
	// We don't own the buffer memory (it's from create_area), so we don't delete it.
}

int32 
RingBuffer::Write(const void* data, size_t len) 
{
	int32 startOffset = fHead;
	size_t firstPart = 0;
	size_t secondPart = 0;

	if (fHead + len <= fSize) {
		firstPart = len;
	} else {
		firstPart = fSize - fHead;
		secondPart = len - firstPart;
	}

	memcpy(fBuffer + fHead, data, firstPart);
	
	if (secondPart > 0) {
		memcpy(fBuffer, (uint8*)data + firstPart, secondPart);
		fHead = secondPart;
	} else {
		fHead += len;
		if (fHead == fSize) fHead = 0;
	}
	
	return startOffset;
}
