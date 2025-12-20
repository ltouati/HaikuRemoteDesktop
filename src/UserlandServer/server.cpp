#include <Application.h>
#include <OS.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>

#include "ScreenCapture.h"
#include "VideoEncoder.h"
#include "NetworkServer.h"
#include "NetworkServer.h"
#include "InputDriverManager.h"
#include "NetworkUtils.h"
#include <Screen.h>
#include <Clipboard.h>

#define APP_SIGNATURE "application/x-vnd.Haiku-ScreenCaster"
#define AREA_NAME "screen_capture_shm"
#define RING_BUFFER_SIZE (16 * 1024 * 1024)

class ScreenApp : public BApplication {
public:
    ScreenApp() : BApplication(APP_SIGNATURE) {
        fNetworkThread = -1;
        fCaptureThread = -1;
        fTerminating = false;
        fCapturing = false;
        fScreenCapture = new ScreenCapture();
        fVideoEncoder = new VideoEncoder();
        fNetworkServer = nullptr;
        fInputManager = new InputDriverManager();
        fCurrentCodec = "vp8";
        fTargetFps = 30;
        fFrameWaitTime = 33333; // ~30 FPS
        fCaptureSem = create_sem(0, "CaptureSignal");
    }

    ~ScreenApp() {
        if (fScreenCapture) {
            delete fScreenCapture;
        }
        if (fVideoEncoder) {
            delete fVideoEncoder;
        }
        if (fNetworkServer) {
            delete fNetworkServer;
        }
        if (fInputManager) {
            delete fInputManager;
        }
        delete_sem(fCaptureSem);
    }

    virtual void ReadyToRun() {
        status_t err = _InitResources();
        if (err < B_OK) {
            fprintf(stderr, "Failed to initialize resources: %s\n", strerror(err));
            Quit();
            return;
        }


        // 1. Install Driver & Restart Input Server
#ifndef RELEASE_MODE
        fInputManager->InstallDriver();

        // Wait for Input Server to come up? InstallDriver snoozes 1s.
        snooze(1000000);
#endif

        // 2. Start Network Server
        // Retry finding port loop? Input Server might take a moment to create the port.
        port_id inputPort = -1;
        for (int i = 0; i < 20; i++) {
            inputPort = find_port("virtual_mouse_input");
            if (inputPort >= 0) break;
            printf("Waiting for virtual_mouse_input port... (%d/20)\n", i + 1);
            snooze(1000000);
        }

        if (inputPort < B_OK) {
            fprintf(stderr, "Warning: VirtualMouse input port not found after install.\n");
        }

        fNetworkServer = new NetworkServer(inputPort);
        if (fNetworkServer->Start(8443) < B_OK) {
            fprintf(stderr, "Failed to bind to port 8443\n");
            Quit();
            return;
        }

        fNetworkServer->SetTarget(BMessenger(this));

        fNetworkThread = spawn_thread(_NetworkLoopSync, "Network Server",
                                      B_NORMAL_PRIORITY, this);

        if (fNetworkThread < B_OK) {
            fprintf(stderr, "Failed to spawn network thread\n");
            Quit();
            return;
        }

        resume_thread(fNetworkThread);

        // Waiting for client...
        printf("Screen Capture Server Running on http://0.0.0.0:8443 (Waiting for client)\n");
    }

