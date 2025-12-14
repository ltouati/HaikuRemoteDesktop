/*
 * ScreenCapture.h
 * Handles interaction with BScreen and Double Buffering
 */
#ifndef SCREEN_CAPTURE_H
#define SCREEN_CAPTURE_H

#include <InterfaceDefs.h>
#include <Bitmap.h>
#include <Screen.h>

class ScreenCapture {
public:
	ScreenCapture();
	~ScreenCapture();

	status_t Init();
	
	// Captures the screen into the "current" buffer
	status_t Capture(bool waitRetrace = true);
	
	// Returns the buffer that was just captured
	BBitmap* GetBitmap();
	BBitmap* GetPreviousBitmap(); // Access back buffer
	
	// Swaps buffers for the NEXT capture
	void SwapBuffers();
	
	int32 Width() const { return fWidth; }
	int32 Height() const { return fHeight; }

private:
	BBitmap* fBitmaps[2];
	int fCurrentIdx;
	int32 fWidth;
	int32 fHeight;
};

#endif // SCREEN_CAPTURE_H
