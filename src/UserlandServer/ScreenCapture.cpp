/*
 * ScreenCapture.cpp
 */
#include "ScreenCapture.h"
#include <stdio.h>
#include <string.h>
#include <InterfaceDefs.h>
#include <WindowScreen.h>

ScreenCapture::ScreenCapture()
    : BDirectWindow(BRect(-10, -10, -9, -9), "ScreenCapture", B_NO_BORDER_WINDOW_LOOK, B_FLOATING_ALL_WINDOW_FEEL,
                    B_NOT_MOVABLE | B_NOT_RESIZABLE | B_AVOID_FOCUS | B_AVOID_FRONT | B_NOT_ANCHORED_ON_ACTIVATE),
      fLock("ScreenCaptureLock"), fScreenBits(nullptr), fRowBytes(0), fWidth(0), fHeight(0) {
    fCaptureView = new BView(BRect(0, 0, 1, 1), "CursorTrackingView", B_FOLLOW_NONE, 0);
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

    // Keep it 1x1 and off-screen. 
    // As long as it's "Show()", we should get DirectConnected updates.
    Show();

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