    virtual void MessageReceived(BMessage *msg) {
        switch (msg->what) {
            case MSG_CLIENTS_CONNECTED:
                printf("Client Connected: Starting/Restarting Capture\n");
                _StartCapture();
                break;
            case MSG_NO_CLIENTS:
                printf("No Clients: Stopping Capture\n");
                _StopCapture();
                break;
            case MSG_UPDATE_BITRATE: {
                int32 bitrate;
                if (msg->FindInt32("bitrate", &bitrate) == B_OK) {
                    if (fVideoEncoder) fVideoEncoder->SetBitrate(bitrate);
                }
                break;
            }
            case MSG_CHANGE_RESOLUTION: {
                int32 width, height;
                if (msg->FindInt32("width", &width) == B_OK &&
                    msg->FindInt32("height", &height) == B_OK) {
                    _ChangeResolution(width, height);
                }
                break;
            }
            case MSG_CHANGE_CODEC: {
                BString codec;
                if (msg->FindString("codec", &codec) == B_OK) {
                    _ChangeCodec(codec.String());
                }
                break;
            }
            case MSG_CLIPBOARD_EVENT: {
                const char *text;
                if (msg->FindString("text", &text) == B_OK) {
                    _HandleClipboard(text);
                }
                break;
            }
            case MSG_WAKE_CAPTURE:
                release_sem(fCaptureSem);
                break;
            case MSG_CHANGE_FPS: {
                int32 fps;
                if (msg->FindInt32("fps", &fps) == B_OK) {
                    _ChangeFps(fps);
                }
                break;
            }
            default:
                BApplication::MessageReceived(msg);
        }
    }

    virtual bool QuitRequested() {
        fTerminating = true;

        if (fNetworkServer) {
            fNetworkServer->Stop();
        }

        status_t exitValue;
        if (fNetworkThread > 0) {
            wait_for_thread(fNetworkThread, &exitValue);
        }

        _StopCapture();

        if (fScreenCapture) {
            if (fScreenCapture->Lock()) {
                fScreenCapture->Quit();
            }
            fScreenCapture = nullptr;
        }

        // Uninstall Driver (Optional? Or keep it for next time)
#ifndef RELEASE_MODE
        fInputManager->UninstallDriver();
#endif

        _Cleanup();
        return true;
    }

private:
    thread_id fNetworkThread;
    thread_id fCaptureThread;

    volatile bool fTerminating;
    volatile bool fCapturing;

    ScreenCapture *fScreenCapture;
    VideoEncoder *fVideoEncoder;
    NetworkServer *fNetworkServer;
    InputDriverManager *fInputManager;
    BString fCurrentCodec;
    int32 fTargetFps;
    bigtime_t fFrameWaitTime;

    int fFrameCount;
    sem_id fCaptureSem;

    static status_t _NetworkLoopSync(void *data) {
        return ((ScreenApp *) data)->_NetworkLoop();
    }

    static status_t _CaptureLoopSync(void *data) {
        return ((ScreenApp *) data)->_CaptureLoop();
    }

	status_t _InitResources() {
		return B_OK;
	}

    status_t _NetworkLoop() {
        while (!fTerminating && fNetworkServer) {
            fNetworkServer->ProcessEvents();
        }
        return B_OK;
    }

    void _StartCapture() {
        _StopCapture();

        if (fScreenCapture->Init() != B_OK) {
            fprintf(stderr, "Failed to init ScreenCapture\n");
            return;
        }

        fNetworkServer->SetScreenCapture(fScreenCapture);

        if (fVideoEncoder->Init(fScreenCapture->Width(), fScreenCapture->Height(), 2000, fCurrentCodec.String()) !=
            B_OK) {
            fprintf(stderr, "Failed to init VideoEncoder\n");
            return;
        }

        fFrameCount = 0;
        fCapturing = true;

        fCaptureThread = spawn_thread(_CaptureLoopSync, "Screen Capture",
                                      B_DISPLAY_PRIORITY, this);

        if (fCaptureThread >= B_OK) resume_thread(fCaptureThread);
        else fCapturing = false;

        // Send Init Config to Client
        BString config;
        config << "{\"type\": \"init\", \"width\": " << fScreenCapture->Width()
                << ", \"height\": " << fScreenCapture->Height()
                << ", \"codec\": \"" << fVideoEncoder->GetCodecName() << "\"}";

        uint8 headerBuf[16];
        size_t headerLen = NetworkUtils::MakeWebSocketHeader(config.Length(), headerBuf, 0x01); // 0x01 = Text


        fNetworkServer->Broadcast(headerBuf, headerLen, config.String(), config.Length());
        fNetworkServer->SetWelcomeMessage(config.String(), config.Length());
    }

