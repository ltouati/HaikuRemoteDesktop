/*
 * ScreenCapture.cpp
 */
#include "ScreenCapture.h"
#include <InterfaceDefs.h>
#include <WindowScreen.h>

    // 1x1 Transparent Spy Window at (0,0)
ScreenCapture::ScreenCapture()
    : BDirectWindow(BRect(0, 0, 0, 0), "ScreenCapture", B_NO_BORDER_WINDOW_LOOK, B_NORMAL_WINDOW_FEEL,
                    B_NOT_MOVABLE | B_NOT_RESIZABLE | B_AVOID_FOCUS | B_AVOID_FRONT | B_NOT_ANCHORED_ON_ACTIVATE),
      fLock("ScreenCaptureLock"), fScreenBits(nullptr), fRowBytes(0), fWidth(0), fHeight(0) {
    fCaptureView = new BView(BRect(0, 0, 0, 0), "CursorTrackingView", B_FOLLOW_NONE, 0);
    fCaptureView->SetViewColor(B_TRANSPARENT_COLOR);
    AddChild(fCaptureView);
}

ScreenCapture::~ScreenCapture() {
}

status_t
ScreenCapture::Init() {
    BScreen screen(B_MAIN_SCREEN_ID);
    if (!screen.IsValid()) return B_ERROR;

    BRect frame = screen.Frame();
    fWidth = (int32) frame.IntegerWidth() + 1;
    fHeight = (int32) frame.IntegerHeight() + 1;

    // Strategy: 1x1 Transparent Window at Top-Left
    // We need to be "on screen" to get DirectConnected, but we want to be invisible.
    // Fullscreen window caused visual obstruction. 1x1 should be negligible.
    MoveTo(0, 0);
    ResizeTo(0, 0); // 1x1 (width/height is size-1 in Haiku Rect?) No, resize to width/height. 0,0 is 1px?
    // BRect(0,0,0,0) is 1 pixel wide/high (0 to 0 inclusive).
    
    Show();
    SendBehind(nullptr); 
    
    return B_OK;
}

void
ScreenCapture::DirectConnected(direct_buffer_info *info) {
    fLock.Lock();

    switch (info->buffer_state & B_DIRECT_MODE_MASK) {
        case B_DIRECT_START:
        case B_DIRECT_MODIFY:
            fScreenBits = (uint8 *) info->bits;
            fRowBytes = info->bytes_per_row;
            fBounds = info->window_bounds;
            fFormat = info->pixel_format;
            break;

        case B_DIRECT_STOP:
            fScreenBits = nullptr;
            break;
    }

    fLock.Unlock();
}

status_t
ScreenCapture::Capture(bool waitRetrace) {
    if (waitRetrace) {
        BScreen screen(B_MAIN_SCREEN_ID);
        screen.WaitForRetrace(33000);
    }

    return fScreenBits ? B_OK : B_ERROR;
}