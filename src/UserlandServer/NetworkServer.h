/*
 * NetworkServer.h
 * Handles Sockets, HTTP, and WebSocket Protocol (Custom Implementation)
 */
#ifndef NETWORK_SERVER_H
#define NETWORK_SERVER_H

#include <SupportDefs.h>
#include <OS.h>
#include <List.h>
#include <String.h>
#include <Messenger.h>
#include <Locker.h>
#include <Locker.h>
#include <vector>
#include <map>
#include <string>

#include <openssl/ssl.h>
#include <openssl/err.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/uio.h>

#include "VirtualMouse.h"
#include "ScreenCapture.h"


enum {
    MSG_CLIENTS_CONNECTED = 'CLCN',
    MSG_NO_CLIENTS = 'NOCL',
    MSG_UPDATE_BITRATE = 'UPBR',
    MSG_CHANGE_RESOLUTION = 'CHRS',
    MSG_CHANGE_CODEC = 'CHCD',
    MSG_CLIPBOARD_EVENT = 'CLPB',
    MSG_CHANGE_FPS = 'CFPS',
    MSG_WAKE_CAPTURE = 'WKCP'
};

class NetworkServer {
public:
    NetworkServer(port_id inputPort);

    ~NetworkServer();

    status_t Start(uint16 port, const char* certPath = "server.crt", const char* keyPath = "server.key");

    void Stop();

    void SetTarget(BMessenger target);

    void SetScreenCapture(ScreenCapture *screenCapture);

    void WakeCapture();

    // Process socket events (Non-blocking)
    void ProcessEvents();

    // Broadcast frame to all WebSocket clients (Scatter/Gather)
    void Broadcast(const struct iovec *vec, int count);

    // Legacy helper (wraps above)
    void Broadcast(const void *header, size_t headerLen, const void *data, size_t dataLen);

    struct ClientState {
        int socket;
        bool isWebSocket;
        std::vector<uint8> buffer;
        SSL *ssl;
        bool sslAccepted;
    };

    // Accessors for Handlers
    port_id GetInputPort() const { return fInputPort; }
    int32 GetBitrate() const { return fCurrentBitrate; }
    void SetBitrate(int32 bitrate) { fCurrentBitrate = bitrate; }

    void SendMessageToTarget(BMessage *msg);

    void SendToClient(ClientState *client, const void *data, size_t len);

    void SetWelcomeMessage(const char *msg, size_t len) {
        fWelcomeMessage.SetTo(msg, len);
    }

    void SetCursor(float x, float y);

    bool GetLastCursor(float &x, float &y, bigtime_t &time);

private:
    SSL_CTX *fSSLContext;
    BString fWelcomeMessage;

    float fLastX, fLastY;
    bigtime_t fLastCursorTime;

    int fServerSocket;
    BList fClients; // List of ClientState*
    port_id fInputPort;
    bool fRunning;

    BMessenger fTarget;
    int32 fWebSocketClientCount;
    BLocker fLock;
    int32 fCurrentBitrate;

    void _HandleNewConnection();

    // Returns true if connection should be closed
    bool _ParseHTTP(ClientState *client, const char *data, ssize_t len);

    // Returns number of bytes consumed from buffer. 0 if incomplete.
    size_t _ParseWebSocketFrame(ClientState *client);

    void _HandleInputPacket(ClientState *client, uint8 opcode, const uint8 *data, size_t len);

    BString _MakeWebSocketResponse(const char *key);

    BString fLastClipboardData;

    void _CheckClipboard();

    ScreenCapture *fScreenCapture;
};

#endif // NETWORK_SERVER_H