    void _StopCapture() {
        fCapturing = false;
        if (fCaptureThread > 0) {
            status_t exitVal;
            wait_for_thread(fCaptureThread, &exitVal);
            fCaptureThread = -1;
        }
    }

    status_t _CaptureLoop() {
        bigtime_t lastKeyframeTime = system_time();
        bigtime_t nextFrameTime = system_time();

        while (fCapturing && !fTerminating) {
            // Absolute Timing Loop: Sleep only the remainder of the frame budget
            bigtime_t now = system_time();
            bigtime_t waitTime = nextFrameTime - now;

            if (waitTime < 0) {
                // We are late or just starting
                waitTime = 0;
                // If extremely late (e.g. paused/lag spike > 100ms), reset schedule
                if (waitTime < -100000) nextFrameTime = now;
            }

            acquire_sem_etc(fCaptureSem, 1, B_RELATIVE_TIMEOUT, waitTime);
            
            // Advance schedule for next frame
            nextFrameTime += fFrameWaitTime;

            int64 pts = fFrameCount++;

            // Disable waitRetrace (false) to let fFrameWaitTime control the FPS.
            // Otherwise, WaitForRetrace + acquire_sem delay caps us at ~30FPS on 60Hz systems.
            if (fScreenCapture->Capture(false) != B_OK) {
                snooze(10000); // Wait bit more if screen not ready
                continue;
            }

            if (!fScreenCapture->IsConnected()) {
                snooze(10000);
                continue;
            }

            bool forceKeyframe = false;
            now = system_time();
            if (now - lastKeyframeTime > 60000000) forceKeyframe = true;

            // Zero Copy! Direct access to screen memory
            if (fVideoEncoder->Encode(fScreenCapture->GetScreenBits(), fScreenCapture->GetRowBytes(), pts,
                                      forceKeyframe) == B_OK) {
                vpx_codec_iter_t iter = nullptr;
                const vpx_codec_cx_pkt_t *pkt = nullptr;

                while ((pkt = fVideoEncoder->GetNextPacket(&iter)) != nullptr) {
                    if (pkt->kind == VPX_CODEC_CX_FRAME_PKT) {
                        bool isKey = (pkt->data.frame.flags & VPX_FRAME_IS_KEY);
                        if (isKey) lastKeyframeTime = system_time();

                        // Construct Payload: [Meta(1)] + [Frame(N)] + [Magic(4)]
                        size_t frameSz = pkt->data.frame.sz;
                        size_t payloadSz = 1 + frameSz + 4;

                        // Construct WebSocket Header
                        uint8 headerBuf[16];
                        size_t headerLen = NetworkUtils::MakeWebSocketHeader(payloadSz, headerBuf, 0x02); // Binary

                        // Prepare Data Chunks (Scatter/Gather)
                        uint8 metaByte = isKey ? 0x01 : 0x00;
                        static const uint8 magicBytes[] = {0xDE, 0xAD, 0xBE, 0xEF};

                        struct iovec vec[4];

                        // 1. WebSocket Frame Header
                        vec[0].iov_base = headerBuf;
                        vec[0].iov_len = headerLen;

                        // 2. Meta
                        vec[1].iov_base = &metaByte;
                        vec[1].iov_len = 1;

                        // 3. Video Frame (Zero Copy from Encoder output)
                        vec[2].iov_base = pkt->data.frame.buf;
                        vec[2].iov_len = frameSz;

                        // 4. Magic
                        vec[3].iov_base = (void *) magicBytes;
                        vec[3].iov_len = 4;

                        // Broadcast
                        fNetworkServer->Broadcast(vec, 4);
                    }
                }
            }

            // printf("Process Time: %ld us (Wait: %ld us)\n", endProcess - startProcess, waitTime);
            // Only print if taking too long (> 10ms)
        }
        return B_OK;
    }

