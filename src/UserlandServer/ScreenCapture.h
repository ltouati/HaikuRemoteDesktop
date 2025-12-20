/*
 * ScreenCapture.h
 * Handles interaction with BScreen and Double Buffering
 */
#ifndef SCREEN_CAPTURE_H
#define SCREEN_CAPTURE_H

#include <InterfaceDefs.h>
#include <Bitmap.h>
#include <Screen.h>
#include <DirectWindow.h>
#include <Locker.h>
#include <View.h>

class ScreenCapture : public BDirectWindow {
public:
    ScreenCapture();

    ~ScreenCapture();

    status_t Init();

    // DirectConnected hook
    virtual void DirectConnected(direct_buffer_info *info);

    // Captures the screen (legacy, now mostly no-op or sync)
    status_t Capture(bool waitRetrace = true);

    // Returns the raw bits and stride
    const uint8 *GetScreenBits() const { return fScreenBits; }
    uint32 GetRowBytes() const { return fRowBytes; }

    int32 Width() const { return fWidth; }
    int32 Height() const { return fHeight; }

    bool IsConnected() const { return fScreenBits != nullptr; }

    BView *GetCaptureView() const { return fCaptureView; }

private:
    BLocker fLock;
    uint8 *fScreenBits;
    uint32 fRowBytes;
    clipping_rect fBounds;
    color_space fFormat;

    int32 fWidth;
    int32 fHeight;

    BView *fCaptureView;
};

#endif // SCREEN_CAPTURE_H