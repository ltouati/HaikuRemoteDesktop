/*
 * ScreenCapture.cpp
 */
#include "ScreenCapture.h"
#include <stdio.h>
#include <string.h>

ScreenCapture::ScreenCapture()
	: fCurrentIdx(0), fWidth(0), fHeight(0)
{
	fBitmaps[0] = NULL;
	fBitmaps[1] = NULL;
}

ScreenCapture::~ScreenCapture()
{
	if (fBitmaps[0]) delete fBitmaps[0];
	if (fBitmaps[1]) delete fBitmaps[1];
}

status_t 
ScreenCapture::Init() 
{
	if (fBitmaps[0]) {
		delete fBitmaps[0];
		fBitmaps[0] = NULL;
	}
	if (fBitmaps[1]) {
		delete fBitmaps[1];
		fBitmaps[1] = NULL;
	}

	BScreen screen(B_MAIN_SCREEN_ID);
	if (!screen.IsValid()) return B_ERROR;

	BRect frame = screen.Frame();
	fWidth = (int32)frame.IntegerWidth() + 1;
	fHeight = (int32)frame.IntegerHeight() + 1;

	// Create BBitmaps (Double Buffering)
	fBitmaps[0] = new BBitmap(frame, B_RGB32);
	fBitmaps[1] = new BBitmap(frame, B_RGB32);
	
	if (fBitmaps[0]->InitCheck() != B_OK) return fBitmaps[0]->InitCheck();
	if (fBitmaps[1]->InitCheck() != B_OK) return fBitmaps[1]->InitCheck();
	
	// Clear them to zero (black)
	memset(fBitmaps[0]->Bits(), 0, fBitmaps[0]->BitsLength());
	memset(fBitmaps[1]->Bits(), 0, fBitmaps[1]->BitsLength());

	return B_OK;
}

status_t 
ScreenCapture::Capture(bool waitRetrace) 
{
	BScreen screen(B_MAIN_SCREEN_ID);
	
	// Force screen on (in case DPMS)
	// screen.SetDPMS(B_DPMS_ON); // Move to app level? Or keep here.
	// BScreen is lightweight so this is fine.
	
	if (waitRetrace) screen.WaitForRetrace(33000);
	
	return screen.ReadBitmap(fBitmaps[fCurrentIdx]);
}

BBitmap* 
ScreenCapture::GetBitmap() 
{
	return fBitmaps[fCurrentIdx];
}

BBitmap* 
ScreenCapture::GetPreviousBitmap() 
{
	return fBitmaps[1 - fCurrentIdx];
}

void 
ScreenCapture::SwapBuffers() 
{
	fCurrentIdx = 1 - fCurrentIdx;
}