    void _ChangeResolution(int32 width, int32 height) {
        printf("Resolution Change Requested: %ldx%ld\n", width, height);

        BScreen screen(B_MAIN_SCREEN_ID);
        if (!screen.IsValid()) {
            fprintf(stderr, "BScreen is invalid\n");
            return;
        }

        display_mode *modeList = nullptr;
        display_mode targetMode;
        display_mode currentMode;

        screen.GetMode(&currentMode);

        uint32 count = 0;
        if (screen.GetModeList(&modeList, &count) != B_OK) {
            fprintf(stderr, "Failed to get mode list\n");
            return;
        }

        // Find closest mode
        int32 bestDiff = 0x7FFFFFFF;
        int32 bestIndex = -1;

        for (uint32 i = 0; i < count; i++) {
            int32 w = modeList[i].virtual_width;
            int32 h = modeList[i].virtual_height;

            // Exact Match?
            if (w == width && h == height) {
                bestIndex = i;
                break;
            }

            // Closest by area or distance? Let's do simple pixel difference sum
            long diff = abs(w - width) + abs(h - height);

            if (diff < bestDiff) {
                bestDiff = diff;
                bestIndex = i;
            }
        }

        if (bestIndex >= 0) {
            targetMode = modeList[bestIndex];
        }

        delete[] modeList;

        if (bestIndex >= 0) {
            printf("Switching to closest mode: %dx%d\n",
                   targetMode.virtual_width, targetMode.virtual_height);

            if (targetMode.virtual_width == currentMode.virtual_width &&
                targetMode.virtual_height == currentMode.virtual_height) {
                printf("Already in requested mode.\n");
                return;
            }

            // Stop Capture First
            bool wasCapturing = fCapturing;
            if (wasCapturing) _StopCapture();

            screen.SetMode(&targetMode);

            // Give it a moment to settle
            snooze(200000);

            if (wasCapturing) _StartCapture();
        } else {
            fprintf(stderr, "No suitable mode found\n");
        }
    }

    void _ChangeCodec(const char *codec) {
        printf("Codec Change Requested: %s\n", codec);
        if (fCurrentCodec == codec) return;
        fCurrentCodec = codec;
        // Restart Capture
        bool wasCapturing = fCapturing;
        if (wasCapturing) _StopCapture();
        if (wasCapturing) _StartCapture();
        if (wasCapturing) _StartCapture();
        // If not capturing, it will be used next start
    }

    void _ChangeFps(int32 fps) {
        if (fps < 1) fps = 1;
        if (fps > 120) fps = 120;
        
        fTargetFps = fps;
        fFrameWaitTime = 1000000 / fps; // microseconds per frame
        
        printf("FPS Changed to %d (WaitTime: %ld us)\n", fTargetFps, fFrameWaitTime);
    }

    void _Cleanup() {
    }

    void _HandleClipboard(const char *text) {
        if (!text) return;

        printf("[Server] Handling Clipboard Update: '%s'\n", text);

        BClipboard clipboard("system");
        if (clipboard.Lock()) {
            clipboard.Clear();
            BMessage *clipMsg = clipboard.Data();
            if (clipMsg) {
                clipMsg->AddData("text/plain", B_MIME_TYPE, text, strlen(text));
                status_t ignore = clipboard.Commit();
                printf("[Server] Clipboard Commit Result: %s\n", strerror(ignore));
            } else {
                printf("[Server] Error: Could not get clipboard data container\n");
            }
            clipboard.Unlock();
            printf("[Server] Clipboard updated: %s\n", text);
        } else {
            printf("[Server] Error: Failed to lock clipboard\n");
        }
    }
};

void signal_handler(int sig) {
    printf("\nCaught signal %d, quitting...\n", sig);
    if (be_app) be_app->PostMessage(B_QUIT_REQUESTED);
}

int main() {
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    rename_thread(find_thread(nullptr), "Screen Server");
    ScreenApp app;
    app.Run();
    return 0;
}